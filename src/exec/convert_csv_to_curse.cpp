#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>

#include "src/core/csv.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"
#include "src/exec/hits_schema.hpp"

void PrintStats(std::ostream& out, const curse::Schema& schema, const std::vector<curse::Stats>& stats);

int main(int argc, char** argv) {
    auto print_usage = [&] {
        std::cerr << std::format(
                         "usage: {} <INPUT_FILE> <OUTPUT_FILE> [--stats]\n"
                         "if INPUT_FILE is \"-\", reads from stdin\n",
                         argv[0])
                  << std::endl;
        exit(1);
    };

    if (argc != 3 && argc != 4) {
        print_usage();
    }
    if (argc == 4) {
        std::string s = std::string(argv[3]);
        if (s != "-s" && s != "--stats") {
            print_usage();
        }
    }

    bool print_stats = argc == 4;

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    std::unique_ptr<curse::BatchStream> input_stream;
    if (input_file == "-") {
        input_stream = std::make_unique<curse::CsvReader>(std::cin, kHitsSchema);
    } else {
        std::unique_ptr<std::ifstream> fin = std::make_unique<std::ifstream>(input_file);
        input_stream = std::make_unique<curse::CsvReader>(std::move(fin), kHitsSchema);
    }
    if (!print_stats) {
        WriteAsCurse(output_file, std::move(input_stream));
    } else {
        std::vector<curse::Stats> stats;
        WriteAsCurse(output_file, std::move(input_stream), stats);

        PrintStats(std::cerr, kHitsSchema, stats);
    }
    return 0;
}

void PrintStats(std::ostream& out, const curse::Schema& schema, const std::vector<curse::Stats>& stats) {
    auto format_sz = [&](size_t sz) { return std::format("{:.3f} MB", sz / static_cast<double>(1 << 20)); };

    auto print = [&](const std::vector<std::vector<std::vector<std::string>>>& vec) {
        // эта функция нейрослоп
        if (vec.empty()) {
            return;
        }

        std::vector<size_t> max_col_widths;
        for (const auto& row : vec) {
            if (row.size() > max_col_widths.size()) {
                max_col_widths.resize(row.size(), 0);
            }
            for (size_t col = 0; col < row.size(); ++col) {
                const auto& item = row[col];
                if (item.size() > 1) {
                    size_t current_len = item[1].length();
                    std::string align = (item.size() > 2) ? item[2] : "right";
                    if (align == "left" && col < row.size() - 1) {
                        current_len += 2;
                    }
                    if (current_len > max_col_widths[col]) {
                        max_col_widths[col] = current_len;
                    }
                }
            }
        }

        for (const auto& row : vec) {
            std::string line;
            for (size_t col = 0; col < row.size(); ++col) {
                const auto& item = row[col];
                if (item.empty()) {
                    continue;
                }
                std::string key = item[0];
                std::string value = (item.size() > 1) ? item[1] : "";
                std::string align = (item.size() > 2) ? item[2] : "right";

                bool is_last_col = (col == row.size() - 1);
                std::string element_str;

                if (align == "left") {
                    std::string content = key + ": " + value;
                    if (!is_last_col) {
                        content += "  ";
                    }
                    size_t visual_len = value.length() + (is_last_col ? 0 : 2);
                    size_t padding = max_col_widths[col] > visual_len ? max_col_widths[col] - visual_len : 0;
                    element_str = content + std::string(padding, ' ');
                } else {
                    size_t padding = max_col_widths[col] > value.length() ? max_col_widths[col] - value.length() : 0;
                    element_str = key + ": " + std::string(padding, ' ') + value;
                    if (!is_last_col) {
                        element_str += "  ";
                    }
                }

                line += element_str;
            }
            out << line << std::endl;
        }
    };

    using namespace curse;  // NOLINT

    const std::map<TypeId, std::string> type_to_str =  //
        {{TypeId::Int8, "Int8"},          {TypeId::Int16, "Int16"},   {TypeId::Int32, "Int32"},
         {TypeId::Int64, "Int64"},        {TypeId::Int128, "Int128"}, {TypeId::Float64, "Float64"},
         {TypeId::Char, "Char"},          {TypeId::String, "String"}, {TypeId::Date, "Date"},
         {TypeId::Timestamp, "Timestamp"}};

    std::map<std::string, std::pair<int, Stats>> map2;
    std::pair<int, Stats> total(0, {});

    std::vector<std::vector<std::vector<std::string>>> table1, table2, table3;

    for (size_t i = 0; i < schema.Columns().size(); i++) {
        TypeId id = schema.Columns()[i].type;
        std::string tp = type_to_str.contains(id) ? type_to_str.find(id)->second : "unknown";

        table1.push_back({
            {"name", schema.Columns()[i].name, "left"},
            {"type", tp, "left"},
            {"raw", format_sz(stats[i].total_bytes)},
            {"compressed", format_sz(stats[i].total_bytes_compressed)},
        });

        map2[tp].first += 1;
        map2[tp].second.total_bytes += stats[i].total_bytes;
        map2[tp].second.total_bytes_compressed += stats[i].total_bytes_compressed;

        total.first += 1;
        total.second.total_bytes += stats[i].total_bytes;
        total.second.total_bytes_compressed += stats[i].total_bytes_compressed;
    }

    for (auto [id, tp] : type_to_str) {
        auto [cnt, st] = map2[tp];

        if (cnt > 0) {
            table2.push_back({
                {"type", tp, "left"},
                {"count", std::to_string(cnt), "left"},
                {"raw", format_sz(st.total_bytes)},
                {"compressed", format_sz(st.total_bytes_compressed)},
            });
        }
    }

    table3.push_back({
        {"count", std::to_string(total.first)},
        {"raw", format_sz(total.second.total_bytes)},
        {"compressed", format_sz(total.second.total_bytes_compressed)},
    });

    print(table1);
    out << std::format("====== stats by column type ======") << std::endl;
    print(table2);
    out << std::format("==========================") << std::endl;
    print(table3);
    out << std::format("==========================") << std::endl;
}
