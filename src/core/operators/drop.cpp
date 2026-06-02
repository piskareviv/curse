#include "drop.hpp"

#include <memory>
#include <unordered_set>
#include <vector>

#include "src/core/assert.hpp"
#include "src/core/types.hpp"

namespace curse {

DropOperator::DropOperator(std::vector<std::string> cols_to_drop) : m_cols_to_drop(std::move(cols_to_drop)) {}

class DropOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    std::vector<size_t> m_cols_to_keep;

    struct Secret {};
    friend class DropOperator;

public:
    DropOperatorStream(Secret, const std::vector<std::string>& cols_to_drop, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)) {

        ENSURE(m_stream != nullptr);

        const auto& cols = m_stream->GetSchema()->Columns();

        std::vector<Schema::ColumnInfo> new_cols;
        std::unordered_set<std::string> set(cols_to_drop.begin(), cols_to_drop.end());

        for (size_t i = 0; i < cols.size(); i++) {
            if (!set.erase(cols[i].name)) {
                m_cols_to_keep.push_back(i);
                new_cols.push_back(cols[i]);
            }
        }

        ENSURE_MSG(set.empty(), "can't find some columns");

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

        std::vector<Column> cols = batch->ExtractColumns();

        std::vector<Column> result;
        result.reserve(m_cols_to_keep.size());
        for (size_t ind : m_cols_to_keep) {
            result.emplace_back(std::move(cols[ind]));
        }

        return std::make_unique<Batch>(m_schema, std::move(result));
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> DropOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<DropOperatorStream>(DropOperatorStream::Secret(), m_cols_to_drop, std::move(stream));
}

}  // namespace curse
