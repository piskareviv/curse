#include "count.hpp"

#include "src/core/aggregators.hpp"
#include "src/core/types.hpp"

namespace curse {

CountOperator::CountOperator(std::string out_col) : m_out_col(std::move(out_col)) {}

class CountOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    struct Secret {};
    friend class CountOperator;

    static constexpr TypeId kResultTypeId = AggregatorImpl<AggType::Count, TypeId::Int8>::kResultTypeId;

public:
    CountOperatorStream(Secret, const std::string& out_col, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)) {
        ENSURE(m_stream != nullptr);

        std::vector<Schema::ColumnInfo> cols{
            Schema::ColumnInfo{
                .name = out_col,
                .type = kResultTypeId,
            },
        };
        m_schema = std::make_shared<const Schema>(std::move(cols));
    }

    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }
        size_t count = 0;
        for (std::unique_ptr<Batch> batch = m_stream->Next(); batch; batch = m_stream->Next()) {
            count += batch->NRows();
        }

        std::vector<Column> result = {Column(Value::From<kResultTypeId>(count))};
        m_stream = nullptr;
        return std::make_unique<Batch>(m_schema, result);
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> CountOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<CountOperatorStream>(CountOperatorStream::Secret(), m_out_col, std::move(stream));
}

}  // namespace curse
