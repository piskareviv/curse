#include <format>
#include <iostream>
#include <memory>

#include "src/core/csv.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << std::format(
                         "usage: {} [INPUT_FILE] [OUTPUT_FILE]\n"
                         "if INPUT_FILE is \"-\", reads from stdin\n",
                         argv[0])
                  << std::endl;
        return 1;
    }

    using namespace curse;  // NOLINT

    const curse::Schema hits_schema = curse::Schema({
        {"WatchID", TypeId::Int64},
        {"JavaEnable", TypeId::Int16},
        {"Title", TypeId::String},
        {"GoodEvent", TypeId::Int16},
        {"EventTime", TypeId::Timestamp},
        {"EventDate", TypeId::Date},
        {"CounterID", TypeId::Int32},
        {"ClientIP", TypeId::Int32},
        {"RegionID", TypeId::Int32},
        {"UserID", TypeId::Int64},
        {"CounterClass", TypeId::Int16},
        {"OS", TypeId::Int16},
        {"UserAgent", TypeId::Int16},
        {"URL", TypeId::String},
        {"Referer", TypeId::String},
        {"IsRefresh", TypeId::Int16},
        {"RefererCategoryID", TypeId::Int16},
        {"RefererRegionID", TypeId::Int32},
        {"URLCategoryID", TypeId::Int16},
        {"URLRegionID", TypeId::Int32},
        {"ResolutionWidth", TypeId::Int16},
        {"ResolutionHeight", TypeId::Int16},
        {"ResolutionDepth", TypeId::Int16},
        {"FlashMajor", TypeId::Int16},
        {"FlashMinor", TypeId::Int16},
        {"FlashMinor2", TypeId::String},
        {"NetMajor", TypeId::Int16},
        {"NetMinor", TypeId::Int16},
        {"UserAgentMajor", TypeId::Int16},
        {"UserAgentMinor", TypeId::String},
        {"CookieEnable", TypeId::Int16},
        {"JavascriptEnable", TypeId::Int16},
        {"IsMobile", TypeId::Int16},
        {"MobilePhone", TypeId::Int16},
        {"MobilePhoneModel", TypeId::String},
        {"Params", TypeId::String},
        {"IPNetworkID", TypeId::Int32},
        {"TraficSourceID", TypeId::Int16},
        {"SearchEngineID", TypeId::Int16},
        {"SearchPhrase", TypeId::String},
        {"AdvEngineID", TypeId::Int16},
        {"IsArtifical", TypeId::Int16},
        {"WindowClientWidth", TypeId::Int16},
        {"WindowClientHeight", TypeId::Int16},
        {"ClientTimeZone", TypeId::Int16},
        {"ClientEventTime", TypeId::Timestamp},
        {"SilverlightVersion1", TypeId::Int16},
        {"SilverlightVersion2", TypeId::Int16},
        {"SilverlightVersion3", TypeId::Int32},
        {"SilverlightVersion4", TypeId::Int16},
        {"PageCharset", TypeId::String},
        {"CodeVersion", TypeId::Int32},
        {"IsLink", TypeId::Int16},
        {"IsDownload", TypeId::Int16},
        {"IsNotBounce", TypeId::Int16},
        {"FUniqID", TypeId::Int64},
        {"OriginalURL", TypeId::String},
        {"HID", TypeId::Int32},
        {"IsOldCounter", TypeId::Int16},
        {"IsEvent", TypeId::Int16},
        {"IsParameter", TypeId::Int16},
        {"DontCountHits", TypeId::Int16},
        {"WithHash", TypeId::Int16},
        {"HitColor", TypeId::Char},
        {"LocalEventTime", TypeId::Timestamp},
        {"Age", TypeId::Int16},
        {"Sex", TypeId::Int16},
        {"Income", TypeId::Int16},
        {"Interests", TypeId::Int16},
        {"Robotness", TypeId::Int16},
        {"RemoteIP", TypeId::Int32},
        {"WindowName", TypeId::Int32},
        {"OpenerName", TypeId::Int32},
        {"HistoryLength", TypeId::Int16},
        {"BrowserLanguage", TypeId::String},
        {"BrowserCountry", TypeId::String},
        {"SocialNetwork", TypeId::String},
        {"SocialAction", TypeId::String},
        {"HTTPError", TypeId::Int16},
        {"SendTiming", TypeId::Int32},
        {"DNSTiming", TypeId::Int32},
        {"ConnectTiming", TypeId::Int32},
        {"ResponseStartTiming", TypeId::Int32},
        {"ResponseEndTiming", TypeId::Int32},
        {"FetchTiming", TypeId::Int32},
        {"SocialSourceNetworkID", TypeId::Int16},
        {"SocialSourcePage", TypeId::String},
        {"ParamPrice", TypeId::Int64},
        {"ParamOrderID", TypeId::String},
        {"ParamCurrency", TypeId::String},
        {"ParamCurrencyID", TypeId::Int16},
        {"OpenstatServiceName", TypeId::String},
        {"OpenstatCampaignID", TypeId::String},
        {"OpenstatAdID", TypeId::String},
        {"OpenstatSourceID", TypeId::String},
        {"UTMSource", TypeId::String},
        {"UTMMedium", TypeId::String},
        {"UTMCampaign", TypeId::String},
        {"UTMContent", TypeId::String},
        {"UTMTerm", TypeId::String},
        {"FromTag", TypeId::String},
        {"HasGCLID", TypeId::Int16},
        {"RefererHash", TypeId::Int64},
        {"URLHash", TypeId::Int64},
        {"CLID", TypeId::Int32},
    });

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    std::unique_ptr<curse::BatchStream> input_stream;
    if (input_file == "-") {
        input_stream = std::make_unique<curse::CsvReader>(std::cin, hits_schema);
    } else {
        input_stream = std::make_unique<curse::CsvReader>(input_file, hits_schema);
    }

    WriteAsCurse(output_file, std::move(input_stream));

    return 0;
}
