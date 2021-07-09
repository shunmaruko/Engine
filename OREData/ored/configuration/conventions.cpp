/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file configuration/conventions.cpp
    \brief Currency and instrument specific conventions
    \ingroup
*/

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

using namespace QuantLib;
using namespace std;
using boost::lexical_cast;
using QuantExt::SubPeriodsCoupon1;

namespace {

// TODO move to parsers
SubPeriodsCoupon1::Type parseSubPeriodsCouponType(const string& s) {
    if (s == "Compounding")
        return SubPeriodsCoupon1::Compounding;
    else if (s == "Averaging")
        return SubPeriodsCoupon1::Averaging;
    else
        QL_FAIL("SubPeriodsCoupon type " << s << " not recognized");
};

void checkContinuationMappings(const map<Natural, Natural>& mp, const string& name) {
    
    Natural previousValue = 0;
    for (const auto& kv : mp) {
        QL_REQUIRE(kv.first <= kv.second, "Not allowed a " << name << " continuation mapping where From (" <<
            kv.first << ") is greater than To (" << kv.second << ").");
        QL_REQUIRE(kv.second > previousValue, "The To " << name << " continuation mappings should be strictly " <<
            "increasing but got " << kv.second << " <= " << previousValue);
        previousValue = kv.second;
    }
}

} // namespace

namespace ore {
namespace data {

namespace {
// helper that returns an Ibor or Overnight convention if this exists or a nullptr otherwise
boost::shared_ptr<Convention> getIborOrOvernightConvention(const Conventions* c, const string& s) {
    if (c != nullptr && (c->has(s, Convention::Type::IborIndex) || c->has(s, Convention::Type::OvernightIndex))) {
        return c->get(s);
    } else {
        return nullptr;
    }
}
} // namespace

Convention::Convention(const string& id, Type type) : type_(type), id_(id) {}

ZeroRateConvention::ZeroRateConvention(const string& id, const string& dayCounter, const string& compounding,
                                       const string& compoundingFrequency)
    : Convention(id, Type::Zero), tenorBased_(false), strDayCounter_(dayCounter), strCompounding_(compounding),
      strCompoundingFrequency_(compoundingFrequency) {
    build();
}

ZeroRateConvention::ZeroRateConvention(const string& id, const string& dayCounter, const string& tenorCalendar,
                                       const string& compounding, const string& compoundingFrequency,
                                       const string& spotLag, const string& spotCalendar, const string& rollConvention,
                                       const string& eom)
    : Convention(id, Type::Zero), tenorBased_(true), strDayCounter_(dayCounter), strTenorCalendar_(tenorCalendar),
      strCompounding_(compounding), strCompoundingFrequency_(compoundingFrequency), strSpotLag_(spotLag),
      strSpotCalendar_(spotCalendar), strRollConvention_(rollConvention), strEom_(eom) {
    build();
}

void ZeroRateConvention::build() {
    dayCounter_ = parseDayCounter(strDayCounter_);
    compounding_ = strCompounding_.empty() ? Continuous : parseCompounding(strCompounding_);
    compoundingFrequency_ = strCompoundingFrequency_.empty() ? Annual : parseFrequency(strCompoundingFrequency_);
    if (tenorBased_) {
        tenorCalendar_ = parseCalendar(strTenorCalendar_);
        spotLag_ = strSpotLag_.empty() ? 0 : lexical_cast<Natural>(strSpotLag_);
        spotCalendar_ = strSpotCalendar_.empty() ? NullCalendar() : parseCalendar(strSpotCalendar_);
        rollConvention_ = strRollConvention_.empty() ? Following : parseBusinessDayConvention(strRollConvention_);
        eom_ = strEom_.empty() ? false : parseBool(strEom_);
    }
}

void ZeroRateConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Zero");
    type_ = Type::Zero;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    tenorBased_ = XMLUtils::getChildValueAsBool(node, "TenorBased", true);

    // Get string values from xml
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strCompoundingFrequency_ = XMLUtils::getChildValue(node, "CompoundingFrequency", false);
    strCompounding_ = XMLUtils::getChildValue(node, "Compounding", false);
    if (tenorBased_) {
        strTenorCalendar_ = XMLUtils::getChildValue(node, "TenorCalendar", true);
        strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", false);
        strSpotCalendar_ = XMLUtils::getChildValue(node, "SpotCalendar", false);
        strRollConvention_ = XMLUtils::getChildValue(node, "RollConvention", false);
        strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    }
    build();
}

XMLNode* ZeroRateConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("Zero");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "TenorBased", tenorBased_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "CompoundingFrequency", strCompoundingFrequency_);
    XMLUtils::addChild(doc, node, "Compounding", strCompounding_);
    if (tenorBased_) {
        XMLUtils::addChild(doc, node, "TenorCalendar", strTenorCalendar_);
        XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
        XMLUtils::addChild(doc, node, "SpotCalendar", strSpotCalendar_);
        XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);
        XMLUtils::addChild(doc, node, "EOM", strEom_);
    }

    return node;
}

DepositConvention::DepositConvention(const string& id, const string& index)
    : Convention(id, Type::Deposit), index_(index), indexBased_(true) {}

DepositConvention::DepositConvention(const string& id, const string& calendar, const string& convention,
                                     const string& eom, const string& dayCounter, const string& settlementDays)
    : Convention(id, Type::Deposit), indexBased_(false), strCalendar_(calendar), strConvention_(convention),
      strEom_(eom), strDayCounter_(dayCounter), strSettlementDays_(settlementDays) {
    build();
}

void DepositConvention::build() {
    calendar_ = parseCalendar(strCalendar_);
    convention_ = parseBusinessDayConvention(strConvention_);
    eom_ = parseBool(strEom_);
    dayCounter_ = parseDayCounter(strDayCounter_);
    settlementDays_ = parseInteger(strSettlementDays_);
}

void DepositConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Deposit");
    type_ = Type::Deposit;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    indexBased_ = XMLUtils::getChildValueAsBool(node, "IndexBased", true);

    // Get string values from xml
    if (indexBased_) {
        index_ = XMLUtils::getChildValue(node, "Index", true);
    } else {
        strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
        strConvention_ = XMLUtils::getChildValue(node, "Convention", true);
        strEom_ = XMLUtils::getChildValue(node, "EOM", true);
        strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
        strSettlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", true);
        build();
    }
}

XMLNode* DepositConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("Deposit");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "IndexBased", indexBased_);
    if (indexBased_) {
        XMLUtils::addChild(doc, node, "Index", index_);
    } else {
        XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
        XMLUtils::addChild(doc, node, "Convention", strConvention_);
        XMLUtils::addChild(doc, node, "EOM", strEom_);
        XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
        XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    }

    return node;
}

FutureConvention::FutureConvention(const string& id, const string& index, const Conventions* conventions)
    : FutureConvention(id, index, QuantLib::RateAveraging::Type::Compound, DateGenerationRule::IMM, conventions) {}

FutureConvention::FutureConvention(const string& id, const string& index,
                                   const QuantLib::RateAveraging::Type overnightIndexFutureNettingType,
                                   const DateGenerationRule dateGenerationRule, const Conventions* conventions)
    : Convention(id, Type::Future), strIndex_(index),
      index_(parseIborIndex(strIndex_, Handle<YieldTermStructure>(),
                            getIborOrOvernightConvention(conventions, strIndex_))),
      overnightIndexFutureNettingType_(overnightIndexFutureNettingType), conventions_(conventions) {}

void FutureConvention::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Future");
    type_ = Type::Future;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    index_ =
        parseIborIndex(strIndex_, Handle<YieldTermStructure>(), getIborOrOvernightConvention(conventions_, strIndex_));
    string nettingTypeStr = XMLUtils::getChildValue(node, "OvernightIndexFutureNettingType", false);
    overnightIndexFutureNettingType_ =
        nettingTypeStr.empty() ? RateAveraging::Type::Compound : parseOvernightIndexFutureNettingType(nettingTypeStr);
    string dateGenerationStr = XMLUtils::getChildValue(node, "DateGenerationRule", false);
    dateGenerationRule_ =
        dateGenerationStr.empty() ? DateGenerationRule::IMM : parseFutureDateGenerationRule(dateGenerationStr);
}

XMLNode* FutureConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("Future");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "OvernightIndexFutureNettingType", ore::data::to_string(overnightIndexFutureNettingType_));
    XMLUtils::addChild(doc, node, "DateGenerationRule", ore::data::to_string(dateGenerationRule_));
    return node;
}

FraConvention::FraConvention(const string& id, const string& index, const Conventions* conventions)
    : Convention(id, Type::FRA), strIndex_(index),
      index_(parseIborIndex(strIndex_, Handle<YieldTermStructure>(),
                            getIborOrOvernightConvention(conventions, strIndex_))),
      conventions_(conventions) {}

void FraConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "FRA");
    type_ = Type::FRA;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    index_ =
        parseIborIndex(strIndex_, Handle<YieldTermStructure>(), getIborOrOvernightConvention(conventions_, strIndex_));
}

XMLNode* FraConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("FRA");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);

    return node;
}

OisConvention::OisConvention(const string& id, const string& spotLag, const string& index,
                             const string& fixedDayCounter, const string& paymentLag, const string& eom,
                             const string& fixedFrequency, const string& fixedConvention,
                             const string& fixedPaymentConvention, const string& rule, const string& paymentCal,
                             const Conventions* conventions)
    : Convention(id, Type::OIS), strSpotLag_(spotLag), strIndex_(index), strFixedDayCounter_(fixedDayCounter),
      strPaymentLag_(paymentLag), strEom_(eom), strFixedFrequency_(fixedFrequency),
      strFixedConvention_(fixedConvention), strFixedPaymentConvention_(fixedPaymentConvention), strRule_(rule),
      strPaymentCal_(paymentCal), conventions_(conventions) {
    build();
}

void OisConvention::build() {
    // First check that we have an overnight index.
    index_ = boost::dynamic_pointer_cast<OvernightIndex>(
        parseIborIndex(strIndex_, Handle<YieldTermStructure>(), getIborOrOvernightConvention(conventions_, strIndex_)));
    QL_REQUIRE(index_, "The index string, " << strIndex_ << ", does not represent an overnight index.");

    spotLag_ = lexical_cast<Natural>(strSpotLag_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    paymentLag_ = strPaymentLag_.empty() ? 0 : lexical_cast<Natural>(strPaymentLag_);
    eom_ = strEom_.empty() ? false : parseBool(strEom_);
    fixedFrequency_ = strFixedFrequency_.empty() ? Annual : parseFrequency(strFixedFrequency_);
    fixedConvention_ = strFixedConvention_.empty() ? Following : parseBusinessDayConvention(strFixedConvention_);
    fixedPaymentConvention_ =
        strFixedPaymentConvention_.empty() ? Following : parseBusinessDayConvention(strFixedPaymentConvention_);
    rule_ = strRule_.empty() ? DateGeneration::Backward : parseDateGenerationRule(strRule_);
    paymentCal_ = strPaymentCal_.empty() ? Calendar() : parseCalendar(strPaymentCal_);
}

void OisConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "OIS");
    type_ = Type::OIS;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);
    strPaymentLag_ = XMLUtils::getChildValue(node, "PaymentLag", false);
    strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", false);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", false);
    strFixedPaymentConvention_ = XMLUtils::getChildValue(node, "FixedPaymentConvention", false);
    strRule_ = XMLUtils::getChildValue(node, "Rule", false);
    strPaymentCal_ = XMLUtils::getChildValue(node, "PaymentCalendar", false);

    build();
}

