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

std::unique_ptr<curse::BatchStream> Q7(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q8(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q9(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q10(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q11(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q12(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q13(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q14(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q15(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q16(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q17(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q18(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q19(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q20(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q21(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q22(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q23(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q24(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q25(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q26(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q27(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q28(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q29(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q30(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q31(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q32(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q33(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q34(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q35(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q36(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q37(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q38(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q39(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q40(const std::string&) {
    return nullptr;
}
std::unique_ptr<curse::BatchStream> Q41(const std::string&) {
    return nullptr;
}
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
