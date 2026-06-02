#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>

#include "src/core/assert.hpp"
#include "src/core/convert.hpp"
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

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

std::unique_ptr<BatchStream> Q0(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    AggregationOperator count({AggregationOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "1"}});

    return std::move(reader) >= count;
}

std::unique_ptr<BatchStream> Q1(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID"}));

    FilterOperator filt("AdvEngineID");

    AggregationOperator count(
        {AggregationOperator::Params{.tp = AggType::Count, .inp_col = "AdvEngineID", .out_col = "1"}});

    return std::move(reader) >= filt >= count;
}

std::unique_ptr<BatchStream> Q2(const std::string& file) {
    std::unique_ptr<BatchStream> reader =
        std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID", "ResolutionWidth"}));

    AggregationOperator aggr({
        AggregationOperator::Params{.tp = AggType::Sum, .inp_col = "AdvEngineID", .out_col = "1"},
        AggregationOperator::Params{.tp = AggType::Count, .inp_col = "AdvEngineID", .out_col = "2"},
        AggregationOperator::Params{.tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "3"},
    });

    return std::move(reader) >= aggr;
}

std::unique_ptr<BatchStream> Q3(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    AggregationOperator avg({
        AggregationOperator::Params{.tp = AggType::Average, .inp_col = "UserID", .out_col = "1"},
    });

    return std::move(reader) >= avg;
}

std::unique_ptr<BatchStream> Q4(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    AggregationOperator count_distinct({
        AggregationOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "1"},
    });

    return std::move(reader) >= count_distinct;
}

std::unique_ptr<BatchStream> Q5(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));

    AggregationOperator count_distinct({
        AggregationOperator::Params{.tp = AggType::CountDistinct, .inp_col = "SearchPhrase", .out_col = "1"},
    });

    return std::move(reader) >= count_distinct;
}

std::unique_ptr<BatchStream> Q6(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"EventDate"}));

    AggregationOperator min_max({
        AggregationOperator::Params{.tp = AggType::Min, .inp_col = "EventDate", .out_col = "1"},
        AggregationOperator::Params{.tp = AggType::Max, .inp_col = "EventDate", .out_col = "2"},
    });

    return std::move(reader) >= min_max;
}

// SELECT AdvEngineID, COUNT(*) FROM hits WHERE AdvEngineID <> 0 GROUP BY AdvEngineID ORDER BY COUNT(*) DESC;
std::unique_ptr<BatchStream> Q7(const std::string& file) {
    std::unique_ptr<BatchStream> reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"AdvEngineID"}));

    FilterOperator filt("AdvEngineID");

    GroupByOperator group_by(
        {"AdvEngineID"}, {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "AdvEngineID", .out_col = "cnt"}});

    SortOperator sort({SortOperator::Params{.inp_col = "cnt", .reversed = true}});

    return std::move(reader) >= filt >= group_by >= sort;
}

