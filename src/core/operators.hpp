
#include <cstddef>
#include <functional>
#include <memory>
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

}  // namespace curse
