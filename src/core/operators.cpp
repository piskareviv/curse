#include "src/core/operators.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <optional>
#include <regex>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "src/core/assert.hpp"
#include "src/core/types.hpp"

namespace curse {

SingletonStream::SingletonStream(std::unique_ptr<Batch> batch) : m_batch(std::move(batch)) {
    ENSURE(m_batch);
    m_schema = m_batch->GetSchema();
}

std::unique_ptr<Batch> SingletonStream::Next() {
    return std::exchange(m_batch, nullptr);
}

std::shared_ptr<const Schema> SingletonStream::GetSchema() {
    return m_schema;
}

// #########################################################################################

AggregationOperator::AggregationOperator(std::vector<Params> params)
    : m_params(std::make_shared<const std::vector<Params>>(std::move(params))) {}

class AggregationOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    std::shared_ptr<const std::vector<AggregationOperator::Params>> m_params;

    std::vector<Aggregator> m_aggs;
    std::vector<size_t> m_col_inds;

    struct Secret {};
    friend class AggregationOperator;

public:
    AggregationOperatorStream(Secret, std::shared_ptr<const std::vector<AggregationOperator::Params>> params,
                              std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_params(params) {

        ENSURE(m_stream != nullptr);

        std::vector<Schema::ColumnInfo> cols(params->size());

        m_aggs.reserve(params->size());
        m_col_inds.reserve(params->size());

        for (size_t i = 0; i < params->size(); i++) {
            auto schema = m_stream->GetSchema();
            size_t ind = schema->IndexOf((*params)[i].inp_col);
            TypeId inp_type = schema->Columns()[ind].type;

            m_col_inds.push_back(ind);
            m_aggs.emplace_back((*params)[i].tp, inp_type);
            cols[i] = Schema::ColumnInfo{.name = (*params)[i].out_col, .type = m_aggs[i].OutputType()};
        }

        m_schema = std::make_shared<const Schema>(std::move(cols));
    }

    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }

        for (std::unique_ptr<Batch> batch = m_stream->Next(); batch; batch = m_stream->Next()) {
            for (size_t i = 0; i < m_params->size(); i++) {
                m_aggs[i].Update(batch->Columns()[m_col_inds[i]]);
            }
        }

        std::vector<Column> result;
        for (size_t i = 0; i < m_params->size(); i++) {
            Value v = m_aggs[i].Get();
            result.emplace_back(v);
        }
        m_stream = nullptr;

        return std::make_unique<Batch>(m_schema, result);
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> AggregationOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<AggregationOperatorStream>(AggregationOperatorStream::Secret(), m_params,
                                                       std::move(stream));
}

// #########################################################################################

FilterOperator::FilterOperator(std::string col) : m_col(std::move(col)) {}

class FilterOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    std::string m_filt_col;
    size_t m_col_ind;

    struct Secret {};
    friend class FilterOperator;

public:
    FilterOperatorStream(Secret, std::string col, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_filt_col(std::move(col)), m_col_ind(0) {

        ENSURE(m_stream != nullptr);
        m_schema = m_stream->GetSchema();
        m_col_ind = m_schema->IndexOf(m_filt_col);
        TypeId id = m_schema->Columns()[m_col_ind].type;

        ENSURE_MSG(id == TypeId::Int8 || id == TypeId::Int16 || id == TypeId::Int32 || id == TypeId::Int64 ||
                       id == TypeId::Int128 || id == TypeId::String,
                   "filter only supports integral and text columns ");
    }

    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }
        std::unique_ptr<Batch> batch = m_stream->Next();
        if (!batch) {
            m_stream = nullptr;
            return nullptr;
        }

        size_t n_cols = m_schema->Columns().size();
        size_t n_rows = batch->NRows();

        std::vector<char> filt(n_rows);
        std::visit(
            [&]<TypeId id>(const ColumnT<id>& col) {
                if constexpr (IsIntegral(id)) {
                    for (size_t i = 0; i < n_rows; i++) {
                        filt[i] = col.values[i] != 0;
                    }
                } else if constexpr (id == TypeId::String) {
                    for (size_t i = 0; i < n_rows; i++) {
                        filt[i] = col.values[i].size() != 0;
                    }
                } else {
                    ENSURE("something went wrong");
                }
            },
            batch->Columns()[m_col_ind].Values());

        std::vector<Column> result;
        for (size_t i = 0; i < n_cols; i++) {
            std::visit([&]<TypeId id>(const ColumnT<id>& col) { result.emplace_back(col.Filter(filt)); },
                       batch->Columns()[i].Values());
        }

        return std::make_unique<Batch>(m_schema, std::move(result));
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> FilterOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<FilterOperatorStream>(FilterOperatorStream::Secret(), m_col, std::move(stream));
}

