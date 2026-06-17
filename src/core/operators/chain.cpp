#include "chain.hpp"

#include <memory>

#include "src/core/types.hpp"

namespace curse {

ChainOperator::ChainOperator(std::vector<std::unique_ptr<Operator>> operators) : m_operators(std::move(operators)) {}

std::unique_ptr<BatchStream> ChainOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    for (const std::unique_ptr<Operator>& op : m_operators) {
        stream = std::move(stream) >= *op;
    }
    return stream;
}

};  // namespace curse
