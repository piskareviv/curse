#include "storage.hpp"
//

#include <cstddef>
#include <future>
#include <limits>
#include <memory>
#include <vector>

#include "src/core/constants.hpp"
#include "src/core/convert.hpp"
#include "src/core/file_io.hpp"
#include "src/core/types.hpp"
#include "src/util/assert.hpp"
#include "src/util/lz4.hpp"
#include "src/util/thread_pool.hpp"
#include "src/util/util.hpp"

namespace curse {

std::vector<char> SizeToFourBytes(size_t sz) {
    ENSURE_MSG(sz <= std::numeric_limits<int>::max(), "invalid conversion");
    return ValueToBytes<int>(sz);
}

void WriteSchema(FileWriter &writer, const Schema &schema) {
    size_t n_cols = schema.Columns().size();

    ColumnT<TypeId::String> names;
    std::vector<int> types;

    types.resize(n_cols);

    for (size_t i = 0; i < n_cols; i++) {
        names.Append(schema.Columns()[i].name);
        types[i] = static_cast<int>(schema.Columns()[i].type);
        ENSURE_MSG(!names[i].starts_with(kAuxPrefix), "invalid column name");
    }

    std::vector<char> bytes1 = SizeToFourBytes(n_cols);
    std::vector<char> bytes2 = VecToBytes<int>(types);
    std::vector<char> bytes4 = ConvertCol<ColumnT<TypeId::String>>::ToBytes(names);
    std::vector<char> bytes3 = SizeToFourBytes(bytes4.size());
    std::vector<char> bytes0 = SizeToFourBytes(4 + bytes1.size() + bytes2.size() + bytes3.size() + bytes4.size());

    writer.Write(Concat(bytes0, bytes1, bytes2, bytes3, bytes4));
}

void WriteBatch(FileWriter &writer, std::unique_ptr<Batch> batch) {
    std::vector<Column> cols = std::move(*batch).ExtractColumns();
    size_t n_cols = cols.size();

    std::vector<std::vector<char>> vec(n_cols);
    std::vector<std::future<void>> futures(n_cols);

    for (size_t i = 0; i < n_cols; i++) {
        futures[i] = thread_pool.Push([&, i] {
            CompressLZ4(ConvertCol<Column>::ToBytes(cols[i]), vec[i]);
            cols[i].Clear();
        });
    }
    for (size_t i = 0; i < n_cols; i++) {
        futures[i].wait();
    }

    std::vector<int> offsets(n_cols);
    size_t dlt = 4 + n_cols * 4;
    for (size_t i = 0; i < n_cols; i++) {
        dlt += vec[i].size();
        offsets[i] = dlt;
    }

    writer.Write(Concat(SizeToFourBytes(dlt), VecToBytes(offsets)));
    for (const auto &bytes : vec) {
        writer.Write(bytes);
    }
}

void WriteAsCurse(const std::string &file, std::unique_ptr<BatchStream> stream) {
    OfstreamWriter ofstream_writer(file);
    FileWriter &writer = ofstream_writer;

    writer.Write(kFormatMarker);

    WriteSchema(writer, *stream->GetSchema());
    for (std::unique_ptr<Batch> batch = stream->Next(); batch; batch = stream->Next()) {
        WriteBatch(writer, std::move(batch));
    }

    writer.Write(kFormatMarker);
}

}  // namespace curse
