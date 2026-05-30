#include "src/core/file_io.hpp"

#include <stdexcept>

#include "src/core/assert.hpp"

namespace curse {

void IfstreamReader::Error() {
    throw std::runtime_error("file read failed");
}

IfstreamReader::IfstreamReader(const std::string& file) : m_file(file, std::ios::binary) {}

void IfstreamReader::ReadBytes(size_t beg, std::span<char> buf) {
    if (!m_file.seekg(beg)) {
        Error();
    }
    if (!m_file.read(buf.data(), buf.size())) {
        Error();
    }
}

size_t IfstreamReader::GetSize() {
    if (!m_file.seekg(0, std::ios::end)) {
        Error();
    }
    using pos_type = decltype(m_file)::pos_type;  // NOLINT
    pos_type end = m_file.tellg();
    if (end == pos_type(-1)) {
        Error();
    }
    return static_cast<size_t>(end);
}

void OfstreamWriter::Error() {
    throw std::runtime_error("file write failed");
}

OfstreamWriter::OfstreamWriter(const std::string& file) : m_file(file, std::ios::binary) {}

void OfstreamWriter::Seek(size_t pos) {
    if (!m_file.seekp(pos)) {
        Error();
    }
}

void OfstreamWriter::Write(std::span<const char> bytes) {
    if (!m_file.write(bytes.data(), bytes.size())) {
        Error();
    }
}

// void OfstreamWriter::WriteAt(size_t pos, std::span<const char> bytes) {
//     Seek(pos);
//     Write(bytes);
// }

BufferedReader::BufferedReader(std::unique_ptr<FileReader> reader, size_t buf_size)
    : m_reader(std::move(reader)), m_beg(0), m_end(0), m_ptr(0), m_file_size(m_reader->GetSize()) {
    m_buf.resize(buf_size);
}

char BufferedReader::Peek() {
    if (m_beg == m_end) {
        size_t dlt = std::min(m_file_size - m_ptr, m_buf.size());
        ENSURE_MSG(dlt > 0, "there are no more bytes in the file");
        m_reader->ReadBytes(dlt, std::span(m_buf).subspan(0, dlt));

        m_beg = 0;
        m_end = dlt;
        m_ptr += dlt;
    }
    return m_buf[m_beg];
}

char BufferedReader::NextChar() {
    Peek();
    return m_buf[m_beg++];
}

bool BufferedReader::IsEnd() {
    return m_beg == m_end && m_ptr == m_file_size;
}

}  // namespace curse
