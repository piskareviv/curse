#include "lz4.hpp"

#include "dependencies/lz4/lz4.h"
#include "src/util/assert.hpp"
#include "src/util/util.hpp"

namespace curse {

void CompressLZ4(const std::span<const char> span, std::vector<char> &vec) {
    ENSURE(span.size() <= LZ4_MAX_INPUT_SIZE);

    size_t bound = LZ4_compressBound(span.size());
    vec.resize(bound + 4);
    int ret = LZ4_compress_default(span.data(), vec.data() + 4, span.size(), bound);

    ENSURE_MSG(ret > 0, "lz4 compression failed");
    vec.resize(ret + 4);
    ValueToBytes<int>(span.size(), std::span(vec).subspan(0, 4));
}

void DecompressLZ4(const std::span<const char> span, std::vector<char> &vec) {
    ENSURE(span.size() >= 4);
    size_t len = ValueFromBytes<int>(span.subspan(0, 4));
    vec.resize(len);
    int ret = LZ4_decompress_safe(span.data() + 4, vec.data(), span.size() - 4, vec.size());
    ENSURE_MSG(ret >= 0 && static_cast<size_t>(ret) >= len, "lz4 decompression failed");
}

}  // namespace curse