XMLNode* OisConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("OIS");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "PaymentLag", strPaymentLag_);
    XMLUtils::addChild(doc, node, "EOM", strEom_);
    XMLUtils::addChild(doc, node, "FixedFrequency", strFixedFrequency_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedPaymentConvention", strFixedPaymentConvention_);
    XMLUtils::addChild(doc, node, "Rule", strRule_);
    XMLUtils::addChild(doc, node, "PaymentCalendar", strPaymentCal_);

    return node;
}

IborIndexConvention::IborIndexConvention(const string& id, const string& fixingCalendar, const string& dayCounter,
                                         const Size settlementDays, const string& businessDayConvention,
                                         const bool endOfMonth)
    : Convention(id, Type::IborIndex), localId_(id), strFixingCalendar_(fixingCalendar), strDayCounter_(dayCounter),
      settlementDays_(settlementDays), strBusinessDayConvention_(businessDayConvention), endOfMonth_(endOfMonth) {
    build();
}

void IborIndexConvention::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "IborIndex");
    type_ = Type::IborIndex;
    localId_ = XMLUtils::getChildValue(node, "Id", true);
    strFixingCalendar_ = XMLUtils::getChildValue(node, "FixingCalendar", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    settlementDays_ = XMLUtils::getChildValueAsInt(node, "SettlementDays", true);
    strBusinessDayConvention_ = XMLUtils::getChildValue(node, "BusinessDayConvention", true);
    endOfMonth_ = XMLUtils::getChildValueAsBool(node, "EndOfMonth", true);
    build();
}

XMLNode* IborIndexConvention::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("IborIndex");
    XMLUtils::addChild(doc, node, "Id", localId_);
    XMLUtils::addChild(doc, node, "FixingCalendar", strFixingCalendar_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "SettlementDays", static_cast<int>(settlementDays_));
    XMLUtils::addChild(doc, node, "BusinessDayConvention", strBusinessDayConvention_);
    XMLUtils::addChild(doc, node, "EndOfMonth", endOfMonth_);
    return node;
}

void IborIndexConvention::build() {
    // just a check really that the id is consistent with the ibor index name rules
    vector<string> tokens;
    split(tokens, localId_, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2 || tokens.size() == 3,
               "Two or three tokens required in IborIndexConvention " << localId_ << ": CCY-INDEX or CCY-INDEX-TERM");

    // set the Id - this converts the local id term from "7D" to "1W", "28D" to "1M" etc, so if can be picked
    // up by searches
    id_ = (tokens.size() == 3) ? (tokens[0] + "-" + tokens[1] + "-" + to_string(parsePeriod(tokens[2]))) : localId_;
}

OvernightIndexConvention::OvernightIndexConvention(const string& id, const string& fixingCalendar,
                                                   const string& dayCounter, const Size settlementDays)
    : Convention(id, Type::OvernightIndex), strFixingCalendar_(fixingCalendar), strDayCounter_(dayCounter),
      settlementDays_(settlementDays) {
    build();
}

void OvernightIndexConvention::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "OvernightIndex");
    type_ = Type::OvernightIndex;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strFixingCalendar_ = XMLUtils::getChildValue(node, "FixingCalendar", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    settlementDays_ = XMLUtils::getChildValueAsInt(node, "SettlementDays", true);
    build();
}

XMLNode* OvernightIndexConvention::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("OvernightIndex");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "FixingCalendar", strFixingCalendar_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "SettlementDays", static_cast<int>(settlementDays_));
    return node;
}

void OvernightIndexConvention::build() {
    // just a check really that the id is consistent with the ibor index name rules
    vector<string> tokens;
    split(tokens, id_, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2, "Two tokens required in OvernightIndexConvention " << id_ << ": CCY-INDEX");
}

SwapIndexConvention::SwapIndexConvention(const string& id, const string& conventions, const string& fixingCalendar)
    : Convention(id, Type::SwapIndex), strConventions_(conventions), fixingCalendar_(fixingCalendar) {}

void SwapIndexConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "SwapIndex");
    type_ = Type::SwapIndex;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strConventions_ = XMLUtils::getChildValue(node, "Conventions", true);
    fixingCalendar_ = XMLUtils::getChildValue(node, "FixingCalendar", false);
}

XMLNode* SwapIndexConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("SwapIndex");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Conventions", strConventions_);
    XMLUtils::addChild(doc, node, "FixingCalendar", fixingCalendar_);

    return node;
}

IRSwapConvention::IRSwapConvention(const string& id, const string& fixedCalendar, const string& fixedFrequency,
                                   const string& fixedConvention, const string& fixedDayCounter, const string& index,
                                   bool hasSubPeriod, const string& floatFrequency, const string& subPeriodsCouponType,
                                   const Conventions* conventions)
    : Convention(id, Type::Swap), hasSubPeriod_(hasSubPeriod), strFixedCalendar_(fixedCalendar),
      strFixedFrequency_(fixedFrequency), strFixedConvention_(fixedConvention), strFixedDayCounter_(fixedDayCounter),
      strIndex_(index), strFloatFrequency_(floatFrequency), strSubPeriodsCouponType_(subPeriodsCouponType),
      conventions_(conventions) {
    build();
}

void IRSwapConvention::build() {
    fixedCalendar_ = parseCalendar(strFixedCalendar_);
    fixedFrequency_ = parseFrequency(strFixedFrequency_);
    fixedConvention_ = parseBusinessDayConvention(strFixedConvention_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    index_ =
        parseIborIndex(strIndex_, Handle<YieldTermStructure>(), getIborOrOvernightConvention(conventions_, strIndex_));

    if (hasSubPeriod_) {
        floatFrequency_ = parseFrequency(strFloatFrequency_);
        subPeriodsCouponType_ = parseSubPeriodsCouponType(strSubPeriodsCouponType_);
    } else {
        floatFrequency_ = NoFrequency;
        subPeriodsCouponType_ = SubPeriodsCoupon1::Compounding;
    }
}

void IRSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Swap");
    type_ = Type::Swap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strFixedCalendar_ = XMLUtils::getChildValue(node, "FixedCalendar", true);
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", true);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);

    // optional
    strFloatFrequency_ = XMLUtils::getChildValue(node, "FloatFrequency", false);
    strSubPeriodsCouponType_ = XMLUtils::getChildValue(node, "SubPeriodsCouponType", false);
    hasSubPeriod_ = (strFloatFrequency_ != "");

    build();
}

XMLNode* IRSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("Swap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "FixedCalendar", strFixedCalendar_);
    XMLUtils::addChild(doc, node, "FixedFrequency", strFixedFrequency_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    if (hasSubPeriod_) {
        XMLUtils::addChild(doc, node, "FloatFrequency", strFloatFrequency_);
        XMLUtils::addChild(doc, node, "SubPeriodsCouponType", strSubPeriodsCouponType_);
    }

    return node;
}

AverageOisConvention::AverageOisConvention(const string& id, const string& spotLag, const string& fixedTenor,
                                           const string& fixedDayCounter, const string& fixedCalendar,
                                           const string& fixedConvention, const string& fixedPaymentConvention,
                                           const string& index, const string& onTenor, const string& rateCutoff,
                                           const Conventions* conventions)
    : Convention(id, Type::AverageOIS), strSpotLag_(spotLag), strFixedTenor_(fixedTenor),
      strFixedDayCounter_(fixedDayCounter), strFixedCalendar_(fixedCalendar), strFixedConvention_(fixedConvention),
      strFixedPaymentConvention_(fixedPaymentConvention), strIndex_(index), strOnTenor_(onTenor),
      strRateCutoff_(rateCutoff), conventions_(conventions) {
    build();
}

void AverageOisConvention::build() {
    // First check that we have an overnight index.
    index_ = boost::dynamic_pointer_cast<OvernightIndex>(
        parseIborIndex(strIndex_, Handle<YieldTermStructure>(), getIborOrOvernightConvention(conventions_, strIndex_)));
    QL_REQUIRE(index_, "The index string, " << strIndex_ << ", does not represent an overnight index.");

    spotLag_ = lexical_cast<Natural>(strSpotLag_);
    fixedTenor_ = parsePeriod(strFixedTenor_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    fixedCalendar_ = parseCalendar(strFixedCalendar_);
    fixedConvention_ = parseBusinessDayConvention(strFixedConvention_);
    fixedPaymentConvention_ = parseBusinessDayConvention(strFixedPaymentConvention_);
    onTenor_ = parsePeriod(strOnTenor_);
    rateCutoff_ = lexical_cast<Natural>(strRateCutoff_);
}

void AverageOisConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "AverageOIS");
    type_ = Type::AverageOIS;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", true);
    strFixedTenor_ = XMLUtils::getChildValue(node, "FixedTenor", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);
    strFixedCalendar_ = XMLUtils::getChildValue(node, "FixedCalendar", true);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", true);
    strFixedPaymentConvention_ = XMLUtils::getChildValue(node, "FixedPaymentConvention", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strOnTenor_ = XMLUtils::getChildValue(node, "OnTenor", true);
    strRateCutoff_ = XMLUtils::getChildValue(node, "RateCutoff", true);

    build();
}

XMLNode* AverageOisConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("AverageOIS");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
    XMLUtils::addChild(doc, node, "FixedTenor", strFixedTenor_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "FixedCalendar", strFixedCalendar_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedPaymentConvention", strFixedPaymentConvention_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "OnTenor", strOnTenor_);
    XMLUtils::addChild(doc, node, "RateCutoff", strRateCutoff_);

    return node;
}

TenorBasisSwapConvention::TenorBasisSwapConvention(const string& id, const string& longIndex, const string& shortIndex,
                                                   const string& shortPayTenor, const string& spreadOnShort,
                                                   const string& includeSpread, const string& subPeriodsCouponType,
                                                   const Conventions* conventions)
    : Convention(id, Type::TenorBasisSwap), strLongIndex_(longIndex), strShortIndex_(shortIndex),
      strShortPayTenor_(shortPayTenor), strSpreadOnShort_(spreadOnShort), strIncludeSpread_(includeSpread),
      strSubPeriodsCouponType_(subPeriodsCouponType), conventions_(conventions) {
    build();
}

void TenorBasisSwapConvention::build() {
    longIndex_ = parseIborIndex(strLongIndex_, Handle<YieldTermStructure>(),
                                getIborOrOvernightConvention(conventions_, strLongIndex_));
    shortIndex_ = parseIborIndex(strShortIndex_, Handle<YieldTermStructure>(),
                                 getIborOrOvernightConvention(conventions_, strShortIndex_));
    shortPayTenor_ = strShortPayTenor_.empty() ? shortIndex_->tenor() : parsePeriod(strShortPayTenor_);
    spreadOnShort_ = strSpreadOnShort_.empty() ? true : parseBool(strSpreadOnShort_);
    includeSpread_ = strIncludeSpread_.empty() ? false : parseBool(strIncludeSpread_);
    subPeriodsCouponType_ = strSubPeriodsCouponType_.empty() ? SubPeriodsCoupon1::Compounding
                                                             : parseSubPeriodsCouponType(strSubPeriodsCouponType_);
}

void TenorBasisSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "TenorBasisSwap");
    type_ = Type::TenorBasisSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strLongIndex_ = XMLUtils::getChildValue(node, "LongIndex", true);
    strShortIndex_ = XMLUtils::getChildValue(node, "ShortIndex", true);
    strShortPayTenor_ = XMLUtils::getChildValue(node, "ShortPayTenor", false);
    strSpreadOnShort_ = XMLUtils::getChildValue(node, "SpreadOnShort", false);
    strIncludeSpread_ = XMLUtils::getChildValue(node, "IncludeSpread", false);
    strSubPeriodsCouponType_ = XMLUtils::getChildValue(node, "SubPeriodsCouponType", false);

    build();
}

