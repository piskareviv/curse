#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include "src/core/assert.hpp"

namespace curse {

enum class TypeId {
    Int8,
    Int16,
    Int32,
    Int64,
    Int128,

    Float64,

    Char,
    String,

    Date,
    Timestamp,
};

template <TypeId... types>
struct TypeIdHolder {};

using AllTypesIds = TypeIdHolder<TypeId::Int8, TypeId::Int16, TypeId::Int32, TypeId::Int64, TypeId::Int128,
                                 TypeId::Float64, TypeId::Char, TypeId::String, TypeId::Date, TypeId::Timestamp>;

constexpr bool IsIntegral(TypeId id) {
    return id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 || id == TypeId::Int64 ||
           id == TypeId::Int128;
}

// struct IsIntegral

void ExecFor(TypeId id, auto&& func) {
    [&]<TypeId... types>(TypeIdHolder<types...>) {
        size_t cnt = 0;
        ((id == types ? (cnt += 1, func.template operator()<types>(), 0) : 0), ...);
        ENSURE(cnt == 1);
    }.template operator()<>(AllTypesIds());
}

template <TypeId>
struct ReprType;

template <>
struct ReprType<TypeId::Int8> {
    using T = int8_t;
};

template <>
struct ReprType<TypeId::Int16> {
    using T = int16_t;
};

template <>
struct ReprType<TypeId::Int32> {
    using T = int32_t;
};

template <>
struct ReprType<TypeId::Int64> {
    using T = int64_t;
};

template <>
struct ReprType<TypeId::Int128> {
    using T = __int128_t;
};

template <>
struct ReprType<TypeId::Float64> {
    using T = double;
};

template <>
struct ReprType<TypeId::Char> {
    using T = char;
};

template <>
struct ReprType<TypeId::Date> {
    using T = std::chrono::year_month_day;
};

template <>
struct ReprType<TypeId::Timestamp> {
    using T = std::chrono::system_clock::time_point;
};

template <>
struct ReprType<TypeId::String> {
    using T = std::string;
};

template <TypeId id>
struct ValueT {
    ReprType<id>::T value;

    friend bool operator==(const ValueT& a, const ValueT& b) {
        return a.value == b.value;
    }
};

struct MyHasher {
    template <typename T>
    std::size_t operator()(const T& value) const {
        if constexpr (std::is_same_v<T, ReprType<TypeId::Timestamp>::T> ||
                      std::is_same_v<T, ReprType<TypeId::Date>::T>) {
            if constexpr (std::is_same_v<T, ReprType<TypeId::Date>::T>) {
                auto x = std::chrono::sys_days{value}.time_since_epoch().count();
                return std::hash<decltype(x)>()(x);
            } else {
                auto x = value.time_since_epoch().count();
                return std::hash<decltype(x)>()(x);
            }
        } else {
            return std::hash<T>()(value);
        }
    }
};

template <TypeId id>
struct ValueT_Hasher {  // NOLINT
    std::size_t operator()(const ValueT<id>& value) const {
        return MyHasher()(value.value);
    }
};

class Value {
private:
    template <typename>
    struct Aux;

    template <TypeId... types>
    struct Aux<TypeIdHolder<types...>> {
        using T = std::variant<ValueT<types>...>;
    };

    using ValueEnum = Aux<AllTypesIds>::T;

public:
    ValueEnum value;

    // Value() {}

    // template <TypeId id>
    // Value(ValueT<id> vl) : value(std::move(vl)) {}

    // template <TypeId id>
    // Value(ReprType<id>::T vl) : Value(ValueT<id>(std::move(vl))) {}

    friend bool operator==(const Value& a, const Value& b) {
        return a.value == b.value;
    }

    TypeId Type() const {
        return std::visit([&]<TypeId id>(const ValueT<id>&) { return id; }, value);
    }
};

struct ValueHasher {  // NOLINT
    size_t operator()(const Value& value) const {
        return std::visit([&]<TypeId id>(const ValueT<id>& vl) { return ValueT_Hasher<id>()(vl); }, value.value);
    }
};

template <TypeId, typename = void>
struct ColumnT;

template <TypeId id>
struct ColumnT<id> {
    using T = ReprType<id>::T;
    static constexpr TypeId kId = id;

    std::deque<T> values;

    void Append(T value) {
        values.push_back(std::move(value));
    }

