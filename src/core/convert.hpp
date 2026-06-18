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

#include "src/core/types.hpp"
#include "src/util/assert.hpp"
#include "src/util/util.hpp"

namespace curse {

template <TypeId>
struct ConvertRaw {
    static const bool kHasRawType = false;
};

template <TypeId>
struct ConvertVal;

template <typename>
struct ConvertCol;

template <>
struct ConvertRaw<TypeId::Date> {
    static const bool kHasRawType = true;
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
struct ConvertRaw<TypeId::Timestamp> {
    static const bool kHasRawType = true;
    using T = ReprType<TypeId::Timestamp>::T;
    using RawT = int64_t;

    static RawT ToRaw(const T& value) {
        return static_cast<int64_t>(std::chrono::system_clock::time_point(value).time_since_epoch().count());
    }

    static T FromRaw(const RawT& raw) {
        return std::chrono::system_clock::time_point(std::chrono::nanoseconds(raw));
    }
};

template <TypeId id>
    requires(id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 || id == TypeId::Int64 ||
             id == TypeId::Int128 || id == TypeId::Char || id == TypeId::Float64)
struct ConvertRaw<id> {
    static const bool kHasRawType = true;

    using T = ReprType<id>::T;
    using RawT = T;

    static RawT ToRaw(const T& value) {
        return value;
    }

    static T FromRaw(const RawT& raw) {
        return raw;
    }
};

template <TypeId id>
    requires(id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 || id == TypeId::Int64 ||
             id == TypeId::Float64)
struct ConvertVal<id> {
    using T = ReprType<id>::T;

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
template <TypeId id>
    requires(id == TypeId::Int128)
struct ConvertVal<id> {
    using T = ReprType<id>::T;

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

template <TypeId id>
    requires(id == TypeId::Date)
struct ConvertVal<id> {
    using T = ReprType<id>::T;

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

template <TypeId id>
    requires(id == TypeId::Timestamp)
struct ConvertVal<id> {
    using T = ReprType<id>::T;

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

template <TypeId id>
    requires(id == TypeId::Char)
struct ConvertVal<id> {
    using T = ReprType<id>::T;

    static std::string ToString(const T& value) {
        return std::string(1, value);
    }
    static T FromString(std::string_view sv) {
        ENSURE(sv.size() == 1);
        return sv[0];
    }
};

template <TypeId id>
    requires(id == TypeId::String)
struct ConvertVal<id> {
    using T = ReprType<id>::T;

    static std::string ToString(std::string_view value) {
        return std::string(value);
    }
    static std::string ToString(const T& value) {
        return value;
    }
    static T FromString(std::string_view sv) {
        return T(sv);
    }
};

template <TypeId id>
    requires(id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 || id == TypeId::Int64 ||
             id == TypeId::Int128 || id == TypeId::Char || id == TypeId::Float64)
struct ConvertCol<ColumnT<id>> {

    using T = ReprType<id>::T;

    static std::vector<char> ToBytes(const ColumnT<id>& col) {
        return VecToBytes<T>(col.ToVector());
    }
    static ColumnT<id> FromBytes(std::span<const char> bytes) {
        return ColumnT<id>::FromVector(VecFromBytes<T>(bytes));
    }
};

template <TypeId id>
    requires(id == TypeId::Date || id == TypeId::Timestamp)
struct ConvertCol<ColumnT<id>> {
    using T = ReprType<id>::T;
    using RawT = ConvertRaw<id>::RawT;
    static constexpr TypeId kId = id;

    static std::vector<char> ToBytes(const ColumnT<id>& col) {
        std::vector<RawT> vec(col.Size());
        for (size_t i = 0; i < col.Size(); i++) {
            vec[i] = ConvertRaw<id>::ToRaw(col[i]);
        }
        return VecToBytes(vec);
    }

    static ColumnT<id> FromBytes(std::span<const char> bytes) {
        std::vector<RawT> vec(VecFromBytes<RawT>(bytes));
        ColumnT<id> col;
        col.Reserve(vec.size());
        for (size_t i = 0; i < vec.size(); i++) {
            col.Append(ConvertRaw<id>::FromRaw(vec[i]));
        }
        return col;
    }
};

template <TypeId id>
    requires(id == TypeId::String)
struct ConvertCol<ColumnT<id>> {
    using T = ReprType<id>::T;
    static constexpr TypeId kId = id;

private:
    static constexpr char kDelim = 0;
    static constexpr char kEscape = 1;

public:
    static std::vector<char> ToBytes(const ColumnT<id>& col) {
        std::vector<char> result;
        for (size_t i = 0; i < col.Size(); i++) {
            for (char ch : col[i]) {
                if (ch == kDelim || ch == kEscape) {
                    result.push_back(kEscape);
                }
                result.push_back(ch);
            }
            result.push_back(kDelim);
        }

        return result;
    }

    static ColumnT<id> FromBytes(std::string_view sv) {
        return FromBytes(std::span(sv));
    }

    static ColumnT<id> FromBytes(std::span<const char> bytes) {
        if (bytes.empty()) {
            return ColumnT<id>();
        }

        ENSURE_MSG(bytes.back() == 0, "invalid input");

        ColumnT<id> col;
        size_t sz = 0;
        std::string token;

        size_t i = 0;
        for (; i + 1 < bytes.size();) {
            size_t dlt = std::min<size_t>(bytes.size() - i - 1, 1024);
            size_t min_sz = sz + dlt + 10;

            if (token.size() < min_sz) {
                token.resize(std::max(2 * token.size() + 10, min_sz + 5));
            }

            size_t beg = i;
            for (; i - beg < dlt; i++) {
                char ch = bytes[i];
                unsigned char ch_u = ch;

                // if (ch != kDelim && ch != kEscape) {
                static_assert(kDelim == 0 && kEscape == 1);
                if (ch_u > static_cast<unsigned char>(1)) {
                    token[sz++] = ch;
                } else {
                    if (ch == kDelim) {
                        col.Append(std::string_view(token).substr(0, sz));
                        sz = 0;
                    } else {
                        token[sz++] = bytes[i + 1];
                        i += 1;
                    }
                }
            }
        }
        ENSURE_MSG(i + 1 == bytes.size(), "invalid input");

        col.Append(std::string_view(token).substr(0, sz));

        return col;
    }
};

template <>
struct ConvertCol<Column> {
    static std::vector<char> ToBytes(const Column& column) {
        return std::visit([]<TypeId id>(const ColumnT<id>& col) { return ConvertCol<ColumnT<id>>::ToBytes(col); },
                          column.m_column);
    }

    static Column FromBytes(TypeId id, std::span<const char> bytes) {
        Column col;
        ExecFor(id, [&]<TypeId id> { col.m_column = ConvertCol<ColumnT<id>>::FromBytes(bytes); });
        return col;
    }
};

}  // namespace curse