XMLNode* TenorBasisSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("TenorBasisSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "LongIndex", strLongIndex_);
    XMLUtils::addChild(doc, node, "ShortIndex", strShortIndex_);
    XMLUtils::addChild(doc, node, "ShortPayTenor", strShortPayTenor_);
    XMLUtils::addChild(doc, node, "SpreadOnShort", strSpreadOnShort_);
    XMLUtils::addChild(doc, node, "IncludeSpread", strIncludeSpread_);
    if (strSubPeriodsCouponType_ != "")
        XMLUtils::addChild(doc, node, "SubPeriodsCouponType", strSubPeriodsCouponType_);
    return node;
}

TenorBasisTwoSwapConvention::TenorBasisTwoSwapConvention(
    const string& id, const string& calendar, const string& longFixedFrequency, const string& longFixedConvention,
    const string& longFixedDayCounter, const string& longIndex, const string& shortFixedFrequency,
    const string& shortFixedConvention, const string& shortFixedDayCounter, const string& shortIndex,
    const string& longMinusShort, const Conventions* conventions)
    : Convention(id, Type::TenorBasisTwoSwap), strCalendar_(calendar), strLongFixedFrequency_(longFixedFrequency),
      strLongFixedConvention_(longFixedConvention), strLongFixedDayCounter_(longFixedDayCounter),
      strLongIndex_(longIndex), strShortFixedFrequency_(shortFixedFrequency),
      strShortFixedConvention_(shortFixedConvention), strShortFixedDayCounter_(shortFixedDayCounter),
      strShortIndex_(shortIndex), strLongMinusShort_(longMinusShort), conventions_(conventions) {
    build();
}

void TenorBasisTwoSwapConvention::build() {
    calendar_ = parseCalendar(strCalendar_);
    longFixedFrequency_ = parseFrequency(strLongFixedFrequency_);
    longFixedConvention_ = parseBusinessDayConvention(strLongFixedConvention_);
    longFixedDayCounter_ = parseDayCounter(strLongFixedDayCounter_);
    longIndex_ = parseIborIndex(strLongIndex_, Handle<YieldTermStructure>(),
                                getIborOrOvernightConvention(conventions_, strLongIndex_));
    shortFixedFrequency_ = parseFrequency(strShortFixedFrequency_);
    shortFixedConvention_ = parseBusinessDayConvention(strShortFixedConvention_);
    shortFixedDayCounter_ = parseDayCounter(strShortFixedDayCounter_);
    shortIndex_ = parseIborIndex(strShortIndex_, Handle<YieldTermStructure>(),
                                 getIborOrOvernightConvention(conventions_, strShortIndex_));
    longMinusShort_ = strLongMinusShort_.empty() ? true : parseBool(strLongMinusShort_);
}

void TenorBasisTwoSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "TenorBasisTwoSwap");
    type_ = Type::TenorBasisTwoSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
    strLongFixedFrequency_ = XMLUtils::getChildValue(node, "LongFixedFrequency", true);
    strLongFixedConvention_ = XMLUtils::getChildValue(node, "LongFixedConvention", true);
    strLongFixedDayCounter_ = XMLUtils::getChildValue(node, "LongFixedDayCounter", true);
    strLongIndex_ = XMLUtils::getChildValue(node, "LongIndex", true);
    strShortFixedFrequency_ = XMLUtils::getChildValue(node, "ShortFixedFrequency", true);
    strShortFixedConvention_ = XMLUtils::getChildValue(node, "ShortFixedConvention", true);
    strShortFixedDayCounter_ = XMLUtils::getChildValue(node, "ShortFixedDayCounter", true);
    strShortIndex_ = XMLUtils::getChildValue(node, "ShortIndex", true);
    strLongMinusShort_ = XMLUtils::getChildValue(node, "LongMinusShort", false);

    build();
}

XMLNode* TenorBasisTwoSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("TenorBasisTwoSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
    XMLUtils::addChild(doc, node, "LongFixedFrequency", strLongFixedFrequency_);
    XMLUtils::addChild(doc, node, "LongFixedConvention", strLongFixedConvention_);
    XMLUtils::addChild(doc, node, "LongFixedDayCounter", strLongFixedDayCounter_);
    XMLUtils::addChild(doc, node, "LongIndex", strLongIndex_);
    XMLUtils::addChild(doc, node, "ShortFixedFrequency", strShortFixedFrequency_);
    XMLUtils::addChild(doc, node, "ShortFixedConvention", strShortFixedConvention_);
    XMLUtils::addChild(doc, node, "ShortFixedDayCounter", strShortFixedDayCounter_);
    XMLUtils::addChild(doc, node, "ShortIndex", strShortIndex_);
    XMLUtils::addChild(doc, node, "LongMinusShort", strLongMinusShort_);

    return node;
}

BMABasisSwapConvention::BMABasisSwapConvention(const string& id, const string& longIndex, const string& shortIndex,
                                               const Conventions* conventions)
    : Convention(id, Type::BMABasisSwap), strLiborIndex_(longIndex), strBmaIndex_(shortIndex),
      conventions_(conventions) {
    build();
}

void BMABasisSwapConvention::build() {
    liborIndex_ = parseIborIndex(strLiborIndex_, Handle<YieldTermStructure>(),
                                 getIborOrOvernightConvention(conventions_, strLiborIndex_));
    bmaIndex_ = boost::dynamic_pointer_cast<QuantExt::BMAIndexWrapper>(parseIborIndex(
        strBmaIndex_, Handle<YieldTermStructure>(), getIborOrOvernightConvention(conventions_, strBmaIndex_)));
}

void BMABasisSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "BMABasisSwap");
    type_ = Type::BMABasisSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strLiborIndex_ = XMLUtils::getChildValue(node, "LiborIndex", true);
    strBmaIndex_ = XMLUtils::getChildValue(node, "BMAIndex", true);

    build();
}

XMLNode* BMABasisSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("BMABasisSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "LiborIndex", strLiborIndex_);
    XMLUtils::addChild(doc, node, "BMAIndex", strBmaIndex_);

    return node;
}

FXConvention::FXConvention(const string& id, const string& spotDays, const string& sourceCurrency,
                           const string& targetCurrency, const string& pointsFactor, const string& advanceCalendar,
                           const string& spotRelative)
    : Convention(id, Type::FX), strSpotDays_(spotDays), strSourceCurrency_(sourceCurrency),
      strTargetCurrency_(targetCurrency), strPointsFactor_(pointsFactor), strAdvanceCalendar_(advanceCalendar),
      strSpotRelative_(spotRelative) {
    build();
}

void FXConvention::build() {
    spotDays_ = lexical_cast<Natural>(strSpotDays_);
    sourceCurrency_ = parseCurrency(strSourceCurrency_);
    targetCurrency_ = parseCurrency(strTargetCurrency_);
    pointsFactor_ = parseReal(strPointsFactor_);
    advanceCalendar_ = strAdvanceCalendar_.empty() ? NullCalendar() : parseCalendar(strAdvanceCalendar_);
    spotRelative_ = strSpotRelative_.empty() ? true : parseBool(strSpotRelative_);
}

void FXConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "FX");
    type_ = Type::FX;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSpotDays_ = XMLUtils::getChildValue(node, "SpotDays", true);
    strSourceCurrency_ = XMLUtils::getChildValue(node, "SourceCurrency", true);
    strTargetCurrency_ = XMLUtils::getChildValue(node, "TargetCurrency", true);
    strPointsFactor_ = XMLUtils::getChildValue(node, "PointsFactor", true);
    strAdvanceCalendar_ = XMLUtils::getChildValue(node, "AdvanceCalendar", false);
    strSpotRelative_ = XMLUtils::getChildValue(node, "SpotRelative", false);

    build();
}

XMLNode* FXConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("FX");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotDays", strSpotDays_);
    XMLUtils::addChild(doc, node, "SourceCurrency", strSourceCurrency_);
    XMLUtils::addChild(doc, node, "TargetCurrency", strTargetCurrency_);
    XMLUtils::addChild(doc, node, "PointsFactor", strPointsFactor_);
    XMLUtils::addChild(doc, node, "AdvanceCalendar", strAdvanceCalendar_);
    XMLUtils::addChild(doc, node, "SpotRelative", strSpotRelative_);

    return node;
}

CrossCcyBasisSwapConvention::CrossCcyBasisSwapConvention(
    const string& id, const string& strSettlementDays, const string& strSettlementCalendar,
    const string& strRollConvention, const string& flatIndex, const string& spreadIndex, const string& strEom,
    const string& strIsResettable, const string& strFlatIndexIsResettable, const string& strFlatTenor,
    const string& strSpreadTenor, const Conventions* conventions)
    : Convention(id, Type::CrossCcyBasis), strSettlementDays_(strSettlementDays),
      strSettlementCalendar_(strSettlementCalendar), strRollConvention_(strRollConvention), strFlatIndex_(flatIndex),
      strSpreadIndex_(spreadIndex), strEom_(strEom), strIsResettable_(strIsResettable),
      strFlatIndexIsResettable_(strFlatIndexIsResettable), strFlatTenor_(strFlatTenor), strSpreadTenor_(strSpreadTenor),
      conventions_(conventions) {
    build();
}

void CrossCcyBasisSwapConvention::build() {
    settlementDays_ = lexical_cast<Natural>(strSettlementDays_);
    settlementCalendar_ = parseCalendar(strSettlementCalendar_);
    rollConvention_ = parseBusinessDayConvention(strRollConvention_);
    flatIndex_ = parseIborIndex(strFlatIndex_, Handle<YieldTermStructure>(),
                                getIborOrOvernightConvention(conventions_, strFlatIndex_));
    spreadIndex_ = parseIborIndex(strSpreadIndex_, Handle<YieldTermStructure>(),
                                  getIborOrOvernightConvention(conventions_, strSpreadIndex_));
    eom_ = strEom_.empty() ? false : parseBool(strEom_);
    isResettable_ = strIsResettable_.empty() ? false : parseBool(strIsResettable_);
    flatIndexIsResettable_ = strFlatIndexIsResettable_.empty() ? true : parseBool(strFlatIndexIsResettable_);
    flatTenor_ = strFlatTenor_.empty() ? flatIndex_->tenor() : parsePeriod(strFlatTenor_);
    spreadTenor_ = strSpreadTenor_.empty() ? spreadIndex_->tenor() : parsePeriod(strSpreadTenor_);
}

void CrossCcyBasisSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CrossCurrencyBasis");
    type_ = Type::CrossCcyBasis;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSettlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", true);
    strSettlementCalendar_ = XMLUtils::getChildValue(node, "SettlementCalendar", true);
    strRollConvention_ = XMLUtils::getChildValue(node, "RollConvention", true);
    strFlatIndex_ = XMLUtils::getChildValue(node, "FlatIndex", true);
    strSpreadIndex_ = XMLUtils::getChildValue(node, "SpreadIndex", true);
    strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    strIsResettable_ = XMLUtils::getChildValue(node, "IsResettable", false);
    strFlatIndexIsResettable_ = XMLUtils::getChildValue(node, "FlatIndexIsResettable", false);
    strFlatTenor_ = XMLUtils::getChildValue(node, "FlatTenor", false);
    strSpreadTenor_ = XMLUtils::getChildValue(node, "SpreadTenor", false);

    build();
}

XMLNode* CrossCcyBasisSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CrossCurrencyBasis");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    XMLUtils::addChild(doc, node, "SettlementCalendar", strSettlementCalendar_);
    XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);
    XMLUtils::addChild(doc, node, "FlatIndex", strFlatIndex_);
    XMLUtils::addChild(doc, node, "SpreadIndex", strSpreadIndex_);
    XMLUtils::addChild(doc, node, "EOM", strEom_);
    XMLUtils::addChild(doc, node, "IsResettable", strIsResettable_);
    XMLUtils::addChild(doc, node, "FlatIndexIsResettable", strFlatIndexIsResettable_);
    XMLUtils::addChild(doc, node, "FlatTenor", strFlatTenor_);
    XMLUtils::addChild(doc, node, "SpreadTenor", strSpreadTenor_);

    return node;
}

