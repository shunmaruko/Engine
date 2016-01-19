/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file analyticlgmswaptionengine.hpp
    \brief analytic engine for european swaptions in the LGM model
*/

#ifndef quantext_analytic_lgm_swaption_engine_hpp
#define quantext_analytic_lgm_swaption_engine_hpp

#include <ql/instruments/swaption.hpp>
#include <qle/models/lgm.hpp>

namespace QuantExt {

//! Analytic LGM swaption engine for european exercise
/*! \ingroup swaptionengines

    All fixed coupons with start date greater or equal to the respective
    option expiry are considered to be
    part of the exercise into right.

    References:

    Hagan, Evaluating and hedging exotic swap instruments via LGM

    Lichters, Stamm, Gallagher: Modern Derivatives Pricing and Credit Exposure
    Analysis, Palgrave Macmillan, 2015, 11.2.2

    \warning Cash settled swaptions are not supported

    The basis between the given discounting curve (or - if not given - the
    model curve) and the forwarding curve attached to the underlying swap's
    ibor index is taken into account by a static correction spread for
    the underlying's fixed leg. Likewise a spread on the floating leg is
    taken into account.

    Note that we assume H'>0, but this is a general requirement of the
    LGM parametrization anyway (see the base parametrization class).
*/

class AnalyticLgmSwaptionEngine
    : public GenericEngine<Swaption::arguments, Swaption::results> {

  public:
    /*! nextCoupon is Mapping A, proRata is Mapping B
        in Lichters, Stamm, Gallagher (2015), 11.2.2 */
    enum FloatSpreadMapping { nextCoupon, proRata };

    /*! Lgm model based constructor */
    AnalyticLgmSwaptionEngine(
        const boost::shared_ptr<Lgm> &model,
        const Handle<YieldTermStructure> &discountCurve =
            Handle<YieldTermStructure>(),
        const FloatSpreadMapping floatSpreadMapping = proRata);

    /*! XAsset model based constructor */
    AnalyticLgmSwaptionEngine(
        const boost::shared_ptr<XAssetModel> &model, const Size ccy,
        const Handle<YieldTermStructure> &discountCurve =
            Handle<YieldTermStructure>(),
        const FloatSpreadMapping floatSpreadMapping = proRata);

    /*! parametrization based constructor, note that updates in the
        parametrization are not observed by the engine, you would
        have to call update() on the engine explicitly */
    AnalyticLgmSwaptionEngine(
        const boost::shared_ptr<IrLgm1fParametrization> irlgm1f,
        const Handle<YieldTermStructure> &discountCurve =
            Handle<YieldTermStructure>(),
        const FloatSpreadMapping floatSpreadMapping = proRata);

    void calculate() const;

  private:
    Real yStarHelper(const Real y) const;
    const boost::shared_ptr<IrLgm1fParametrization> p_;
    const Handle<YieldTermStructure> c_;
    const FloatSpreadMapping floatSpreadMapping_;
    mutable Real H0_, D0_, zetaex_, S_m1;
    mutable std::vector<Real> S_, Hj_, Dj_;
    mutable Size j1_, k1_;
};

} // namespace QuantExt

#endif
