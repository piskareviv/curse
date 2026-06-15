#include "enumerate.hpp"

#include "src/core/types.hpp"
#include "src/util/assert.hpp"

namespace curse {

constexpr bool ValidTypeForEnumerate(TypeId id) {
    return id == TypeId::Int64 || id == TypeId::Int128;
}

EnumerateOperator::EnumerateOperator(std::string out_col, Value start) : m_out_col(std::move(out_col)), m_start(start) {
    ENSURE_MSG(ValidTypeForEnumerate(start.Type()), "invalid type for enumerate");
}

EnumerateOperator::EnumerateOperator(std::string out_col, int64_t start)
    : EnumerateOperator(std::move(out_col), Value(ValueT<TypeId::Int64>(start))) {}

class EnumerateOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    Value m_cur;

    struct Secret {};
    friend class EnumerateOperator;

public:
    EnumerateOperatorStream(Secret, const std::string& col, Value m_cur, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_cur(m_cur) {
        ENSURE(m_stream != nullptr);
        m_schema = AddColumn(*m_stream->GetSchema(), Schema::ColumnInfo{.name = col, .type = m_cur.Type()});
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

        const size_t n_rows = batch->NRows();

        std::vector<Column> result = std::move(*batch).ExtractColumns();
        std::visit(
            [&]<TypeId id>(ValueT<id>& vl) {
                if constexpr (ValidTypeForEnumerate(id)) {
                    ColumnT<id> col;
                    col.Reserve(n_rows);
                    for (size_t i = 0; i < n_rows; i++) {
                        col.Append(vl.value + i);
                    }
                    result.emplace_back(std::move(col));
                    vl.value += n_rows;
                } else {
                    ENSURE_MSG(false, "something went wrong");
                }
            },
            m_cur.value);

        return std::make_unique<Batch>(m_schema, std::move(result));
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> EnumerateOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<EnumerateOperatorStream>(EnumerateOperatorStream::Secret(), m_out_col, m_start,
                                                     std::move(stream));
}

}  // namespace curse
