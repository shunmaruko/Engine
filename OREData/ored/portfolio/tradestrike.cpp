/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/portfolio/tradestrike.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/export.hpp>

namespace ore {
namespace data {

void TradeStrike::fromXML(XMLNode* node) { TradeMonetary::fromXML(XMLUtils::getChildNode(node, "StrikeData")); }

XMLNode* TradeStrike::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("StrikeData");
    XMLUtils::appendNode(node, TradeMonetary::toXML(doc));
    return node;
}

}
} // namespace ore