CrossCcyFixFloatSwapConvention::CrossCcyFixFloatSwapConvention(
    const string& id, const string& settlementDays, const string& settlementCalendar,
    const string& settlementConvention, const string& fixedCurrency, const string& fixedFrequency,
    const string& fixedConvention, const string& fixedDayCounter, const string& index, const string& eom,
    const Conventions* conventions, const std::string& strIsResettable, const std::string& strFloatIndexIsResettable)
    : Convention(id, Type::CrossCcyFixFloat), strSettlementDays_(settlementDays),
      strSettlementCalendar_(settlementCalendar), strSettlementConvention_(settlementConvention),
      strFixedCurrency_(fixedCurrency), strFixedFrequency_(fixedFrequency), strFixedConvention_(fixedConvention),
      strFixedDayCounter_(fixedDayCounter), strIndex_(index), strEom_(eom), conventions_(conventions),
      strIsResettable_(strIsResettable), strFloatIndexIsResettable_(strFloatIndexIsResettable){

    build();
}

void CrossCcyFixFloatSwapConvention::build() {
    settlementDays_ = lexical_cast<Natural>(strSettlementDays_);
    settlementCalendar_ = parseCalendar(strSettlementCalendar_);
    settlementConvention_ = parseBusinessDayConvention(strSettlementConvention_);
    fixedCurrency_ = parseCurrency(strFixedCurrency_);
    fixedFrequency_ = parseFrequency(strFixedFrequency_);
    fixedConvention_ = parseBusinessDayConvention(strFixedConvention_);
    fixedDayCounter_ = parseDayCounter(strFixedDayCounter_);
    index_ =
        parseIborIndex(strIndex_, Handle<YieldTermStructure>(), getIborOrOvernightConvention(conventions_, strIndex_));
    eom_ = strEom_.empty() ? false : parseBool(strEom_);
    isResettable_ = strIsResettable_.empty() ? false : parseBool(strIsResettable_);
    floatIndexIsResettable_ = strFloatIndexIsResettable_.empty() ? true : parseBool(strFloatIndexIsResettable_);
}

void CrossCcyFixFloatSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CrossCurrencyFixFloat");
    type_ = Type::CrossCcyFixFloat;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSettlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", true);
    strSettlementCalendar_ = XMLUtils::getChildValue(node, "SettlementCalendar", true);
    strSettlementConvention_ = XMLUtils::getChildValue(node, "SettlementConvention", true);
    strFixedCurrency_ = XMLUtils::getChildValue(node, "FixedCurrency", true);
    strFixedFrequency_ = XMLUtils::getChildValue(node, "FixedFrequency", true);
    strFixedConvention_ = XMLUtils::getChildValue(node, "FixedConvention", true);
    strFixedDayCounter_ = XMLUtils::getChildValue(node, "FixedDayCounter", true);

    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    strIsResettable_ = XMLUtils::getChildValue(node, "IsResettable", false);
    strFloatIndexIsResettable_ = XMLUtils::getChildValue(node, "FloatIndexIsResettable", false);

    build();
}

XMLNode* CrossCcyFixFloatSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CrossCurrencyFixFloat");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    XMLUtils::addChild(doc, node, "SettlementCalendar", strSettlementCalendar_);
    XMLUtils::addChild(doc, node, "SettlementConvention", strSettlementConvention_);
    XMLUtils::addChild(doc, node, "FixedCurrency", strFixedCurrency_);
    XMLUtils::addChild(doc, node, "FixedFrequency", strFixedFrequency_);
    XMLUtils::addChild(doc, node, "FixedConvention", strFixedConvention_);
    XMLUtils::addChild(doc, node, "FixedDayCounter", strFixedDayCounter_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "EOM", strEom_);
    XMLUtils::addChild(doc, node, "IsResettable", strIsResettable_);
    XMLUtils::addChild(doc, node, "FloatIndexIsResettable", strFloatIndexIsResettable_);

    return node;
}

CdsConvention::CdsConvention() : settlementDays_(0), frequency_(Quarterly), paymentConvention_(Following),
    rule_(DateGeneration::CDS2015), settlesAccrual_(true), paysAtDefaultTime_(true), upfrontSettlementDays_(3) {}

CdsConvention::CdsConvention(const string& id, const string& strSettlementDays, const string& strCalendar,
                             const string& strFrequency, const string& strPaymentConvention, const string& strRule,
                             const string& strDayCounter, const string& strSettlesAccrual,
                             const string& strPaysAtDefaultTime, const string& strUpfrontSettlementDays,
                             const string& lastPeriodDayCounter)
    : Convention(id, Type::CDS), strSettlementDays_(strSettlementDays), strCalendar_(strCalendar),
      strFrequency_(strFrequency), strPaymentConvention_(strPaymentConvention), strRule_(strRule),
      strDayCounter_(strDayCounter), strSettlesAccrual_(strSettlesAccrual),
      strPaysAtDefaultTime_(strPaysAtDefaultTime), strUpfrontSettlementDays_(strUpfrontSettlementDays),
      strLastPeriodDayCounter_(lastPeriodDayCounter) {
    build();
}

void CdsConvention::build() {
    settlementDays_ = lexical_cast<Natural>(strSettlementDays_);
    calendar_ = parseCalendar(strCalendar_);
    frequency_ = parseFrequency(strFrequency_);
    paymentConvention_ = parseBusinessDayConvention(strPaymentConvention_);
    rule_ = parseDateGenerationRule(strRule_);
    dayCounter_ = parseDayCounter(strDayCounter_);
    settlesAccrual_ = parseBool(strSettlesAccrual_);
    paysAtDefaultTime_ = parseBool(strPaysAtDefaultTime_);

    upfrontSettlementDays_ = 3;
    if (!strUpfrontSettlementDays_.empty())
        upfrontSettlementDays_ = lexical_cast<Natural>(strUpfrontSettlementDays_);

    lastPeriodDayCounter_ = DayCounter();
    if (!strLastPeriodDayCounter_.empty())
        lastPeriodDayCounter_ = parseDayCounter(strLastPeriodDayCounter_);
}

void CdsConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CDS");
    type_ = Type::CDS;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strSettlementDays_ = XMLUtils::getChildValue(node, "SettlementDays", true);
    strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
    strFrequency_ = XMLUtils::getChildValue(node, "Frequency", true);
    strPaymentConvention_ = XMLUtils::getChildValue(node, "PaymentConvention", true);
    strRule_ = XMLUtils::getChildValue(node, "Rule", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strSettlesAccrual_ = XMLUtils::getChildValue(node, "SettlesAccrual", true);
    strPaysAtDefaultTime_ = XMLUtils::getChildValue(node, "PaysAtDefaultTime", true);
    strUpfrontSettlementDays_ = XMLUtils::getChildValue(node, "UpfrontSettlementDays", false);
    strLastPeriodDayCounter_ = XMLUtils::getChildValue(node, "LastPeriodDayCounter", false);

    build();
}

XMLNode* CdsConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CDS");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SettlementDays", strSettlementDays_);
    XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
    XMLUtils::addChild(doc, node, "Frequency", strFrequency_);
    XMLUtils::addChild(doc, node, "PaymentConvention", strPaymentConvention_);
    XMLUtils::addChild(doc, node, "Rule", strRule_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "SettlesAccrual", strSettlesAccrual_);
    XMLUtils::addChild(doc, node, "PaysAtDefaultTime", strPaysAtDefaultTime_);
    if (!strUpfrontSettlementDays_.empty())
        XMLUtils::addChild(doc, node, "UpfrontSettlementDays", strUpfrontSettlementDays_);
    if (!strLastPeriodDayCounter_.empty())
        XMLUtils::addChild(doc, node, "LastPeriodDayCounter", strLastPeriodDayCounter_);
    return node;
}

InflationSwapConvention::InflationSwapConvention(const boost::shared_ptr<Conventions>& conventions)
    : fixConvention_(Following), interpolated_(false), adjustInfObsDates_(false),
      infConvention_(Following), conventions_(conventions), publicationRoll_(PublicationRoll::None) {}

InflationSwapConvention::InflationSwapConvention(const string& id, const string& strFixCalendar,
                                                 const string& strFixConvention, const string& strDayCounter,
                                                 const string& strIndex, const string& strInterpolated,
                                                 const string& strObservationLag, const string& strAdjustInfObsDates,
                                                 const string& strInfCalendar, const string& strInfConvention,
                                                 const boost::shared_ptr<Conventions>& conventions,
                                                 PublicationRoll publicationRoll,
                                                 const boost::shared_ptr<ScheduleData>& publicationScheduleData)
    : Convention(id, Type::InflationSwap), strFixCalendar_(strFixCalendar), strFixConvention_(strFixConvention),
      strDayCounter_(strDayCounter), strIndex_(strIndex), strInterpolated_(strInterpolated),
      strObservationLag_(strObservationLag), strAdjustInfObsDates_(strAdjustInfObsDates),
      strInfCalendar_(strInfCalendar), strInfConvention_(strInfConvention), conventions_(conventions),
      publicationRoll_(publicationRoll), publicationScheduleData_(publicationScheduleData) {
    build();
}

void InflationSwapConvention::build() {
    fixCalendar_ = parseCalendar(strFixCalendar_);
    fixConvention_ = parseBusinessDayConvention(strFixConvention_);
    dayCounter_ = parseDayCounter(strDayCounter_);
    interpolated_ = parseBool(strInterpolated_);
    index_ = parseZeroInflationIndex(strIndex_, interpolated_, Handle<ZeroInflationTermStructure>(), conventions_);
    observationLag_ = parsePeriod(strObservationLag_);
    adjustInfObsDates_ = parseBool(strAdjustInfObsDates_);
    infCalendar_ = parseCalendar(strInfCalendar_);
    infConvention_ = parseBusinessDayConvention(strInfConvention_);
    if (publicationRoll_ != PublicationRoll::None) {
        QL_REQUIRE(publicationScheduleData_, "Publication roll is " << publicationRoll_ << " for " << id() <<
            " so expect non-null publication schedule data.");
        publicationSchedule_ = makeSchedule(*publicationScheduleData_);
    }
}

void InflationSwapConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "InflationSwap");
    type_ = Type::InflationSwap;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Get string values from xml
    strFixCalendar_ = XMLUtils::getChildValue(node, "FixCalendar", true);
    strFixConvention_ = XMLUtils::getChildValue(node, "FixConvention", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strIndex_ = XMLUtils::getChildValue(node, "Index", true);
    strInterpolated_ = XMLUtils::getChildValue(node, "Interpolated", true);
    strObservationLag_ = XMLUtils::getChildValue(node, "ObservationLag", true);
    strAdjustInfObsDates_ = XMLUtils::getChildValue(node, "AdjustInflationObservationDates", true);
    strInfCalendar_ = XMLUtils::getChildValue(node, "InflationCalendar", true);
    strInfConvention_ = XMLUtils::getChildValue(node, "InflationConvention", true);

    publicationRoll_ = PublicationRoll::None;
    if (XMLNode* n = XMLUtils::getChildNode(node, "PublicationRoll")) {
        publicationRoll_ = parseInflationSwapPublicationRoll(XMLUtils::getNodeValue(n));
    }

    if (publicationRoll_ != PublicationRoll::None) {
        XMLNode* n = XMLUtils::getChildNode(node, "PublicationSchedule");
        QL_REQUIRE(n, "PublicationRoll is " << publicationRoll_ << " for " << id() <<
            " so expect non-empty PublicationSchedule.");
        publicationScheduleData_ = boost::make_shared<ScheduleData>();
        publicationScheduleData_->fromXML(n);
    }

    build();
}

