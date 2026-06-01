#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>

#include "src/core/assert.hpp"
#include "src/core/csv.hpp"
#include "src/core/operators.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"
#include "src/exec/hits_schema.hpp"

curse::Schema SubSchema(const curse::Schema& schema, std::vector<std::string> sub_schema) {
    std::vector<curse::Schema::ColumnInfo> cols;
    for (const std::string& name : sub_schema) {
        cols.push_back(schema.Columns()[schema.IndexOf(name)]);
    }
    return curse::Schema(cols);
}

std::unique_ptr<curse::BatchStream> Q0(const std::string& file) {
    std::unique_ptr<curse::BatchStream> reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    curse::AggregationOperator count(
        {curse::AggregationOperator::Params{.tp = curse::AggType::Count, .inp_col = "UserID", .out_col = "1"}});

    return std::move(reader) >= count;
}

std::unique_ptr<curse::BatchStream> Q1(const std::string& file) {
    std::unique_ptr<curse::BatchStream> reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID"}));

    curse::FilterOperator filt("AdvEngineID");

    curse::AggregationOperator count(
        {curse::AggregationOperator::Params{.tp = curse::AggType::Count, .inp_col = "AdvEngineID", .out_col = "1"}});

    return std::move(reader) >= filt >= count;
}

std::unique_ptr<curse::BatchStream> Q2(const std::string& file) {
    std::unique_ptr<curse::BatchStream> reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID", "ResolutionWidth"}));

    curse::AggregationOperator aggr({
        curse::AggregationOperator::Params{.tp = curse::AggType::Sum, .inp_col = "AdvEngineID", .out_col = "1"},
        curse::AggregationOperator::Params{.tp = curse::AggType::Count, .inp_col = "AdvEngineID", .out_col = "2"},
        curse::AggregationOperator::Params{.tp = curse::AggType::Average, .inp_col = "ResolutionWidth", .out_col = "3"},
    });

    return std::move(reader) >= aggr;
}

std::unique_ptr<curse::BatchStream> Q3(const std::string& file) {
    std::unique_ptr<curse::BatchStream> reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    curse::AggregationOperator avg({
        curse::AggregationOperator::Params{.tp = curse::AggType::Average, .inp_col = "UserID", .out_col = "1"},
    });

    return std::move(reader) >= avg;
}

std::unique_ptr<curse::BatchStream> Q4(const std::string& file) {
    std::unique_ptr<curse::BatchStream> reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    curse::AggregationOperator count_distinct({
        curse::AggregationOperator::Params{.tp = curse::AggType::CountDistinct, .inp_col = "UserID", .out_col = "1"},
    });

    return std::move(reader) >= count_distinct;
}

std::unique_ptr<curse::BatchStream> Q5(const std::string& file) {
    std::unique_ptr<curse::BatchStream> reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));

    curse::AggregationOperator count_distinct({
        curse::AggregationOperator::Params{
            .tp = curse::AggType::CountDistinct, .inp_col = "SearchPhrase", .out_col = "1"},
    });

    return std::move(reader) >= count_distinct;
}

std::unique_ptr<curse::BatchStream> Q6(const std::string& file) {
    std::unique_ptr<curse::BatchStream> reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"EventDate"}));

    curse::AggregationOperator min_max({
        curse::AggregationOperator::Params{.tp = curse::AggType::Min, .inp_col = "EventDate", .out_col = "1"},
        curse::AggregationOperator::Params{.tp = curse::AggType::Max, .inp_col = "EventDate", .out_col = "2"},
    });

    return std::move(reader) >= min_max;
}

// SELECT AdvEngineID, COUNT(*) FROM hits WHERE AdvEngineID <> 0 GROUP BY AdvEngineID ORDER BY COUNT(*) DESC;
std::unique_ptr<curse::BatchStream> Q7(const std::string& file) {
    std::unique_ptr<curse::BatchStream> reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID"}));

    curse::FilterOperator filt("AdvEngineID");

    curse::GroupByOperator group_by(
        {"AdvEngineID"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::Count, .inp_col = "AdvEngineID", .out_col = "cnt"}});

    curse::SortOperator sort({curse::SortOperator::Params{.inp_col = "cnt", .reversed = true}});

    return std::move(reader) >= filt >= group_by >= sort;
}

