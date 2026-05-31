#pragma once

#include <memory>
#include <ostream>

#include "src/core/constants.hpp"
#include "src/core/types.hpp"

namespace curse {

class CsvReader : public BatchStream {
private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<const Schema> m_schema;
    bool m_end_reached;

    size_t m_target_batch_size;

public:
    CsvReader(std::istream& input, const Schema& schema, size_t target_batch_bytes = kBatchMemory);
    CsvReader(std::unique_ptr<std::istream> input, const Schema& schema, size_t target_batch_bytes = kBatchMemory);
    CsvReader(const std::string& file, const Schema& schema, size_t target_batch_bytes = kBatchMemory);

    ~CsvReader() override;

    std::shared_ptr<const Schema> GetSchema() override;
    std::unique_ptr<Batch> Next() override;
};

void WriteAsCsv(std::ostream& out, std::unique_ptr<BatchStream> stream);
void WriteAsCsv(const std::string& file, std::unique_ptr<BatchStream> stream);

};  // namespace curse