XMLNode* InflationSwapConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("InflationSwap");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "FixCalendar", strFixCalendar_);
    XMLUtils::addChild(doc, node, "FixConvention", strFixConvention_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "Index", strIndex_);
    XMLUtils::addChild(doc, node, "Interpolated", strInterpolated_);
    XMLUtils::addChild(doc, node, "ObservationLag", strObservationLag_);
    XMLUtils::addChild(doc, node, "AdjustInflationObservationDates", strAdjustInfObsDates_);
    XMLUtils::addChild(doc, node, "InflationCalendar", strInfCalendar_);
    XMLUtils::addChild(doc, node, "InflationConvention", strInfConvention_);

    if (publicationRoll_ != PublicationRoll::None) {
        XMLUtils::addChild(doc, node, "RollOnPublication", to_string(publicationRoll_));
        QL_REQUIRE(publicationScheduleData_, "PublicationRoll is " << publicationRoll_ << " for "
            << id() << " so expect PublicationSchedule.");

        // Need to change the name from ScheduleData to PublicationSchedule.
        XMLNode* n = publicationScheduleData_->toXML(doc);
        XMLUtils::setNodeName(doc, n, "PublicationSchedule");
        XMLUtils::appendNode(node, n);
    }

    return node;
}

SecuritySpreadConvention::SecuritySpreadConvention(const string& id, const string& dayCounter,
                                                   const string& compounding, const string& compoundingFrequency)
    : Convention(id, Type::SecuritySpread), tenorBased_(false), strDayCounter_(dayCounter),
      strCompounding_(compounding), strCompoundingFrequency_(compoundingFrequency) {
    build();
}

SecuritySpreadConvention::SecuritySpreadConvention(const string& id, const string& dayCounter,
                                                   const string& tenorCalendar, const string& compounding,
                                                   const string& compoundingFrequency, const string& spotLag,
                                                   const string& spotCalendar, const string& rollConvention,
                                                   const string& eom)
    : Convention(id, Type::SecuritySpread), tenorBased_(true), strDayCounter_(dayCounter),
      strTenorCalendar_(tenorCalendar), strCompounding_(compounding), strCompoundingFrequency_(compoundingFrequency),
      strSpotLag_(spotLag), strSpotCalendar_(spotCalendar), strRollConvention_(rollConvention), strEom_(eom) {
    build();
}

void SecuritySpreadConvention::build() {
    dayCounter_ = parseDayCounter(strDayCounter_);
    compounding_ = strCompounding_.empty() ? Continuous : parseCompounding(strCompounding_);
    compoundingFrequency_ = strCompoundingFrequency_.empty() ? Annual : parseFrequency(strCompoundingFrequency_);
    if (tenorBased_) {
        tenorCalendar_ = parseCalendar(strTenorCalendar_);
        spotLag_ = strSpotLag_.empty() ? 0 : lexical_cast<Natural>(strSpotLag_);
        spotCalendar_ = strSpotCalendar_.empty() ? NullCalendar() : parseCalendar(strSpotCalendar_);
        rollConvention_ = strRollConvention_.empty() ? Following : parseBusinessDayConvention(strRollConvention_);
        eom_ = strEom_.empty() ? false : parseBool(strEom_);
    }
}

void SecuritySpreadConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "BondSpread");
    type_ = Type::SecuritySpread;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    tenorBased_ = XMLUtils::getChildValueAsBool(node, "TenorBased", true);

    // Get string values from xml
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strCompoundingFrequency_ = XMLUtils::getChildValue(node, "CompoundingFrequency", false);
    strCompounding_ = XMLUtils::getChildValue(node, "Compounding", false);
    if (tenorBased_) {
        strTenorCalendar_ = XMLUtils::getChildValue(node, "TenorCalendar", true);
        strSpotLag_ = XMLUtils::getChildValue(node, "SpotLag", false);
        strSpotCalendar_ = XMLUtils::getChildValue(node, "SpotCalendar", false);
        strRollConvention_ = XMLUtils::getChildValue(node, "RollConvention", false);
        strEom_ = XMLUtils::getChildValue(node, "EOM", false);
    }
    build();
}

XMLNode* SecuritySpreadConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("BondSpread");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "TenorBased", tenorBased_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "CompoundingFrequency", strCompoundingFrequency_);
    XMLUtils::addChild(doc, node, "Compounding", strCompounding_);
    if (tenorBased_) {
        XMLUtils::addChild(doc, node, "TenorCalendar", strTenorCalendar_);
        XMLUtils::addChild(doc, node, "SpotLag", strSpotLag_);
        XMLUtils::addChild(doc, node, "SpotCalendar", strSpotCalendar_);
        XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);
        XMLUtils::addChild(doc, node, "EOM", strEom_);
    }

    return node;
}

CmsSpreadOptionConvention::CmsSpreadOptionConvention(const string& id, const string& strForwardStart,
                                                     const string& strSpotDays, const string& strSwapTenor,
                                                     const string& strFixingDays, const string& strCalendar,
                                                     const string& strDayCounter, const string& strConvention)
    : Convention(id, Type::CMSSpreadOption), strForwardStart_(strForwardStart), strSpotDays_(strSpotDays),
      strSwapTenor_(strSwapTenor), strFixingDays_(strFixingDays), strCalendar_(strCalendar),
      strDayCounter_(strDayCounter), strRollConvention_(strConvention) {
    build();
}

void CmsSpreadOptionConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CmsSpreadOption");
    type_ = Type::CMSSpreadOption;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    strForwardStart_ = XMLUtils::getChildValue(node, "ForwardStart", true);
    strSpotDays_ = XMLUtils::getChildValue(node, "SpotDays", true);
    strSwapTenor_ = XMLUtils::getChildValue(node, "SwapTenor", true);
    strFixingDays_ = XMLUtils::getChildValue(node, "FixingDays", true);
    strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
    strDayCounter_ = XMLUtils::getChildValue(node, "DayCounter", true);
    strRollConvention_ = XMLUtils::getChildValue(node, "RollConvention", true);

    build();
}

void CmsSpreadOptionConvention::build() {

    forwardStart_ = parsePeriod(strForwardStart_);
    spotDays_ = parsePeriod(strSpotDays_);
    swapTenor_ = parsePeriod(strSwapTenor_);
    fixingDays_ = lexical_cast<Natural>(strFixingDays_);
    calendar_ = parseCalendar(strCalendar_);
    dayCounter_ = parseDayCounter(strDayCounter_);
    rollConvention_ = parseBusinessDayConvention(strRollConvention_);
}

XMLNode* CmsSpreadOptionConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CmsSpreadOption");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "ForwardStart", strForwardStart_);
    XMLUtils::addChild(doc, node, "SpotDays", strSpotDays_);
    XMLUtils::addChild(doc, node, "SwapTenor", strSwapTenor_);
    XMLUtils::addChild(doc, node, "FixingDays", strFixingDays_);
    XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
    XMLUtils::addChild(doc, node, "DayCounter", strDayCounter_);
    XMLUtils::addChild(doc, node, "RollConvention", strRollConvention_);

    return node;
}

CommodityForwardConvention::CommodityForwardConvention(const string& id, const string& spotDays,
                                                       const string& pointsFactor, const string& advanceCalendar,
                                                       const string& spotRelative, BusinessDayConvention bdc,
                                                       bool outright)
    : Convention(id, Type::CommodityForward), bdc_(bdc), outright_(outright), strSpotDays_(spotDays),
      strPointsFactor_(pointsFactor), strAdvanceCalendar_(advanceCalendar), strSpotRelative_(spotRelative) {
    build();
}

void CommodityForwardConvention::build() {
    spotDays_ = strSpotDays_.empty() ? 2 : lexical_cast<Natural>(strSpotDays_);
    pointsFactor_ = strPointsFactor_.empty() ? 1.0 : parseReal(strPointsFactor_);
    advanceCalendar_ = strAdvanceCalendar_.empty() ? NullCalendar() : parseCalendar(strAdvanceCalendar_);
    spotRelative_ = strSpotRelative_.empty() ? true : parseBool(strSpotRelative_);
}

void CommodityForwardConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CommodityForward");
    type_ = Type::CommodityForward;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    strSpotDays_ = XMLUtils::getChildValue(node, "SpotDays", false);
    strPointsFactor_ = XMLUtils::getChildValue(node, "PointsFactor", false);
    strAdvanceCalendar_ = XMLUtils::getChildValue(node, "AdvanceCalendar", false);
    strSpotRelative_ = XMLUtils::getChildValue(node, "SpotRelative", false);

    bdc_ = Following;
    if (XMLNode* n = XMLUtils::getChildNode(node, "BusinessDayConvention")) {
        bdc_ = parseBusinessDayConvention(XMLUtils::getNodeValue(n));
    }

    outright_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "Outright")) {
        outright_ = parseBool(XMLUtils::getNodeValue(n));
    }

    build();
}

XMLNode* CommodityForwardConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CommodityForward");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "SpotDays", strSpotDays_);
    XMLUtils::addChild(doc, node, "PointsFactor", strPointsFactor_);
    XMLUtils::addChild(doc, node, "AdvanceCalendar", strAdvanceCalendar_);
    XMLUtils::addChild(doc, node, "SpotRelative", strSpotRelative_);
    XMLUtils::addChild(doc, node, "BusinessDayConvention", ore::data::to_string(bdc_));
    XMLUtils::addChild(doc, node, "Outright", outright_);

    return node;
}

CommodityFutureConvention::AveragingData::AveragingData()
    : useBusinessDays_(true), deliveryRollDays_(0), futureMonthOffset_(0),
      dailyExpiryOffset_(Null<Natural>()), period_(CalculationPeriod::ExpiryToExpiry) {}

CommodityFutureConvention::AveragingData::AveragingData(const string& commodityName, const string& period,
    const string& pricingCalendar, bool useBusinessDays, const string& conventionsId,
    Natural deliveryRollDays, Natural futureMonthOffset, Natural dailyExpiryOffset)
    : commodityName_(commodityName), strPeriod_(period), strPricingCalendar_(pricingCalendar),
      useBusinessDays_(useBusinessDays), conventionsId_(conventionsId), deliveryRollDays_(deliveryRollDays),
      futureMonthOffset_(futureMonthOffset), dailyExpiryOffset_(dailyExpiryOffset),
      period_(CalculationPeriod::ExpiryToExpiry) {
    build();
}

const string& CommodityFutureConvention::AveragingData::commodityName() const {
    return commodityName_;
}

CommodityFutureConvention::AveragingData::CalculationPeriod CommodityFutureConvention::AveragingData::period() const {
    return period_;
}

const Calendar& CommodityFutureConvention::AveragingData::pricingCalendar() const {
    return pricingCalendar_;
}

bool CommodityFutureConvention::AveragingData::useBusinessDays() const {
    return useBusinessDays_;
}

const string& CommodityFutureConvention::AveragingData::conventionsId() const {
    return conventionsId_;
}

Natural CommodityFutureConvention::AveragingData::deliveryRollDays() const {
    return deliveryRollDays_;
}

Natural CommodityFutureConvention::AveragingData::futureMonthOffset() const {
    return futureMonthOffset_;
}

Natural CommodityFutureConvention::AveragingData::dailyExpiryOffset() const {
    return dailyExpiryOffset_;
}

bool CommodityFutureConvention::AveragingData::empty() const {
    return commodityName_.empty();
}

