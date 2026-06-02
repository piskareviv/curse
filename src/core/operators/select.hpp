#pragma once

#include <memory>
#include <string>

#include "src/core/types.hpp"

namespace curse {

class SelectOperator : public Operator {
private:
    std::vector<std::string> m_cols_to_select;

    friend class SelectOperatorStream;

public:
    SelectOperator(std::vector<std::string> cols_to_select);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

};  // namespace curse
