#include "src/core/operators.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <optional>
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
                       id == TypeId::Int128,
                   "filter only supports integral columns ");
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
            std::vector<Column> cols = batch->ExtractColumns();
            for (size_t i = 0; i < n_cols; i++) {
                std::visit([&]<TypeId id>(ColumnT<id>& col) { col.Append(std::get<ColumnT<id>>(cols[i].Values())); },
                           vec[i].Values());
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
    struct VecValueHasher {
        size_t operator()(const std::vector<Value>& vec) const {
            uint64_t hash = 0;
            for (size_t i = 0; i < vec.size(); i++) {
                uint64_t h = ValueHasher()(vec[i]);
                hash += i * i * i * 123 + h * i * 456 + h * h * 789;
            }
            return hash;
        }
    };

public:
    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }

        std::unordered_map<std::vector<Value>, std::vector<Aggregator>, VecValueHasher> map;

        for (std::unique_ptr<Batch> batch = m_stream->Next(); batch; batch = m_stream->Next()) {
            const std::vector<Column>& cols = batch->Columns();
            size_t n_rows = batch->NRows();

            for (size_t r = 0; r < n_rows; r++) {
                auto extract_cols = [&](const std::vector<size_t> inds) {
                    std::vector<Value> res;

                    res.reserve(inds.size());
                    for (size_t i = 0; i < inds.size(); i++) {
                        std::visit(
                            [&]<TypeId id>(const ColumnT<id>& col) {
                                ValueT<id> vl{.value = col[r]};
                                res.emplace_back(std::move(vl));
                            },
                            cols[i].Values());
                    }
                    return res;
                };

                auto proccess = [&](std::vector<Aggregator>& vec) {
                    std::vector<Value> vals = extract_cols(m_aggr_col_inds);
                    for (size_t i = 0; i < m_aggr_col_inds.size(); i++) {
                        vec[i].Update(vals[i]);
                    }
                };

                std::vector<Value> key = extract_cols(m_key_col_inds);

                if (!m_limit.has_value() || map.size() < m_limit.value()) {
                    std::vector<Aggregator>& vec = map[key];
                    if (vec.empty()) {
                        vec.reserve(m_params.size());
                        for (size_t i = 0; i < m_params.size(); i++) {
                            size_t ind = m_aggr_col_inds[i];
                            TypeId id = batch->GetSchema()->Columns()[ind].type;
                            vec.emplace_back(m_params[i].tp, id);
                        }
                    }
                    proccess(vec);
                } else {
                    auto it = map.find(key);
                    if (it != map.end()) {
                        proccess(it->second);
                    }
                }
            }
        }

        size_t n_keys = m_key_col_inds.size();
        size_t n_aggs = m_aggr_col_inds.size();

        std::vector<Column> result;

        result.reserve(n_keys + n_aggs);
        for (size_t i = 0; i < n_keys; i++) {
            result.emplace_back(m_schema->Columns()[i].type);
        }
        for (size_t i = 0; i < n_aggs; i++) {
            result.emplace_back(m_schema->Columns()[n_keys + i].type);
        }

        for (const auto& [key, val] : map) {
            for (size_t i = 0; i < n_keys; i++) {
                result[i].Append(key[i]);
            }
            for (size_t i = 0; i < n_aggs; i++) {
                result[n_keys + i].Append(val[i].Get());
            }
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