void CommodityFutureConvention::AveragingData::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "AveragingData");
    commodityName_ = XMLUtils::getChildValue(node, "CommodityName", true);
    strPeriod_ = XMLUtils::getChildValue(node, "Period", true);
    strPricingCalendar_ = XMLUtils::getChildValue(node, "PricingCalendar", true);
    useBusinessDays_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "UseBusinessDays")) {
        useBusinessDays_ = parseBool(XMLUtils::getNodeValue(n));
    }
    conventionsId_ = XMLUtils::getChildValue(node, "Conventions", false);

    deliveryRollDays_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "DeliveryRollDays")) {
        deliveryRollDays_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    futureMonthOffset_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "FutureMonthOffset")) {
        futureMonthOffset_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    dailyExpiryOffset_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "DailyExpiryOffset")) {
        dailyExpiryOffset_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    build();
}

XMLNode* CommodityFutureConvention::AveragingData::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("AveragingData");
    XMLUtils::addChild(doc, node, "CommodityName", commodityName_);
    XMLUtils::addChild(doc, node, "Period", strPeriod_);
    XMLUtils::addChild(doc, node, "PricingCalendar", strPricingCalendar_);
    XMLUtils::addChild(doc, node, "UseBusinessDays", useBusinessDays_);
    if (!conventionsId_.empty())
        XMLUtils::addChild(doc, node, "Conventions", conventionsId_);
    if (deliveryRollDays_ != 0)
        XMLUtils::addChild(doc, node, "DeliveryRollDays", static_cast<int>(deliveryRollDays_));
    if (futureMonthOffset_ != 0)
        XMLUtils::addChild(doc, node, "FutureMonthOffset", static_cast<int>(futureMonthOffset_));
    if (dailyExpiryOffset_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "DailyExpiryOffset", static_cast<int>(dailyExpiryOffset_));

    return node;
}

void CommodityFutureConvention::AveragingData::build() {
    period_ = parseAveragingDataPeriod(strPeriod_);
    pricingCalendar_ = parseCalendar(strPricingCalendar_);
}

CommodityFutureConvention::OffPeakPowerIndexData::OffPeakPowerIndexData() : offPeakHours_(0.0) {}

CommodityFutureConvention::OffPeakPowerIndexData::OffPeakPowerIndexData(
    const string& offPeakIndex,
    const string& peakIndex,
    const string& offPeakHours,
    const string& peakCalendar)
    : offPeakIndex_(offPeakIndex),
      peakIndex_(peakIndex),
      strOffPeakHours_(offPeakHours),
      strPeakCalendar_(peakCalendar) {
    build();
}

void CommodityFutureConvention::OffPeakPowerIndexData::build() {
    offPeakHours_ = parseReal(strOffPeakHours_);
    peakCalendar_ = parseCalendar(strPeakCalendar_);
}

void CommodityFutureConvention::OffPeakPowerIndexData::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "OffPeakPowerIndexData");
    offPeakIndex_ = XMLUtils::getChildValue(node, "OffPeakIndex", true);
    peakIndex_ = XMLUtils::getChildValue(node, "PeakIndex", true);
    strOffPeakHours_ = XMLUtils::getChildValue(node, "OffPeakHours", true);
    strPeakCalendar_ = XMLUtils::getChildValue(node, "PeakCalendar", true);

    build();
}

XMLNode* CommodityFutureConvention::OffPeakPowerIndexData::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("OffPeakPowerIndexData");
    XMLUtils::addChild(doc, node, "OffPeakIndex", offPeakIndex_);
    XMLUtils::addChild(doc, node, "PeakIndex", peakIndex_);
    XMLUtils::addChild(doc, node, "OffPeakHours", strOffPeakHours_);
    XMLUtils::addChild(doc, node, "PeakCalendar", strPeakCalendar_);

    return node;
}

CommodityFutureConvention::ProhibitedExpiry::ProhibitedExpiry()
    : forFuture_(true), futureBdc_(Preceding), forOption_(true), optionBdc_(Preceding) {}

CommodityFutureConvention::ProhibitedExpiry::ProhibitedExpiry(const Date& expiry,
    bool forFuture,
    BusinessDayConvention futureBdc,
    bool forOption,
    BusinessDayConvention optionBdc)
    : expiry_(expiry), forFuture_(true), futureBdc_(Preceding),
      forOption_(true), optionBdc_(Preceding) {}

void CommodityFutureConvention::ProhibitedExpiry::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Date");
    expiry_ = parseDate(XMLUtils::getNodeValue(node));
    string tmp = XMLUtils::getAttribute(node, "forFuture");
    forFuture_ = tmp.empty() ? true : parseBool(tmp);
    tmp = XMLUtils::getAttribute(node, "convention");
    futureBdc_ = tmp.empty() ? Preceding : parseBusinessDayConvention(tmp);
    tmp = XMLUtils::getAttribute(node, "forOption");
    forOption_ = tmp.empty() ? true : parseBool(tmp);
    tmp = XMLUtils::getAttribute(node, "optionConvention");
    optionBdc_ = tmp.empty() ? Preceding : parseBusinessDayConvention(tmp);
}

XMLNode* CommodityFutureConvention::ProhibitedExpiry::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("Date", to_string(expiry_));
    XMLUtils::addAttribute(doc, node, "forFuture", to_string(forFuture_));
    XMLUtils::addAttribute(doc, node, "convention", to_string(futureBdc_));
    XMLUtils::addAttribute(doc, node, "forOption", to_string(forOption_));
    XMLUtils::addAttribute(doc, node, "optionConvention", to_string(optionBdc_));

    return node;
}

bool operator<(const CommodityFutureConvention::ProhibitedExpiry& lhs,
    const CommodityFutureConvention::ProhibitedExpiry& rhs) {
    return lhs.expiry() < rhs.expiry();
}

CommodityFutureConvention::CommodityFutureConvention()
    : anchorType_(AnchorType::DayOfMonth),
      dayOfMonth_(1),
      nth_(1),
      weekday_(Mon),
      calendarDaysBefore_(0),
      contractFrequency_(Monthly),
      oneContractMonth_(Jan),
      offsetDays_(0),
      bdc_(Following),
      optionExpiryOffset_(0),
      expiryMonthLag_(0),
      adjustBeforeOffset_(false),
      isAveraging_(false),
      optionExpiryMonthLag_(0),
      optionExpiryDay_(Null<Natural>()),
      optionBdc_(Preceding),
      hoursPerDay_(Null<Natural>()) {}

CommodityFutureConvention::CommodityFutureConvention(const string& id, const DayOfMonth& dayOfMonth,
                                                     const string& contractFrequency, const string& calendar,
                                                     const string& expiryCalendar, Size expiryMonthLag,
                                                     const string& oneContractMonth, const string& offsetDays,
                                                     const string& bdc, bool adjustBeforeOffset, bool isAveraging,
                                                     const string& optionExpiryOffset,
                                                     const set<ProhibitedExpiry>& prohibitedExpiries,
                                                     Size optionExpiryMonthLag,
                                                     Natural optionExpiryDay,
                                                     const string& optionBdc,
                                                     const map<Natural, Natural>& futureContinuationMappings,
                                                     const map<Natural, Natural>& optionContinuationMappings,
                                                     const AveragingData& averagingData,
                                                     Natural hoursPerDay,
                                                     const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData,
                                                     const string& indexName)
    : Convention(id, Type::CommodityFuture), anchorType_(AnchorType::DayOfMonth),
      strDayOfMonth_(dayOfMonth.dayOfMonth_), strContractFrequency_(contractFrequency), strCalendar_(calendar),
      strExpiryCalendar_(expiryCalendar), expiryMonthLag_(expiryMonthLag), strOneContractMonth_(oneContractMonth),
      strOffsetDays_(offsetDays), strBdc_(bdc), adjustBeforeOffset_(adjustBeforeOffset), isAveraging_(isAveraging),
      strOptionExpiryOffset_(optionExpiryOffset), prohibitedExpiries_(prohibitedExpiries),
      optionExpiryMonthLag_(optionExpiryMonthLag), optionExpiryDay_(optionExpiryDay), strOptionBdc_(optionBdc),
      futureContinuationMappings_(futureContinuationMappings),
      optionContinuationMappings_(optionContinuationMappings), averagingData_(averagingData),
      hoursPerDay_(hoursPerDay), offPeakPowerIndexData_(offPeakPowerIndexData), indexName_(indexName) {
    build();
}

CommodityFutureConvention::CommodityFutureConvention(const string& id, const string& nth, const string& weekday,
                                                     const string& contractFrequency, const string& calendar,
                                                     const string& expiryCalendar, Size expiryMonthLag,
                                                     const string& oneContractMonth, const string& offsetDays,
                                                     const string& bdc, bool adjustBeforeOffset, bool isAveraging,
                                                     const string& optionExpiryOffset,
                                                     const set<ProhibitedExpiry>& prohibitedExpiries,
                                                     Size optionExpiryMonthLag,
                                                     Natural optionExpiryDay,
                                                     const string& optionBdc,
                                                     const map<Natural, Natural>& futureContinuationMappings,
                                                     const map<Natural, Natural>& optionContinuationMappings,
                                                     const AveragingData& averagingData,
                                                     Natural hoursPerDay,
                                                     const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData,
                                                     const string& indexName)
    : Convention(id, Type::CommodityFuture), anchorType_(AnchorType::NthWeekday), strNth_(nth), strWeekday_(weekday),
      strContractFrequency_(contractFrequency), strCalendar_(calendar), strExpiryCalendar_(expiryCalendar),
      expiryMonthLag_(expiryMonthLag), strOneContractMonth_(oneContractMonth), strOffsetDays_(offsetDays), strBdc_(bdc),
      adjustBeforeOffset_(adjustBeforeOffset), isAveraging_(isAveraging), strOptionExpiryOffset_(optionExpiryOffset),
      prohibitedExpiries_(prohibitedExpiries), optionExpiryMonthLag_(optionExpiryMonthLag),
      optionExpiryDay_(optionExpiryDay), strOptionBdc_(optionBdc),
      futureContinuationMappings_(futureContinuationMappings),
      optionContinuationMappings_(optionContinuationMappings), averagingData_(averagingData),
      hoursPerDay_(hoursPerDay), offPeakPowerIndexData_(offPeakPowerIndexData), indexName_(indexName) {
    build();
}

CommodityFutureConvention::CommodityFutureConvention(const string& id, const CalendarDaysBefore& calendarDaysBefore,
                                                     const string& contractFrequency, const string& calendar,
                                                     const string& expiryCalendar, Size expiryMonthLag,
                                                     const string& oneContractMonth, const string& offsetDays,
                                                     const string& bdc, bool adjustBeforeOffset, bool isAveraging,
                                                     const string& optionExpiryOffset,
                                                     const set<ProhibitedExpiry>& prohibitedExpiries,
                                                     Size optionExpiryMonthLag,
                                                     Natural optionExpiryDay,
                                                     const string& optionBdc,
                                                     const map<Natural, Natural>& futureContinuationMappings,
                                                     const map<Natural, Natural>& optionContinuationMappings,
                                                     const AveragingData& averagingData,
                                                     Natural hoursPerDay,
                                                     const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData,
                                                     const string& indexName)
    : Convention(id, Type::CommodityFuture), anchorType_(AnchorType::CalendarDaysBefore),
      strCalendarDaysBefore_(calendarDaysBefore.calendarDaysBefore_), strContractFrequency_(contractFrequency),
      strCalendar_(calendar), strExpiryCalendar_(expiryCalendar), expiryMonthLag_(expiryMonthLag),
      strOneContractMonth_(oneContractMonth), strOffsetDays_(offsetDays), strBdc_(bdc),
      adjustBeforeOffset_(adjustBeforeOffset), isAveraging_(isAveraging), strOptionExpiryOffset_(optionExpiryOffset),
      prohibitedExpiries_(prohibitedExpiries), optionExpiryMonthLag_(optionExpiryMonthLag),
      optionExpiryDay_(optionExpiryDay), strOptionBdc_(optionBdc),
      futureContinuationMappings_(futureContinuationMappings),
      optionContinuationMappings_(optionContinuationMappings), averagingData_(averagingData),
      hoursPerDay_(hoursPerDay), offPeakPowerIndexData_(offPeakPowerIndexData), indexName_(indexName) {
    build();
}

void CommodityFutureConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CommodityFuture");
    type_ = Type::CommodityFuture;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    // Parse contract frequency first. If it is Daily, we do not need the AnchorDay node.
    strContractFrequency_ = XMLUtils::getChildValue(node, "ContractFrequency", true);
    populateFrequency();

    // Variables related to the anchor day in a given month. Not needed if daily.
    if (contractFrequency_ != Daily) {
        XMLNode* anchorNode = XMLUtils::getChildNode(node, "AnchorDay");
        QL_REQUIRE(anchorNode, "Expected an AnchorDay node in the FutureExpiry convention");
        if (XMLNode* nthNode = XMLUtils::getChildNode(anchorNode, "NthWeekday")) {
            anchorType_ = AnchorType::NthWeekday;
            strNth_ = XMLUtils::getChildValue(nthNode, "Nth", true);
            strWeekday_ = XMLUtils::getChildValue(nthNode, "Weekday", true);
        } else if (XMLNode* tmp = XMLUtils::getChildNode(anchorNode, "DayOfMonth")) {
            anchorType_ = AnchorType::DayOfMonth;
            strDayOfMonth_ = XMLUtils::getNodeValue(tmp);
        } else if (XMLNode* tmp = XMLUtils::getChildNode(anchorNode, "CalendarDaysBefore")) {
            anchorType_ = AnchorType::CalendarDaysBefore;
            strCalendarDaysBefore_ = XMLUtils::getNodeValue(tmp);
        } else {
            QL_FAIL("Failed to parse AnchorDay node");
        }
    }

    strCalendar_ = XMLUtils::getChildValue(node, "Calendar", true);
    strExpiryCalendar_ = XMLUtils::getChildValue(node, "ExpiryCalendar", false);

    expiryMonthLag_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "ExpiryMonthLag")) {
        expiryMonthLag_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    strOneContractMonth_ = XMLUtils::getChildValue(node, "OneContractMonth", false);
    strOffsetDays_ = XMLUtils::getChildValue(node, "OffsetDays", false);
    strBdc_ = XMLUtils::getChildValue(node, "BusinessDayConvention", false);

    adjustBeforeOffset_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "AdjustBeforeOffset")) {
        adjustBeforeOffset_ = parseBool(XMLUtils::getNodeValue(n));
    }

    isAveraging_ = false;
    if (XMLNode* n = XMLUtils::getChildNode(node, "IsAveraging")) {
        isAveraging_ = parseBool(XMLUtils::getNodeValue(n));
    }

    strOptionExpiryOffset_ = XMLUtils::getChildValue(node, "OptionExpiryOffset", false);

    if (XMLNode* n = XMLUtils::getChildNode(node, "ProhibitedExpiries")) {
        auto datesNode = XMLUtils::getChildNode(n, "Dates");
        QL_REQUIRE(datesNode, "ProhibitedExpiries node must have a Dates node.");
        auto dateNodes = XMLUtils::getChildrenNodes(datesNode, "Date");
        for (const auto& dateNode : dateNodes) {
            ProhibitedExpiry pe;
            pe.fromXML(dateNode);
            if (validateBdc(pe)) {
                // First date is inserted, subsequent duplicates are ignored.
                prohibitedExpiries_.insert(pe);
            }
        }
    }

    optionExpiryMonthLag_ = 0;
    if (XMLNode* n = XMLUtils::getChildNode(node, "OptionExpiryMonthLag")) {
        optionExpiryMonthLag_ = parseInteger(XMLUtils::getNodeValue(n));
    }
    optionExpiryDay_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "OptionExpiryDay")) {
        optionExpiryDay_ = parseInteger(XMLUtils::getNodeValue(n));
    }
    strOptionBdc_ = XMLUtils::getChildValue(node, "OptionBusinessDayConvention", false);

    futureContinuationMappings_.clear();
    auto tmp = XMLUtils::getChildrenValues(node, "FutureContinuationMappings",
        "ContinuationMapping", "From", "To", false);
    for (const auto& kv : tmp) {
        futureContinuationMappings_[parseInteger(kv.first)] = parseInteger(kv.second);
    }

    optionContinuationMappings_.clear();
    tmp = XMLUtils::getChildrenValues(node, "OptionContinuationMappings",
        "ContinuationMapping", "From", "To", false);
    for (const auto& kv : tmp) {
        optionContinuationMappings_[parseInteger(kv.first)] = parseInteger(kv.second);
    }

    if (isAveraging_) {
        if (XMLNode* n = XMLUtils::getChildNode(node, "AveragingData")) {
            averagingData_.fromXML(n);
        }
    }

    hoursPerDay_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "HoursPerDay")) {
        hoursPerDay_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "OffPeakPowerIndexData")) {
        offPeakPowerIndexData_ = OffPeakPowerIndexData();
        offPeakPowerIndexData_->fromXML(n);
    }

    indexName_ = XMLUtils::getChildValue(node, "IndexName", false);

    build();
}

XMLNode* CommodityFutureConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("CommodityFuture");
    XMLUtils::addChild(doc, node, "Id", id_);

    if (contractFrequency_ != Daily) {
        XMLNode* anchorNode = doc.allocNode("AnchorDay");
        if (anchorType_ == AnchorType::DayOfMonth) {
            XMLUtils::addChild(doc, anchorNode, "DayOfMonth", strDayOfMonth_);
        } else if (anchorType_ == AnchorType::NthWeekday) {
            XMLNode* nthNode = doc.allocNode("NthWeekday");
            XMLUtils::addChild(doc, nthNode, "Nth", strNth_);
            XMLUtils::addChild(doc, nthNode, "Weekday", strWeekday_);
            XMLUtils::appendNode(anchorNode, nthNode);
        } else {
            XMLUtils::addChild(doc, anchorNode, "CalendarDaysBefore", strCalendarDaysBefore_);
        }
        XMLUtils::appendNode(node, anchorNode);
    }

    XMLUtils::addChild(doc, node, "ContractFrequency", strContractFrequency_);
    XMLUtils::addChild(doc, node, "Calendar", strCalendar_);
    if (!strExpiryCalendar_.empty())
        XMLUtils::addChild(doc, node, "ExpiryCalendar", strExpiryCalendar_);
    XMLUtils::addChild(doc, node, "ExpiryMonthLag", static_cast<int>(expiryMonthLag_));

    if (!strOneContractMonth_.empty())
        XMLUtils::addChild(doc, node, "OneContractMonth", strOneContractMonth_);

    if (!strOffsetDays_.empty())
        XMLUtils::addChild(doc, node, "OffsetDays", strOffsetDays_);

    if (!strBdc_.empty())
        XMLUtils::addChild(doc, node, "BusinessDayConvention", strBdc_);

    XMLUtils::addChild(doc, node, "AdjustBeforeOffset", adjustBeforeOffset_);
    XMLUtils::addChild(doc, node, "IsAveraging", isAveraging_);

    if (!strOptionExpiryOffset_.empty())
        XMLUtils::addChild(doc, node, "OptionExpiryOffset", strOptionExpiryOffset_);

    if (!prohibitedExpiries_.empty()) {
        XMLNode* prohibitedExpiriesNode = doc.allocNode("ProhibitedExpiries");
        XMLNode* datesNode = XMLUtils::addChild(doc, prohibitedExpiriesNode, "Dates");
        for (ProhibitedExpiry pe : prohibitedExpiries_) {
            XMLUtils::appendNode(datesNode, pe.toXML(doc));
        }
        XMLUtils::appendNode(node, prohibitedExpiriesNode);
    }

    XMLUtils::addChild(doc, node, "OptionExpiryMonthLag", static_cast<int>(optionExpiryMonthLag_));
    if (optionExpiryDay_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "OptionExpiryDay", static_cast<int>(optionExpiryDay_));

    if (!strOptionBdc_.empty())
        XMLUtils::addChild(doc, node, "OptionBusinessDayConvention", strOptionBdc_);

    if (!futureContinuationMappings_.empty()) {
        map<string, string> tmp;
        for (const auto& kv : futureContinuationMappings_) {
            tmp[to_string(kv.first)] = to_string(kv.second);
        }
        XMLUtils::addChildren(doc, node, "FutureContinuationMappings",
            "ContinuationMapping", "From", "To", tmp);
    }

    if (!optionContinuationMappings_.empty()) {
        map<string, string> tmp;
        for (const auto& kv : optionContinuationMappings_) {
            tmp[to_string(kv.first)] = to_string(kv.second);
        }
        XMLUtils::addChildren(doc, node, "OptionContinuationMappings",
            "ContinuationMapping", "From", "To", tmp);
    }

    if (!averagingData_.empty()) {
        XMLUtils::appendNode(node, averagingData_.toXML(doc));
    }

    if (hoursPerDay_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "HoursPerDay", static_cast<int>(hoursPerDay_));

    if (offPeakPowerIndexData_) {
        XMLUtils::appendNode(node, offPeakPowerIndexData_->toXML(doc));
    }

    if (!indexName_.empty())
        XMLUtils::addChild(doc, node, "IndexName", indexName_);

    return node;
}

void CommodityFutureConvention::build() {

    populateFrequency();

    if (contractFrequency_ != Daily) {
        if (anchorType_ == AnchorType::DayOfMonth) {
            dayOfMonth_ = lexical_cast<Natural>(strDayOfMonth_);
        } else if (anchorType_ == AnchorType::CalendarDaysBefore) {
            calendarDaysBefore_ = lexical_cast<Natural>(strCalendarDaysBefore_);
        } else {
            nth_ = lexical_cast<Natural>(strNth_);
            weekday_ = parseWeekday(strWeekday_);
        }
    }

    calendar_ = parseCalendar(strCalendar_);
    expiryCalendar_ = strExpiryCalendar_.empty() ? calendar_ : parseCalendar(strExpiryCalendar_);

    // Optional entries
    oneContractMonth_ = strOneContractMonth_.empty() ? Month::Jan : parseMonth(strOneContractMonth_);
    offsetDays_ = strOffsetDays_.empty() ? 0 : lexical_cast<Integer>(strOffsetDays_);
    bdc_ = strBdc_.empty() ? Preceding : parseBusinessDayConvention(strBdc_);
    optionExpiryOffset_ = strOptionExpiryOffset_.empty() ? 0 : lexical_cast<Natural>(strOptionExpiryOffset_);
    optionBdc_ = strOptionBdc_.empty() ? Preceding : parseBusinessDayConvention(strOptionBdc_);

    // Check the continuation mappings
    checkContinuationMappings(futureContinuationMappings_, "future");
    checkContinuationMappings(optionContinuationMappings_, "option");

    // Check that neither of the indexes in OffPeakPowerIndexData self reference
    if (offPeakPowerIndexData_) {
        const string& opIdx = offPeakPowerIndexData_->offPeakIndex();
        QL_REQUIRE(id_ != opIdx, "The off-peak index (" << opIdx << ") cannot equal the index for which" <<
            " we are providing conventions (" << id_ << ").");
        const string& pIdx = offPeakPowerIndexData_->peakIndex();
        QL_REQUIRE(id_ != pIdx, "The peak index (" << pIdx << ") cannot equal the index for which" <<
            " we are providing conventions (" << id_ << ").");
    }
}

