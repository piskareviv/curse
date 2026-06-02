#include "aggregation.hpp"

namespace curse {

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

}  // namespace curse
