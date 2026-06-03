#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <variant>

#include "dependencies/gtl/include/gtl/phmap.hpp"
#include "src/core/assert.hpp"
#include "src/core/types.hpp"

namespace curse {

enum class AggType {
    Count,
    Sum,
    Average,
    Min,
    Max,

    CountDistinct,
};

template <AggType...>
struct AggTypeHolder {};

using AllAggTypes =
    AggTypeHolder<AggType::Count, AggType::Sum, AggType::Average, AggType::Min, AggType::Max, AggType::CountDistinct>;

void ExecFor(AggType tp, auto&& func) {
    [&]<AggType... types>(AggTypeHolder<types...>) {
        size_t cnt = 0;
        ((tp == types ? (cnt += 1, func.template operator()<types>(), 0) : 0), ...);
        ENSURE(cnt == 1);
    }.template operator()<>(AllAggTypes());
}

template <AggType tp, TypeId id>
struct AggregatorImpl {
    using T = ReprType<id>::T;
    static constexpr TypeId kResultTypeId = id;
    using ResultT = ReprType<kResultTypeId>::T;

    AggregatorImpl() {
        ENSURE_MSG(false, "something went wrong");
    }

    void Update(const T&) {
        ENSURE_MSG(false, "something went wrong");
    }

    void Update(const ColumnT<id>&) {
        ENSURE_MSG(false, "something went wrong");
    }

    ResultT Get() const {
        ENSURE_MSG(false, "something went wrong");
    }
};

template <TypeId id>
struct AggregatorImpl<AggType::Count, id> {
    using T = ReprType<id>::T;

    static constexpr TypeId kResultTypeId = TypeId::Int64;
    using ResultT = ReprType<kResultTypeId>::T;

    size_t counter;

    AggregatorImpl() : counter(0) {}

    void Update(const T& value) {
        counter += 1;
        (void)value;
    }

    void Update(const ColumnT<id>& col) {
        counter += col.Size();
    }

    ResultT Get() const {
        return static_cast<ResultT>(counter);
    }
};

template <TypeId id>
    requires(id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 || id == TypeId::Int64 ||
             id == TypeId::Int128)
struct AggregatorImpl<AggType::Sum, id> {
    using T = ReprType<id>::T;

    static constexpr TypeId kResultTypeId = TypeId::Int128;
    using ResultT = ReprType<kResultTypeId>::T;

    ResultT sum;

    AggregatorImpl() : sum(0) {}

    void Update(const T& value) {
        sum += value;
    }

    void Update(const ColumnT<id>& col) {
        for (const T& value : col.values) {
            Update(value);
        }
    }

    ResultT Get() const {
        return sum;
    }
};

template <TypeId id>
    requires(id == TypeId::Float64)
struct AggregatorImpl<AggType::Sum, id> {
    using T = ReprType<id>::T;

    static constexpr TypeId kResultTypeId = TypeId::Float64;
    using ResultT = ReprType<kResultTypeId>::T;

    ResultT sum;

    AggregatorImpl() : sum(0) {}

    void Update(const T& value) {
        sum += value;
    }

    void Update(const ColumnT<id>& col) {
        for (const T& value : col.values) {
            Update(value);
        }
    }

    ResultT Get() const {
        return sum;
    }
};

template <TypeId id>
    requires(id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 || id == TypeId::Int64 ||
             id == TypeId::Int128 || id == TypeId::Float64)
struct AggregatorImpl<AggType::Average, id> {
    using T = ReprType<id>::T;

    static constexpr TypeId kResultTypeId = TypeId::Float64;
    using ResultT = ReprType<kResultTypeId>::T;

    AggregatorImpl<AggType::Sum, id> sum;
    AggregatorImpl<AggType::Count, id> count;

    AggregatorImpl() {}

    void Update(const T& value) {
        sum.Update(value);
        count.Update(value);
    }

    void Update(const ColumnT<id>& col) {
        for (const T& value : col.values) {
            Update(value);
        }
    }

    ResultT Get() const {
        return sum.Get() / static_cast<ReprType<TypeId::Float64>::T>(count.Get());
    }
};

template <AggType tp, TypeId id>
    requires(tp == AggType::Min || tp == AggType::Max)
struct AggregatorImpl<tp, id> {
    using T = ReprType<id>::T;
    using CmpTp = std::conditional_t<tp == AggType::Min, std::less<T>, std::greater<T>>;