// SELECT RegionID, COUNT(DISTINCT UserID) AS u FROM hits GROUP BY RegionID ORDER BY u DESC LIMIT 10;
std::unique_ptr<BatchStream> Q8(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"RegionID", "UserID"}));

    auto group_by = GroupByOperator(
        {"RegionID"}, {GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});

    auto sort = SortOperator({{.inp_col = "u", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

// SELECT RegionID, SUM(AdvEngineID), COUNT(*) AS c, AVG(ResolutionWidth), COUNT(DISTINCT UserID) FROM hits GROUP BY
// RegionID ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q9(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"RegionID", "AdvEngineID", "ResolutionWidth", "UserID"}));

    auto group_by = GroupByOperator(
        {"RegionID"},
        {
            GroupByOperator::Params{.tp = AggType::Sum, .inp_col = "AdvEngineID", .out_col = "sum_AdvEngineID"},
            GroupByOperator::Params{.tp = AggType::Count, .inp_col = "RegionID", .out_col = "c"},
            GroupByOperator::Params{
                .tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "avg_ResolutionWidth"},
            GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "distinct_UserID"},
        });

    auto sort = SortOperator({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

// SELECT MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE MobilePhoneModel <> '' GROUP BY MobilePhoneModel
// ORDER BY u DESC LIMIT 10;
std::unique_ptr<BatchStream> Q10(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"MobilePhoneModel", "UserID"}));

    auto filter = FilterOperator("MobilePhoneModel");

    auto group_by =
        GroupByOperator({"MobilePhoneModel"},
                        {GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});

    auto sort = SortOperator({{.inp_col = "u", .reversed = true}}, 10);

    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT MobilePhone, MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits WHERE MobilePhoneModel <> '' GROUP BY
// MobilePhone, MobilePhoneModel ORDER BY u DESC LIMIT 10;
std::unique_ptr<BatchStream> Q11(const std::string& file) {
    auto reader =
        std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"MobilePhone", "MobilePhoneModel", "UserID"}));

    auto filter = FilterOperator("MobilePhoneModel");

    auto group_by =
        GroupByOperator({"MobilePhone", "MobilePhoneModel"},
                        {GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});

    auto sort = SortOperator({{.inp_col = "u", .reversed = true}}, 10);

    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q12(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));
    auto filter = FilterOperator("SearchPhrase");
    auto group_by = GroupByOperator(
        {"SearchPhrase"}, {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "SearchPhrase", .out_col = "c"}});
    auto sort = SortOperator({{.inp_col = "c", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT SearchPhrase, COUNT(DISTINCT UserID) AS u FROM hits WHERE SearchPhrase <> '' GROUP BY SearchPhrase ORDER BY u
// DESC LIMIT 10;
std::unique_ptr<BatchStream> Q13(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "UserID"}));
    auto filter = FilterOperator("SearchPhrase");
    auto group_by = GroupByOperator(
        {"SearchPhrase"}, {GroupByOperator::Params{.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"}});
    auto sort = SortOperator({{.inp_col = "u", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT SearchEngineID, SearchPhrase, COUNT(*) AS c FROM hits WHERE SearchPhrase <> '' GROUP BY SearchEngineID,
// SearchPhrase ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q14(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchEngineID", "SearchPhrase"}));
    auto filter = FilterOperator("SearchPhrase");
    auto group_by =
        GroupByOperator({"SearchEngineID", "SearchPhrase"},
                        {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "SearchPhrase", .out_col = "c"}});
    auto sort = SortOperator({{.inp_col = "c", .reversed = true}}, 10);
    return std::move(reader) >= filter >= group_by >= sort;
}

// SELECT UserID, COUNT(*) FROM hits GROUP BY UserID ORDER BY COUNT(*) DESC LIMIT 10;
std::unique_ptr<BatchStream> Q15(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));
    auto group_by = GroupByOperator(
        {"UserID"}, {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "cnt"}});
    auto sort = SortOperator({{.inp_col = "cnt", .reversed = true}}, 10);
    return std::move(reader) >= group_by >= sort;
}

// SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase ORDER BY COUNT(*) DESC LIMIT 10;
std::unique_ptr<BatchStream> Q16(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID", "SearchPhrase"}));
    auto group_by =
        GroupByOperator({"UserID", "SearchPhrase"},
                        {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "cnt"}});
    auto sort = SortOperator({{.inp_col = "cnt", .reversed = true}}, 10);
    return std::move(reader) >= group_by >= sort;
}

// SELECT UserID, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, SearchPhrase LIMIT 10;
std::unique_ptr<BatchStream> Q17(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID", "SearchPhrase"}));
    auto group_by =
        GroupByOperator({"UserID", "SearchPhrase"},
                        {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "cnt"}}, 10);
    return std::move(reader) >= group_by;
}

