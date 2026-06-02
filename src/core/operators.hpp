#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "src/core/aggregators.hpp"
#include "src/core/types.hpp"

namespace curse {

// Streams single batch
class SingletonStream : BatchStream {
private:
    std::shared_ptr<const Schema> m_schema;
    std::unique_ptr<Batch> m_batch;

public:
    SingletonStream(std::unique_ptr<Batch> batch);

    std::unique_ptr<Batch> Next() override;
    std::shared_ptr<const Schema> GetSchema() override;
};

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

class FilterOperator : public Operator {
private:
    std::string m_col;

    friend class FilterOperatorStream;

public:
    FilterOperator(std::string col);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

class SkipOperator : public Operator {
private:
    size_t m_to_skip;

    friend class SkipOperatorStream;

public:
    SkipOperator(size_t n_rows_to_skip);
    virtual std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

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

namespace impl {

class FuncOperator : public Operator {
private:
    std::function<std::unique_ptr<Batch>(std::unique_ptr<Batch>, const std::shared_ptr<const Schema>&)>
        m_transform_batch;
    std::function<Schema(const Schema&)> m_transform_schema;

    FuncOperator(std::function<std::unique_ptr<Batch>(std::unique_ptr<Batch>, const std::shared_ptr<const Schema>&)>
                     transform_batch,
                 std::function<Schema(const Schema&)> transform_schema);

    friend FuncOperator MakeOperator(
        std::function<std::unique_ptr<Batch>(std::unique_ptr<Batch>, const std::shared_ptr<const Schema>&)>
            transform_batch,
        std::function<Schema(const Schema&)> transform_schema);

public:
    std::unique_ptr<BatchStream> Transform(std::unique_ptr<BatchStream>) const override;
};

// to make the constructor private
FuncOperator MakeOperator(
    std::function<std::unique_ptr<Batch>(std::unique_ptr<Batch>, const std::shared_ptr<const Schema>&)> transform_batch,
    std::function<Schema(const Schema&)> transform_schema);

}  // namespace impl

impl::FuncOperator MakeDropOperator(std::vector<std::string> col_to_drop);
impl::FuncOperator MakeSelectOperator(std::vector<std::string> cols_to_select);

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
    std::function<Column(std::span<const Column*>)> m_transform;
    std::function<TypeId(std::span<TypeId>)> m_result_type;
    std::vector<std::string> m_input_cols;
    std::string m_output_col;

    ColumnOperation(std::function<Column(std::span<const Column*>)> transform,
                    std::function<TypeId(std::span<TypeId>)> result_type, std::vector<std::string> input_cols,
                    std::string output_col);

    friend impl::FuncOperator MakeColumnTransformOperator(std::vector<ColumnOperation> ops);

public:
    ColumnOperation(Transform trs, std::string inp_col, std::string out_col);

    static ColumnOperation LogicalAnd(std::string col1, std::string col2, std::string out_col);
    static ColumnOperation LogicalOr(std::string col1, std::string col2, std::string out_col);

    // dst[i] = col[i] ? col1[i] : col2[i]
    static ColumnOperation Select(std::string mask_col, std::string col1, std::string col2, std::string out_col);
};

impl::FuncOperator MakeColumnTransformOperator(std::vector<ColumnOperation> ops);

}  // namespace curse
