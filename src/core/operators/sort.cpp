#include "sort.hpp"

#include <cstddef>
#include <numeric>

#include "src/core/types.hpp"

namespace curse {

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
    void SortColumns(std::vector<Column> &vec) {
        if (vec.empty()) {
            return;
        }

        std::vector<size_t> inds(vec[0].Size());
        std::iota(inds.begin(), inds.end(), static_cast<size_t>(0));

        for (size_t i = 0; i < m_params.size(); i++) {
            size_t ind = m_col_inds.rbegin()[i];
            const auto &[col_name, rev] = m_params.rbegin()[i];
            std::visit([&]<TypeId id>(const ColumnT<id> &col) { col.StableArgsort(inds, rev); }, vec[ind].Values());
        }

        if (m_limit.has_value()) {
            inds.resize(std::min(inds.size(), m_limit.value()));
        }

        for (size_t i = 0; i < vec.size(); i++) {
            std::visit([&]<TypeId id>(ColumnT<id> &col) { col = col.Select(inds); }, vec[i].Values());
        }
    }

public:
    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }

        const size_t n_cols = m_schema->Columns().size();

        if (n_cols == 0) {
            size_t count = 0;

            for (std::unique_ptr<Batch> batch = m_stream->Next(); batch; batch = m_stream->Next()) {
                count += batch->NRows();
            }
            if (m_limit.has_value()) {
                count = std::min(count, m_limit.value());
            }

            m_stream = nullptr;
            return std::make_unique<Batch>(m_schema, std::vector<Column>{}, count);
        }

        std::vector<Column> vec;
        vec.reserve(n_cols);
        for (size_t i = 0; i < n_cols; i++) {
            vec.emplace_back(m_schema->Columns()[i].type);
        }

        for (std::unique_ptr<Batch> batch = m_stream->Next(); batch; batch = m_stream->Next()) {
            const size_t n_rows = batch->NRows();
            std::vector<Column> cols = std::move(*batch).ExtractColumns();

            if (m_limit.has_value()) {
                // to handle big batches
                const size_t stride = std::max<size_t>(4096, m_limit.value());

                for (size_t i = 0; i < n_rows;) {
                    size_t dlt = std::min(stride, n_rows - i);

                    for (size_t j = 0; j < n_cols; j++) {
                        std::visit(
                            [&]<TypeId id>(ColumnT<id> &col) {
                                auto &cl = std::get<ColumnT<id>>(cols[j].Values());
                                for (size_t k = 0; k < dlt; k++) {
                                    col.Append(cl[i + k]);
                                }
                            },
                            vec[j].Values());
                    }

                    SortColumns(vec);
                    i += dlt;
                }
            } else {
                for (size_t i = 0; i < n_cols; i++) {
                    std::visit(
                        [&]<TypeId id>(ColumnT<id> &col) { col.Append(std::get<ColumnT<id>>(cols[i].Values())); },
                        vec[i].Values());
                }
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

}  // namespace curse