// SELECT UserID, extract(minute FROM EventTime) AS m, SearchPhrase, COUNT(*) FROM hits GROUP BY UserID, m, SearchPhrase
// ORDER BY COUNT(*) DESC LIMIT 10;
std::unique_ptr<BatchStream> Q18(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID", "EventTime", "SearchPhrase"}));

    // Add a column for the extracted minute
    auto minute_op = ColumnOperation(Transform::ExtractMinute(), "EventTime", "m");

    auto transform = MakeColumnTransformOperator({minute_op});

    auto group_by =
        GroupByOperator({"UserID", "m", "SearchPhrase"},
                        {GroupByOperator::Params{.tp = AggType::Count, .inp_col = "UserID", .out_col = "cnt"}});

    auto sort = SortOperator({{.inp_col = "cnt", .reversed = true}}, 10);
    return std::move(reader) >= transform >= group_by >= sort;
}

// SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY EventTime LIMIT 10;
std::unique_ptr<BatchStream> Q24(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "EventTime"}));
    auto filter = FilterOperator("SearchPhrase");
    auto sort = SortOperator({{.inp_col = "EventTime"}}, 10);
    return std::move(reader) >= filter >= sort;
}

// SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY SearchPhrase LIMIT 10;
std::unique_ptr<BatchStream> Q25(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase"}));
    auto filter = FilterOperator("SearchPhrase");
    auto sort = SortOperator({{.inp_col = "SearchPhrase"}}, 10);
    return std::move(reader) >= filter >= sort;
}

// SELECT SearchPhrase FROM hits WHERE SearchPhrase <> '' ORDER BY EventTime, SearchPhrase LIMIT 10;
std::unique_ptr<BatchStream> Q26(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "EventTime"}));
    auto filter = FilterOperator("SearchPhrase");
    auto sort = SortOperator({{.inp_col = "EventTime"}, {.inp_col = "SearchPhrase"}}, 10);
    return std::move(reader) >= filter >= sort;
}
// ####################################

std::unique_ptr<BatchStream> Q19(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"UserID"}));

    auto cmp = MakeColumnTransformOperator({ColumnOperation(
        Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{435090932899640449LL})),
        "UserID", "match")});

    FilterOperator filter("match");

    return std::move(reader) >= cmp >= filter >= MakeDropOperator({"match"});
}

std::unique_ptr<BatchStream> Q20(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"URL"}));

    auto trs = MakeColumnTransformOperator({ColumnOperation(Transform::RegexpSearch("google"), "URL", "match")});

    FilterOperator filter("match");

    AggregationOperator count({{.tp = AggType::Count, .inp_col = "URL", .out_col = "1"}});

    return std::move(reader) >= trs >= filter >= count;
}

std::unique_ptr<BatchStream> Q21(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"SearchPhrase", "URL"}));

    auto trs = MakeColumnTransformOperator({ColumnOperation(Transform::RegexpSearch("google"), "URL", "google")});

    FilterOperator google("google");
    FilterOperator phrase("SearchPhrase");

    GroupByOperator group_by({"SearchPhrase"}, {
                                                   {.tp = AggType::Min, .inp_col = "URL", .out_col = "url"},
                                                   {.tp = AggType::Count, .inp_col = "URL", .out_col = "c"},
                                               });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= trs >= google >= phrase >= group_by >= sort;
}

std::unique_ptr<BatchStream> Q22(const std::string& file) {
    auto reader =
        std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"Title", "URL", "SearchPhrase", "UserID"}));

    auto trs =
        MakeColumnTransformOperator({ColumnOperation(Transform::RegexpSearch("Google"), "Title", "title_match"),

                                     ColumnOperation(Transform::RegexpSearch("\\.google\\."), "URL", "url_google"),

                                     ColumnOperation(Transform::LogicalNot(), "url_google", "url_ok"),

                                     ColumnOperation::LogicalAnd("title_match", "url_ok", "keep")});

    FilterOperator keep("keep");
    FilterOperator phrase("SearchPhrase");

    GroupByOperator group_by({"SearchPhrase"}, {
                                                   {.tp = AggType::Min, .inp_col = "URL", .out_col = "url"},
                                                   {.tp = AggType::Min, .inp_col = "Title", .out_col = "title"},
                                                   {.tp = AggType::Count, .inp_col = "UserID", .out_col = "c"},
                                                   {.tp = AggType::CountDistinct, .inp_col = "UserID", .out_col = "u"},
                                               });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= trs >= keep >= phrase >= group_by >= sort;
}

// SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10;
std::unique_ptr<BatchStream> Q23(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, kHitsSchema);

    auto trs = MakeColumnTransformOperator({ColumnOperation(Transform::RegexpSearch("google"), "URL", "match")});

    FilterOperator filter("match");

    SortOperator sort({{.inp_col = "EventTime"}}, 10);

    return std::move(reader) >= trs >= filter >= sort;
}

// SELECT CounterID, AVG(length(URL)) AS l, COUNT(*) AS c FROM hits WHERE URL <> '' GROUP BY CounterID HAVING COUNT(*) >
// 100000 ORDER BY l DESC LIMIT 25;
std::unique_ptr<BatchStream> Q27(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"CounterID", "URL"}));

    FilterOperator non_empty("URL");

    auto trs = MakeColumnTransformOperator({ColumnOperation(Transform::Strlen(), "URL", "len")});

    GroupByOperator group_by({"CounterID"}, {
                                                {.tp = AggType::Average, .inp_col = "len", .out_col = "l"},
                                                {.tp = AggType::Count, .inp_col = "CounterID", .out_col = "c"},
                                            });

    auto having = MakeColumnTransformOperator({ColumnOperation(
        Transform::Compare(Transform::ComparisonType::GreaterThan, Value(ValueT<TypeId::Int64>{100000})), "c",
        "keep")});

    FilterOperator keep("keep");

    SortOperator sort({{.inp_col = "l", .reversed = true}}, 25);

    auto drop = MakeDropOperator({"keep"});

    return std::move(reader) >= non_empty >= trs >= group_by >= having >= keep >= drop >= sort;
}

std::unique_ptr<BatchStream> Q28(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"Referer"}));

    FilterOperator non_empty("Referer");

    auto trs = MakeColumnTransformOperator(
        {ColumnOperation(Transform::RegexpReplace("^https?://(?:www\\.)?([^/]+)/.*$", "$1"), "Referer", "k"),

         ColumnOperation(Transform::Strlen(), "Referer", "len")});

    GroupByOperator group_by({"k"}, {
                                        {.tp = AggType::Average, .inp_col = "len", .out_col = "l"},
                                        {.tp = AggType::Count, .inp_col = "Referer", .out_col = "c"},
                                        {.tp = AggType::Min, .inp_col = "Referer", .out_col = "r"},
                                    });

    auto having = MakeColumnTransformOperator({ColumnOperation(
        Transform::Compare(Transform::ComparisonType::GreaterThan, Value(ValueT<TypeId::Int64>{100000})), "c",
        "keep")});

    FilterOperator keep("keep");

    SortOperator sort({{.inp_col = "l", .reversed = true}}, 25);

    return std::move(reader) >= non_empty >= trs >= group_by >= having >= keep >= sort;
}

std::unique_ptr<BatchStream> Q30(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"SearchEngineID", "ClientIP", "IsRefresh", "ResolutionWidth", "SearchPhrase"}));

    FilterOperator phrase("SearchPhrase");

    GroupByOperator group_by({"SearchEngineID", "ClientIP"},
                             {
                                 {.tp = AggType::Count, .inp_col = "ClientIP", .out_col = "c"},
                                 {.tp = AggType::Sum, .inp_col = "IsRefresh", .out_col = "r"},
                                 {.tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "w"},
                             });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= phrase >= group_by >= sort;
}

// SELECT WatchID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits WHERE SearchPhrase <> ''
// GROUP BY WatchID, ClientIP ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q31(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"WatchID", "ClientIP", "IsRefresh", "ResolutionWidth", "SearchPhrase"}));

    FilterOperator phrase("SearchPhrase");

    GroupByOperator group_by({"WatchID", "ClientIP"},
                             {
                                 {.tp = AggType::Count, .inp_col = "WatchID", .out_col = "c"},
                                 {.tp = AggType::Sum, .inp_col = "IsRefresh", .out_col = "r"},
                                 {.tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "w"},
                             });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= phrase >= group_by >= sort;
}

