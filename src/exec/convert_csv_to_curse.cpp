#include <format>
#include <fstream>
#include <iostream>
#include <memory>

#include "src/core/csv.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"
#include "src/exec/hits_schema.hpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << std::format(
                         "usage: {} [INPUT_FILE] [OUTPUT_FILE]\n"
                         "if INPUT_FILE is \"-\", reads from stdin\n",
                         argv[0])
                  << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    std::unique_ptr<curse::BatchStream> input_stream;
    if (input_file == "-") {
        input_stream = std::make_unique<curse::CsvReader>(std::cin, kHitsSchema);
    } else {
        std::unique_ptr<std::ifstream> fin = std::make_unique<std::ifstream>(input_file);
        input_stream = std::make_unique<curse::CsvReader>(std::move(fin), kHitsSchema);
    }

    WriteAsCurse(output_file, std::move(input_stream));

    return 0;
}
