/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*
 Copyright (C) 2008, 2009 Jose Aparicio
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008, 2009 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/pricingengines/midpointcdsengine.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/instruments/claim.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

MidPointCdsEngine::MidPointCdsEngine(const Handle<DefaultProbabilityTermStructure>& probability, Real recoveryRate,
                                     const Handle<YieldTermStructure>& discountCurve,
                                     boost::optional<bool> includeSettlementDateFlows)
    : MidPointCdsEngineBase(discountCurve, includeSettlementDateFlows), probability_(probability),
      recoveryRate_(recoveryRate) {
    registerWith(discountCurve_);
    registerWith(probability_);
}

Real MidPointCdsEngine::survivalProbability(const Date& d) const { return probability_->survivalProbability(d); }

Real MidPointCdsEngine::defaultProbability(const Date& d1, const Date& d2) const {
    return probability_->defaultProbability(d1, d2);
}

Real MidPointCdsEngine::expectedLoss(const Date& defaultDate, const Date& d1, const Date& d2,
                                     const Real notional) const {
    return arguments_.claim->amount(defaultDate, notional, recoveryRate_) * probability_->defaultProbability(d1, d2);
}

void MidPointCdsEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "no discount term structure set");
    QL_REQUIRE(!probability_.empty(), "no probability term structure set");
    MidPointCdsEngineBase::calculate(probability_->referenceDate(), arguments_, results_);
}

