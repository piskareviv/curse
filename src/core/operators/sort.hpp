#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "src/core/aggregators.hpp"
#include "src/core/types.hpp"

namespace curse {

class SortOperator : public Operator {
public:
    struct Params {
        std::string inp_col;
        bool reversed = false;
    };

private:
    std::vector<Params> m_params;
    std::optional<size_t> m_limit;

    friend class SortOperatorStream;

public:
    SortOperator(std::vector<Params> params, std::optional<size_t> limit = std::nullopt);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};
};  // namespace curse