// #########################################################################################

SkipOperator::SkipOperator(size_t n_rows_to_skip) : m_to_skip(n_rows_to_skip) {}

class SkipOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    size_t m_rows_left;

    struct Secret {};
    friend class SkipOperator;

public:
    SkipOperatorStream(Secret, size_t n_rows, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_rows_left(n_rows) {
        ENSURE(m_stream != nullptr);
        m_schema = m_stream->GetSchema();
    }

    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }

        std::unique_ptr<Batch> batch = m_stream->Next();
        if (!batch) {
            m_stream = nullptr;
            return nullptr;
        }

        size_t n_cols = m_schema->Columns().size();
        size_t n_rows = batch->NRows();

        size_t to_skip = std::min(n_rows, m_rows_left);
        m_rows_left -= to_skip;

        std::vector<char> filt(n_rows, 1);
        std::fill(filt.begin(), filt.begin() + to_skip, 0);

        std::vector<Column> result;
        for (size_t i = 0; i < n_cols; i++) {
            std::visit([&]<TypeId id>(const ColumnT<id>& col) { result.emplace_back(col.Filter(filt)); },
                       batch->Columns()[i].Values());
        }

        return std::make_unique<Batch>(m_schema, std::move(result));
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> SkipOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<SkipOperatorStream>(SkipOperatorStream::Secret(), m_to_skip, std::move(stream));
}

// #########################################################################################

SortOperator::SortOperator(std::vector<SortOperator::Params> params, std::optional<size_t> limit)
    : m_params(std::move(params)), m_limit(limit) {}

class SortOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    std::vector<SortOperator::Params> m_params;
    std::optional<size_t> m_limit;

    std::vector<size_t> m_col_inds;

    struct Secret {};
    friend class SortOperator;

public:
    SortOperatorStream(Secret, std::vector<SortOperator::Params> params, std::optional<size_t> limit,
                       std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_params(std::move(params)), m_limit(limit) {

        ENSURE(m_stream != nullptr);
        m_schema = m_stream->GetSchema();

        m_col_inds.reserve(m_params.size());
        for (size_t i = 0; i < m_params.size(); i++) {
            m_col_inds.push_back({m_schema->IndexOf(m_params[i].inp_col)});
        }
    }

private:
    void SortColumns(std::vector<Column>& vec) {
        if (vec.empty()) {
            return;
        }

        std::vector<size_t> inds(vec[0].Size());
        std::iota(inds.begin(), inds.end(), static_cast<size_t>(0));

        for (size_t i = 0; i < m_params.size(); i++) {
            size_t ind = m_col_inds.rbegin()[i];
            const auto& [col_name, rev] = m_params.rbegin()[i];
            std::visit([&]<TypeId id>(const ColumnT<id>& col) { col.StableArgsort(inds, rev); }, vec[ind].Values());
        }

        if (m_limit.has_value()) {
            inds.resize(std::min(inds.size(), m_limit.value()));
        }

        for (size_t i = 0; i < vec.size(); i++) {
            std::visit([&]<TypeId id>(ColumnT<id>& col) { col = col.Select(inds); }, vec[i].Values());
        }
    }

public:
    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }

        size_t n_cols = m_schema->Columns().size();

        std::vector<Column> vec;
        vec.reserve(n_cols);
        for (size_t i = 0; i < n_cols; i++) {
            vec.emplace_back(m_schema->Columns()[i].type);
        }

        for (std::unique_ptr<Batch> batch = m_stream->Next(); batch; batch = m_stream->Next()) {
            size_t n_rows = batch->NRows();
            std::vector<Column> cols = batch->ExtractColumns();

            // hadle big batches
            if (m_limit.has_value()) {
                size_t stride = std::max<size_t>(4096, m_limit.value());  // !!!! MAGIC CONSTANT
                for (size_t i = 0; i < n_rows;) {
                    size_t dlt = std::min(stride, n_rows - i);

                    for (size_t j = 0; j < n_cols; j++) {
                        std::visit(
                            [&]<TypeId id>(ColumnT<id>& col) {
                                auto& cl = std::get<ColumnT<id>>(cols[j].Values());
                                col.Append(std::span(cl.values).subspan(i, dlt));
                            },
                            vec[j].Values());
                    }

                    SortColumns(vec);

                    i += dlt;
                }
            } else {
                for (size_t i = 0; i < n_cols; i++) {
                    std::visit(
                        [&]<TypeId id>(ColumnT<id>& col) { col.Append(std::get<ColumnT<id>>(cols[i].Values())); },
                        vec[i].Values());
                }
            }
            if (!vec.empty() && m_limit.has_value() && vec[0].Size() > m_limit.value() * 2) {
                SortColumns(vec);
            }
        }

        SortColumns(vec);

        m_stream = nullptr;
        return std::make_unique<Batch>(m_schema, std::move(vec));
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> SortOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<SortOperatorStream>(SortOperatorStream::Secret(), m_params, m_limit, std::move(stream));
}

