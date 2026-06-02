#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "src/core/aggregators.hpp"
#include "src/core/types.hpp"

namespace curse {

class GroupByOperator : public Operator {
public:
    struct Params {
        AggType tp;
        std::string inp_col;
        std::string out_col;
    };

private:
    std::vector<std::string> m_cols;
    std::vector<Params> m_params;
    std::optional<size_t> m_limit;

    friend class SortOperatorStream;

public:
    GroupByOperator(std::vector<std::string> cls, std::vector<Params> params,
                    std::optional<size_t> limit = std::nullopt);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};
};  // namespace curse
