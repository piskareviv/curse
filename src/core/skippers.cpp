#include "skippers.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>
#include <vector>

#include "src/core/operators/enumerate.hpp"
#include "src/core/storage.hpp"
#include "src/core/streams.hpp"
#include "src/core/types.hpp"
#include "src/util/assert.hpp"

namespace curse {

TransformSelector::TransformSelector(std::string col_name, Transform trs)
    : m_col_name(std::move(col_name)), m_trs(std::move(trs)) {}

void TransformSelector::Process(BatchView& bv, std::span<ReprType<TypeId::Int8>::T> mask) const {
    const Column& col = bv.GetColumn(m_col_name);
    Column cl = m_trs.TransformColumn(col);
    ColumnT<TypeId::Int8>& mk = std::get<ColumnT<TypeId::Int8>>(cl.Values());

    ENSURE(mask.size() == mk.Size());
    for (size_t i = 0; i < mask.size(); i++) {
        mask[i] &= mk[i];
    }
}

OperatorSelector::OperatorSelector(std::shared_ptr<const Schema> sub_schema, std::shared_ptr<Operator> op)
    : m_sub_sch(std::move(sub_schema)), m_op(std::move(op)) {}

void OperatorSelector::Process(BatchView& bv, std::span<ReprType<TypeId::Int8>::T> mask) const {
    const size_t n_rows = bv.NRows();

    std::vector<Column> cols;
    cols.reserve(m_sub_sch->Columns().size());
    for (size_t i = 0; i < m_sub_sch->Columns().size(); i++) {
        cols.push_back(bv.GetColumn(m_sub_sch->Columns()[i].name));
    }
    std::unique_ptr<Batch> batch = std::make_unique<Batch>(m_sub_sch, std::move(cols), n_rows);
    std::unique_ptr<BatchStream> stream = MakeSingletonStream(std::move(batch));

    const std::string marker = "_curse_ind";

    std::unique_ptr<Batch> batch_trs = ReadAll(std::move(stream) >= EnumerateOperator(marker) >= *m_op);
    const ColumnT<TypeId::Int64>& inds =
        std::get<ColumnT<TypeId::Int64>>(batch_trs->Columns()[batch_trs->GetSchema()->IndexOf(marker)].Values());

    std::vector<ReprType<TypeId::Int8>::T> mk(n_rows);
    for (size_t i = 0; i < inds.Size(); i++) {
        mk[inds[i]] = 1;
    }

    ENSURE(mask.size() == mk.size());
    for (size_t i = 0; i < mask.size(); i++) {
        mask[i] &= mk[i];
    }
}

std::unique_ptr<BatchStream> operator>=(std::unique_ptr<BatchViewStream> stream, const Skipper& sk) {
    return sk.Transform(std::move(stream));
}

BasicSkipper::BasicSkipper(std::vector<std::shared_ptr<BatchViewSelector>> selectors)
    : m_selectors(std::move(selectors)) {}

class BasicSkipperStream : public BatchStream {
private:
    std::unique_ptr<BatchViewStream> m_stream;
    std::vector<std::shared_ptr<BatchViewSelector>> m_selectors;

    struct Secret {};
    friend class BasicSkipper;

public:
    BasicSkipperStream(Secret, std::vector<std::shared_ptr<BatchViewSelector>> selectors,
                       std::unique_ptr<BatchViewStream> stream)
        : m_stream(std::move(stream)), m_selectors(std::move(selectors)) {}

    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }
        std::unique_ptr<BatchView> batch = m_stream->Next();
        if (!batch) {
            m_stream = nullptr;
            return nullptr;
        }
        if (m_selectors.empty()) {
            return std::move(*batch).ReadAll();
        }

        std::vector<ReprType<TypeId::Int8>::T> mask(batch->NRows(), 1);
        for (auto& ptr : m_selectors) {
            ptr->Process(*batch, mask);

            if (static_cast<size_t>(std::count(mask.begin(), mask.end(), 0)) == mask.size()) {
                return std::move(*batch).ReadSubset(std::vector<size_t>());
            }
        }
        return std::move(*batch).ReadSubset(mask);
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_stream->GetSchema();
    }
};

std::unique_ptr<BatchStream> BasicSkipper::Transform(std::unique_ptr<BatchViewStream> stream) const {
    return std::make_unique<BasicSkipperStream>(BasicSkipperStream::Secret(), m_selectors, std::move(stream));
}

}  // namespace curse
