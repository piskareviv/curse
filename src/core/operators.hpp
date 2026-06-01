
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
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

}  // namespace curse
