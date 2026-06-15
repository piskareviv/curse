#include "storage.hpp"
//
#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include "src/core/constants.hpp"
#include "src/core/convert.hpp"
#include "src/core/file_io.hpp"
#include "src/core/types.hpp"
#include "src/util/assert.hpp"
#include "src/util/lz4.hpp"
#include "src/util/util.hpp"

namespace curse {

void ReadMarker(FileReader &reader, size_t &ptr) {
    std::array<char, kFormatMarker.size()> buf;
    reader.ReadBytes(ptr, buf);
    ENSURE_MSG(buf == kFormatMarker, "format marker not present");
    ptr += kFormatMarker.size();
}

// reads schema from header and skips it
std::shared_ptr<const Schema> ReadSchema(FileReader &reader, size_t &ptr) {
    size_t header_size = [&] {
        std::array<char, 4> buf;
        reader.ReadBytes(ptr, buf);
        return ValueFromBytes<int>(buf);
    }();

    std::vector<char> buf(header_size - 4);
    reader.ReadBytes(ptr + 4, buf);
    std::span<const char> header = buf;

    size_t n_cols = ValueFromBytes<int>(header.subspan(0, 4));
    std::vector<int> types = VecFromBytes<int>(header.subspan(4, n_cols * 4));
    size_t names_size = ValueFromBytes<int>(header.subspan(4 + n_cols * 4, 4));
    ColumnT<TypeId::String> col =
        ConvertCol<ColumnT<TypeId::String>>::FromBytes(header.subspan(4 + n_cols * 4 + 4, names_size));

    std::vector<Schema::ColumnInfo> columns(n_cols);
    for (size_t i = 0; i < n_cols; i++) {
        columns[i] = Schema::ColumnInfo{.name = std::string(col[i]), .type = static_cast<TypeId>(types[i])};
    }

    ptr += header_size;
    return std::make_shared<Schema>(std::move(columns));
}

SimpleCurseReader::SimpleCurseReader(std::unique_ptr<FileReader> reader, std::optional<Schema> read_schema)
    : m_reader(std::move(reader)), m_ptr(0) {

    m_file_size = m_reader->GetSize();

    ReadMarker(*m_reader, m_ptr);
    m_file_schema = ReadSchema(*m_reader, m_ptr);

    if (read_schema.has_value()) {
        m_read_schema = std::make_shared<Schema>(std::move(read_schema.value()));
    } else {
        m_read_schema = m_file_schema;
    }

    m_inds.resize(m_read_schema->Columns().size());
    for (size_t i = 0; i < m_inds.size(); i++) {
        size_t ind = m_file_schema->IndexOf(m_read_schema->Columns()[i].name);
        ENSURE_MSG(m_file_schema->Columns()[ind].type == m_read_schema->Columns()[i].type, "invalid file");
        m_inds[i] = ind;
    }
}

SimpleCurseReader::SimpleCurseReader(const std::string &file, std::optional<Schema> read_schema)
    : SimpleCurseReader(std::make_unique<IfstreamReader>(file), read_schema) {}

std::shared_ptr<const Schema> SimpleCurseReader::GetSchema() {
    return m_read_schema;
}

std::unique_ptr<Batch> SimpleCurseReader::Next() {
    if (m_marker_read && m_ptr == m_file_size) {
        return nullptr;
    }
    if (m_ptr + kFormatMarker.size() == m_file_size) {
        ReadMarker(*m_reader, m_ptr);
        m_marker_read = true;
        return nullptr;
    }

    const size_t n_cols = m_file_schema->Columns().size();

    const size_t batch_size_bytes = [&] {
        std::array<char, 4> ar;
        m_reader->ReadBytes(m_ptr, ar);
        return ValueFromBytes<int>(ar);
    }();
    const size_t n_rows = [&] {
        std::array<char, 4> ar;
        m_reader->ReadBytes(m_ptr + 4, ar);
        return ValueFromBytes<int>(ar);
    }();

    std::vector<char> buf_offsets(4 * n_cols);
    m_reader->ReadBytes(m_ptr + 8, buf_offsets);

    std::vector<size_t> offsets(n_cols + 1);
    offsets[0] = 8 + 4 * n_cols;
    for (size_t i = 0; i < n_cols; i++) {
        offsets[i + 1] = ValueFromBytes<int>(std::span(buf_offsets).subspan(i * 4, 4));
    }

    std::vector<Column> columns;

    std::vector<char> buf;
    std::vector<char> buf2;

    for (size_t i = 0; i < m_inds.size(); i++) {
        size_t ind = m_inds[i];
        size_t beg = offsets[ind];
        size_t end = offsets[ind + 1];

        buf.resize(end - beg);

        m_reader->ReadBytes(m_ptr + beg, buf);
        DecompressLZ4(buf, buf2);

        columns.emplace_back(ConvertCol<Column>::FromBytes(m_read_schema->Columns()[i].type, buf2));
    }

    m_ptr += batch_size_bytes;
    return std::make_unique<Batch>(m_read_schema, std::move(columns), n_rows);
}

}  // namespace curse
