#include "src/core/operators.hpp"

#include <chrono>
#include <csetjmp>
#include <cstddef>
#include <memory>
#include <utility>
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

AggregationOperator::AggregationOperator(std::vector<Params> params)
    : m_params(std::make_shared<const std::vector<Params>>(std::move(params))) {}

class AggregationOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const std::vector<AggregationOperator::Params>> m_params;
    std::shared_ptr<const Schema> m_schema;

    std::vector<Aggregator> m_aggs;

    std::vector<size_t> m_input_columns_indices;

    struct Secret {};
    friend class AggregationOperator;

public:
    AggregationOperatorStream(Secret, std::shared_ptr<const std::vector<AggregationOperator::Params>> params,
                              std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_params(params) {

        ENSURE(m_stream != nullptr);

        std::vector<Schema::ColumnInfo> cols(params->size());

        m_aggs.reserve(params->size());
        m_input_columns_indices.reserve(params->size());

        for (size_t i = 0; i < params->size(); i++) {
            auto schema = m_stream->GetSchema();
            size_t ind = schema->IndexOf((*params)[i].inp_col);
            TypeId inp_type = schema->Columns()[ind].type;

            m_input_columns_indices.push_back(ind);
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
                m_aggs[i].Update(batch->Columns()[m_input_columns_indices[i]]);
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

        return std::make_unique<Batch>(m_schema, result);
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> FilterOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<FilterOperatorStream>(FilterOperatorStream::Secret(), m_col, std::move(stream));
}

}  // namespace curse
