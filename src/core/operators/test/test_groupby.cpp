#include <memory>
#include <sstream>

#include "gtest/gtest.h"
#include "src/core/aggregators.hpp"
#include "src/core/csv.hpp"
#include "src/core/operators/groupby.hpp"
#include "src/core/types.hpp"

using namespace curse;  // NOLINT

std::string SortedLines(const std::string& s) {
    std::vector<std::string> vec;
    vec.emplace_back();
    for (char ch : s) {
        if (ch == '\n') {
            vec.emplace_back();
        } else {
            vec.back() += ch;
        }
    }

    std::sort(vec.begin(), vec.end());
    std::string res;
    for (auto s : vec) {
        res += s;
        res += '\n';
    }

    return res;
}

TEST(Operators_GroupBy, ItWorks) {
    const std::string data =
        "meow,2296\n"
        "meow,1\n"
        "woof,12345\n";

    const Schema schema = Schema({
        Schema::ColumnInfo{.name = "1", .type = TypeId::String},
        Schema::ColumnInfo{.name = "2", .type = TypeId::Int64},
    });

    std::unique_ptr<std::istringstream> sin = std::make_unique<std::istringstream>(data);
    std::unique_ptr<BatchStream> reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    GroupByOperator groupby({"1"}, {
                                       GroupByOperator::Params{.tp = AggType::Sum, .inp_col = "2", .out_col = "_1"},
                                   });

    std::unique_ptr<BatchStream> stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    std::string result = sout.str();

    const std::string expected_result =
        "meow,2297\n"
        "woof,12345\n";

    ASSERT_EQ(SortedLines(result), SortedLines(expected_result));
}

// ниже нейрослоп

TEST(Operators_GroupBy, GroupByOneToFiftyKeys) {
    for (int num_keys = 1; num_keys <= 50; ++num_keys) {
        std::vector<Schema::ColumnInfo> columns;

        for (int i = 0; i < num_keys; ++i) {
            columns.push_back(Schema::ColumnInfo{
                .name = "k" + std::to_string(i),
                .type = TypeId::String,
            });
        }

        columns.push_back(Schema::ColumnInfo{
            .name = "value",
            .type = TypeId::Int64,
        });

        const Schema schema(columns);

        std::ostringstream data;

        // Group A
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < num_keys; ++col) {
                if (col != 0) {
                    data << ",";
                }
                data << "A" << col;
            }
            data << "," << (row + 1) << "\n";
        }

        // Group B
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < num_keys; ++col) {
                if (col != 0) {
                    data << ",";
                }
                data << "B" << col;
            }
            data << "," << 10 << "\n";
        }

        auto sin = std::make_unique<std::istringstream>(data.str());
        auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

        std::vector<std::string> group_cols;
        for (int i = 0; i < num_keys; ++i) {
            group_cols.push_back("k" + std::to_string(i));
        }

        GroupByOperator groupby(group_cols, {
                                                {
                                                    .tp = AggType::Sum,
                                                    .inp_col = "value",
                                                    .out_col = "sum",
                                                },
                                            });

        auto stream = std::move(reader) >= groupby;

        std::ostringstream sout;
        WriteAsCsv(sout, std::move(stream));

        std::ostringstream expected;

        for (int col = 0; col < num_keys; ++col) {
            if (col != 0) {
                expected << ",";
            }
            expected << "A" << col;
        }
        expected << ",6\n";

        for (int col = 0; col < num_keys; ++col) {
            if (col != 0) {
                expected << ",";
            }
            expected << "B" << col;
        }
        expected << ",20\n";

        ASSERT_EQ(SortedLines(sout.str()), SortedLines(expected.str())) << "Failed for num_keys=" << num_keys;
    }
}