// SELECT WatchID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth) FROM hits GROUP BY WatchID, ClientIP
// ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q32(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"WatchID", "ClientIP", "IsRefresh", "ResolutionWidth"}));

    GroupByOperator group_by({"WatchID", "ClientIP"},
                             {
                                 {.tp = AggType::Count, .inp_col = "WatchID", .out_col = "c"},
                                 {.tp = AggType::Sum, .inp_col = "IsRefresh", .out_col = "r"},
                                 {.tp = AggType::Average, .inp_col = "ResolutionWidth", .out_col = "w"},
                             });

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

std::unique_ptr<BatchStream> Q33(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"URL"}));

    GroupByOperator group_by({"URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "c"}});

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= group_by >= sort;
}

std::unique_ptr<BatchStream> Q34(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {"URL"}));

    auto trs = MakeColumnTransformOperator(
        {ColumnOperation(Transform::Constant(Value(ValueT<TypeId::Int32>{1})), "URL", "one")});

    GroupByOperator group_by({"one", "URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "c"}});

    SortOperator sort({{.inp_col = "c", .reversed = true}}, 10);

    return std::move(reader) >= trs >= group_by >= sort;
}

std::unique_ptr<BatchStream> Q36(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"URL", "CounterID", "EventDate", "DontCountHits", "IsRefresh"}));

    auto d1 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-31")});

    auto filt = MakeColumnTransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "DontCountHits", "d"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),

        ColumnOperation::LogicalAnd("c", "ge", "k1"),
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
        ColumnOperation::LogicalAnd("k2", "d", "k3"),
        ColumnOperation::LogicalAnd("k3", "r", "keep"),
    });

    FilterOperator keep("keep");
    FilterOperator url("URL");

    GroupByOperator group_by({"URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "PageViews"}});

    SortOperator sort({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= filt >= keep >= url >= group_by >= sort;
}

std::unique_ptr<BatchStream> Q37(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"Title", "CounterID", "EventDate", "DontCountHits", "IsRefresh"}));

    auto d1 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-31")});

    auto filt = MakeColumnTransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "DontCountHits", "d"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),

        ColumnOperation::LogicalAnd("c", "ge", "k1"),
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
        ColumnOperation::LogicalAnd("k2", "d", "k3"),
        ColumnOperation::LogicalAnd("k3", "r", "keep"),
    });

    FilterOperator keep("keep");
    FilterOperator title("Title");

    GroupByOperator group_by({"Title"}, {{.tp = AggType::Count, .inp_col = "Title", .out_col = "PageViews"}});

    SortOperator sort({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= filt >= keep >= title >= group_by >= sort;
}

std::unique_ptr<BatchStream> Q38(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"URL", "CounterID", "EventDate", "IsRefresh", "IsLink", "IsDownload"}));

    auto d1 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-01")});
    auto d2 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-31")});

    auto filt = MakeColumnTransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::NotEqual, Value(ValueT<TypeId::Int16>{0})),
                        "IsLink", "l"),
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsDownload", "d"),

        ColumnOperation::LogicalAnd("c", "ge", "k1"),
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
        ColumnOperation::LogicalAnd("k2", "r", "k3"),
        ColumnOperation::LogicalAnd("k3", "l", "k4"),
        ColumnOperation::LogicalAnd("k4", "d", "keep"),
    });

    GroupByOperator group_by({"URL"}, {{.tp = AggType::Count, .inp_col = "URL", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}}, 1000 + 10);

    SkipOperator skip(1000);

    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= filt >= FilterOperator("keep") >= group_by >= sort1 >= skip >= sort2;
}

