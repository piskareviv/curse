#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "src/core/file_io.hpp"
#include "src/core/types.hpp"

namespace curse {

class CurseReader : public BatchStream {
private:
    std::unique_ptr<FileReader> m_reader;
    size_t m_ptr;  // current postion in file
    size_t m_file_size;
    bool m_marker_read = false;

    std::shared_ptr<const Schema> m_file_schema;
    std::shared_ptr<const Schema> m_read_schema;
    std::vector<size_t> m_inds;  // indices in file_schema of columns in read_schema

public:
    // if read_schema is provided, reads only subset of columns specified by read_schema
    CurseReader(const std::string& file, std::optional<const Schema*> read_schema = std::nullopt);
    CurseReader(std::unique_ptr<FileReader> reader, std::optional<const Schema*> read_schema = std::nullopt);

    std::unique_ptr<Batch> Next() override;
    std::shared_ptr<const Schema> GetSchema() override;
};

void WriteAsCurse(const std::string& file, std::unique_ptr<BatchStream> stream);

}  // namespace curse
