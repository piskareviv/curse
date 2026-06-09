#include "groupby.hpp"

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <numeric>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "dependencies/gtl/include/gtl/phmap.hpp"
#include "src/core/aggregators.hpp"
#include "src/core/assert.hpp"
#include "src/core/convert.hpp"
#include "src/core/types.hpp"
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
    struct BytesHasher {
        template <size_t sz>
        size_t operator()(const std::array<char, sz>& ar) const {
            const uint64_t k = sizeof(uint64_t);
            static_assert(sz % k == 0);

            uint64_t res = 0;
            const uint64_t n = sz / k;
            for (uint64_t i = 0; i < n; i += 1) {
                uint64_t val = 0;
                memcpy(&val, &ar[i * k], k);
                res = res * 2131231231231231 + val + 123 + val * val * i * i + i * i * i;
            }

            return res;
        }

        size_t operator()(std::span<const char> sp) const {
            const uint64_t k = sizeof(uint64_t);
            ENSURE(sp.size() % k == 0);

            uint64_t res = 0;
            uint64_t n = sp.size() / k;
            for (uint64_t i = 0; i < n; i += 1) {
                uint64_t val = 0;
                memcpy(&val, &sp[i * k], k);
                res = res * 2131231231231231 + val + 123 + val * val * i * i + i * i * i;
            }

            return res;
        }
    };

    template <TypeId id>
    using HashMap = gtl::parallel_flat_hash_map<typename ReprType<id>::T, size_t, MyHasher, std::equal_to<>>;

    template <AggType tp, TypeId id>
    using AggrVec = std::deque<AggregatorImpl<tp, id>>;

    using HashMapEnum = MakeEnum<HashMap, AllTypesIds>::T;
    using AggrVecEnum = MakeEnumAggr<AggrVec, AllAggTypes, AllTypesIds>::T;

    static constexpr size_t kChunkSize = sizeof(uint64_t);
    static constexpr size_t kMaxChunks = 8;

    // use std::array<char> instead of std::vector<char> up to kBytesVector inclusive
    static constexpr size_t kBytesVector = kMaxChunks * kChunkSize;

    template <size_t n_chunks>
    using IdBundleT =
        std::conditional_t<n_chunks <= kMaxChunks, std::array<char, n_chunks * kChunkSize>, std::vector<char>>;

    template <size_t n>
    using Map2T = gtl::parallel_flat_hash_map<IdBundleT<n>, size_t, BytesHasher>;

    template <typename>
    struct Aux;

    template <size_t... inds>
    struct Aux<std::index_sequence<inds...>> {
        using T = std::variant<Map2T<inds>...>;
    };

    using Map2Enum = Aux<std::make_index_sequence<kMaxChunks + 2>>::T;

public:
    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }

        std::vector<Column> result;
        const size_t n_keys = m_key_col_inds.size();
        const size_t n_aggs = m_aggr_col_inds.size();

        const std::shared_ptr<const Schema>& stream_schema = m_stream->GetSchema();

        std::vector<size_t> key_sizes;

        for (size_t ind : m_key_col_inds) {
            TypeId id = stream_schema->Columns()[ind].type;
            ExecFor(id, [&]<TypeId id> {
                if constexpr (ConvertRaw<id>::kHasRawType) {
                    key_sizes.push_back(sizeof(typename ConvertRaw<id>::RawT));
                } else {
                    key_sizes.push_back(sizeof(size_t));
                }
            });
        }
        size_t n_bytes = std::accumulate(key_sizes.begin(), key_sizes.end(), static_cast<size_t>(0));
        size_t n_chunks = (n_bytes + (kChunkSize - 1)) / kChunkSize;

        std::vector<size_t> order(n_keys);
        std::iota(order.begin(), order.end(), static_cast<size_t>(0));
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return key_sizes[a] > key_sizes[b]; });

        std::vector<HashMapEnum> maps1;
        // absl::flat_hash_map<IdBundleT, size_t, VecHasher<size_t>> map2;
        Map2Enum map2;
        std::vector<AggrVecEnum> aggrs;

        size_t aggrs_rows = 0;

        if (n_chunks <= kMaxChunks) {
            StaticFor<kMaxChunks + 1>([&](auto n) {
                if (n_chunks == n) {
                    map2 = Map2T<n>();
                }
            });
        } else {
            map2 = Map2T<kMaxChunks + 1>();
        }

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
            const size_t dlt = 32;
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

            std::vector<std::optional<size_t>> aggrs_inds(n_rows);
            std::vector<char> append_to_result(n_rows, 0);

            auto func = [&]<size_t n> {
                std::vector<IdBundleT<n>> map2_keys(n_rows);

                Map2T<n>& mp2 = std::get<Map2T<n>>(map2);

                if constexpr (n <= kMaxChunks) {
                    for (size_t i = 0; i < n_rows; i++) {
                        map2_keys[i].fill(0);
                    }
                } else {
                    for (size_t i = 0; i < n_rows; i++) {
                        map2_keys[i].resize(n_chunks * kChunkSize, 0);
                    }
                }
                constexpr size_t kBlockSize = 512;
                for (size_t l = 0; l < n_rows; l += kBlockSize) {
                    size_t r = std::min(n_rows, l + kBlockSize);

                    for (size_t i = 0, dlt = 0; i < n_keys; i++) {
                        size_t ind = order[i];
                        std::visit(
                            [&]<TypeId id>(const ColumnT<id>& col) {
                                if constexpr (ConvertRaw<id>::kHasRawType) {
                                    constexpr size_t kBytes = sizeof(typename ConvertRaw<id>::RawT);

                                    for (size_t j = l; j < r; j++) {
                                        typename ConvertRaw<id>::RawT key = ConvertRaw<id>::ToRaw(col[j]);
                                        memcpy(&map2_keys[j][dlt], &key, kBytes);
                                    }

                                    dlt += kBytes;
                                } else {
                                    constexpr size_t kBytes = sizeof(size_t);

                                    HashMap<id>& map = std::get<HashMap<id>>(maps1[ind]);
                                    for (size_t j = l; j < r; j++) {
                                        typename ReprType<id>::T val(col[j]);
                                        auto [it, inserted] = map.insert({std::move(val), map.size()});
                                        size_t key = it->second;
                                        memcpy(&map2_keys[j][dlt], &key, kBytes);
                                    }

                                    dlt += kBytes;
                                }
                            },
                            cols[m_key_col_inds[ind]].Values());
                    }
                }

                for (size_t i = 0; i < n_rows; i++) {
                    if (!m_limit.has_value() || mp2.size() < m_limit.value()) {
                        auto [it, inserted] = mp2.insert({std::move(map2_keys[i]), mp2.size()});
                        if (inserted) {
                            extend_aggrs(mp2.size());
                            append_to_result[i] = 1;
                        }
                        aggrs_inds[i] = it->second;
                    } else {
                        auto it = mp2.find(map2_keys[i]);
                        if (it != mp2.end()) {
                            aggrs_inds[i] = it->second;
                        }
                    }
                }
            };

            if (n_chunks <= kMaxChunks) {
                StaticFor<kMaxChunks + 1>([&](auto n) {
                    if (n_chunks == n) {
                        func.template operator()<n>();
                    }
                });
            } else {
                func.template operator()<kMaxChunks + 1>();
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

        const size_t n_result_rows = std::visit([&](const auto& m) { return m.size(); }, map2);

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

}  // namespace curse