// SELECT RegionID, COUNT(DISTINCT UserID) AS u FROM hits GROUP BY RegionID ORDER BY u DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q8(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"RegionID", "UserID"}));

    auto group_by = curse::GroupByOperator(
        {"RegionID"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});

    auto sort = curse::SortOperator({{.inp_col = "u", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

// SELECT RegionID, SUM(AdvEngineID), COUNT(*) AS c, AVG(ResolutionWidth), COUNT(DISTINCT UserID) FROM hits GROUP BY
// RegionID ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q9(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(
        file, SubSchema(kHitsSchema, {"RegionID", "AdvEngineID", "ResolutionWidth", "UserID"}));

    auto group_by = curse::GroupByOperator(
        {"RegionID"},
        {
            curse::GroupByOperator::Params{
                .tp = curse::AggType::Sum, .inp_col = "AdvEngineID", .out_col = "sum_AdvEngineID"},
            curse::GroupByOperator::Params{.tp = curse::AggType::Count, .inp_col = "RegionID", .out_col = "c"},
            curse::GroupByOperator::Params{
                .tp = curse::AggType::Average, .inp_col = "ResolutionWidth", .out_col = "avg_ResolutionWidth"},
            curse::GroupByOperator::Params{
                .tp = curse::AggType::CountDistinct, .inp_col = "UserID", .out_col = "distinct_UserID"},
        });

    auto sort = curse::SortOperator({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

// SELECT MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE MobilePhoneModel <> '' GROUP BY MobilePhoneModel
// ORDER BY u DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q10(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"MobilePhoneModel", "UserID"}));

    auto filter = curse::FilterOperator("MobilePhoneModel");

    auto group_by = curse::GroupByOperator(
        {"MobilePhoneModel"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});

    auto sort = curse::SortOperator({{.inp_col = "u", .reversed = true}}, 10);

    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT MobilePhone, MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE MobilePhoneModel <> '' GROUP BY
// MobilePhone, MobilePhoneModel ORDER BY u DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q11(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(
        file, SubSchema(kHitsSchema, {"MobilePhone", "MobilePhoneModel", "UserID"}));

    auto filter = curse::FilterOperator("MobilePhoneModel");

    auto group_by = curse::GroupByOperator(
        {"MobilePhone", "MobilePhoneModel"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});

    auto sort = curse::SortOperator({{.inp_col = "u", .reversed = true}}, 10);

    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q12(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));
    auto filter = curse::FilterOperator("SearchPhrase");
    auto group_by = curse::GroupByOperator(
        {"SearchPhrase"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::Count, .inp_col = "SearchPhrase", .out_col = "c"}});
    auto sort = curse::SortOperator({{.inp_col = "c", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT SearchPhrase, COUNT(DISTINCT UserID) AS u FROM hits WHERE SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY u
// DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q13(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "UserID"}));
    auto filter = curse::FilterOperator("SearchPhrase");
    auto group_by = curse::GroupByOperator(
        {"SearchPhrase"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});
    auto sort = curse::SortOperator({{.inp_col = "u", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT SearchEngineID, SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP BY SearchEngineID,
// SearchPhrase ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q14(const std::string& file) {
    auto reader =
        std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"SearchEngineID", "SearchPhrase"}));
    auto filter = curse::FilterOperator("SearchPhrase");
    auto group_by = curse::GroupByOperator(
        {"SearchEngineID", "SearchPhrase"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::Count, .inp_col = "SearchPhrase", .out_col = "c"}});
    auto sort = curse::SortOperator({{.inp_col = "c", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT UserID, COUNT(*) FROM hits GROUP BY UserID ORDER BY COUNT(*) DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q15(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));
    auto group_by = curse::GroupByOperator(
        {"UserID"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::Count, .inp_col = "UserID", .out_col = "cnt"}});
    auto sort = curse::SortOperator({{.inp_col = "cnt", .reversed = true}}, 10);
    return std::move(reader) >= group_by >= sort;
}

// SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase ORDER BY COUNT(*) DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q16(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"UserID", "SearchPhrase"}));
    auto group_by = curse::GroupByOperator(
        {"UserID", "SearchPhrase"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::Count, .inp_col = "UserID", .out_col = "cnt"}});
    auto sort = curse::SortOperator({{.inp_col = "cnt", .reversed = true}}, 10);
    return std::move(reader) >= group_by >= sort;
}

// SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase LIMIT 10;
std::unique_ptr<curse::BatchStream> Q17(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"UserID", "SearchPhrase"}));
    auto group_by = curse::GroupByOperator(
        {"UserID", "SearchPhrase"},
        {curse::GroupByOperator::Params{.tp = curse::AggType::Count, .inp_col = "UserID", .out_col = "cnt"}}, 10);
    return std::move(reader) >= group_by;
}

