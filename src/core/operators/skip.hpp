#pragma once

#include <cstddef>
#include <memory>

#include "src/core/types.hpp"

namespace curse {

class SkipOperator : public Operator {
private:
    size_t m_to_skip;

    friend class SkipOperatorStream;

public:
    SkipOperator(size_t n_rows_to_skip);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};
};  // namespace curse
