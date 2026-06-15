#include "skip.hpp"

#include <numeric>

namespace curse {

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

        std::vector<size_t> inds(n_rows - to_skip);
        std::iota(inds.begin(), inds.end(), static_cast<size_t>(to_skip));

        std::vector<Column> result;
        for (size_t i = 0; i < n_cols; i++) {
            std::visit([&]<TypeId id>(const ColumnT<id>& col) { result.emplace_back(col.Select(inds)); },
                       batch->Columns()[i].Values());
        }

        return std::make_unique<Batch>(m_schema, std::move(result), inds.size());
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> SkipOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<SkipOperatorStream>(SkipOperatorStream::Secret(), m_to_skip, std::move(stream));
}

}  // namespace curse
