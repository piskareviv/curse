#include <format>
#include <iostream>
#include <memory>

#include "src/core/csv.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << std::format("usage: {} [INPUT_FILE] [OUTPUT_FILE]\n", argv[0]) << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    std::unique_ptr<curse::BatchStream> input_stream = std::make_unique<curse::CurseReader>(input_file);
    curse::WriteAsCsv(output_file, std::move(input_stream));

    return 0;
}
