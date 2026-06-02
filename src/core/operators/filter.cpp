#include "filter.hpp"

namespace curse {

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

}  // namespace curse