// #########################################################################################

GroupByOperator::GroupByOperator(std::vector<std::string> cols, std::vector<GroupByOperator::Params> params,
                                 std::optional<size_t> limit)
    : m_cols(std::move(cols)), m_params(std::move(params)), m_limit(limit) {}

class GroupByOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    std::vector<std::string> m_cols;
    std::vector<GroupByOperator::Params> m_params;
    std::optional<size_t> m_limit;

    std::vector<size_t> m_key_col_inds;
    std::vector<size_t> m_aggr_col_inds;

    struct Secret {};
    friend class GroupByOperator;

public:
    GroupByOperatorStream(Secret, std::vector<std::string> cols, std::vector<GroupByOperator::Params> params,
                          std::optional<size_t> limit, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_cols(std::move(cols)), m_params(std::move(params)), m_limit(limit) {

        ENSURE(m_stream != nullptr);

        m_key_col_inds.reserve(m_cols.size());
        for (size_t i = 0; i < m_cols.size(); i++) {
            m_key_col_inds.push_back(m_stream->GetSchema()->IndexOf(m_cols[i]));
        }

        m_aggr_col_inds.reserve(m_params.size());
        for (size_t i = 0; i < m_params.size(); i++) {
            m_aggr_col_inds.push_back(m_stream->GetSchema()->IndexOf(m_params[i].inp_col));
        }

        std::vector<Schema::ColumnInfo> vec;
        vec.reserve(m_key_col_inds.size() + m_aggr_col_inds.size());

        for (size_t i = 0; i < m_key_col_inds.size(); i++) {
            vec.push_back(m_stream->GetSchema()->Columns()[m_key_col_inds[i]]);
        }
        for (size_t i = 0; i < m_aggr_col_inds.size(); i++) {
            size_t ind = m_aggr_col_inds[i];
            TypeId id = m_stream->GetSchema()->Columns()[ind].type;
            Aggregator agg(m_params[i].tp, id);

            vec.push_back({.name = m_params[i].out_col, .type = agg.OutputType()});
        }

        m_schema = std::make_shared<const Schema>(std::move(vec));
    }

private:
    template <typename T, typename Hasher = std::hash<T>>
    struct VecHasher {
        size_t operator()(const std::vector<T>& vec) const {
            uint64_t hash = 0;
            for (size_t i = 0; i < vec.size(); i++) {
                uint64_t h = Hasher()(vec[i]);
                hash += i * i * i * 123 + h * i * 456 + h * h * 789 + hash * 1234567;
            }
            return hash;
        }
    };

    using VecValueHasher = VecHasher<Value, ValueHasher>;

    template <TypeId id>
    using HashMap = std::unordered_map<typename ReprType<id>::T, size_t, MyHasher>;

    template <AggType tp, TypeId id>
    using AggrVec = std::deque<AggregatorImpl<tp, id>>;

    struct Shenanigans {

        template <typename>
        struct Aux1;

        template <TypeId... ids>
        struct Aux1<TypeIdHolder<ids...>> {
            using T = std::variant<HashMap<ids>...>;
        };

        using HashMapEnum = Aux1<AllTypesIds>::T;

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
            using TpIdHolder = TypeIdHolder<ids...>;

            template <AggType tp>
            using Aux = CartProd<AggTypeHolder<tp>, TpIdHolder>::T;

        public:
            using T = AuxPairHolderConcatMany<Aux<tps>...>::T;
        };

        template <typename>
        struct Aux2;

        template <std::pair<AggType, TypeId>... pairs>
        struct Aux2<AuxPairHolder<pairs...>> {
            using T = std::variant<AggrVec<pairs.first, pairs.second>...>;
        };

        using AggrVecEnum = Aux2<CartProd<AllAggTypes, AllTypesIds>::T>::T;
    };

    using HashMapEnum = Shenanigans::HashMapEnum;
    using AggrVecEnum = Shenanigans::AggrVecEnum;

