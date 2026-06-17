#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>

#include "src/core/csv.hpp"
#include "src/core/types.hpp"
#include "src/exec/queries.hpp"
#include "src/util/assert.hpp"

// часть запросов была захадкожена с помощью LLM
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
        std::cerr << std::format("usage: {} <CURSED FILE> <OUTPUT> <QUERY_NUM> \n", argv[0]) << std::endl;
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