void MidPointCdsEngineBase::calculate(const Date& refDate, const CreditDefaultSwap::arguments& arguments,
                                      CreditDefaultSwap::results& results) const {
    Date today = Settings::instance().evaluationDate();
    Date settlementDate = discountCurve_->referenceDate();

    // Upfront amount.
    Real upfPVO1 = 0.0;
    results.upfrontNPV = 0.0;
    if (arguments.upfrontPayment &&
        !arguments.upfrontPayment->hasOccurred(settlementDate, includeSettlementDateFlows_)) {
        upfPVO1 = discountCurve_->discount(arguments.upfrontPayment->date());
        results.upfrontNPV = upfPVO1 * arguments.upfrontPayment->amount();
    }

    // Accrual rebate.
    results.accrualRebateNPV = 0.;
    results.accrualRebateNPVCurrent = 0.;
    if (arguments.accrualRebate && !arguments.accrualRebate->hasOccurred(settlementDate, includeSettlementDateFlows_)) {
        results.accrualRebateNPV =
            discountCurve_->discount(arguments.accrualRebate->date()) * arguments.accrualRebate->amount();
    }
    if (arguments.accrualRebateCurrent &&
        !arguments.accrualRebateCurrent->hasOccurred(settlementDate, includeSettlementDateFlows_)) {
        results.accrualRebateNPVCurrent =
            discountCurve_->discount(arguments.accrualRebateCurrent->date()) * arguments.accrualRebateCurrent->amount();
    }

    results.couponLegNPV = 0.0;
    results.defaultLegNPV = 0.0;

    std::vector<Date> protectionPaymentDates;
    std::vector<Real> midpointDiscounts;
    std::vector<Real> expectedLosses;
    std::vector<Real> defaultProbabilities;

    for (Size i = 0; i < arguments.leg.size(); ++i) {
        if (arguments.leg[i]->hasOccurred(settlementDate, includeSettlementDateFlows_))
            continue;

        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(arguments.leg[i]);
        QL_REQUIRE(coupon, "MidPointCdsEngine: expected coupon, simple cashflows are not allowed");

        // In order to avoid a few switches, we calculate the NPV
        // of both legs as a positive quantity. We'll give them
        // the right sign at the end.

        Date paymentDate = coupon->date(), startDate = coupon->accrualStartDate(), endDate = coupon->accrualEndDate();
        // this is the only point where it might not coincide
        if (i == 0)
            startDate = arguments.protectionStart;
        Date effectiveStartDate = (startDate <= today && today <= endDate) ? today : startDate;
        Date defaultDate = // mid-point
            effectiveStartDate + (endDate - effectiveStartDate) / 2;

        Probability S = survivalProbability(paymentDate);
        Probability P = defaultProbability(effectiveStartDate, endDate);

        Date protectionPaymentDate;
        if (arguments.protectionPaymentTime == CreditDefaultSwap::ProtectionPaymentTime::atDefault) {
            protectionPaymentDate = defaultDate;
        } else if (arguments.protectionPaymentTime == CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd) {
            protectionPaymentDate = paymentDate;
        } else if (arguments.protectionPaymentTime == CreditDefaultSwap::ProtectionPaymentTime::atMaturity) {
            protectionPaymentDate = arguments.maturity;
        } else {
            QL_FAIL("protectionPaymentTime not handled");
        }

        // on one side, we add the fixed rate payments in case of
        // survival...
        results.couponLegNPV += S * coupon->amount() * discountCurve_->discount(paymentDate);
        // ...possibly including accrual in case of default.
        if (arguments.settlesAccrual) {
            results.couponLegNPV +=
                P * coupon->accruedAmount(defaultDate) * discountCurve_->discount(protectionPaymentDate);
        }

        //         on the other side, we add the payment in case of default.
        Real midpointDiscount = discountCurve_->discount(protectionPaymentDate);
        Real expectLoss = expectedLoss(defaultDate, effectiveStartDate, endDate, coupon->nominal());
        results.defaultLegNPV += expectLoss *
                                 midpointDiscount;

        protectionPaymentDates.push_back(protectionPaymentDate);
        midpointDiscounts.push_back(midpointDiscount);
        expectedLosses.push_back(expectLoss);
        defaultProbabilities.push_back(P);
    }

    results.additionalResults["protectionPaymentDates"] = protectionPaymentDates;
    results.additionalResults["midpointDiscounts"] = midpointDiscounts;
    results.additionalResults["expectedLosses"] = expectedLosses;
    results.additionalResults["defaultProbabilities"] = defaultProbabilities;

    Real upfrontSign = 1.0;
    switch (arguments.side) {
    case Protection::Seller:
        results.defaultLegNPV *= -1.0;
        results.accrualRebateNPV *= -1.0;
        results.accrualRebateNPVCurrent *= -1.0;
        break;
    case Protection::Buyer:
        results.couponLegNPV *= -1.0;
        results.upfrontNPV *= -1.0;
        upfrontSign = -1.0;
        break;
    default:
        QL_FAIL("unknown protection side");
    }

    results.value = results.defaultLegNPV + results.couponLegNPV + results.upfrontNPV + results.accrualRebateNPV;
    results.errorEstimate = Null<Real>();

    if (results.couponLegNPV != 0.0) {
        results.fairSpreadDirty =
            -results.defaultLegNPV * arguments.spread / (results.couponLegNPV + results.accrualRebateNPV);
        results.fairSpreadClean =
            -results.defaultLegNPV * arguments.spread / (results.couponLegNPV + results.accrualRebateNPVCurrent);
    } else {
        results.fairSpreadDirty = Null<Rate>();
        results.fairSpreadClean = Null<Rate>();
    }

    Real upfrontSensitivity = upfPVO1 * arguments.notional;
    if (upfrontSensitivity > 0.0) {
        results.fairUpfront = -upfrontSign * (results.defaultLegNPV + results.couponLegNPV +
            results.accrualRebateNPV) / upfrontSensitivity;
    } else {
        results.fairUpfront = Null<Rate>();
    }

    static const Rate basisPoint = 1.0e-4;

    if (arguments.spread != 0.0) {
        results.couponLegBPS = results.couponLegNPV * basisPoint / arguments.spread;
    } else {
        results.couponLegBPS = Null<Rate>();
    }

    if (arguments.upfront && *arguments.upfront != 0.0) {
        results.upfrontBPS = results.upfrontNPV * basisPoint / (*arguments.upfront);
    } else {
        results.upfrontBPS = Null<Rate>();
    }

    results.additionalResults["upfrontPremium"] = arguments.upfrontPayment->amount();
    results.additionalResults["upfrontPremiumNPV"] = results.upfrontNPV;
    results.additionalResults["premiumLegNPVDirty"] = results.couponLegNPV;
    results.additionalResults["premiumLegNPVClean"] = results.couponLegNPV + results.accrualRebateNPVCurrent;
    results.additionalResults["accrualRebateNPV"] = results.accrualRebateNPV;
    results.additionalResults["accrualRebateNPVCurrent"] = results.accrualRebateNPVCurrent;
    results.additionalResults["protectionLegNPV"] = results.defaultLegNPV;
    results.additionalResults["fairSpreadDirty"] = results.fairSpreadDirty;
    results.additionalResults["fairSpreadClean"] = results.fairSpreadClean;
    results.additionalResults["fairUpfront"] = results.fairUpfront;
    results.additionalResults["couponLegBPS"] = results.couponLegBPS;
    results.additionalResults["upfrontBPS"] = results.upfrontBPS;

} // MidPointCdsEngineBase::calculate()

} // namespace QuantExt
