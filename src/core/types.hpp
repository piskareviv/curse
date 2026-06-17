#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "src/util/assert.hpp"

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

template <template <TypeId> typename, typename>
struct MakeEnum;

template <template <TypeId> typename X, TypeId... ids>
struct MakeEnum<X, TypeIdHolder<ids...>> {
    using T = std::variant<X<ids>...>;
};

constexpr bool IsIntegral(TypeId id) {
    return id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 || id == TypeId::Int64 ||
           id == TypeId::Int128;
}

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
template <>
struct ValueT<TypeId::String> {
    ReprType<TypeId ::String>::T value;

    ValueT() {}

    ValueT(std::string val) : value(std::move(val)) {}
    ValueT(std::string_view val) : value(val) {}

    friend bool operator==(const ValueT& a, const ValueT& b) {
        return a.value == b.value;
    }
};

class Value {
private:
    using ValueEnum = MakeEnum<ValueT, AllTypesIds>::T;

public:
    ValueEnum value;

    template <TypeId id>
    static Value From(ValueT<id> vl) {
        return Value{.value = std::move(vl)};
    }

    template <TypeId id>
    static Value From(ReprType<id>::T vl) {
        return Value{.value = ValueT<id>(std::move(vl))};
    }

    friend bool operator==(const Value& a, const Value& b) {
        return a.value == b.value;
    }

    TypeId Type() const {
        return std::visit([&]<TypeId id>(const ValueT<id>&) { return id; }, value);
    }
};

template <TypeId id>
struct ColumnT {
    using T = ReprType<id>::T;
    static constexpr TypeId kId = id;

private:
    std::vector<T> m_values;

public:
    ColumnT() {}
    ColumnT(ValueT<id> val) : m_values{val.value} {}

    void Append(T value) {
        m_values.push_back(std::move(value));
    }

    void Append(std::span<const T> sp) {
        for (const auto& value : sp) {
            Append(value);
        }
    }

    void Append(const ColumnT& col) {
        for (const auto& value : col.m_values) {
            Append(value);
        }
    }

    void Reserve(size_t n) {
        m_values.reserve(n);
    }

    void Clear() {
        m_values.clear();
    }

    std::vector<T> ToVector() && {
        return std::move(m_values);
    }
    std::vector<T> ToVector() const {
        return std::vector<T>(m_values.begin(), m_values.end());
    }

    static ColumnT FromVector(std::vector<T> vec) {
        ColumnT col;
        col.m_values = std::move(vec);
        return col;
    }

    const T& operator[](size_t ind) const {
        return m_values[ind];
    }

    size_t Size() const {
        return m_values.size();
    }

    friend bool operator==(const ColumnT& a, const ColumnT& b) {
        return a.m_values == b.m_values;
    }

    ColumnT Filter(std::span<const ReprType<TypeId::Int8>::T> filt) const {
        ENSURE(filt.size() == Size());
        ColumnT col;
        for (size_t i = 0; i < m_values.size(); i++) {
            if (filt[i]) {
                col.Append(m_values[i]);
            }
        }
        return col;
    }

    ColumnT Select(std::span<const size_t> inds) const {
        ColumnT col;
        col.Reserve(inds.size());
        for (size_t i = 0; i < inds.size(); i++) {
            col.Append(m_values[inds[i]]);
        }
        return col;
    }

    void StableArgsort(std::span<size_t> sp, bool reversed = false) const {
        if (!reversed) {
            std::stable_sort(sp.begin(), sp.end(), [&](size_t a, size_t b) { return m_values[a] < m_values[b]; });
        } else {
            std::stable_sort(sp.begin(), sp.end(), [&](size_t a, size_t b) { return m_values[a] > m_values[b]; });
        }
    }
};

template <>
struct ColumnT<TypeId::String> {
    using T = ReprType<TypeId::String>::T;
    static constexpr TypeId kId = TypeId::String;

private:
    std::string m_data;
    std::vector<size_t> m_offsets;

public:
    ColumnT() : m_offsets{0} {}
    ColumnT(ValueT<TypeId::String> val) {
        m_data = std::move(val.value);
        m_offsets = {0, m_data.size()};
    }

    void Append(std::string_view sv) {
        m_data += sv;
        m_offsets.push_back(m_offsets.back() + sv.size());
    }

    void Append(std::span<const T> sp) {
        for (const auto& value : sp) {
            Append(value);
        }
    }

    void Reserve(size_t n) {
        m_offsets.reserve(n + 1);
    }

    void Append(const ColumnT& col) {
        for (size_t i = 0; i < col.Size(); i++) {
            Append(col[i]);
        }
    }

    void Clear() {
        *this = ColumnT();
    }

    std::vector<T> ToVector() const {
        std::vector<std::string> vec(Size());
        for (size_t i = 0; i < Size(); i++) {
            vec[i] = (*this)[i];
        }
        return vec;
    }

    static ColumnT FromVector(const std::vector<T>& vec) {
        ColumnT col;
        col.Reserve(vec.size());
        for (auto& val : vec) {
            col.Append(val);
        }
        return col;
    }

    std::string_view operator[](size_t ind) const {
        size_t l = m_offsets[ind];
        size_t r = m_offsets[ind + 1];
        return std::string_view(m_data).substr(l, r - l);
    }

    size_t Size() const {
        return m_offsets.size() - 1;
    }

    friend bool operator==(const ColumnT& a, const ColumnT& b) {
        return a.m_data == b.m_data && a.m_offsets == b.m_offsets;
    }

    ColumnT Filter(std::span<const ReprType<TypeId::Int8>::T> filt) const {
        ENSURE(filt.size() == Size());
        ColumnT col;
        for (size_t i = 0; i < Size(); i++) {
            if (filt[i]) {
                col.Append((*this)[i]);
            }
        }
        return col;
    }

    ColumnT Select(std::span<const size_t> inds) const {
        ColumnT col;
        col.Reserve(inds.size());
        for (size_t i = 0; i < inds.size(); i++) {
            col.Append((*this)[inds[i]]);
        }
        return col;
    }

    void StableArgsort(std::span<size_t> sp, bool reversed = false) const {
        if (!reversed) {
            std::stable_sort(sp.begin(), sp.end(), [&](size_t a, size_t b) { return (*this)[a] < (*this)[b]; });
        } else {
            std::stable_sort(sp.begin(), sp.end(), [&](size_t a, size_t b) { return (*this)[a] > (*this)[b]; });
        }
    }
};

class Column {
private:
    using ColumnEnum = MakeEnum<ColumnT, AllTypesIds>::T;

    ColumnEnum m_column;

    Column();

    template <typename>
    friend struct ConvertCol;

public:
    Column(TypeId id);
    Column(Value val);

    template <TypeId id>
    Column(ColumnT<id> column) : m_column(std::move(column)) {}

    TypeId Type() const;
    size_t Size() const;

    void Append(Value);
    void Append(const Column&);
    void Reserve(size_t);
    void Clear();

    ColumnEnum& Values();
    const ColumnEnum& Values() const;

    friend bool operator==(const Column& a, const Column& b);

    Column Filter(std::span<const ReprType<TypeId::Int8>::T> filt) const;
    Column Select(std::span<const size_t> inds) const;
    void StableArgsort(std::span<size_t> sp, bool reversed = false) const;
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
    size_t m_num_rows;

public:
    Batch(const std::shared_ptr<const Schema>& schema, std::vector<Column> columns,
          std::optional<size_t> n_rows = std::nullopt);

    std::vector<Column> ExtractColumns() &&;

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
