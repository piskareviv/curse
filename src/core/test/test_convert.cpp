#include <chrono>
#include <iostream>
#include <vector>

#include "gtest/gtest.h"
#include "src/core/convert.hpp"
#include "src/core/types.hpp"

using namespace curse;        // NOLINT
using namespace std::chrono;  // NOLINT

bool CheckConvert(Column col) {
    TypeId id = col.Type();
    return col.Values() == Convert<Column>::FromBytes(id, Convert<Column>::ToBytes(col)).Values();
}

TEST(Convert, AllTypes) {
    CheckConvert(ColumnT<TypeId::Int8>{.values = {-1, 0, 1, 2, 10, -100}});
    CheckConvert(ColumnT<TypeId::Int16>{.values = {-1, 0, 1, 2, 10, -100}});
    CheckConvert(ColumnT<TypeId::Int32>{.values = {-1, 0, 1, 2, 10, -100}});
    CheckConvert(ColumnT<TypeId::Int64>{.values = {-1, 0, 1, 2, 10, -100}});
    CheckConvert(ColumnT<TypeId::Int128>{.values = {-1, 0, 1, 2, 10, -100}});

    CheckConvert(ColumnT<TypeId::Float64>{.values = {-1, 0, 1, 2, 10, -100, 0.1}});

    CheckConvert(ColumnT<TypeId::Char>{.values = {'a', 'b', '0', 'a', '\0', '\n'}});
    CheckConvert(ColumnT<TypeId::String>{.values = {"abacabadabacaba", "new\nline", "a\tb"}});

    CheckConvert(ColumnT<TypeId::Date>{.values = {year_month_day(2020y, February, 24d)}});
    CheckConvert(ColumnT<TypeId::Timestamp>{
        .values = {sys_days{year_month_day(2020y, February, 24d)} + 20h + 5min + 10s + 1ns}});
}

TEST(Convert, Empty) {
    CheckConvert(ColumnT<TypeId::Int8>{});
    CheckConvert(ColumnT<TypeId::Int16>{});
    CheckConvert(ColumnT<TypeId::Int32>{});
    CheckConvert(ColumnT<TypeId::Int64>{});
    CheckConvert(ColumnT<TypeId::Int128>{});
    CheckConvert(ColumnT<TypeId::Float64>{});
    CheckConvert(ColumnT<TypeId::Char>{});
    CheckConvert(ColumnT<TypeId::String>{});
    CheckConvert(ColumnT<TypeId::Date>{});
    CheckConvert(ColumnT<TypeId::Timestamp>{});
}

TEST(Convert, StringEdgeCases) {
    std::vector<char> special_chars = {'\0', '\1', '\n', ' ', 'a'};

    std::vector<std::vector<std::string>> vec(6);
    vec[0] = {""};

    for (size_t i = 0; i + 1 < vec.size(); i++) {
        for (const auto& s : vec[i]) {
            for (char ch : special_chars) {
                vec[i + 1].push_back(s + ch);
            }
        }
    }
    std::vector<std::string> all;
    for (const auto& v : vec) {
        all.insert(all.end(), v.begin(), v.end());
    }

    for (auto a : all) {
        CheckConvert(ColumnT<TypeId::String>{.values = {a}});
        for (auto b : all) {
            CheckConvert(ColumnT<TypeId::String>{.values = {a, b}});
            if (std::max({a.size(), b.size()}) > 3) {
                break;
            }
        }
    }
}

TEST(Convert, StringEdgeCases2) {
    std::vector<char> special_chars = {'\0', '\1'};

    std::vector<std::vector<std::string>> vec(5);
    vec[0] = {""};

    for (size_t i = 0; i + 1 < vec.size(); i++) {
        for (const auto& s : vec[i]) {
            for (char ch : special_chars) {
                vec[i + 1].push_back(s + ch);
            }
        }
    }
    std::vector<std::string> all;
    for (const auto& v : vec) {
        all.insert(all.end(), v.begin(), v.end());
    }

    for (auto a : all) {
        CheckConvert(ColumnT<TypeId::String>{.values = {a}});
        for (auto b : all) {
            CheckConvert(ColumnT<TypeId::String>{.values = {a, b}});
            for (auto c : all) {
                CheckConvert(ColumnT<TypeId::String>{.values = {a, b, c}});
            }
        }
    }
}
