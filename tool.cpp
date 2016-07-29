#include "apd/common/UcLogReport.hpp"

#include <apd/common/BomAvailPricingRq.hpp>
#include <apd/common/BomAvailPricingRs.hpp>
#include <cri/shopping/BomPropertyStay.hpp>
#include <cri/shopping/BomRoomStay.hpp>
#include <cri/shopping/CriShoppingChannelHelper.hpp>
#include <apd/commonutils/OtfVarRetriever.hpp>
#include <apd/common/DCXHelper.hpp>
#include <apd/common/ApdCatchMacros.hpp>
#include <kit/FldDateTime.hpp>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
//#include <boost/foreach.hpp>
#include <boost/foreach.hpp> //boost foreach is not accessible in the current MW Pack
#include <apd/commonutils/OtfVarRetriever.hpp>
#include <apd/common/ApdOtfVarsTemp.hpp>
#include <apd/common/roomcodeclassifier/BomResult.hpp>

using namespace CRI::shopping;  
//namespace is the scope, (if there is another function with the same name in another library, 
//the name will refer to the function in shopping)

namespace APD {

std::string const UcLogReport::EMPTY_FIELD = "";
std::string const UcLogReport::ROOM_PARSER_STATS = "RoomParser";

// ////////////////////////////////////////////////////////////////////////////
//constructor of class UcLogReport
UcLogReport::UcLogReport( BomAvailPricingRs  const* const iResponse
                        , BomAvailPricingRq  const* const iRequest
                        , toolbox::TimeValue const& iRequestTimestamp
                        , std::string        const& iAtid
                        , std::string        const& iOfficeId
                        , std::string        const& iChannel
                        , std::string        const& iSubChannel
                        , std::string        const& iRequestedRates
                        )
: _responseTime(getTimeElapsedSince(iRequestTimestamp))
, _atid(iAtid)
, _officeId(iOfficeId)
, _channel(iChannel)
, _subChannel(iSubChannel)
, _crawling(false)
, _sampling(false)
{
    if (iResponse) {
        log(*iResponse, iRequest, EMPTY_FIELD, iRequestedRates, false);
        logRoomParserStats(*iResponse, false);
    }
    else {
      APD_LOG_INFO("APD_REPORT ==> Error: response is NULL");
    }
}


UcLogReport::UcLogReport( BomAvailPricingRs  const* const iResponse
                        , BomAvailPricingRq  const* const iRequest
                        , toolbox::TimeValue const& iRequestTimestamp
                        , bool                      iMultiSingle
                        )
: _responseTime(getTimeElapsedSince(iRequestTimestamp))
, _atid(getAtid())
, _officeId(getOfficeIdFromRequest(iRequest))
, _channel(getChannelFromRequest(iRequest))
, _subChannel(getSubChannelFromRequest(iRequest))
, _crawling(iRequest?iRequest->isCrawlingRequest():false)
, _sampling(iRequest?iRequest->isFromSampling():false)
{
    if (iResponse) {
        log(*iResponse, iRequest, getProvidersFromRequest(iRequest), getRatesFromRequest(iRequest), iMultiSingle);
        logRoomParserStats(*iResponse, iMultiSingle);
    }
    else {
      APD_LOG_INFO("APD_REPORT ==> Error: response is NULL");
    }
}

// If log changes:
// Increase LOG_VERSION
// Update wiki: http://hdpdoc/doku.php?id=teams:hda:projects:search_engine:reporting#apd_logs
uint32_t const UcLogReport::LOG_VERSION = 1;
// ////////////////////////////////////////////////////////////////////////////
void UcLogReport::log(BomAvailPricingRs const& iResponse, BomAvailPricingRq const* const iRequest, std::string const& iProviders,
                        std::string const& iRequestedRates, bool iMultiSingle)
{
    APD_LOG_INFO("APD_REPORT - log()");
    std::stringstream theReport;
    //theReport is a stringstream
    //<< is the operator the put string in the stream, then theReport.str() will output the string in the stream
    // SECTION_START is |, I guess, FIELD_SEPARATOR is |
        try {
            std::vector<BomPropertyStay*> const theProperties = iResponse.getCandidateProperties();
            theReport << LOG_VERSION << SECTION_START;
            if (iMultiSingle) theReport << "MultiSingle";
            else theReport << getFunctionality(&iResponse);
            theReport << getCrawlingSamplingSuffix()       << FIELD_SEPARATOR
                      << generateTransactionDate()         << FIELD_SEPARATOR
                      << _responseTime                     << FIELD_SEPARATOR
                      << _officeId                         << FIELD_SEPARATOR
                      << _atid                             << FIELD_SEPARATOR
                      << _channel                          << FIELD_SEPARATOR
                      << _subChannel                       << FIELD_SEPARATOR
                      << getLengthOfStay(iRequest) << FIELD_SEPARATOR
                      << getCheckInDate(iRequest)  << FIELD_SEPARATOR
                      << getOccupancy(iRequest)    << FIELD_SEPARATOR
                      << iProviders  << FIELD_SEPARATOR
                      << iRequestedRates << FIELD_SEPARATOR;

            if(OtfVarRetriever::getOTFVarBool(OtfVarsTemp::kStrOtfVarLogRequestCriteria, false)) {
                theReport   << getCitiesFromRequest(iRequest) << FIELD_SEPARATOR
                            << getChainsFromRequest(iRequest) << FIELD_SEPARATOR
                            << getPropertiesFromRequest(iRequest) << FIELD_SEPARATOR;
            }

            theReport << theProperties.size() //In case of Single or Pricing it will be 1 (I hope)
                      ;

            BOOST_FOREACH(const BomPropertyStay* aProperty, theProperties)
            {
                if (aProperty) {
                    std::vector<BomRoomStay*> const aVectorBomRoomStay = aProperty->getRoomStays();

                    std::string aOrigin = (aProperty->getSource() != kUnknownSource) ? getOrigin(aProperty):getOrigin(&iResponse);
                    theReport << SECTION_START   << aOrigin
                              << FIELD_SEPARATOR << getPropertyId(aProperty)
                              << FIELD_SEPARATOR << getChainCode(aProperty)
                              << FIELD_SEPARATOR << aVectorBomRoomStay.size()
                              ;
                    BOOST_FOREACH(const BomRoomStay* aRoomStay, aVectorBomRoomStay )
                    {
                        if (aRoomStay && !aRoomStay->getRoomRates().empty()) {
                            BomRoomRate const* const aRoomRate = aRoomStay->getRoomRates().at(0);

                            theReport << SECTION_START   << getBookingCode(aRoomRate)
                                      << FIELD_SEPARATOR << getCurrency(aRoomRate)
                                      << FIELD_SEPARATOR << getBaseAmount(aRoomRate)
                                      << FIELD_SEPARATOR << getTotalAmount(aRoomRate)
                                      << FIELD_SEPARATOR << getRateCode(aRoomRate)
                                      ;
                        }
                        else {
                            theReport << SECTION_START << FIELD_SEPARATOR << FIELD_SEPARATOR
                                      << FIELD_SEPARATOR << FIELD_SEPARATOR;
                        }
                    }
                }
            }

            //this HDP_LOG_REPORT write the log to log file, I guess
            HDP_LOG_REPORT(theReport.str());
            HDP_LOG_DEBUG(theReport.str());

        } APD_CATCH_DO_NOTHING;
}


class ChainStats
{
public:

