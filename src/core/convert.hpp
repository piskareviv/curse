#pragma once

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <format>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "src/core/assert.hpp"
#include "src/core/types.hpp"
#include "src/core/util.hpp"

namespace curse {

template <TypeId>
struct Helper;

template <>
struct Helper<TypeId::Date> {
    using T = ReprType<TypeId::Date>::T;
    using RawT = int32_t;

    static RawT ToRaw(const T& value) {
        return static_cast<int32_t>(std::chrono::sys_days(value).time_since_epoch().count());
    }

    static T FromRaw(const RawT& raw) {
        return std::chrono::year_month_day(std::chrono::sys_days(std::chrono::days(raw)));
    }
};

template <>
struct Helper<TypeId::Timestamp> {
    using T = ReprType<TypeId::Timestamp>::T;
    using RawT = int64_t;

    static RawT ToRaw(const T& value) {
        return static_cast<int64_t>(std::chrono::system_clock::time_point(value).time_since_epoch().count());
    }

    static T FromRaw(const RawT& raw) {
        return std::chrono::system_clock::time_point(std::chrono::nanoseconds(raw));
    }
};

template <typename T, typename = void>
struct Convert;

template <typename T>
    requires(std::is_same_v<T, ReprType<TypeId::Int8>::T> || std::is_same_v<T, ReprType<TypeId::Int16>::T> ||
             std::is_same_v<T, ReprType<TypeId::Int32>::T> || std::is_same_v<T, ReprType<TypeId::Int64>::T> ||
             std::is_same_v<T, ReprType<TypeId::Float64>::T>)
struct Convert<T> {
    static std::string ToString(const T& value) {
        char buf[128];
        auto [ptr, err] = std::to_chars(buf, buf + sizeof(buf), value);
        if (err != std::errc()) {
            throw std::runtime_error("Convert::ToString failed");
        }
        return std::string(buf, ptr);
    }
    static T FromString(std::string_view sv) {
        T value = 0;
        auto [ptr, err] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (err != std::errc() || ptr != sv.data() + sv.size()) {
            throw std::runtime_error("Convert::FromString failed");
        }
        return value;
    }
};
template <typename T>
    requires(std::is_same_v<T, ReprType<TypeId::Int128>::T>)
struct Convert<T> {
    static std::string ToString(const T& value) {
        if (value == 0) {
            return "0";
        }
        T n = value;
        std::string result;
        bool sign = n < 0;
        while (n != 0) {
            result.push_back('0' + std::abs(static_cast<int>(n % 10)));
            n /= 10;
        }
        if (sign) {
            result.push_back('-');
        }
        std::reverse(result.begin(), result.end());
        return result;
    }
    static T FromString(std::string_view sv) {
        ENSURE(!sv.empty());

        bool sign = false;
        if (sv[0] == '-') {
            sign = true;
            sv = sv.substr(1);
        }

        const std::string_view exp2_127 = "170141183460469231731687303715884105728";
        if (!sign) {
            ENSURE_MSG(sv.size() < exp2_127.size() || (sv.size() == exp2_127.size() && sv < exp2_127),
                       "number too big to be converted to int128");
        } else {
            ENSURE_MSG(sv.size() < exp2_127.size() || (sv.size() == exp2_127.size() && sv <= exp2_127),
                       "number too big to be converted to int128");
        }

        T value = 0;
        for (char ch : sv) {
            ENSURE('0' <= ch && ch <= '9');
            value = value * 10 + (ch - '0') * (2 * !sign - 1);
        }
        return value;
    }
};

template <typename T>
    requires(std::is_same_v<T, ReprType<TypeId::Date>::T>)
struct Convert<T> {
    static std::string ToString(const T& value) {
        return std::format("{:%F}", value);
    }
    static T FromString(std::string_view sv) {
        T value{};
        std::istringstream ss{std::string(sv)};
        ss >> std::chrono::parse("%F", value);
        if (ss.fail()) {
            throw std::runtime_error("date parsing failed");
        }
        return value;
    }
};

template <typename T>
    requires(std::is_same_v<T, ReprType<TypeId::Timestamp>::T>)
struct Convert<T> {
    static std::string ToString(const T& value) {
        auto value_seconds = std::chrono::time_point_cast<std::chrono::seconds>(value);
        if (value == value_seconds) {
            return std::format("{:%F %T}", value_seconds);
        } else {
            return std::format("{:%F %T}", value);
        }
    }
    static T FromString(std::string_view sv) {
        T value{};
        std::istringstream ss{std::string(sv)};
        ss >> std::chrono::parse("%F %T", value);
        if (ss.fail()) {
            throw std::runtime_error("date parsing failed");
        }
        return value;
    }
};

template <typename T>
    requires(std::is_same_v<T, ReprType<TypeId::Char>::T>)
struct Convert<T> {
    static std::string ToString(const T& value) {
        return std::string(1, value);
    }
    static T FromString(std::string_view sv) {
        ENSURE(sv.size() == 1);
        return sv[0];
    }
};

template <typename T>
    requires(std::is_same_v<T, ReprType<TypeId::String>::T>)
struct Convert<T> {
    static std::string ToString(const T& value) {
        return value;
    }
    static T FromString(std::string_view sv) {
        return T(sv);
    }
};

template <TypeId id>
struct Convert<ColumnT<id>, std::enable_if_t<id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 ||
                                             id == TypeId::Int64 || id == TypeId::Int128 || id == TypeId::Char ||
                                             id == TypeId::Float64>> {

    using T = ReprType<id>::T;

    static std::vector<char> ToBytes(const ColumnT<id>& col) {
        return VecToBytes<T>(col.values);
    }
    static ColumnT<id> FromBytes(std::span<const char> bytes) {
        return ColumnT<id>{.values = VecFromBytes<T>(bytes)};
    }
};

template <TypeId id>
struct Convert<ColumnT<id>, std::enable_if_t<id == TypeId::Date || id == TypeId::Timestamp>> {
    using T = ReprType<id>::T;
    using RawT = Helper<id>::RawT;
    static constexpr TypeId kId = id;

