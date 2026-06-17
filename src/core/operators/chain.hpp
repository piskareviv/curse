#pragma once

#include <memory>

#include "src/core/types.hpp"

namespace curse {

class ChainOperator : public Operator {
private:
    std::vector<std::unique_ptr<Operator>> m_operators;

public:
    ChainOperator(std::vector<std::unique_ptr<Operator>> operators);

    template <typename... T>
    static ChainOperator From(T... args) {
        std::vector<std::unique_ptr<Operator>> operators;
        operators.reserve(sizeof...(T));
        (operators.push_back(std::make_unique<T>(args)), ...);
        return ChainOperator(std::move(operators));
    }

    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

};  // namespace curse