// SELECT UserID, extract(minute FROM EventTime) AS m, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, m, SearchPhrase
// ORDER BY COUNT(*) DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q18(const std::string&) {
    return nullptr;
}
// SELECT UserID FROM hits WHERE UserID = 435090932899640449;
std::unique_ptr<curse::BatchStream> Q19(const std::string&) {
    return nullptr;
}

// SELECT COUNT(*) FROM hits WHERE URL LIKE '%google%';
std::unique_ptr<curse::BatchStream> Q20(const std::string&) {
    return nullptr;
}
// SELECT SearchPhrase, MIN(URL), COUNT(*) AS c FROM hits WHERE URL LIKE '%google%' AND SearchPhrase <> '' GROUP BY
// SearchPhrase ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q21(const std::string&) {
    return nullptr;
}

// SELECT SearchPhrase, MIN(URL), MIN(Title), COUNT(*) AS c, COUNT(DISTINCT UserID) FROM hits WHERE Title LIKE
// '%Google%' AND URL NOT LIKE '%.google.%' AND SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q22(const std::string&) {
    return nullptr;
}

// SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
std::unique_ptr<curse::BatchStream> Q23(const std::string&) {
    return nullptr;
}

// SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY EventTime LIMIT 10;
std::unique_ptr<curse::BatchStream> Q24(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "EventTime"}));
    auto filter = curse::FilterOperator("SearchPhrase");
    auto sort = curse::SortOperator({{.inp_col = "EventTime"}}, 10);
    return std::move(reader) >= filter >= sort;
}

// SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY SearchPhrase LIMIT 10;
std::unique_ptr<curse::BatchStream> Q25(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));
    auto filter = curse::FilterOperator("SearchPhrase");
    auto sort = curse::SortOperator({{.inp_col = "SearchPhrase"}}, 10);
    return std::move(reader) >= filter >= sort;
}

// SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY EventTime, SearchPhrase LIMIT 10;
std::unique_ptr<curse::BatchStream> Q26(const std::string& file) {
    auto reader = std::make_unique<curse::CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "EventTime"}));
    auto filter = curse::FilterOperator("SearchPhrase");
    auto sort = curse::SortOperator({{.inp_col = "EventTime"}, {.inp_col = "SearchPhrase"}}, 10);
    return std::move(reader) >= filter >= sort;
}

// SELECT CounterID, AVG(length(URL)) AS l, COUNT(*) AS c FROM hits WHERE URL <> '' GROUP BY CounterID HAVING COUNT(*) >
// 100000 ORDER BY l DESC LIMIT 25;
std::unique_ptr<curse::BatchStream> Q27(const std::string&) {
    return nullptr;
}

// SELECT REGEXP_REPLACE(Referer, '^https?://(?:www\.)?([^/]+)/.*$', '\1') AS k, AVG(length(Referer)) AS l, COUNT(*) AS
// c, MIN(Referer) FROM hits WHERE Referer <> '' GROUP BY k HAVING COUNT(*) > 100000 ORDER BY l DESC LIMIT 25;
std::unique_ptr<curse::BatchStream> Q28(const std::string&) {
    return nullptr;
}