    static std::vector<char> ToBytes(const ColumnT<id>& col) {
        const std::vector<T>& values = col.values;

        std::vector<RawT> vec(values.size());
        for (size_t i = 0; i < values.size(); i++) {
            vec[i] = Helper<id>::ToRaw(values[i]);
        }
        return VecToBytes(vec);
    }

    static ColumnT<id> FromBytes(std::span<const char> bytes) {
        std::vector<RawT> vec(VecFromBytes<RawT>(bytes));
        std::vector<T> values(vec.size());
        for (size_t i = 0; i < vec.size(); i++) {
            values[i] = Helper<id>::FromRaw(vec[i]);
        }
        return ColumnT<id>{.values = values};
    }
};

template <TypeId id>
struct Convert<ColumnT<id>, std::enable_if_t<id == TypeId::String>> {
    using T = ReprType<id>::T;
    static constexpr TypeId kId = id;

private:
    static constexpr char kDelim = 0;
    static constexpr char kEscape = 1;

public:
    static std::vector<char> ToBytes(const ColumnT<id>& col) {
        const std::vector<std::string>& values = col.values;

        std::vector<char> result;

        for (size_t i = 0; i < values.size(); i++) {
            for (char ch : values[i]) {
                if (ch == kDelim || ch == kEscape) {
                    result.push_back(kEscape);
                }
                result.push_back(ch);
            }
            result.push_back(kDelim);
        }

        return result;
    }

    static ColumnT<id> FromBytes(std::span<const char> bytes) {
        if (bytes.empty()) {
            return ColumnT<id>{.values = {}};
        }

        ENSURE_MSG(bytes.back() == 0, "invalid input");

        std::vector<std::string> values;
        std::string token;

        size_t i = 0;
        for (; i + 1 < bytes.size(); i++) {
            char ch = bytes[i];
            if (ch == kDelim) {
                values.emplace_back(std::move(token));
                token.clear();
            } else {
                if (ch != kEscape) {
                    token.push_back(ch);
                } else {
                    token.push_back(bytes[i + 1]);
                    i += 1;
                }
            }
        }
        ENSURE_MSG(i + 1 == bytes.size(), "invalid input");

        values.emplace_back(std::move(token));

        return ColumnT<id>{.values = std::move(values)};
    }
};

template <>
struct Convert<Column> {
    static std::vector<char> ToBytes(const Column& column) {
        return std::visit([]<TypeId id>(const ColumnT<id>& col) { return Convert<ColumnT<id>>::ToBytes(col); },
                          column.m_column);
    }

    static Column FromBytes(TypeId id, std::span<const char> bytes) {
        Column col;
        ExecFor(id, [&]<TypeId id> { col.m_column = Convert<ColumnT<id>>::FromBytes(bytes); });
        return col;
    }
};

}  // namespace curse