public:
    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }

        const std::shared_ptr<const Schema>& stream_schema = m_stream->GetSchema();

        const size_t n_keys = m_key_col_inds.size();
        const size_t n_aggs = m_aggr_col_inds.size();

        std::vector<HashMapEnum> maps1;
        std::unordered_map<std::vector<size_t>, size_t, VecHasher<size_t>> map2;

        std::vector<Column> result;

        std::vector<AggrVecEnum> aggrs;
        size_t aggrs_rows = 0;

        result.reserve(n_keys + n_aggs);
        for (size_t i = 0; i < n_keys; i++) {
            result.emplace_back(m_schema->Columns()[i].type);
        }

        for (size_t ind : m_key_col_inds) {
            ExecFor(stream_schema->Columns()[ind].type, [&]<TypeId id> { maps1.push_back(HashMap<id>()); });
        }
        for (size_t i = 0; i < n_aggs; i++) {
            size_t ind = m_aggr_col_inds[i];
            ExecFor(stream_schema->Columns()[ind].type, [&]<TypeId id> {
                ExecFor(m_params[i].tp, [&]<AggType tp> { aggrs.push_back(AggrVec<tp, id>()); });
            });
        }

        auto extend_aggrs = [&](size_t lower_bound) {
            const size_t dlt = 8;
            while (aggrs_rows < lower_bound) {
                aggrs_rows += dlt;

                for (size_t i = 0; i < n_aggs; i++) {
                    size_t ind = m_aggr_col_inds[i];
                    ExecFor(stream_schema->Columns()[ind].type, [&]<TypeId id> {
                        ExecFor(m_params[i].tp, [&]<AggType tp> {
                            AggrVec<tp, id>& aggr = std::get<AggrVec<tp, id>>(aggrs[i]);
                            for (size_t j = 0; j < dlt; j++) {
                                aggr.push_back(AggregatorImpl<tp, id>());
                            }
                        });
                    });
                }
            }
        };

        for (std::unique_ptr<Batch> batch = m_stream->Next(); batch; batch = m_stream->Next()) {
            const std::vector<Column>& cols = batch->Columns();
            const size_t n_rows = batch->NRows();

            std::vector<std::vector<size_t>> map2_keys(n_rows);
            for (size_t i = 0; i < n_rows; i++) {
                map2_keys[i].reserve(n_keys);
            }

            for (size_t i = 0; i < n_keys; i++) {
                std::visit(
                    [&]<TypeId id>(const ColumnT<id>& col) {
                        HashMap<id>& map = std::get<HashMap<id>>(maps1[i]);
                        for (size_t j = 0; j < n_rows; j++) {
                            auto& val = col[j];
                            auto [it, inserted] = map.insert({val, map.size()});
                            // if (inserted) {
                            //     map.values.Append(val);
                            // }
                            map2_keys[j].push_back(it->second);
                        }
                    },
                    cols[m_key_col_inds[i]].Values());
            }

            std::vector<std::optional<size_t>> aggrs_inds(n_rows);
            std::vector<char> append_to_result(n_rows, 0);
            for (size_t i = 0; i < n_rows; i++) {
                if (!m_limit.has_value() || map2.size() < m_limit.value()) {
                    auto [it, inserted] = map2.insert({std::move(map2_keys[i]), map2.size()});
                    if (inserted) {
                        extend_aggrs(map2.size());
                        append_to_result[i] = 1;
                    }
                    aggrs_inds[i] = it->second;
                } else {
                    auto it = map2.find(map2_keys[i]);
                    if (it != map2.end()) {
                        aggrs_inds[i] = it->second;
                    }
                }
            }

            for (size_t i = 0; i < n_aggs; i++) {
                std::visit(
                    [&]<AggType tp, TypeId id>(AggrVec<tp, id>& aggr) {
                        const ColumnT<id>& col = std::get<ColumnT<id>>(cols[m_aggr_col_inds[i]].Values());
                        for (size_t j = 0; j < n_rows; j++) {
                            auto ind = aggrs_inds[j];
                            if (ind.has_value()) {
                                aggr[ind.value()].Update(col[j]);
                            }
                        }
                    },
                    aggrs[i]);
            }
            for (size_t i = 0; i < n_keys; i++) {
                std::visit(
                    [&]<TypeId id>(const ColumnT<id>& col) {
                        ColumnT<id>& res_col = std::get<ColumnT<id>>(result[i].Values());
                        for (size_t j = 0; j < n_rows; j++) {
                            if (append_to_result[j]) {
                                res_col.Append(col[j]);
                            }
                        }
                    },
                    cols[m_key_col_inds[i]].Values());
            }
        }

        const size_t n_result_rows = map2.size();

        for (size_t i = 0; i < n_aggs; i++) {
            std::visit(
                [&]<AggType tp, TypeId id>(AggrVec<tp, id>& aggr) {
                    ColumnT<AggregatorImpl<tp, id>::kResultTypeId> col;
                    for (size_t j = 0; j < n_result_rows; j++) {
                        col.Append(aggr[j].Get());
                    }
                    result.emplace_back(std::move(col));
                },
                aggrs[i]);
        }

        m_stream = nullptr;
        return std::make_unique<Batch>(m_schema, std::move(result));
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> GroupByOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<GroupByOperatorStream>(GroupByOperatorStream::Secret(), m_cols, m_params, m_limit,
                                                   std::move(stream));
}