// SELECT SUM(ResolutionWidth), SUM(ResolutionWidth + 1), SUM(ResolutionWidth + 2), SUM(ResolutionWidth + 3),
// SUM(ResolutionWidth + 4), SUM(ResolutionWidth + 5), SUM(ResolutionWidth + 6), SUM(ResolutionWidth + 7),
// SUM(ResolutionWidth + 8), SUM(ResolutionWidth + 9), SUM(ResolutionWidth + 10), SUM(ResolutionWidth + 11),
// SUM(ResolutionWidth + 12), SUM(ResolutionWidth + 13), SUM(ResolutionWidth + 14), SUM(ResolutionWidth + 15),
// SUM(ResolutionWidth + 16), SUM(ResolutionWidth + 17), SUM(ResolutionWidth + 18), SUM(ResolutionWidth + 19),
// SUM(ResolutionWidth + 20), SUM(ResolutionWidth + 21), SUM(ResolutionWidth + 22), SUM(ResolutionWidth + 23),
// SUM(ResolutionWidth + 24), SUM(ResolutionWidth + 25), SUM(ResolutionWidth + 26), SUM(ResolutionWidth + 27),
// SUM(ResolutionWidth + 28), SUM(ResolutionWidth + 29), SUM(ResolutionWidth + 30), SUM(ResolutionWidth + 31),
// SUM(ResolutionWidth + 32), SUM(ResolutionWidth + 33), SUM(ResolutionWidth + 34), SUM(ResolutionWidth + 35),
// SUM(ResolutionWidth + 36), SUM(ResolutionWidth + 37), SUM(ResolutionWidth + 38), SUM(ResolutionWidth + 39),
// SUM(ResolutionWidth + 40), SUM(ResolutionWidth + 41), SUM(ResolutionWidth + 42), SUM(ResolutionWidth + 43),
// SUM(ResolutionWidth + 44), SUM(ResolutionWidth + 45), SUM(ResolutionWidth + 46), SUM(ResolutionWidth + 47),
// SUM(ResolutionWidth + 48), SUM(ResolutionWidth + 49), SUM(ResolutionWidth + 50), SUM(ResolutionWidth + 51),
// SUM(ResolutionWidth + 52), SUM(ResolutionWidth + 53), SUM(ResolutionWidth + 54), SUM(ResolutionWidth + 55),
// SUM(ResolutionWidth + 56), SUM(ResolutionWidth + 57), SUM(ResolutionWidth + 58), SUM(ResolutionWidth + 59),
// SUM(ResolutionWidth + 60), SUM(ResolutionWidth + 61), SUM(ResolutionWidth + 62), SUM(ResolutionWidth + 63),
// SUM(ResolutionWidth + 64), SUM(ResolutionWidth + 65), SUM(ResolutionWidth + 66), SUM(ResolutionWidth + 67),
// SUM(ResolutionWidth + 68), SUM(ResolutionWidth + 69), SUM(ResolutionWidth + 70), SUM(ResolutionWidth + 71),
// SUM(ResolutionWidth + 72), SUM(ResolutionWidth + 73), SUM(ResolutionWidth + 74), SUM(ResolutionWidth + 75),
// SUM(ResolutionWidth + 76), SUM(ResolutionWidth + 77), SUM(ResolutionWidth + 78), SUM(ResolutionWidth + 79),
// SUM(ResolutionWidth + 80), SUM(ResolutionWidth + 81), SUM(ResolutionWidth + 82), SUM(ResolutionWidth + 83),
// SUM(ResolutionWidth + 84), SUM(ResolutionWidth + 85), SUM(ResolutionWidth + 86), SUM(ResolutionWidth + 87),
// SUM(ResolutionWidth + 88), SUM(ResolutionWidth + 89) FROM hits;
std::unique_ptr<curse::BatchStream> Q29(const std::string&) {
    return nullptr;
}

// SELECT SearchEngineID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits WHERE SearchPhrase <>
// '' GROUP BY SearchEngineID, ClientIP ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q30(const std::string&) {
    return nullptr;
}

// SELECT WatchID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits WHERE SearchPhrase <> ''
// GROUP BY WatchID, ClientIP ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q31(const std::string&) {
    return nullptr;
}
// SELECT WatchID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits GROUP BY WatchID, ClientIP
// ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q32(const std::string&) {
    return nullptr;
}
// SELECT URL, COUNT(*) AS c FROM hits GROUP BY URL ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q33(const std::string&) {
    return nullptr;
}
// SELECT 1, URL, COUNT(*) AS c FROM hits GROUP BY 1, URL ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q34(const std::string&) {
    return nullptr;
}
// SELECT ClientIP, ClientIP - 1, ClientIP - 2, ClientIP - 3, COUNT(*) AS c FROM hits GROUP BY ClientIP, ClientIP - 1,
// ClientIP - 2, ClientIP - 3 ORDER BY c DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q35(const std::string&) {
    return nullptr;
}
// SELECT URL, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <=
// '2013-07-31' AND DontCountHits = 0 AND IsRefresh = 0 AND URL <> '' GROUP BY URL ORDER BY PageViews DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q36(const std::string&) {
    return nullptr;
}

