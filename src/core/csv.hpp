#pragma once

#include <memory>

#include "src/core/types.hpp"

namespace curse {

class CsvReader : public BatchStream {
private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<const Schema> m_schema;
    bool m_end_reached;

public:
    CsvReader(const std::string& file, const Schema& schema);
    ~CsvReader() override;

    std::shared_ptr<const Schema> GetSchema() override;
    std::unique_ptr<Batch> Next() override;
};

void WriteAsCsv(const std::string& file, std::unique_ptr<BatchStream> stream);

};  // namespace curse