// ###################################################

namespace impl {

FuncOperator::FuncOperator(
    std::function<std::unique_ptr<Batch>(std::unique_ptr<Batch>, const std::shared_ptr<const Schema>&)> transform_batch,
    std::function<Schema(const Schema&)> transform_schema)
    : m_transform_batch(std::move(transform_batch)), m_transform_schema(std::move(transform_schema)) {}

class FuncOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    std::function<std::unique_ptr<Batch>(std::unique_ptr<Batch>, const std::shared_ptr<const Schema>&)>
        m_transform_batch;
    std::function<Schema(const Schema&)> m_transform_schema;

    struct Secret {};
    friend class FuncOperator;

public:
    FuncOperatorStream(
        Secret,
        std::function<std::unique_ptr<Batch>(std::unique_ptr<Batch>, const std::shared_ptr<const Schema>&)>
            transform_batch,
        std::function<Schema(const Schema&)> transform_schema, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_transform_batch(std::move(transform_batch)) {

        ENSURE(m_stream != nullptr);
        m_schema = std::make_shared<const Schema>(transform_schema(*m_stream->GetSchema()));
    }

    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }
        std::unique_ptr<Batch> batch = m_stream->Next();
        if (!batch) {
            m_stream = nullptr;
            return nullptr;
        }
        return m_transform_batch(std::move(batch), m_schema);
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> FuncOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<FuncOperatorStream>(FuncOperatorStream::Secret(), m_transform_batch, m_transform_schema,
                                                std::move(stream));
}

