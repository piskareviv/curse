#pragma once

#include <memory>

#include "src/core/types.hpp"

namespace curse {

class CountOperator : public Operator {
private:
    std::string m_out_col;

    friend class CountOperatorStream;

public:
    CountOperator(std::string out_col);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

}  // namespace curse