void CommodityFutureConvention::populateFrequency() {
    contractFrequency_ = parseFrequency(strContractFrequency_);
    QL_REQUIRE(contractFrequency_ == Quarterly || contractFrequency_ == Monthly || contractFrequency_ == Daily,
        "Contract frequency should be quarterly, monthly or daily but got " << contractFrequency_);
}

bool CommodityFutureConvention::validateBdc(const ProhibitedExpiry& pe) const {
    vector<BusinessDayConvention> bdcs{ pe.futureBdc(), pe.optionBdc() };
    for (auto bdc : bdcs) {
        if (!(bdc == Preceding || bdc == Following || bdc == ModifiedPreceding || bdc == ModifiedFollowing)) {
            WLOG("Prohibited expiry bdc must be one of {Preceding, Following, ModifiedPreceding," <<
                " ModifiedFollowing} but got " << bdc << " for date " << io::iso_date(pe.expiry()) << ".");
            return false;
        }
    }
    return true;
}

FxOptionConvention::FxOptionConvention(const string& id, const string& atmType, const string& deltaType,
                                       const string& switchTenor, const string& longTermAtmType,
                                       const string& longTermDeltaType, const string& riskReversalInFavorOf,
                                       const string& butterflyStyle, const string& fxConventionID)
    : Convention(id, Type::FxOption), fxConventionID_(fxConventionID), strAtmType_(atmType), strDeltaType_(deltaType),
      strSwitchTenor_(switchTenor), strLongTermAtmType_(longTermAtmType), strLongTermDeltaType_(longTermDeltaType),
      strRiskReversalInFavorOf_(riskReversalInFavorOf), strButterflyStyle_(butterflyStyle) {
    build();
}

void FxOptionConvention::build() {
    atmType_ = parseAtmType(strAtmType_);
    deltaType_ = parseDeltaType(strDeltaType_);
    if (!strSwitchTenor_.empty()) {
        switchTenor_ = parsePeriod(strSwitchTenor_);
        longTermAtmType_ = parseAtmType(strLongTermAtmType_);
        longTermDeltaType_ = parseDeltaType(strLongTermDeltaType_);
    } else {
        switchTenor_ = 0 * Days;
        longTermAtmType_ = atmType_;
        longTermDeltaType_ = deltaType_;
    }
    if (!strRiskReversalInFavorOf_.empty()) {
        riskReversalInFavorOf_ = parseOptionType(strRiskReversalInFavorOf_);
    } else {
        riskReversalInFavorOf_ = Option::Call;
    }
    if (strButterflyStyle_.empty() || strButterflyStyle_ == "Broker") {
        butterflyIsBrokerStyle_ = true;
    } else if (strButterflyStyle_ == "Smile") {
        butterflyIsBrokerStyle_ = false;
    } else {
        QL_FAIL("invalid butterfly style '" << strButterflyStyle_ << "', expected Broker or Smile");
    }
}

void FxOptionConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "FxOption");
    type_ = Type::FxOption;
    id_ = XMLUtils::getChildValue(node, "Id", true);
    fxConventionID_ = XMLUtils::getChildValue(node, "FXConventionID", false);

    // Get string values from xml
    strAtmType_ = XMLUtils::getChildValue(node, "AtmType", true);
    strDeltaType_ = XMLUtils::getChildValue(node, "DeltaType", true);
    strSwitchTenor_ = XMLUtils::getChildValue(node, "SwitchTenor", false);
    strLongTermAtmType_ = XMLUtils::getChildValue(node, "LongTermAtmType", false);
    strLongTermDeltaType_ = XMLUtils::getChildValue(node, "LongTermDeltaType", false);
    strRiskReversalInFavorOf_ = XMLUtils::getChildValue(node, "RiskReversalInFavorOf", false);
    strButterflyStyle_ = XMLUtils::getChildValue(node, "ButterflyStyle", false);
    build();
}

XMLNode* FxOptionConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("FxOption");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "FXConventionID", fxConventionID_);
    XMLUtils::addChild(doc, node, "AtmType", strAtmType_);
    XMLUtils::addChild(doc, node, "DeltaType", strDeltaType_);
    XMLUtils::addChild(doc, node, "SwitchTenor", strSwitchTenor_);
    XMLUtils::addChild(doc, node, "LongTermAtmType", strLongTermAtmType_);
    XMLUtils::addChild(doc, node, "LongTermDeltaType", strLongTermDeltaType_);
    XMLUtils::addChild(doc, node, "RiskReversalInFavorOf", strRiskReversalInFavorOf_);
    XMLUtils::addChild(doc, node, "ButterflyStyle", strButterflyStyle_);
    return node;
}

ZeroInflationIndexConvention::ZeroInflationIndexConvention()
    : revised_(false), frequency_(Monthly) {}

ZeroInflationIndexConvention::ZeroInflationIndexConvention(
    const string& id,
    const string& regionName,
    const string& regionCode,
    bool revised,
    const string& frequency,
    const string& availabilityLag,
    const string& currency)
    : Convention(id, Type::ZeroInflationIndex),
      regionName_(regionName),
      regionCode_(regionCode),
      revised_(revised),
      strFrequency_(frequency),
      strAvailabilityLag_(availabilityLag),
      strCurrency_(currency),
      frequency_(Monthly) {
    build();
}

QuantLib::Region ZeroInflationIndexConvention::region() const {
    return QuantLib::CustomRegion(regionName_, regionCode_);
}

void ZeroInflationIndexConvention::build() {
    frequency_ = parseFrequency(strFrequency_);
    availabilityLag_ = parsePeriod(strAvailabilityLag_);
    currency_ = parseCurrency(strCurrency_);
}

void ZeroInflationIndexConvention::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "ZeroInflationIndex");
    type_ = Type::ZeroInflationIndex;
    id_ = XMLUtils::getChildValue(node, "Id", true);

    regionName_ = XMLUtils::getChildValue(node, "RegionName", true);
    regionCode_ = XMLUtils::getChildValue(node, "RegionCode", true);
    revised_ = parseBool(XMLUtils::getChildValue(node, "Revised", true));
    strFrequency_ = XMLUtils::getChildValue(node, "Frequency", true);
    strAvailabilityLag_ = XMLUtils::getChildValue(node, "AvailabilityLag", true);
    strCurrency_ = XMLUtils::getChildValue(node, "Currency", true);

    build();
}

XMLNode* ZeroInflationIndexConvention::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("ZeroInflationIndex");
    XMLUtils::addChild(doc, node, "Id", id_);
    XMLUtils::addChild(doc, node, "RegionName", regionName_);
    XMLUtils::addChild(doc, node, "RegionCode", regionCode_);
    XMLUtils::addChild(doc, node, "Revised", revised_);
    XMLUtils::addChild(doc, node, "Frequency", strFrequency_);
    XMLUtils::addChild(doc, node, "AvailabilityLag", strAvailabilityLag_);
    XMLUtils::addChild(doc, node, "Currency", strCurrency_);

    return node;
}

void Conventions::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Conventions");

    for (XMLNode* child = XMLUtils::getChildNode(node); child; child = XMLUtils::getNextSibling(child)) {

        boost::shared_ptr<Convention> convention;
        string childName = XMLUtils::getNodeName(child);

        // Some conventions depend on the already read conventions, since they parse an ibor or overnight
        // index which may be convention based. In this case we require the index convention to appear
        // before the convention that depends on it in the input.

        if (childName == "Zero") {
            convention.reset(new ZeroRateConvention());
        } else if (childName == "Deposit") {
            convention.reset(new DepositConvention());
        } else if (childName == "Future") {
            convention.reset(new FutureConvention(this));
        } else if (childName == "FRA") {
            convention.reset(new FraConvention(this));
        } else if (childName == "OIS") {
            convention.reset(new OisConvention(this));
        } else if (childName == "Swap") {
            convention.reset(new IRSwapConvention(this));
        } else if (childName == "AverageOIS") {
            convention.reset(new AverageOisConvention(this));
        } else if (childName == "TenorBasisSwap") {
            convention.reset(new TenorBasisSwapConvention(this));
        } else if (childName == "TenorBasisTwoSwap") {
            convention.reset(new TenorBasisTwoSwapConvention(this));
        } else if (childName == "BMABasisSwap") {
            convention.reset(new BMABasisSwapConvention(this));
        } else if (childName == "FX") {
            convention.reset(new FXConvention());
        } else if (childName == "CrossCurrencyBasis") {
            convention.reset(new CrossCcyBasisSwapConvention(this));
        } else if (childName == "CrossCurrencyFixFloat") {
            convention.reset(new CrossCcyFixFloatSwapConvention(this));
        } else if (childName == "CDS") {
            convention.reset(new CdsConvention());
        } else if (childName == "SwapIndex") {
            convention.reset(new SwapIndexConvention());
        } else if (childName == "InflationSwap") {
            convention.reset(new InflationSwapConvention(shared_from_this()));
        } else if (childName == "CmsSpreadOption") {
            convention.reset(new CmsSpreadOptionConvention());
        } else if (childName == "CommodityForward") {
            convention = boost::make_shared<CommodityForwardConvention>();
        } else if (childName == "CommodityFuture") {
            convention = boost::make_shared<CommodityFutureConvention>();
        } else if (childName == "FxOption") {
            convention = boost::make_shared<FxOptionConvention>();
        } else if (childName == "IborIndex") {
            convention = boost::make_shared<IborIndexConvention>();
        } else if (childName == "OvernightIndex") {
            convention = boost::make_shared<OvernightIndexConvention>();
        } else if (childName == "ZeroInflationIndex") {
            convention = boost::make_shared<ZeroInflationIndexConvention>();
        } else {
            // No need to QL_FAIL here, just go to the next one
            WLOG("Convention name, " << childName << ", not recognized.");
            continue;
        }

        string id = XMLUtils::getChildValue(child, "Id", true);

        try {
            DLOG("Loading Convention " << id);
            convention->fromXML(child);
            add(convention);
        } catch (exception& e) {
            WLOG("Exception parsing convention "
                 "XML Node (id = "
                 << id << ") : " << e.what());
        }
    }
}

XMLNode* Conventions::toXML(XMLDocument& doc) {

    XMLNode* conventionsNode = doc.allocNode("Conventions");

    map<string, boost::shared_ptr<Convention>>::const_iterator it;
    for (it = data_.begin(); it != data_.end(); ++it) {
        XMLUtils::appendNode(conventionsNode, data_[it->first]->toXML(doc));
    }
    return conventionsNode;
}

void Conventions::clear() { data_.clear(); }

boost::shared_ptr<Convention> Conventions::get(const string& id) const {
    auto it = data_.find(id);
    QL_REQUIRE(it != data_.end(), "Cannot find conventions for id " << id);
    return it->second;
}

pair<bool, boost::shared_ptr<Convention>> Conventions::get(const string& id, const Convention::Type& type) const {
    auto c = data_.find(id);
    if (c == data_.end() || c->second->type() != type)
        return make_pair(false, nullptr);
    else
        return make_pair(true, c->second);
}

std::set<boost::shared_ptr<Convention>> Conventions::get(const Convention::Type& type) const {
    std::set<boost::shared_ptr<Convention>> result;
    for (auto const& d : data_) {
        if (d.second->type() == type)
            result.insert(d.second);
    }
    return result;
}

bool Conventions::has(const string& id) const { return data_.count(id) == 1; }

bool Conventions::has(const std::string& id, const Convention::Type& type) const {
    return get(id, type).first;
}

void Conventions::add(const boost::shared_ptr<Convention>& convention) {
    const string& id = convention->id();
    QL_REQUIRE(data_.find(id) == data_.end(), "Convention already exists for id " << id);
    data_[id] = convention;
}

} // namespace data
} // namespace ore
