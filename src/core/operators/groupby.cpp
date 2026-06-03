#include "groupby.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <type_traits>

#include "dependencies/gtl/include/gtl/phmap.hpp"
#include "src/core/util.hpp"

namespace curse {

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
    template <TypeId id>
    using HashMap = gtl::parallel_flat_hash_map<typename ReprType<id>::T, size_t, MyHasher>;

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

        std::vector<Column> result;
        const size_t n_keys = m_key_col_inds.size();
        const size_t n_aggs = m_aggr_col_inds.size();

        // use std::array<size_t, n_keys> instead of std::vector<size_t> up to kNKeysVector inclusive
        constexpr size_t kNKeysVector = 8;

        auto next = [&]<size_t NKeys> -> void {
            const std::shared_ptr<const Schema>& stream_schema = m_stream->GetSchema();

            using IdBundleT = std::conditional_t<NKeys <= kNKeysVector, std::array<size_t, NKeys>, std::vector<size_t>>;

            std::vector<HashMapEnum> maps1;
            gtl::parallel_flat_hash_map<IdBundleT, size_t, VecHasher<size_t>> map2;
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

                std::vector<IdBundleT> map2_keys(n_rows);
                if constexpr (std::is_same_v<IdBundleT, std::vector<size_t>>) {
                    for (size_t i = 0; i < n_rows; i++) {
                        map2_keys[i].resize(n_keys);
                    }
                }

                for (size_t i = 0; i < n_keys; i++) {
                    std::visit(
                        [&]<TypeId id>(const ColumnT<id>& col) {
                            HashMap<id>& map = std::get<HashMap<id>>(maps1[i]);
                            for (size_t j = 0; j < n_rows; j++) {
                                auto& val = col[j];
                                auto [it, inserted] = map.insert({val, map.size()});
                                map2_keys[j][i] = it->second;
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
        };

        StaticFor<kNKeysVector + 2>([&](auto n) {
            if (n_keys == n) {
                next.template operator()<n>();
            }
        });

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

}  // namespace curse
