#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/core/aggregators.hpp"
#include "src/core/types.hpp"

namespace curse {

class AggregationOperator : public Operator {
public:
    struct Params {
        AggType tp;
        std::string inp_col;
        std::string out_col;
    };

private:
    std::shared_ptr<const std::vector<Params>> m_params;

    friend class AggregationOperatorStream;

public:
    AggregationOperator(std::vector<Params> params);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

}  // namespace curse
