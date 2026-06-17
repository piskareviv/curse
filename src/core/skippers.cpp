#include "skippers.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>

#include "src/core/storage.hpp"
#include "src/core/types.hpp"
#include "src/util/assert.hpp"

namespace curse {

TransformSelector::TransformSelector(std::string col_name, Transform trs)
    : m_col_name(std::move(col_name)), m_trs(std::move(trs)) {}

void TransformSelector::Process(BatchView& bv, std::vector<char>& mask) const {
    const Column& col = bv.GetColumn(m_col_name);
    Column cl = m_trs.TransformColumn(col);
    ColumnT<TypeId::Int8>& mk = std::get<ColumnT<TypeId::Int8>>(cl.Values());

    ENSURE(mask.size() == mk.Size());
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

        std::vector<char> mask(batch->NRows(), 1);
        for (auto& ptr : m_selectors) {
            ptr->Process(*batch, mask);
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
