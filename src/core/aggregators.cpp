#include "src/core/aggregators.hpp"

namespace curse {

Aggregator::Aggregator() {}

Aggregator::Aggregator(AggType tp, TypeId id) {
    ExecFor(tp, [&]<AggType tp> { ExecFor(id, [&]<TypeId id> { m_agg = AggregatorImpl<tp, id>(); }); });
}

TypeId Aggregator::OutputType() const {
    return std::visit(
        [&]<AggType tp, TypeId id>(const AggregatorImpl<tp, id>&) { return AggregatorImpl<tp, id>::kResultTypeId; },
        m_agg.value());
}

void Aggregator::Update(const Value& value) {
    return std::visit(
        [&]<AggType tp, TypeId id>(AggregatorImpl<tp, id>& ag) {
            ValueT<id> val = std::get<ValueT<id>>(value.value);
            ag.Update(val.value);
        },
        m_agg.value());
}

void Aggregator::Update(const Column& col) {
    return std::visit(
        [&]<AggType tp, TypeId id>(AggregatorImpl<tp, id>& ag) {
            const ColumnT<id>& cl = std::get<ColumnT<id>>(col.Values());
            ag.Update(cl);
        },
        m_agg.value());
}

Value Aggregator::Get() const {
    return std::visit(
        [&]<AggType tp, TypeId id>(const AggregatorImpl<tp, id>& ag) {
            return Value::From(ValueT<AggregatorImpl<tp, id>::kResultTypeId>(ag.Get()));
        },
        m_agg.value());
}

}  // namespace curse
