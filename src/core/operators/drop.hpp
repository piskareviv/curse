#pragma once

#include <memory>
#include <string>

#include "src/core/types.hpp"

namespace curse {

class DropOperator : public Operator {
private:
    std::vector<std::string> m_cols_to_drop;

    friend class DropOperatorStream;

public:
    DropOperator(std::vector<std::string> cols_to_drop);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

};  // namespace curse