FuncOperator MakeOperator(
    std::function<std::unique_ptr<Batch>(std::unique_ptr<Batch>, const std::shared_ptr<const Schema>& out_schema)>
        transform_batch,
    std::function<Schema(const Schema&)> transform_schema) {
    return FuncOperator(transform_batch, transform_schema);
}

}  // namespace impl

impl::FuncOperator MakeDropOperator(std::vector<std::string> col_to_drop) {
    std::sort(col_to_drop.begin(), col_to_drop.end());

    auto bs = [](const std::vector<std::string>& vec, std::string_view sv) {
        return std::binary_search(vec.begin(), vec.end(), sv);
    };

    auto transform_batch = [=](std::unique_ptr<Batch> batch,
                               const std::shared_ptr<const Schema>& out_schema) -> std::unique_ptr<Batch> {
        std::shared_ptr<const Schema> schema = batch->GetSchema();
        std::vector<Column> cols = batch->ExtractColumns();
        std::vector<Column> result;
        for (size_t i = 0; i < cols.size(); i++) {
            if (!bs(col_to_drop, schema->Columns()[i].name)) {
                result.emplace_back(std::move(cols[i]));
            }
        }
        return std::make_unique<Batch>(out_schema, std::move(result));
    };
    auto transform_schema = [=](const Schema& schema) -> Schema {
        std::vector<Schema::ColumnInfo> cols;
        for (const Schema::ColumnInfo& col : schema.Columns()) {
            if (!bs(col_to_drop, col.name)) {
                cols.push_back(col);
            }
        }
        return cols;
    };

    return impl::MakeOperator(transform_batch, transform_schema);
}

impl::FuncOperator MakeSelectOperator(std::vector<std::string> cols_to_select) {
    ENSURE_MSG(std::unordered_set(cols_to_select.begin(), cols_to_select.end()).size() == cols_to_select.size(),
               "column names must be distinct");

    auto transform_batch = [=](std::unique_ptr<Batch> batch,
                               const std::shared_ptr<const Schema>& out_schema) -> std::unique_ptr<Batch> {
        std::shared_ptr<const Schema> schema = batch->GetSchema();

        std::vector<Column> columns = batch->ExtractColumns();
        std::vector<Column> result;

        for (size_t i = 0; i < cols_to_select.size(); i++) {
            size_t ind = schema->IndexOf(cols_to_select[i]);
            result.emplace_back(std::move(columns[ind]));
        }

        return std::make_unique<Batch>(out_schema, std::move(result));
    };
    auto transform_schema = [=](const Schema& schema) -> Schema {
        std::vector<Schema::ColumnInfo> cols;
        for (const std::string& name : cols_to_select) {
            cols.push_back(schema.Columns()[schema.IndexOf(name)]);
        }
        return cols;
    };

    return impl::MakeOperator(transform_batch, transform_schema);
}

Transform::Transform(std::function<Column(const Column&)> transform, std::function<TypeId(TypeId)> result_type)
    : m_transform(std::move(transform)), m_result_type(std::move(result_type)) {}

Transform Transform::Constant(Value value) {
    return Transform(
        [=](const Column& col) {
            return std::visit(
                [&]<TypeId id>(const ValueT<id>& val) {
                    size_t sz = col.Size();
                    const auto& vl = val.value;

                    ColumnT<id> res;
                    res.values.assign(sz, vl);
                    return Column(std::move(res));
                },
                value.value);
        },
        [=](TypeId) { return value.Type(); });
}

Transform Transform::LogicalNot() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::Int8>& cl = std::get<ColumnT<TypeId::Int8>>(col.Values());
            ColumnT<TypeId::Int8> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                res.Append(!cl[i]);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int8; });
}

Transform Transform::Strlen() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::Int64> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                res.Append(cl[i].size());
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int64; });
}

