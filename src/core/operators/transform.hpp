#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "src/core/types.hpp"

namespace curse {

// column transformer
class Transform {
private:
    std::function<Column(const Column&)> m_transform;
    std::function<TypeId(TypeId)> m_result_type;

    Transform(std::function<Column(const Column&)> transform, std::function<TypeId(TypeId)> result_type);

    friend class ColumnOperation;

public:
    static Transform Constant(Value value);
    static Transform LogicalNot();

    static Transform Strlen();

    enum class ComparisonType {
        Equal,
        NotEqual,
        GreaterThan,
        GreaterThanOrEqual,
        LessThan,
        LessThanOrEqual,
    };

    static Transform Compare(ComparisonType how, Value value);

    static Transform RegexpSearch(std::string pattern);

    static Transform RegexpReplace(std::string pattern, std::string format);

    static Transform ExtractMinute();

    static Transform TruncateToMinutes();
};

class ColumnOperation {
private:
    std::function<Column(std::span<std::reference_wrapper<const Column>>)> m_transform;
    std::function<TypeId(std::span<TypeId>)> m_result_type;
    std::vector<std::string> m_input_cols;
    std::string m_output_col;

    ColumnOperation(std::function<Column(std::span<std::reference_wrapper<const Column>>)> transform,
                    std::function<TypeId(std::span<TypeId>)> result_type, std::vector<std::string> input_cols,
                    std::string output_col);

    friend class TransformOperatorStream;

public:
    ColumnOperation(Transform trs, std::string inp_col, std::string out_col);

    enum class Logical {
        And,
        Or,
    };

    enum class Arithmetic {
        Add,
        Sub,
        Mul,
    };

    static ColumnOperation LogicalOp(std::string col1, std::string col2, std::string out_col, Logical op);

    static ColumnOperation LogicalAnd(std::string col1, std::string col2, std::string out_col);
    static ColumnOperation LogicalOr(std::string col1, std::string col2, std::string out_col);
    static ColumnOperation LogicalNot(std::string col, std::string out_col);

    static ColumnOperation ArithmeticOp(std::string col1, std::string col2, std::string out_col, Arithmetic op,
                                        TypeId out_type);

    // dst[i] = col[i] ? col1[i] : col2[i]
    static ColumnOperation Select(std::string mask_col, std::string col1, std::string col2, std::string out_col);

    static ColumnOperation SetContains(std::vector<std::string> inp_cols, std::string out_col,
                                       std::unique_ptr<BatchStream> stream);
};

class TransformOperator : public Operator {
private:
    std::vector<ColumnOperation> m_ops;

    friend class TransformOperatorStream;

public:
    TransformOperator(std::vector<ColumnOperation> ops);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

};  // namespace curse
