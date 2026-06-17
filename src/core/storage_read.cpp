#include "storage.hpp"
//
#include <algorithm>
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

// ##############################

BatchView::BatchView(Secret, std::shared_ptr<ThreadSafeFileReader> reader, std::shared_ptr<const Schema> file_schema,
                     std::shared_ptr<const Schema> read_schema, std::vector<size_t> inds, size_t ptr,
                     std::vector<char> buf_header)
    : m_reader(std::move(reader)),
      m_file_schema(std::move(file_schema)),
      m_read_schema(std::move(read_schema)),
      m_inds(std::move(inds)),
      m_ptr(ptr),
      m_buf_header(std::move(buf_header)) {

    const size_t n_cols = m_file_schema->Columns().size();
    m_num_rows = ValueFromBytes<int>(std::span(m_buf_header).subspan(4, 4));

    m_offsets.resize(n_cols + 1);
    m_offsets[0] = 8 + 4 * n_cols;
    for (size_t i = 0; i < n_cols; i++) {
        m_offsets[i + 1] = ValueFromBytes<int>(std::span(m_buf_header).subspan(8 + i * 4, 4));
    }

    m_columns.resize(m_read_schema->Columns().size());
}

const Column &BatchView::GetColumn(size_t ind) {
    if (!m_columns[ind].has_value()) {
        size_t ind_file = m_inds[ind];
        size_t beg = m_offsets[ind_file];
        size_t end = m_offsets[ind_file + 1];

        m_buf.resize(end - beg);
        m_reader->ReadBytes(m_ptr + beg, m_buf);
        DecompressLZ4(m_buf, m_buf2);
        m_columns[ind] = ConvertCol<Column>::FromBytes(m_read_schema->Columns()[ind].type, m_buf2);
    }
    return m_columns[ind].value();
}

const Column &BatchView::GetColumn(std::string_view col_name) {
    return GetColumn(m_read_schema->IndexOf(col_name));
}

std::vector<Column> BatchView::ExtractColumns() {
    std::vector<Column> columns;
    columns.reserve(m_columns.size());
    for (size_t i = 0; i < m_columns.size(); i++) {
        GetColumn(i);
        columns.push_back(std::move(m_columns[i].value()));
        m_columns[i] = std::nullopt;
    }
    return columns;
}

std::unique_ptr<Batch> BatchView::ReadAll() && {
    return std::make_unique<Batch>(m_read_schema, ExtractColumns(), m_num_rows);
}

std::unique_ptr<Batch> EmptyBatch(std::shared_ptr<const Schema> sch) {
    const size_t n_cols = sch->Columns().size();
    std::vector<Column> columns;
    columns.reserve(n_cols);
    for (size_t i = 0; i < n_cols; i++) {
        columns.emplace_back(sch->Columns()[i].type);
    }
    return std::make_unique<Batch>(std::move(sch), std::move(columns), 0);
}

std::unique_ptr<Batch> BatchView::ReadSubset(std::span<char> mask) && {
    ENSURE(mask.size() == NRows());

    size_t cnt = mask.size() - std::count(mask.begin(), mask.end(), 0);
    if (cnt == 0) {
        return EmptyBatch(m_read_schema);
    }
    std::vector<Column> columns = ExtractColumns();
    for (size_t i = 0; i < columns.size(); i++) {
        columns[i] = columns[i].Filter(mask);
    }
    return std::make_unique<Batch>(m_read_schema, std::move(columns), cnt);
}
std::unique_ptr<Batch> BatchView::ReadSubset(std::span<size_t> inds) && {
    if (inds.empty()) {
        return EmptyBatch(m_read_schema);
    }
    std::vector<Column> columns = ExtractColumns();
    for (size_t i = 0; i < columns.size(); i++) {
        columns[i] = columns[i].Select(inds);
    }
    return std::make_unique<Batch>(m_read_schema, std::move(columns), inds.size());
}

size_t BatchView::NRows() {
    return m_num_rows;
}
std::shared_ptr<const Schema> BatchView::GetSchema() {
    return m_read_schema;
}

// ##############

CurseReader::CurseReader(std::unique_ptr<FileReader> reader, std::optional<Schema> read_schema)
    : m_reader(std::make_shared<ThreadSafeFileReader>(std::move(reader))), m_ptr(0) {

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

CurseReader::CurseReader(const std::string &file, std::optional<Schema> read_schema)
    : CurseReader(std::make_unique<IfstreamReader>(file), read_schema) {}

std::shared_ptr<const Schema> CurseReader::GetSchema() {
    return m_read_schema;
}

std::unique_ptr<BatchView> CurseReader::Next() {
    if (m_marker_read && m_ptr == m_file_size) {
        return nullptr;
    }
    if (m_ptr + kFormatMarker.size() == m_file_size) {
        ReadMarker(*m_reader, m_ptr);
        m_marker_read = true;
        return nullptr;
    }

    const size_t n_cols = m_file_schema->Columns().size();
    std::vector<char> buf_header(8 + 4 * n_cols);
    m_reader->ReadBytes(m_ptr, buf_header);
    const size_t batch_size_bytes = ValueFromBytes<int>(std::span(buf_header).subspan(0, 4));

    std::unique_ptr<BatchView> result = std::make_unique<BatchView>(
        BatchView::Secret(), m_reader, m_file_schema, m_read_schema, m_inds, m_ptr, std::move(buf_header));
    m_ptr += batch_size_bytes;
    return result;
}

// ##############################################################

SimpleCurseReader::SimpleCurseReader(std::unique_ptr<FileReader> reader, std::optional<Schema> read_schema)
    : m_reader(std::move(reader), read_schema) {}

SimpleCurseReader::SimpleCurseReader(const std::string &file, std::optional<Schema> read_schema)
    : SimpleCurseReader(std::make_unique<IfstreamReader>(file), read_schema) {}

std::shared_ptr<const Schema> SimpleCurseReader::GetSchema() {
    return m_reader.GetSchema();
}

std::unique_ptr<Batch> SimpleCurseReader::Next() {
    std::unique_ptr<BatchView> bv = m_reader.Next();
    if (!bv) {
        return nullptr;
    } else {
        return std::move(*bv).ReadAll();
    }
}

}  // namespace curse
