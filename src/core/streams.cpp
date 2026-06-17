#include "streams.hpp"

#include <memory>
#include <utility>

#include "src/core/types.hpp"
#include "src/util/assert.hpp"

namespace curse {

class SingletonStream : public BatchStream {
private:
    std::unique_ptr<Batch> m_batch;
    std::shared_ptr<const Schema> m_schema;

public:
    SingletonStream(std::unique_ptr<Batch> batch) : m_batch(std::move(batch)) {
        ENSURE(m_batch != nullptr);
        m_schema = m_batch->GetSchema();
    }

    std::unique_ptr<Batch> Next() {
        return std::exchange(m_batch, nullptr);
    }

    std::shared_ptr<const Schema> GetSchema() {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> MakeSingletonStream(std::unique_ptr<Batch> batch) {
    return std::make_unique<SingletonStream>(std::move(batch));
}

std::unique_ptr<Batch> ReadAll(std::unique_ptr<BatchStream> stream) {
    std::shared_ptr<const Schema> sch = stream->GetSchema();
    size_t total_rows = 0;

    std::vector<Column> cols;
    cols.reserve(sch->Columns().size());
    for (size_t i = 0; i < sch->Columns().size(); i++) {
        cols.emplace_back(sch->Columns()[i].type);
    }

    for (std::unique_ptr<Batch> batch = stream->Next(); batch != nullptr; batch = stream->Next()) {
        total_rows += batch->NRows();
        for (size_t i = 0; i < cols.size(); i++) {
            cols[i].Append(batch->Columns()[i]);
        }
    }

    return std::make_unique<Batch>(std::move(sch), std::move(cols), total_rows);
}

}  // namespace curse
