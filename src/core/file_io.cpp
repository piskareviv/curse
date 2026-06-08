#include "src/core/file_io.hpp"

#include "src/core/assert.hpp"

namespace curse {

void IfstreamReader::Error() {
    ENSURE_MSG(false, "IO Error: failed to read from file");
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
    ENSURE_MSG(false, "IO Error: failed to write to file");
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

}  // namespace curse
