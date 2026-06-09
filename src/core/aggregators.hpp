#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <variant>

#include "absl/container/flat_hash_set.h"
#include "src/core/types.hpp"
#include "src/util/assert.hpp"
#include "src/util/hash.hpp"

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
        ENSURE_MSG(false, "invalid aggregation");
    }

    void Update(const auto&) {
        ENSURE_MSG(false, "invalid aggregation");
    }

    void Update(const ColumnT<id>&) {
        ENSURE_MSG(false, "invalid aggregation");
    }

    ResultT Get() const {
        ENSURE_MSG(false, "invalid aggregation");
    }
};

template <TypeId id>
struct AggregatorImpl<AggType::Count, id> {
    using T = ReprType<id>::T;

    static constexpr TypeId kResultTypeId = TypeId::Int64;
    using ResultT = ReprType<kResultTypeId>::T;

    size_t counter;

    AggregatorImpl() : counter(0) {}

    void Update(const auto& value) {
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
        for (size_t i = 0; i < col.Size(); i++) {
            Update(col[i]);
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
        for (size_t i = 0; i < col.Size(); i++) {
            Update(col[i]);
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
        for (size_t i = 0; i < col.Size(); i++) {
            Update(col[i]);
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
    using CmpTp = std::conditional_t<tp == AggType::Min, std::less<>, std::greater<>>;

    static constexpr TypeId kResultTypeId = id;
    using ResultT = ReprType<kResultTypeId>::T;

    std::optional<ResultT> min;

    AggregatorImpl() {}

    void Update(const auto& value) {
        if (!min.has_value() || CmpTp()(value, min.value())) {
            min = value;
        }
    }

    void Update(const ColumnT<id>& col) {
        for (size_t i = 0; i < col.Size(); i++) {
            Update(col[i]);
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

private:
    struct Hasher {
        size_t operator()(const ValueT<id>& val) const {
            return MyHasher()(val.value);
        }
    };

public:
    absl::flat_hash_set<ValueT<id>, Hasher> set;

    AggregatorImpl() {}

    void Update(const auto& value) {
        set.insert(ValueT<id>(value));
    }

    void Update(const ColumnT<id>& col) {
        for (size_t i = 0; i < col.Size(); i++) {
            Update(col[i]);
        }
    }

    ResultT Get() const {
        return static_cast<ResultT>(set.size());
    }
};

template <template <AggType, TypeId> typename X, typename TpsHolder, typename IdsHolder>
struct MakeEnumAggr {
private:
    struct Shenanigans {
        template <AggType tp, TypeId id>
        struct AuxPair {};

        template <std::pair<AggType, TypeId>... pairs>
        struct AuxPairHolder {};

        template <typename, typename>
        struct AuxPairHolderConcat;

        template <std::pair<AggType, TypeId>... pairs1, std::pair<AggType, TypeId>... pairs2>
        struct AuxPairHolderConcat<AuxPairHolder<pairs1...>, AuxPairHolder<pairs2...>> {
            using T = AuxPairHolder<pairs1..., pairs2...>;
        };

        template <typename... Holders>
        struct AuxPairHolderConcatMany;

        template <>
        struct AuxPairHolderConcatMany<> {
            using T = AuxPairHolder<>;
        };

        template <typename Head, typename... Tail>
        struct AuxPairHolderConcatMany<Head, Tail...> {
            using T = AuxPairHolderConcat<Head, typename AuxPairHolderConcatMany<Tail...>::T>::T;
        };

        template <typename, typename>
        struct CartProd;

        template <AggType tp, TypeId... ids>
        struct CartProd<AggTypeHolder<tp>, TypeIdHolder<ids...>> {
            using T = AuxPairHolder<std::pair(tp, ids)...>;
        };

        template <AggType... tps, TypeId... ids>
        struct CartProd<AggTypeHolder<tps...>, TypeIdHolder<ids...>> {
        private:
            template <AggType tp>
            using Aux = CartProd<AggTypeHolder<tp>, TypeIdHolder<ids...>>::T;

        public:
            using T = AuxPairHolderConcatMany<Aux<tps>...>::T;
        };

        template <typename>
        struct Aux2;

        template <std::pair<AggType, TypeId>... pairs>
        struct Aux2<AuxPairHolder<pairs...>> {
            using T = std::variant<X<pairs.first, pairs.second>...>;
        };

        using Enum = Aux2<typename CartProd<TpsHolder, IdsHolder>::T>::T;
    };

public:
    using T = Shenanigans::Enum;
};

class Aggregator {
private:
    using AggsEnum = MakeEnumAggr<AggregatorImpl, AllAggTypes, AllTypesIds>::T;

    std::optional<AggsEnum> m_agg;

public:
    Aggregator();

    Aggregator(AggType tp, TypeId id);

    TypeId OutputType() const;
    void Update(const Value& value);
    void Update(const Column& col);

    Value Get() const;
};

}  // namespace curse