Transform Transform::Compare(ComparisonType how, Value value) {
    return Transform(
        [=](const Column& col) {
            ColumnT<TypeId::Int8> res;
            std::visit(
                [&]<TypeId id>(const ValueT<id>& val) -> void {
                    const auto& vl = val.value;

                    const ColumnT<id>& cl = std::get<ColumnT<id>>(col.Values());
                    size_t sz = cl.Size();

                    res.values.resize(sz);
                    switch (how) {
                        case ComparisonType::Equal:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] == vl;
                            }
                            break;
                        case ComparisonType::NotEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] != vl;
                            }
                            break;
                        case ComparisonType::GreaterThan:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] > vl;
                            }
                            break;
                        case ComparisonType::GreaterThanOrEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] >= vl;
                            }
                            break;
                        case ComparisonType::LessThan:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] < vl;
                            }
                            break;
                        case ComparisonType::LessThanOrEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] <= vl;
                            }
                            break;
                    }
                },
                value.value);
            return res;
        },
        [=](TypeId) { return TypeId::Int8; });
}

Transform Transform::RegexpSearch(std::string pattern) {
    const std::regex re(pattern, std::regex::optimize);

    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::Int8> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                bool found = std::regex_search(cl[i], re);
                res.Append(found);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int8; });
}

Transform Transform::RegexpReplace(std::string pattern, std::string format) {
    const std::regex re(pattern, std::regex::optimize);

    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::String> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::string s = std::regex_replace(cl[i], re, format);
                res.Append(s);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::String; });
}

Transform Transform::ExtractMinute() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::Timestamp>& cl = std::get<ColumnT<TypeId::Timestamp>>(col.Values());
            ColumnT<TypeId::Int64> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::chrono::system_clock::time_point ts = cl.values[i];
                auto minutes = std::chrono::duration_cast<std::chrono::minutes>(ts.time_since_epoch()).count();
                res.Append(minutes % 60);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int64; });
}

Transform Transform::TruncateToMinutes() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::Timestamp>& cl = std::get<ColumnT<TypeId::Timestamp>>(col.Values());
            ColumnT<TypeId::Timestamp> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::chrono::system_clock::time_point ts = cl.values[i];
                std::chrono::system_clock::time_point tr = std::chrono::floor<std::chrono::minutes>(ts);
                res.Append(tr);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Timestamp; });
}

ColumnOperation::ColumnOperation(std::function<Column(std::span<const Column*>)> transform,
                                 std::function<TypeId(std::span<TypeId>)> result_type,
                                 std::vector<std::string> input_cols, std::string output_col)
    : m_transform(std::move(transform)),
      m_result_type(std::move(result_type)),
      m_input_cols{std::move(input_cols)},
      m_output_col(std::move(output_col)) {}

ColumnOperation::ColumnOperation(Transform trs, std::string inp_col, std::string out_col)
    : m_transform([t = std::move(trs.m_transform)](std::span<const Column*> vec) {
          ENSURE(vec.size() == 1);
          return t(*vec[0]);
      }),
      m_result_type([rt = std::move(trs.m_result_type)](std::span<TypeId> sp) {
          ENSURE(sp.size() == 1);
          return rt(sp[0]);
      }),
      m_input_cols{std::move(inp_col)},
      m_output_col(std::move(out_col)) {}

ColumnOperation ColumnOperation::LogicalAnd(std::string col1, std::string col2, std::string out_col) {
    std::function<Column(std::span<const Column*>)> trs = [](std::span<const Column*> sp) -> Column {
        ENSURE(sp.size() == 2);
        const Column& col1 = *sp[0];
        const Column& col2 = *sp[1];
        const ColumnT<TypeId::Int8>& cl1 = std::get<ColumnT<TypeId::Int8>>(col1.Values());
        const ColumnT<TypeId::Int8>& cl2 = std::get<ColumnT<TypeId::Int8>>(col2.Values());

        size_t sz = col1.Size();
        ENSURE(col2.Size() == sz);

        ColumnT<TypeId::Int8> col;
        col.values.reserve(sz);
        for (size_t i = 0; i < sz; i++) {
            col.Append(cl1[i] && cl2[i]);
        }
        return Column(std::move(col));
    };
    auto result_type = [](std::span<TypeId>) { return TypeId::Int8; };
    return ColumnOperation(std::move(trs), result_type, {col1, col2}, out_col);
}