// SELECT Title, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <=
// '2013-07-31' AND DontCountHits = 0 AND IsRefresh = 0 AND Title <> '' GROUP BY Title ORDER BY PageViews DESC LIMIT 10;
std::unique_ptr<curse::BatchStream> Q37(const std::string&) {
    return nullptr;
}
// SELECT URL, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <=
// '2013-07-31' AND IsRefresh = 0 AND IsLink <> 0 AND IsDownload = 0 GROUP BY URL ORDER BY PageViews DESC LIMIT 10
// OFFSET 1000;
std::unique_ptr<curse::BatchStream> Q38(const std::string&) {
    return nullptr;
}
// SELECT TraficSourceID, SearchEngineID, AdvEngineID, CASE WHEN (SearchEngineID = 0 AND AdvEngineID = 0) THEN Referer
// ELSE '' END AS Src, URL AS Dst, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01'
// AND EventDate <= '2013-07-31' AND IsRefresh = 0 GROUP BY TraficSourceID, SearchEngineID, AdvEngineID, Src, Dst ORDER
// BY PageViews DESC LIMIT 10 OFFSET 1000;
std::unique_ptr<curse::BatchStream> Q39(const std::string&) {
    return nullptr;
}
// SELECT URLHash, EventDate, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND
// EventDate <= '2013-07-31' AND IsRefresh = 0 AND TraficSourceID IN (-1, 6) AND RefererHash = 3594120000172545465 GROUP
// BY URLHash, EventDate ORDER BY PageViews DESC LIMIT 10 OFFSET 100;
std::unique_ptr<curse::BatchStream> Q40(const std::string&) {
    return nullptr;
}
// SELECT WindowClientWidth, WindowClientHeight, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >=
// '2013-07-01' AND EventDate <= '2013-07-31' AND IsRefresh = 0 AND DontCountHits = 0 AND URLHash = 2868770270353813622
// GROUP BY WindowClientWidth, WindowClientHeight ORDER BY PageViews DESC LIMIT 10 OFFSET 10000;
std::unique_ptr<curse::BatchStream> Q41(const std::string&) {
    return nullptr;
}
// SELECT DATE_TRUNC('minute', EventTime) AS M, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >=
// '2013-07-14' AND EventDate <= '2013-07-15' AND IsRefresh = 0 AND DontCountHits = 0 GROUP BY DATE_TRUNC('minute',
// EventTime) ORDER BY DATE_TRUNC('minute', EventTime) LIMIT 10 OFFSET 1000;
std::unique_ptr<curse::BatchStream> Q42(const std::string&) {
    return nullptr;
}

std::unique_ptr<curse::BatchStream> ExecuteQuery(int id, const std::string& input_file) {
    static std::unique_ptr<curse::BatchStream> (*queries[])(const std::string&) = {
        Q0,  Q1,  Q2,  Q3,  Q4,  Q5,  Q6,  Q7,  Q8,  Q9,  Q10, Q11, Q12, Q13, Q14, Q15, Q16, Q17, Q18, Q19, Q20, Q21,
        Q22, Q23, Q24, Q25, Q26, Q27, Q28, Q29, Q30, Q31, Q32, Q33, Q34, Q35, Q36, Q37, Q38, Q39, Q40, Q41, Q42};

    ENSURE_MSG(0 <= id && id < (int)std::size(queries), "invalid id");
    return queries[id](input_file);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << std::format("usage: {} [CURSED FILE] [OUTPUT] [QUERY_NUM] \n", argv[0]) << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    int query_num = std::atoi(argv[3]);

    ENSURE_MSG(0 <= query_num && query_num <= 42, "invalid query num");

    std::unique_ptr<curse::BatchStream> output_stream = ExecuteQuery(query_num, input_file);

    if (output_file != "-") {
        curse::WriteAsCsv(output_file, std::move(output_stream));
    } else {
        curse::WriteAsCsv(std::cout, std::move(output_stream));
    }

    return 0;
}