    ChainStats() : _totalRoomCodes(0),
                   _totalRoomCodesIdentified(0),
                   _totalPartialRoomCodesIdentified(0),
                   _totalRoomCategoriesIdentified(0),
                   _totalBedTypesIdentified(0) {

    }

    uint16_t _totalRoomCodes;
    uint16_t _totalRoomCodesIdentified;
    uint16_t _totalPartialRoomCodesIdentified;
    uint16_t _totalRoomCategoriesIdentified;
    uint16_t _totalBedTypesIdentified;
};

bool UcLogReport::isLogRoomParser(BomAvailPricingRs const& iResponse) {

    TransactionTypeT const aTransaction = iResponse.getTransaction();
    if(BomCriAvailPricingRs::kPricing == aTransaction || BomCriAvailPricingRs::kSingleAvail == aTransaction){
        return true;
    }
    return false;
}


void UcLogReport::logRoomParserStats(BomAvailPricingRs const& iResponse, bool iMultiSingle)
{
    if(isLogRoomParser(iResponse) || iMultiSingle){
        APD_LOG_INFO("APD_REPORT - logRoomParserStats()");
        try {
            const std::vector<BomPropertyStay*> aProperties = iResponse.getCandidateProperties();
            std::map<std::string, ChainStats> aChainStats;

            BOOST_FOREACH(const BomPropertyStay* aProperty, aProperties)
            {
                if (aProperty && aProperty->getPropertyProduct() && aProperty->getPropertyProduct()->getChainDetails() &&
                        aProperty->getPropertyProduct()->getChainDetails()->getCode().isValid()) {

                    const std::string aChainCode = aProperty->getPropertyProduct()->getChainDetails()->getCode().get();
                    ChainStats& aChainStat = aChainStats[aChainCode];

                    std::vector<BomRoomStay*> const aRoomStays = aProperty->getRoomStays();
                    BOOST_FOREACH(const BomRoomStay* aRoomStay, aRoomStays )
                    {
                        if (aRoomStay) {

                            BOOST_FOREACH(const BomRoomRate* aRoomRate, aRoomStay->getRoomRates()) {

                                if(aRoomRate && aRoomRate->getRoomDetails()) {

                                    if(aRoomRate->getRoomDetails()->getRoomType().isValid()) {
                                        ++aChainStat._totalRoomCodes;
                                    }

                                    if(aRoomRate->getRoomDetails()->getCalculatedRoomType().isValid()) {
                                        std::string aCalculatedRoomCode = aRoomRate->getRoomDetails()->getCalculatedRoomType().get();

                                        if(aCalculatedRoomCode.at(0) != roomcodeclassifier::kUnknown) {
                                            ++aChainStat._totalRoomCategoriesIdentified;
                                        }

                                        if(aCalculatedRoomCode.at(2) != roomcodeclassifier::kUnknown) {
                                            ++aChainStat._totalBedTypesIdentified;
                                        }

                                        if(aCalculatedRoomCode.find(roomcodeclassifier::kUnknown) == std::string::npos) {
                                            ++aChainStat._totalRoomCodesIdentified;
                                        }
                                        else {
                                            ++aChainStat._totalPartialRoomCodesIdentified;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if(!aChainStats.empty()) {

                std::stringstream aReport;
                aReport << LOG_VERSION << SECTION_START;
                aReport << ROOM_PARSER_STATS << FIELD_SEPARATOR;

                if (iMultiSingle) aReport << "MultiSingle";
                else aReport << getFunctionality(&iResponse);

                for(std::map<std::string, ChainStats>::const_iterator it = aChainStats.begin(); it != aChainStats.end(); ++it) {
                    aReport << SECTION_START   << it->first;

                    ChainStats aChainStat = it->second;
                    aReport << FIELD_SEPARATOR << aChainStat._totalRoomCodes;
                    aReport << FIELD_SEPARATOR << aChainStat._totalRoomCodesIdentified;
                    aReport << FIELD_SEPARATOR << aChainStat._totalPartialRoomCodesIdentified;
                    aReport << FIELD_SEPARATOR << aChainStat._totalRoomCategoriesIdentified;
                    aReport << FIELD_SEPARATOR << aChainStat._totalBedTypesIdentified;
                }

                HDP_LOG_STATS_REPORT(aReport.str());
                HDP_LOG_DEBUG(aReport.str());
            }
        } APD_CATCH_DO_NOTHING;
    }
}



// ////////////////////////////////////////////////////////////////////////////
// Response Time
// ////////////////////////////////////////////////////////////////////////////
double UcLogReport::getTimeElapsedSince(toolbox::TimeValue const& iTimestamp)
{
    using namespace toolbox;
    TimeValue const theDiffTime = TimeValue::GetHRTime() - iTimestamp;

    //APD_LOG_INFO("APD_REPORT - getTimeElapsedSince()\n"
    //             "--> RQ time secs  = '" << iTimestamp.getSecond() << "'\n"
    //             "--> RQ time usec  = '" << iTimestamp.getMicrosecond() << "'\n"
    //             "------------------------------------------\n"
    //             "--> DiffTime secs = '" << theDiffTime.getSecond() << "'\n"
    //             "--> DiffTime usec = '" << theDiffTime.getMicrosecond() << "'\n"
    //            );

    return theDiffTime.getSecond() + (theDiffTime.getMicrosecond() * 0.000001);
}

// ////////////////////////////////////////////////////////////////////////////
// Transaction Date
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::generateTransactionDate()
{
    UP_Time theTime;
    KIT::FldDateTime theDateTime(theTime);
    std::stringstream theTimestampStr;
    theTimestampStr << std::setfill('0')
                    << theDateTime.getYear()
                    << std::setw(2) << theDateTime.getMonth()
                    << std::setw(2) << theDateTime.getDay()
                    << "-"
                    << std::setw(2) << theDateTime.getHour()
                    << std::setw(2) << theDateTime.getMinute()
                    << std::setw(2) << theDateTime.getSecond()
                    ;
    return theTimestampStr.str();
}

// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::formatOrigin(SourceOfReplyEnum const iOrigin)
{
    return ( iOrigin == kProvider_dyn       ? "Provider_dyn"       :
           ( iOrigin == kAmadeus_dyn        ? "Amadeus_dyn"        :
           ( iOrigin == kAccor_dyn          ? "Accor_dyn"          :
           ( iOrigin == kCentralSys         ? "CentralSys"         :
           ( iOrigin == kCache_FSA_Amounts  ? "Cache_FSA_Amounts"  :
           ( iOrigin == kCache_FSA_Seamless ? "Cache_FSA_Seamless" :
           ( iOrigin == kUnknownSource      ? "UnknownSource"      : EMPTY_FIELD
           )))))));
}

// ////////////////////////////////////////////////////////////////////////////
// Transaction Type
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getFunctionality(BomAvailPricingRs const* const iResponse) const
{
    if (iResponse) {
        TransactionTypeT const t = iResponse->getTransaction();
        return ( t == BomCriAvailPricingRs::kPricing     ? "Pricing"     :
               ( t == BomCriAvailPricingRs::kSingleAvail ? "SingleAvail" :
               ( t == BomCriAvailPricingRs::kMultiAvail  ? "MultiAvail"  :
               ( t == BomCriAvailPricingRs::kUnknown     ? "Unknown"     : EMPTY_FIELD
               ))));
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Providers
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getProvidersFromRequest(BomAvailPricingRq const* const iRequest)
{
    if (iRequest) {
        if (iRequest->isForLeisure()) {
            return "leisure";
        } else if (iRequest->isForMixedProviders()) {
            return "mixed";
        } else {
            return "distribution";
        }
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Search criteria
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getCitiesFromRequest(BomAvailPricingRq const* const iRequest)
{
    std::stringstream aCitiesSS;

    if(iRequest->getLocationDetails() &&
       iRequest->getLocationDetails()->getAddress() &&
       iRequest->getLocationDetails()->getAddress()->getCity() &&
       iRequest->getLocationDetails()->getAddress()->getCity()->getCode().isValid()) {

        aCitiesSS << FIELD_VALUE_SEPARATOR << iRequest->getLocationDetails()->getAddress()->getCity()->getCode().get();
    }

    if(iRequest->getLocationDetails() &&
       iRequest->getLocationDetails()->getRelativeLocation() &&
       iRequest->getLocationDetails()->getRelativeLocation()->getPointOfInterest() &&
       iRequest->getLocationDetails()->getRelativeLocation()->getPointOfInterest()->getIATACode().isValid()) {

        aCitiesSS << FIELD_VALUE_SEPARATOR << iRequest->getLocationDetails()->getRelativeLocation()->getPointOfInterest()->getIATACode().get();
    }

    const std::string aCitiesStr = aCitiesSS.str();
    if (!aCitiesStr.empty()) {
        return aCitiesStr.substr(1);
    }

    return EMPTY_FIELD;
}

std::string UcLogReport::getPropertiesFromRequest(BomAvailPricingRq const* const iRequest)
{
    std::stringstream aPropertiesSS;

    if(iRequest->getPropertyProduct() &&
       iRequest->getPropertyProduct()->getPropertyId() &&
       iRequest->getPropertyProduct()->getPropertyId()->isValid()) {

        aPropertiesSS << FIELD_VALUE_SEPARATOR << iRequest->getPropertyProduct()->getPropertyId()->get();
    }

    if(iRequest->getPredefinedPropertyList() && !iRequest->getPredefinedPropertyList()->getPropertyProducts().empty()) {
        BOOST_FOREACH(const BomPropertyProduct* aProperty, iRequest->getPredefinedPropertyList()->getPropertyProducts()) {
            if(aProperty && aProperty->getPropertyId() && aProperty->getPropertyId()->isValid()) {
                aPropertiesSS << FIELD_VALUE_SEPARATOR << aProperty->getPropertyId()->get();
            }
        }
    }

    if(iRequest->getPreferredPropertyList() && !iRequest->getPreferredPropertyList()->getPropertyProducts().empty()) {
        BOOST_FOREACH(const BomPropertyProduct* aProperty, iRequest->getPreferredPropertyList()->getPropertyProducts()) {
            if(aProperty && aProperty->getPropertyId() && aProperty->getPropertyId()->isValid()) {
                aPropertiesSS << FIELD_VALUE_SEPARATOR << aProperty->getPropertyId()->get();
            }
        }
    }

    const std::string aPropertiesStr = aPropertiesSS.str();
    if (!aPropertiesStr.empty()) {
        return aPropertiesStr.substr(1);
    }

    return EMPTY_FIELD;
}

std::string UcLogReport::getChainsFromRequest(BomAvailPricingRq const* const iRequest)
{
    std::stringstream aChainsSS;

    if(iRequest->getChainDetails() && !iRequest->getChainDetails()->getCode().isVoid()) {
        aChainsSS << FIELD_VALUE_SEPARATOR << iRequest->getChainDetails()->getCode().get();
    }

    if(iRequest->getChainList()) {
        BOOST_FOREACH(const KIT::FldString& aChainCode, iRequest->getChainList()->getChainCodes()) {
            if(!aChainCode.isVoid()) {
                aChainsSS << FIELD_VALUE_SEPARATOR << aChainCode.get();
            }
        }
    }

    const std::string aChainsStr = aChainsSS.str();
    if (!aChainsStr.empty()) {
        return aChainsStr.substr(1);
    }

    return EMPTY_FIELD;
}
//the following is for getting the value of the field, e.g. Rates,...

// ////////////////////////////////////////////////////////////////////////////
// Rates
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getRatesFromRequest(BomAvailPricingRq const* const iRequest)
{
    if (iRequest) {
        const BomRateDetails* aBomRateDetails = iRequest->getRateDetails();
        if (aBomRateDetails) {
            std::stringstream aRatePlansSS;
            static const std::string kSeparator = "-";
            std::vector<BomRatePlan*> const& aVectorBomRatePlan = aBomRateDetails->getRatePlans();
            if (!aVectorBomRatePlan.empty()) {
                BOOST_FOREACH(const BomRatePlan* aRatePlan, aVectorBomRatePlan )
                {
                    if (aRatePlan && aRatePlan->getRatePlanCode().isValid())
                        aRatePlansSS  << kSeparator << aRatePlan->getRatePlanCode().get();
                }
                const std::string aRatePlansStr = aRatePlansSS.str();
                if (not aRatePlansStr.empty())
                    return aRatePlansStr.substr(1);
            }
        }
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Property ID
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getPropertyId(BomPropertyStay const* const iProperty) const
{
    if (iProperty && iProperty->getPropertyProduct()
                  && iProperty->getPropertyProduct()->getPropertyId()
                  && iProperty->getPropertyProduct()->getPropertyId()->isValid()) {

        return iProperty->getPropertyProduct()->getPropertyId()->get();
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Chain Code
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getChainCode(BomPropertyStay const* const p) const
{
    if (p && p->getPropertyProduct()
          && p->getPropertyProduct()->getChainDetails()
          && p->getPropertyProduct()->getChainDetails()->getCode().isValid()) {

        return p->getPropertyProduct()->getChainDetails()->getCode().get();
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// OfficeID
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getOfficeIdFromRequest(BomAvailPricingRq const* const iRequest)
{
    if (iRequest && iRequest->getOriginator()) {
        BomOfficeInformation const* const theOffice =
                iRequest->getOriginator()->getOfficeInformation();

        if (theOffice) {
            if (theOffice->getAmadeusOfficeId().isValid()) {
                return theOffice->getAmadeusOfficeId().get();
            }
            if (theOffice->getPseudoCityCode().isValid()) {
                return theOffice->getPseudoCityCode().get();
            }
        }
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// 1A channel code - or channel code by default
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getChannelFromRequest(BomAvailPricingRq const* const rq)
{
    std::string aResult, aSubChannel;
    CRI::shopping::CriShoppingChannelHelper::computeFunctionalChannelAndSubChannel(*rq, aResult, aSubChannel);
    return aResult;
}

// ////////////////////////////////////////////////////////////////////////////
// Primary source
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getSubChannelFromRequest(BomAvailPricingRq const* const rq)
{
    std::string aChannel, aResult;
    CRI::shopping::CriShoppingChannelHelper::computeFunctionalChannelAndSubChannel(*rq, aChannel, aResult);
    return aResult;
}

// ////////////////////////////////////////////////////////////////////////////
// ATID
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getAtid()
{
    std::string aStrAtid = EMPTY_FIELD;
    DCXHelper::getAtid(aStrAtid);
    return aStrAtid;
}

// ////////////////////////////////////////////////////////////////////////////
// CheckIn date
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getCheckInDate(BomAvailPricingRq const* const iRequest) const
{
    try {
        if (iRequest && iRequest->getPeriod()) {
            return iRequest->getPeriod()->getStartDate().format(KIT::FldDateFormat::YYMMDD);
        }
    } APD_CATCH_DO_NOTHING;
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Length of stay
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getLengthOfStay(BomAvailPricingRq const* const iRequest) const
{
    try {
        if (iRequest && iRequest->getPeriod()) {
            std::stringstream theLengthOfStay;
            // It is assumed endDate > startDate
            const KIT::FldDate& endDate = iRequest->getPeriod()->getEndDate().get();
            const KIT::FldDate& startDate = iRequest->getPeriod()->getStartDate().get();

            // Dates are into the same year
            if (endDate.get().year() == startDate.get().year()) {
                theLengthOfStay << (endDate.get().day() - startDate.get().day());
            }
            else { // Dates not into the same year
                int nbDaysEndDate = UP_Date::DaysInYear(endDate.get().year()) - endDate.get().day();
                int nbDaysStartDate = UP_Date::DaysInYear(startDate.get().year()) - startDate.get().day();
                theLengthOfStay << (nbDaysEndDate + nbDaysStartDate);
            }
            return theLengthOfStay.str();
        }
    } APD_CATCH_DO_NOTHING;
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Occupancy
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getOccupancy(BomAvailPricingRq const* const iRequest) const
{
    try {
        if (iRequest && iRequest->getRoomDetails().at(0)) {
            return iRequest->getRoomDetails().at(0)->getOccupancy().get();
        }
    } APD_CATCH_DO_NOTHING;
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Booking Code
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getBookingCode(BomRoomRate const* const iRoomRate) const
{
    if (iRoomRate && iRoomRate->getBookingCode().isValid()) {

        return iRoomRate->getBookingCode().get();
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Currency
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getCurrency(BomRoomRate const* const iRoomRate) const
{
    if (iRoomRate) {
        BomRate const* const aRate = iRoomRate->getBookingRate();
        if (aRate && aRate->getBaseAmountWithTaxes()._amount._currency.isValid())
        {
            return aRate->getBaseAmountWithTaxes()._amount._currency._code.get();
        }
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Base Amount
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getBaseAmount(BomRoomRate const* const iRoomRate) const
{
    try {
        if (iRoomRate) {
            BomRate const* const aRate = iRoomRate->getBookingRate();
            if (aRate && aRate->getBaseAmountWithTaxes()._amount.isValid()) {
                return aRate->getBaseAmountWithTaxes()._amount.getAmount().toString();
            }
        }
    } APD_CATCH_DO_NOTHING;
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Rate Code
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getRateCode(BomRoomRate const* const iRoomRate) const
{
    if (iRoomRate) {
        BomRatePlan const* const aRatePlan = iRoomRate->getRatePlan();
        if (aRatePlan && aRatePlan->getRatePlanCode().isValid())
        {
            return aRatePlan->getRatePlanCode().get();
        }
    }
    return EMPTY_FIELD;
}

// ////////////////////////////////////////////////////////////////////////////
// Total Amount
// ////////////////////////////////////////////////////////////////////////////
std::string UcLogReport::getTotalAmount(BomRoomRate const* const iRoomRate) const
{
    try {
        if (iRoomRate) {
            BomRate const* const aRate = iRoomRate->getBookingRate();
            if (aRate && aRate->getTotalAmountWithTaxes()._amount.isValid()) {
                return aRate->getTotalAmountWithTaxes()._amount.getAmount().toString();
            }
        }
    } APD_CATCH_DO_NOTHING;
    return EMPTY_FIELD;
}

std::string UcLogReport::getCrawlingSamplingSuffix() const
{
    std::string aResult;
    bool aEncode = false;
    static const std::string kOtfVarName = "HOS_APD_LOG_REPORT_ENCODE_CRAWLING_SAMPLING";
    static const std::string kY="Y";
    const KIT::FldString aEncodeStr = OtfVarRetriever::getOTFVar(kOtfVarName);
    if (aEncodeStr.isValid()) {
        aEncode = aEncodeStr.get()==kY;
    }
    if (aEncode) {
        if (_sampling) {
            static const std::string kMinusSampling="-sampling";
            aResult = kMinusSampling;
        } else if (_crawling) {
            static const std::string kMinusCrawling="-crawling";
            aResult = kMinusCrawling;
        }
    }
    return aResult;
}

} // end namespace APD
