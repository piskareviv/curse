#include <chrono>
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
    CheckConvert(ColumnT<TypeId::Int8>::FromVector({-1, 0, 1, 2, 10, -100}));
    CheckConvert(ColumnT<TypeId::Int16>::FromVector({-1, 0, 1, 2, 10, -100}));
    CheckConvert(ColumnT<TypeId::Int32>::FromVector({-1, 0, 1, 2, 10, -100}));
    CheckConvert(ColumnT<TypeId::Int64>::FromVector({-1, 0, 1, 2, 10, -100}));
    CheckConvert(ColumnT<TypeId::Int128>::FromVector({-1, 0, 1, 2, 10, -100}));

    CheckConvert(ColumnT<TypeId::Float64>::FromVector({-1, 0, 1, 2, 10, -100, 0.1}));

    CheckConvert(ColumnT<TypeId::Char>::FromVector({'a', 'b', '0', 'a', '\0', '\n'}));
    CheckConvert(ColumnT<TypeId::String>::FromVector({"abacabadabacaba", "new\nline", "a\tb"}));

    CheckConvert(ColumnT<TypeId::Date>::FromVector({year_month_day(2020y, February, 24d)}));
    CheckConvert(ColumnT<TypeId::Timestamp>::FromVector(
        {sys_days{year_month_day(2020y, February, 24d)} + 20h + 5min + 10s + 1ns}));
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
        CheckConvert(ColumnT<TypeId::String>::FromVector({a}));
        for (auto b : all) {
            CheckConvert(ColumnT<TypeId::String>::FromVector({a, b}));
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
        CheckConvert(ColumnT<TypeId::String>::FromVector({a}));
        for (auto b : all) {
            CheckConvert(ColumnT<TypeId::String>::FromVector({a, b}));
            for (auto c : all) {
                CheckConvert(ColumnT<TypeId::String>::FromVector({a, b, c}));
            }
        }
    }
}

// ниже нейрослоп

#include <gtest/gtest.h>

#include <chrono>
#include <string_view>

// using namespace std::chrono;

// Helper to check round-trip: T -> string -> T
template <typename T>
void ExpectRoundTrip(T input) {
    std::string s = Convert<T>::ToString(input);
    T output = Convert<T>::FromString(s);
    EXPECT_EQ(input, output) << "Failed for string representation: " << s;
}

TEST(ConvertTest, NumericStandardTypes) {
    // Int8
    ExpectRoundTrip<int8_t>(127);
    ExpectRoundTrip<int8_t>(-128);
    ExpectRoundTrip<int8_t>(0);

    // Int64
    ExpectRoundTrip<int64_t>(9223372036854775807LL);
    ExpectRoundTrip<int64_t>(-9223372036854775807LL - 1);

    // Float64
    ExpectRoundTrip<double>(3.1415926535);
    ExpectRoundTrip<double>(0.0);
    ExpectRoundTrip<double>(-1.23e10);

    // Check that garbage fails
    EXPECT_THROW(Convert<int32_t>::FromString("123a"), std::runtime_error);
}

TEST(ConvertTest, Int128EdgeCases) {
    using int128 = __int128_t;  // NOLINT

    ExpectRoundTrip<int128>(0);
    ExpectRoundTrip<int128>(1);
    ExpectRoundTrip<int128>(-1);

    // Max Int128: (2^127) - 1
    int128 max128 = ~static_cast<int128>(0) >> 1;
    ExpectRoundTrip<int128>(max128);

    // Min Int128: -(2^127)
    int128 min128 = static_cast<int128>(1) << 127;
    ExpectRoundTrip<int128>(min128);

    // Check overflow validation
    EXPECT_ANY_THROW(Convert<int128>::FromString("170141183460469231731687303715884105728"));  // Max+1
}

TEST(ConvertTest, ChronoTypes) {
    // Date: 2020-02-24
    auto date_val = 2020y / February / 24d;
    EXPECT_EQ(Convert<year_month_day>::ToString(date_val), "2020-02-24");
    ExpectRoundTrip<year_month_day>(date_val);

    // Timestamp: 2020-02-24 19:00:00
    // Using system_clock::time_point (assuming it maps to Timestamp)
    auto ts_val = sys_days{2020y / February / 24d} + 19h;
    // Note: %T includes seconds. Adjust format if your Precision differs.
    // auto ts_seconds = std::chrono::time_point_cast<std::chrono::seconds>(ts_val);
    EXPECT_EQ(Convert<system_clock::time_point>::ToString(ts_val), "2020-02-24 19:00:00");
    ExpectRoundTrip<system_clock::time_point>(ts_val);

    EXPECT_THROW(Convert<year_month_day>::FromString("not-a-date"), std::runtime_error);
}

TEST(ConvertTest, StringAndChar) {
    // Char
    ExpectRoundTrip<char>('A');
    ExpectRoundTrip<char>('\0');
    ExpectRoundTrip<char>(',');

    // String
    ExpectRoundTrip<std::string>("hello world");
    ExpectRoundTrip<std::string>("line1\nline2");
    ExpectRoundTrip<std::string>("");  // Empty string
}

TEST(ConvertTest, FloatPrecision) {
    // std::to_chars/from_chars should be lossless
    double val = 0.123456789012345;
    std::string s = Convert<double>::ToString(val);
    double out = Convert<double>::FromString(s);

    // Exactly equal, not just close
    EXPECT_DOUBLE_EQ(val, out);
}
