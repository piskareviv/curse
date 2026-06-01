#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "src/core/assert.hpp"
#include "src/core/util.hpp"

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

template <TypeId id>
struct ValueT_Hasher {  // NOLINT
    std::size_t operator()(const ValueT<id>& value) const {
        using T = typename ReprType<id>::T;
        if constexpr (id == TypeId::Date || id == TypeId::Timestamp) {
            if constexpr (id == TypeId::Date) {
                auto x = std::chrono::sys_days{value.value}.time_since_epoch().count();
                return std::hash<decltype(x)>()(x);
            } else {
                auto x = value.value.time_since_epoch().count();
                return std::hash<decltype(x)>()(x);
            }
        } else {
            return std::hash<T>()(value.value);
        }
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

    TypeId Type() const {
        return std::visit([&]<TypeId id>(const ValueT<id>&) { return id; }, value);
    }
};

template <TypeId, typename = void>
struct ColumnT;

template <TypeId id>
struct ColumnT<id> {
    using T = ReprType<id>::T;
    static constexpr TypeId kId = id;

    std::vector<T> values;

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
    const std::vector<ColumnInfo> m_columns;

public:
    Schema(std::vector<ColumnInfo> columns);

    const std::vector<ColumnInfo>& Columns() const;
    size_t IndexOf(std::string_view column_name) const;

    TypeId TypeOf(size_t) const;
    // TypeId TypeOf(std::string_view column_name) const;
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