    void Append(std::span<const T> sp) {
        for (const auto& value : sp) {
            Append(value);
        }
    }

    void Append(const ColumnT<id>& col) {
        for (const auto& value : col.values) {
            Append(value);
        }
    }

    std::vector<T> ToVector() const {
        return std::vector<T>(values.begin(), values.end());
    }
    static ColumnT FromVector(const std::vector<T>& vec) {
        ColumnT col;
        col.values.assign(vec.begin(), vec.end());
        return col;
    }

    T& operator[](size_t ind) {
        return values[ind];
    }
    const T& operator[](size_t ind) const {
        return values[ind];
    }

    size_t Size() const {
        return values.size();
    }

    friend bool operator==(const ColumnT& a, const ColumnT& b) {
        return a.values == b.values;
    }

    ColumnT<id> Filter(std::span<const char> filt) const {
        ENSURE(filt.size() == Size());
        ColumnT<id> col;
        for (size_t i = 0; i < values.size(); i++) {
            if (filt[i]) {
                col.Append(values[i]);
            }
        }
        return col;
    }

    ColumnT<id> Select(std::span<const size_t> inds) const {
        ColumnT<id> col;
        for (size_t i = 0; i < inds.size(); i++) {
            col.Append(values[inds[i]]);
        }
        return col;
    }

    void StableArgsort(std::span<size_t> sp, bool reversed = false) const {
        if (!reversed) {
            std::stable_sort(sp.begin(), sp.end(), [&](size_t a, size_t b) { return values[a] < values[b]; });
        } else {
            std::stable_sort(sp.begin(), sp.end(), [&](size_t a, size_t b) { return values[a] > values[b]; });
        }
    }
};

class Column {
private:
    template <typename>
    struct Aux;

    template <TypeId... types>
    struct Aux<TypeIdHolder<types...>> {
        using T = std::variant<ColumnT<types>...>;
    };

    using ColumnEnum = Aux<AllTypesIds>::T;
    // using ColumnEnum = typename Mystery<TypeId, TypeIdHolder, AllTypesIds, ColumnT, std::variant>::T;

    ColumnEnum m_column;

    Column();

    template <typename, typename>
    friend struct Convert;

public:
    Column(TypeId id);
    Column(Value val);

    template <TypeId id>
    Column(ColumnT<id> column) : m_column(std::move(column)) {}

    TypeId Type() const;
    size_t Size() const;

    void Append(Value value);

    ColumnEnum& Values();
    const ColumnEnum& Values() const;
};

class Schema {
public:
    struct ColumnInfo {
        std::string name;
        TypeId type;
    };

private:
    struct MyHash {
        using is_transparent = void;  // NOLINT

        size_t operator()(std::string_view sv) const {
            return std::hash<std::string_view>{}(sv);
        }
        size_t operator()(const std::string& s) const {
            return std::hash<std::string>{}(s);
        }
    };

    const std::vector<ColumnInfo> m_columns;
    std::unordered_map<std::string, size_t, MyHash, std::equal_to<>> m_map;

public:
    Schema(std::vector<ColumnInfo> columns);

    const std::vector<ColumnInfo>& Columns() const;
    size_t IndexOf(std::string_view column_name) const;
    TypeId TypeOf(std::string_view column_name) const;
};

std::shared_ptr<const Schema> AddColumn(const Schema& schema, const Schema::ColumnInfo& info);
std::shared_ptr<const Schema> AddColumns(const Schema& schema, const std::vector<Schema::ColumnInfo>& cols);

class Batch {
private:
    std::shared_ptr<const Schema> m_schema;
    std::vector<Column> m_columns;

public:
    Batch(const std::shared_ptr<const Schema>& schema, std::vector<Column> columns);

    std::vector<Column> ExtractColumns();

    size_t NRows() const;
    const std::vector<Column>& Columns() const;
    std::shared_ptr<const Schema> GetSchema() const;
};

class BatchStream {
public:
    // null pointer means end of stream
    virtual std::unique_ptr<Batch> Next() = 0;

    virtual std::shared_ptr<const Schema> GetSchema() = 0;

    virtual ~BatchStream() {}
};

class Operator {
public:
    // the returned BatchStream should not depend on Operator being alive
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const = 0;

    // just applies .Transform
    friend std::unique_ptr<BatchStream> operator>=(std::unique_ptr<BatchStream> stream, const Operator& op);

    virtual ~Operator() {}
};

}  // namespace curse