// SELECT TraficSourceID, SearchEngineID, AdvEngineID, CASE WHEN (SearchEngineID = 0 AND AdvEngineID = 0) THEN Referer
// ELSE '' END AS Src, URL AS Dst, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >= '2013-07-01'
// AND EventDate <= '2013-07-31' AND IsRefresh = 0 GROUP BY TraficSourceID, SearchEngineID, AdvEngineID, Src, Dst ORDER
// BY PageViews DESC LIMIT 10 OFFSET 1000;
std::unique_ptr<BatchStream> Q39(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"TraficSourceID", "SearchEngineID", "AdvEngineID", "Referer", "URL", "CounterID",
                                      "EventDate", "IsRefresh"}));

    auto d1 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-31")});

    auto trs = MakeColumnTransformOperator(
        {ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                         "CounterID", "c"),

         ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),

         ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),

         ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                         "IsRefresh", "r"),

         ColumnOperation::LogicalAnd("c", "ge", "k1"), ColumnOperation::LogicalAnd("k1", "le", "k2"),
         ColumnOperation::LogicalAnd("k2", "r", "keep"),

         ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                         "SearchEngineID", "se0"),

         ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                         "AdvEngineID", "ae0"),

         ColumnOperation::LogicalAnd("se0", "ae0", "src_cond"),

         ColumnOperation(Transform::Constant(Value(ValueT<TypeId::String>{""})), "Referer", "empty"),

         ColumnOperation::Select("src_cond", "Referer", "empty", "Src")});

    GroupByOperator group_by({"TraficSourceID", "SearchEngineID", "AdvEngineID", "Src", "URL"},
                             {{.tp = AggType::Count, .inp_col = "URL", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}}, 1000 + 10);

    SkipOperator skip(1000);

    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= trs >= FilterOperator("keep") >= group_by >= sort1 >= skip >= sort2;
}
std::unique_ptr<BatchStream> Q40(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file,
        SubSchema(kHitsSchema, {"URLHash", "EventDate", "CounterID", "IsRefresh", "TraficSourceID", "RefererHash"}));

    auto d1 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-31")});

    auto trs = MakeColumnTransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{-1})),
                        "TraficSourceID", "ts_neg1"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{6})),
                        "TraficSourceID", "ts_6"),

        ColumnOperation(
            Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{3594120000172545465LL})),
            "RefererHash", "rh"),

        ColumnOperation::LogicalOr("ts_neg1", "ts_6", "ts_ok"),

        ColumnOperation::LogicalAnd("c", "ge", "k1"),
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
        ColumnOperation::LogicalAnd("k2", "r", "k3"),
        ColumnOperation::LogicalAnd("k3", "rh", "k4"),
        ColumnOperation::LogicalAnd("k4", "ts_ok", "keep"),
    });

    GroupByOperator({"URLHash", "EventDate"}, {{.tp = AggType::Count, .inp_col = "URLHash", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "PageViews", .reversed = true}});

    SkipOperator skip(100);

    SortOperator sort2({{.inp_col = "PageViews", .reversed = true}}, 10);

    return std::move(reader) >= trs >= FilterOperator("keep") >=
           GroupByOperator({"URLHash", "EventDate"},
                           {{.tp = AggType::Count, .inp_col = "URLHash", .out_col = "PageViews"}}) >= sort1 >= skip >=
           sort2;
}

