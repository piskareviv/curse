#pragma once

#include <memory>
#include <string>

#include "src/core/types.hpp"

namespace curse {

class FilterOperator : public Operator {
private:
    std::string m_col;

    friend class FilterOperatorStream;

public:
    FilterOperator(std::string col);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

};  // namespace curse
