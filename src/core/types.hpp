#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <set>
#include <string>
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

void ExecFor(TypeId id, auto&& func) {
    [&]<TypeId... types>(TypeIdHolder<types...>) {
        size_t cnt = 0;
        ((id == types ? (func.template operator()<types>(), 0) : 0), ...);
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

    ColumnEnum m_column;

    Column();

    template <typename, typename>
    friend struct Convert;

public:
    Column(TypeId id);

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
    std::vector<ColumnInfo> m_columns;

public:
    Schema(std::vector<ColumnInfo> columns);

    const std::vector<ColumnInfo>& Columns() const;
};

// approximate target batch memory consumption in bytes
constexpr size_t kBatchMemory = 128 * (1 << 20);

class Batch {
private:
    std::shared_ptr<const Schema> m_schema;
    std::vector<Column> m_columns;

public:
    Batch(const std::shared_ptr<const Schema>& schema, std::vector<Column> columns);

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