// SELECT WindowClientWidth, WindowClientHeight, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >=
// '2013-07-01' AND EventDate <= '2013-07-31' AND IsRefresh = 0 AND DontCountHits = 0 AND URLHash = 2868770270353813622
// GROUP BY WindowClientWidth, WindowClientHeight ORDER BY PageViews DESC LIMIT 10 OFFSET 10000;
std::unique_ptr<BatchStream> Q41(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(file, SubSchema(kHitsSchema, {
                                                                                 "WindowClientWidth",
                                                                                 "WindowClientHeight",
                                                                                 "CounterID",
                                                                                 "EventDate",
                                                                                 "IsRefresh",
                                                                                 "DontCountHits",
                                                                                 "URLHash",
                                                                             }));

    auto d1 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-01")});

    auto d2 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-31")});

    auto cmp = MakeColumnTransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "DontCountHits", "d"),

        ColumnOperation(
            Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int64>{2868770270353813622LL})),
            "URLHash", "u"),
    });

    auto and1 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("c", "ge", "k1"),
    });

    auto and2 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
    });

    auto and3 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("k2", "r", "k3"),
    });

    auto and4 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("k3", "d", "k4"),
    });

    auto and5 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("k4", "u", "keep"),
    });

    GroupByOperator group_by({"WindowClientWidth", "WindowClientHeight"}, {
                                                                              {
                                                                                  .tp = AggType::Count,
                                                                                  .inp_col = "WindowClientWidth",
                                                                                  .out_col = "PageViews",
                                                                              },
                                                                          });

    size_t to_skip = 10000;
    size_t limit = 10;

    SortOperator sort1(
        {
            {.inp_col = "PageViews", .reversed = true},
        },
        limit + to_skip);

    SkipOperator skip(to_skip);

    return std::move(reader) >= cmp >= and1 >= and2 >= and3 >= and4 >= and5 >= FilterOperator("keep") >= group_by >=
           sort1 >= skip;
}

// SELECT DATE_TRUNC('minute', EventTime) AS M, COUNT(*) AS PageViews FROM hits WHERE CounterID = 62 AND EventDate >=
// '2013-07-14' AND EventDate <= '2013-07-15' AND IsRefresh = 0 AND DontCountHits = 0 GROUP BY DATE_TRUNC('minute',
// EventTime) ORDER BY DATE_TRUNC('minute', EventTime) LIMIT 10 OFFSET 1000;
std::unique_ptr<BatchStream> Q42(const std::string& file) {
    auto reader = std::make_unique<CurseReader>(
        file, SubSchema(kHitsSchema, {"EventTime", "EventDate", "CounterID", "IsRefresh", "DontCountHits"}));

    auto d1 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-14")});

    auto d2 = Value(ValueT<TypeId::Date>{Convert<std::chrono::year_month_day>::FromString("2013-07-15")});

    auto cmp = MakeColumnTransformOperator({
        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int32>{62})),
                        "CounterID", "c"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::GreaterThanOrEqual, d1), "EventDate", "ge"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::LessThanOrEqual, d2), "EventDate", "le"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "IsRefresh", "r"),

        ColumnOperation(Transform::Compare(Transform::ComparisonType::Equal, Value(ValueT<TypeId::Int16>{0})),
                        "DontCountHits", "d"),
    });

    auto and1 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("c", "ge", "k1"),
    });

    auto and2 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("k1", "le", "k2"),
    });

    auto and3 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("k2", "r", "k3"),
    });

    auto and4 = MakeColumnTransformOperator({
        ColumnOperation::LogicalAnd("k3", "d", "keep"),
    });

    auto trunc = MakeColumnTransformOperator({ColumnOperation(Transform::TruncateToMinutes(), "EventTime", "M")});

    FilterOperator keep("keep");

    GroupByOperator group_by({"M"}, {{.tp = AggType::Count, .inp_col = "EventTime", .out_col = "PageViews"}});

    SortOperator sort1({{.inp_col = "M"}});

    SkipOperator skip(1000);

    SortOperator sort2({{.inp_col = "M"}}, 10);

    return std::move(reader) >= cmp >= and1 >= and2 >= and3 >= and4 >= keep >= trunc >= group_by >= sort1 >= skip >=
           sort2;
}

// ##################################

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
std::unique_ptr<BatchStream> Q29(const std::string&) {
    return nullptr;
}

// SELECT ClientIP, ClientIP - 1, ClientIP - 2, ClientIP - 3, COUNT(*) AS c FROM hits GROUP BY ClientIP, ClientIP - 1,
// ClientIP - 2, ClientIP - 3 ORDER BY c DESC LIMIT 10;
std::unique_ptr<BatchStream> Q35(const std::string&) {
    return nullptr;
}

}  // namespace Q

std::unique_ptr<curse::BatchStream> ExecuteQuery(int id, const std::string& input_file) {
    using namespace Q;  // NOLINT

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