ColumnOperation ColumnOperation::LogicalOr(std::string col1, std::string col2, std::string out_col) {
    std::function<Column(std::span<const Column*>)> trs = [](std::span<const Column*> sp) -> Column {
        ENSURE(sp.size() == 2);
        const Column& col1 = *sp[0];
        const Column& col2 = *sp[1];
        const ColumnT<TypeId::Int8>& cl1 = std::get<ColumnT<TypeId::Int8>>(col1.Values());
        const ColumnT<TypeId::Int8>& cl2 = std::get<ColumnT<TypeId::Int8>>(col2.Values());

        size_t sz = col1.Size();
        ENSURE(col2.Size() == sz);

        ColumnT<TypeId::Int8> col;
        col.values.reserve(sz);
        for (size_t i = 0; i < sz; i++) {
            col.Append(cl1[i] || cl2[i]);
        }
        return Column(std::move(col));
    };
    auto result_type = [](std::span<TypeId>) { return TypeId::Int8; };
    return ColumnOperation(std::move(trs), result_type, {col1, col2}, out_col);
}

ColumnOperation ColumnOperation::Select(std::string mask_col, std::string col1, std::string col2, std::string out_col) {
    std::function<Column(std::span<const Column*>)> trs = [](std::span<const Column*> sp) -> Column {
        ENSURE(sp.size() == 3);
        const Column& col1 = *sp[0];
        const Column& col2 = *sp[1];
        const Column& col3 = *sp[2];

        size_t sz = col1.Size();
        const ColumnT<TypeId::Int8>& mask = std::get<ColumnT<TypeId::Int8>>(col1.Values());

        return std::visit(
            [&]<TypeId id>(const ColumnT<id>& cl2) -> Column {
                ColumnT<id> cl3 = std::get<ColumnT<id>>(col3.Values());
                ENSURE(cl2.Size() == sz && cl3.Size() == sz);

                ColumnT<id> result;
                result.values.reserve(sz);
                for (size_t i = 0; i < sz; i++) {
                    result.Append(mask[i] ? cl2[i] : cl3[i]);
                }
                return Column(std::move(result));
            },
            col2.Values());
    };
    auto result_type = [](std::span<TypeId> sp) {
        ENSURE(sp.size() == 3);
        return sp[1];
    };
    return ColumnOperation(std::move(trs), result_type, {mask_col, col1, col2}, out_col);
}

impl::FuncOperator MakeColumnTransformOperator(std::vector<ColumnOperation> ops) {

    auto transform_batch = [=](std::unique_ptr<Batch> batch,
                               const std::shared_ptr<const Schema>& out_schema) -> std::unique_ptr<Batch> {
        std::shared_ptr<const Schema> schema = batch->GetSchema();

        std::vector<Column> columns = batch->ExtractColumns();

        std::vector<const Column*> ptrs;
        for (const ColumnOperation& op : ops) {
            ptrs.clear();
            for (auto& name : op.m_input_cols) {
                ptrs.push_back(&columns[out_schema->IndexOf(name)]);
            }
            columns.emplace_back(op.m_transform(ptrs));
        }

        return std::make_unique<Batch>(out_schema, std::move(columns));
    };
    auto transform_schema = [=](const Schema& schema) -> Schema {
        std::vector<Schema::ColumnInfo> cols = schema.Columns();
        for (const ColumnOperation& op : ops) {
            std::vector<TypeId> ids;
            for (auto& name : op.m_input_cols) {
                size_t ind = std::find_if(cols.begin(), cols.end(), [&](const auto& ci) { return ci.name == name; }) -
                             cols.begin();
                ids.push_back(cols[ind].type);
            }
            cols.push_back({.name = op.m_output_col, .type = op.m_result_type(ids)});
        }
        return Schema(std::move(cols));
    };

    return impl::MakeOperator(transform_batch, transform_schema);
}

}  // namespace curse
