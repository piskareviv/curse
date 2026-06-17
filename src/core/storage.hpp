#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "src/core/file_io.hpp"
#include "src/core/types.hpp"

namespace curse {

class BatchView {
private:
    std::shared_ptr<ThreadSafeFileReader> m_reader;

    std::shared_ptr<const Schema> m_file_schema;
    std::shared_ptr<const Schema> m_read_schema;
    std::vector<size_t> m_inds;  // indices in file_schema of columns in read_schema

    size_t m_ptr;  // batch offset in the file

    std::vector<char> m_buf_header;
    size_t m_num_rows;
    std::vector<size_t> m_offsets;

    std::vector<std::optional<Column>> m_columns;

    std::vector<char> m_buf;
    std::vector<char> m_buf2;

    struct Secret {};
    friend class CurseReader;

    std::vector<Column> ExtractColumns();

public:
    BatchView(Secret, std::shared_ptr<ThreadSafeFileReader> reader, std::shared_ptr<const Schema> file_schema,
              std::shared_ptr<const Schema> read_schema, std::vector<size_t> inds, size_t ptr,
              std::vector<char> buf_header);

    const Column& GetColumn(size_t ind);
    const Column& GetColumn(std::string_view col_name);

    std::unique_ptr<Batch> ReadAll() &&;
    std::unique_ptr<Batch> ReadSubset(std::span<const ReprType<TypeId::Int8>::T> mask) &&;
    std::unique_ptr<Batch> ReadSubset(std::span<const size_t> inds) &&;

    size_t NRows();
    std::shared_ptr<const Schema> GetSchema();
};

class BatchViewStream {
public:
    virtual std::unique_ptr<BatchView> Next() = 0;
    virtual std::shared_ptr<const Schema> GetSchema() = 0;
    virtual ~BatchViewStream() {}
};

class CurseReader : public BatchViewStream {
private:
    std::shared_ptr<ThreadSafeFileReader> m_reader;
    size_t m_ptr;  // current postion in file
    size_t m_file_size;
    bool m_marker_read = false;

    std::shared_ptr<const Schema> m_file_schema;
    std::shared_ptr<const Schema> m_read_schema;
    std::vector<size_t> m_inds;  // indices in file_schema of columns in read_schema

    friend class BatchView;

public:
    // if read_schema is provided, reads only subset of columns specified by read_schema
    CurseReader(const std::string& file, std::optional<Schema> read_schema = std::nullopt);
    CurseReader(std::unique_ptr<FileReader> reader, std::optional<Schema> read_schema = std::nullopt);

    std::unique_ptr<BatchView> Next();
    std::shared_ptr<const Schema> GetSchema();
};

class SimpleCurseReader : public BatchStream {
private:
    CurseReader m_reader;

public:
    // if read_schema is provided, reads only subset of columns specified by read_schema
    SimpleCurseReader(const std::string& file, std::optional<Schema> read_schema = std::nullopt);
    SimpleCurseReader(std::unique_ptr<FileReader> reader, std::optional<Schema> read_schema = std::nullopt);

    std::unique_ptr<Batch> Next() override;
    std::shared_ptr<const Schema> GetSchema() override;
};

// #######################################################################################################

void WriteAsCurse(const std::string& file, std::unique_ptr<BatchStream> stream);

}  // namespace curse
