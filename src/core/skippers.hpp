#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "src/core/operators/transform.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"

namespace curse {

class BatchViewSelector {
public:
    virtual void Process(BatchView &bv, std::span<ReprType<TypeId::Int8>::T> mask) const = 0;
    virtual ~BatchViewSelector() {}
};

class TransformSelector : public BatchViewSelector {
private:
    std::string m_col_name;
    Transform m_trs;

public:
    TransformSelector(std::string col_name, Transform trs);
    void Process(BatchView &bv, std::span<ReprType<TypeId::Int8>::T> mask) const override;
};

class OperatorSelector : public BatchViewSelector {
private:
    std::shared_ptr<const Schema> m_sub_sch;
    std::shared_ptr<Operator> m_op;

public:
    OperatorSelector(std::shared_ptr<const Schema> sub_schema, std::shared_ptr<Operator> op);
    void Process(BatchView &bv, std::span<ReprType<TypeId::Int8>::T> mask) const override;
};

class Skipper {
public:
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchViewStream>) const = 0;
    friend std::unique_ptr<BatchStream> operator>=(std::unique_ptr<BatchViewStream> stream, const Skipper &sk);
    virtual ~Skipper() {}
};

class BasicSkipper : public Skipper {
private:
    std::vector<std::shared_ptr<BatchViewSelector>> m_selectors;

public:
    BasicSkipper(std::vector<std::shared_ptr<BatchViewSelector>> selectors);

    std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchViewStream>) const override;
};

}  // namespace curse