TEST(Operators_GroupBy, AverageAggregation) {
    const std::string data =
        "a,10\n"
        "a,20\n"
        "a,30\n"
        "b,5\n"
        "b,15\n";

    const Schema schema({
        {"grp", TypeId::String},
        {"val", TypeId::Int64},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 2);

    GroupByOperator groupby({"grp"}, {
                                         {.tp = AggType::Average, .inp_col = "val", .out_col = "avg"},
                                     });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    ASSERT_EQ(SortedLines(sout.str()), SortedLines("a,20\n"
                                                   "b,10\n"));
}

TEST(Operators_GroupBy, MinAggregation) {
    const std::string data =
        "a,20\n"
        "a,5\n"
        "a,10\n"
        "b,-1\n"
        "b,7\n";

    const Schema schema({
        {"grp", TypeId::String},
        {"val", TypeId::Int64},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    GroupByOperator groupby({"grp"}, {
                                         {.tp = AggType::Min, .inp_col = "val", .out_col = "min"},
                                     });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    ASSERT_EQ(SortedLines(sout.str()), SortedLines("a,5\n"
                                                   "b,-1\n"));
}

TEST(Operators_GroupBy, MaxAggregation) {
    const std::string data =
        "a,20\n"
        "a,5\n"
        "a,10\n"
        "b,-1\n"
        "b,7\n";

    const Schema schema({
        {"grp", TypeId::String},
        {"val", TypeId::Int64},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    GroupByOperator groupby({"grp"}, {
                                         {.tp = AggType::Max, .inp_col = "val", .out_col = "max"},
                                     });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    ASSERT_EQ(SortedLines(sout.str()), SortedLines("a,20\n"
                                                   "b,7\n"));
}

TEST(Operators_GroupBy, CountAggregation) {
    const std::string data =
        "cat\n"
        "cat\n"
        "dog\n"
        "cat\n";

    const Schema schema({
        {"animal", TypeId::String},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    GroupByOperator groupby({"animal"}, {
                                            {.tp = AggType::Count, .inp_col = "animal", .out_col = "cnt"},
                                        });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    ASSERT_EQ(SortedLines(sout.str()), SortedLines("cat,3\n"
                                                   "dog,1\n"));
}

TEST(Operators_GroupBy, CountDistinctAggregation) {
    const std::string data =
        "a,x\n"
        "a,x\n"
        "a,y\n"
        "a,z\n"
        "b,q\n"
        "b,q\n";

    const Schema schema({
        {"grp", TypeId::String},
        {"val", TypeId::String},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 2);

    GroupByOperator groupby({"grp"}, {
                                         {.tp = AggType::CountDistinct, .inp_col = "val", .out_col = "distinct_cnt"},
                                     });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    ASSERT_EQ(SortedLines(sout.str()), SortedLines("a,3\n"
                                                   "b,1\n"));
}

TEST(Operators_GroupBy, MultipleAggregationsTogether) {
    const std::string data =
        "a,10\n"
        "a,20\n"
        "a,30\n"
        "b,5\n"
        "b,15\n";

    const Schema schema({
        {"grp", TypeId::String},
        {"val", TypeId::Int64},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    GroupByOperator groupby({"grp"}, {
                                         {.tp = AggType::Count, .inp_col = "val", .out_col = "cnt"},
                                         {.tp = AggType::Sum, .inp_col = "val", .out_col = "sum"},
                                         {.tp = AggType::Average, .inp_col = "val", .out_col = "avg"},
                                         {.tp = AggType::Min, .inp_col = "val", .out_col = "min"},
                                         {.tp = AggType::Max, .inp_col = "val", .out_col = "max"},
                                     });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    ASSERT_EQ(SortedLines(sout.str()), SortedLines("a,3,60,20,10,30\n"
                                                   "b,2,20,10,5,15\n"));
}

TEST(Operators_GroupBy, CountDistinctPerCompositeKey) {
    const std::string data =
        "east,a,x\n"
        "east,a,x\n"
        "east,a,y\n"
        "east,b,x\n"
        "west,a,x\n"
        "west,a,x\n";

    const Schema schema({
        {"region", TypeId::String},
        {"grp", TypeId::String},
        {"val", TypeId::String},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 2);

    GroupByOperator groupby({"region", "grp"}, {
                                                   {.tp = AggType::CountDistinct, .inp_col = "val", .out_col = "cnt"},
                                               });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    ASSERT_EQ(SortedLines(sout.str()), SortedLines("east,a,2\n"
                                                   "east,b,1\n"
                                                   "west,a,1\n"));
}

TEST(Operators_GroupBy, FiveGroupColumnsDifferentTypes) {
    const std::string data =
        "alice,10,1.5,1,1,100\n"
        "alice,10,1.5,1,1,200\n"
        "alice,10,1.5,1,1,300\n"
        "bob,20,2.5,2,0,10\n"
        "bob,20,2.5,2,0,20\n";

    const Schema schema({
        {"name", TypeId::String},
        {"i32", TypeId::Int32},
        {"f64", TypeId::Float64},
        {"i64", TypeId::Int64},
        {"flag", TypeId::Int8},
        {"value", TypeId::Int64},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    GroupByOperator groupby({"name", "i32", "f64", "i64", "flag"},
                            {
                                {.tp = AggType::Count, .inp_col = "value", .out_col = "cnt"},
                                {.tp = AggType::Sum, .inp_col = "value", .out_col = "sum"},
                                {.tp = AggType::Average, .inp_col = "value", .out_col = "avg"},
                                {.tp = AggType::Min, .inp_col = "value", .out_col = "min"},
                                {.tp = AggType::Max, .inp_col = "value", .out_col = "max"},
                            });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    const std::string expected =
        "alice,10,1.5,1,1,3,600,200,100,300\n"
        "bob,20,2.5,2,0,2,30,15,10,20\n";

    ASSERT_EQ(SortedLines(sout.str()), SortedLines(expected));
}

TEST(Operators_GroupBy, ManyAggregatorsDifferentInputTypes) {
    const std::string data =
        "alice,10,1.5,1000,1,1,100,10000,foo\n"
        "alice,10,1.5,1000,1,2,200,20000,foo\n"
        "alice,10,1.5,1000,1,3,300,30000,bar\n"
        "bob,20,2.5,2000,0,4,400,40000,baz\n"
        "bob,20,2.5,2000,0,5,500,50000,baz\n";

    const Schema schema({
        {"name", TypeId::String},  // group key
        {"i32", TypeId::Int32},    // group key
        {"f64", TypeId::Float64},  // group key
        {"i64", TypeId::Int64},    // group key
        {"flag", TypeId::Int8},    // group key

        {"v8", TypeId::Int8},
        {"v32", TypeId::Int32},
        {"v64", TypeId::Int64},
        {"tag", TypeId::String},
    });

    auto sin = std::make_unique<std::istringstream>(data);
    auto reader = std::make_unique<CsvReader>(std::move(sin), schema, 1);

    GroupByOperator groupby({"name", "i32", "f64", "i64", "flag"},
                            {
                                {.tp = AggType::Count, .inp_col = "v8", .out_col = "cnt"},

                                {.tp = AggType::Sum, .inp_col = "v8", .out_col = "sum_v8"},
                                {.tp = AggType::Min, .inp_col = "v8", .out_col = "min_v8"},
                                {.tp = AggType::Max, .inp_col = "v8", .out_col = "max_v8"},

                                {.tp = AggType::Sum, .inp_col = "v32", .out_col = "sum_v32"},
                                {.tp = AggType::Average, .inp_col = "v32", .out_col = "avg_v32"},

                                {.tp = AggType::Sum, .inp_col = "v64", .out_col = "sum_v64"},
                                {.tp = AggType::Average, .inp_col = "v64", .out_col = "avg_v64"},
                                {.tp = AggType::Min, .inp_col = "v64", .out_col = "min_v64"},
                                {.tp = AggType::Max, .inp_col = "v64", .out_col = "max_v64"},

                                {.tp = AggType::CountDistinct, .inp_col = "tag", .out_col = "distinct_tags"},
                            });

    auto stream = std::move(reader) >= groupby;

    std::ostringstream sout;
    WriteAsCsv(sout, std::move(stream));

    const std::string expected =
        // alice:
        // count=3
        // v8: 1+2+3=6 min=1 max=3
        // v32: 100+200+300=600 avg=200
        // v64: 10000+20000+30000=60000 avg=20000 min=10000 max=30000
        // distinct(foo,bar)=2
        "alice,10,1.5,1000,1,3,6,1,3,600,200,60000,20000,10000,30000,2\n"

        // bob:
        // count=2
        // v8: 4+5=9 min=4 max=5
        // v32: 400+500=900 avg=450
        // v64: 40000+50000=90000 avg=45000 min=40000 max=50000
        // distinct(baz)=1
        "bob,20,2.5,2000,0,2,9,4,5,900,450,90000,45000,40000,50000,1\n";

    ASSERT_EQ(SortedLines(sout.str()), SortedLines(expected));
}
