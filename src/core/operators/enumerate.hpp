#pragma once

#include <cstdint>
#include <memory>

#include "src/core/types.hpp"

namespace curse {

class EnumerateOperator : public Operator {
private:
    std::string m_out_col;
    Value m_start;

    friend class EnumerateOperatorStream;

public:
    EnumerateOperator(std::string out_col, int64_t start = 0);
    EnumerateOperator(std::string out_col, Value start);

    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

};  // namespace curse