    static constexpr TypeId kResultTypeId = id;
    using ResultT = ReprType<kResultTypeId>::T;

    std::optional<ResultT> min;

    AggregatorImpl() {}

    void Update(const T& value) {
        if (!min.has_value() || CmpTp()(value, min.value())) {
            min = value;
        }
    }

    void Update(const ColumnT<id>& col) {
        for (const T& value : col.values) {
            Update(value);
        }
    }

    ResultT Get() const {
        ENSURE_MSG(min.has_value(), "min/max of empty set is undefined");
        return min.value();
    }
};

template <TypeId id>
struct AggregatorImpl<AggType::CountDistinct, id> {
    using T = ReprType<id>::T;

    static constexpr TypeId kResultTypeId = TypeId::Int64;
    using ResultT = ReprType<kResultTypeId>::T;

    gtl::flat_hash_set<ValueT<id>, ValueT_Hasher<id>> set;

    AggregatorImpl() {}

    void Update(const T& value) {
        set.insert(ValueT<id>(value));
    }

    void Update(const ColumnT<id>& col) {
        for (const T& value : col.values) {
            Update(value);
        }
    }

    ResultT Get() const {
        return static_cast<ResultT>(set.size());
    }
};

template <AggType tp>
struct AggregatorTp {
    template <typename>
    struct Aux;

    template <TypeId... ids>
    struct Aux<TypeIdHolder<ids...>> {
        using T = std::variant<AggregatorImpl<tp, ids>...>;
    };

    using AggsEnum = Aux<AllTypesIds>::T;

    AggsEnum agg;

    AggregatorTp() {}

    AggregatorTp(TypeId id) {
        ExecFor(id, [&]<TypeId id> { agg = AggregatorImpl<tp, id>(); });
    }

    TypeId Type() const {
        return std::visit([&]<TypeId id>(const AggregatorImpl<tp, id>&) { return id; }, agg);
    }
    TypeId OutputType() const {
        return std::visit(
            [&]<TypeId id>(const AggregatorImpl<tp, id>&) { return AggregatorImpl<tp, id>::kResultTypeId; }, agg);
    }

    void Update(const Value& value) {
        std::visit([&]<TypeId id>(AggregatorImpl<tp, id>& ag) { ag.Update(std::get<ValueT<id>>(value.value).value); },
                   agg);
    }

    void Update(const Column& col) {
        std::visit([&]<TypeId id>(AggregatorImpl<tp, id>& ag) { ag.Update(std::get<ColumnT<id>>(col.Values())); }, agg);
    }

    Value Get() const {
        Value result;
        std::visit(
            [&]<TypeId id>(const AggregatorImpl<tp, id>& ag) -> void {
                constexpr TypeId kResId = AggregatorImpl<tp, id>::kResultTypeId;
                result.value = ValueT<kResId>{.value = ag.Get()};
            },
            agg);
        return result;
    }
};

struct Aggregator {
    template <typename>
    struct Aux;

    template <AggType... tps>
    struct Aux<AggTypeHolder<tps...>> {
        using T = std::variant<AggregatorTp<tps>...>;
    };

    using AggsEnum = Aux<AllAggTypes>::T;

    std::optional<AggsEnum> agg;

    Aggregator() {}

    Aggregator(AggType tp, TypeId id) {
        ExecFor(tp, [&]<AggType tp> { agg = AggregatorTp<tp>(id); });
    }

    AggType AggTp() const {
        return std::visit([&]<AggType tp>(const AggregatorTp<tp>&) { return tp; }, agg.value());
    }
    TypeId OutputType() const {
        return std::visit([&]<AggType tp>(const AggregatorTp<tp>& ag) { return ag.OutputType(); }, agg.value());
    }

    void Update(const Value& value) {
        return std::visit([&]<AggType tp>(AggregatorTp<tp>& ag) { ag.Update(value); }, agg.value());
    }

    void Update(const Column& col) {
        return std::visit([&]<AggType tp>(AggregatorTp<tp>& ag) { ag.Update(col); }, agg.value());
    }

    Value Get() const {
        return std::visit([&]<AggType tp>(const AggregatorTp<tp>& ag) { return ag.Get(); }, agg.value());
    }
};

}  // namespace curse
