#include "select.hpp"

#include <memory>
#include <vector>

#include "src/core/types.hpp"
#include "src/util/assert.hpp"

namespace curse {

SelectOperator::SelectOperator(std::vector<std::string> cols_to_select) : m_cols_to_select(std::move(cols_to_select)) {}

class SelectOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    std::vector<size_t> m_cols_to_select;

    struct Secret {};
    friend class SelectOperator;

public:
    SelectOperatorStream(Secret, const std::vector<std::string>& cols_to_select, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)) {

        ENSURE(m_stream != nullptr);

        const auto& schema = m_stream->GetSchema();

        std::vector<Schema::ColumnInfo> new_cols;

        for (const auto& s : cols_to_select) {
            size_t ind = schema->IndexOf(s);
            m_cols_to_select.push_back(ind);
            new_cols.push_back(schema->Columns()[ind]);
        }

        m_schema = std::make_shared<const Schema>(std::move(new_cols));
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

        std::vector<Column> cols = std::move(*batch).ExtractColumns();

        std::vector<Column> result;
        result.reserve(m_cols_to_select.size());
        for (size_t ind : m_cols_to_select) {
            result.emplace_back(std::move(cols[ind]));
        }

        return std::make_unique<Batch>(m_schema, std::move(result));
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> SelectOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<SelectOperatorStream>(SelectOperatorStream::Secret(), m_cols_to_select, std::move(stream));
}

}  // namespace curse
