#include "src/core/csv.hpp"

#include <cstddef>
#include <fstream>
#include <functional>
#include <future>
#include <istream>
#include <memory>
#include <ostream>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include "dependencies/vince/csv.hpp"
#include "src/core/convert.hpp"
#include "src/core/thread_pool.hpp"
#include "src/core/types.hpp"

namespace curse {

static const auto kFormat = csv::CSVFormat().no_header();

struct CsvReader::Impl {
    csv::CSVReader reader;

    Impl(std::istream& input) : reader(input, kFormat) {}
    Impl(std::unique_ptr<std::istream> input) : reader(std::move(input), kFormat) {}
    Impl(const std::string& file) : reader(file, kFormat) {}
};

CsvReader::CsvReader(std::istream& input, const Schema& schema, size_t target_batch_bytes)
    : m_impl(std::make_unique<CsvReader::Impl>(input)),
      m_schema(std::make_shared<Schema>(schema)),
      m_end_reached(false),
      m_target_batch_size(target_batch_bytes) {}

CsvReader::CsvReader(std::unique_ptr<std::istream> input, const Schema& schema, size_t target_batch_bytes)
    : m_impl(std::make_unique<CsvReader::Impl>(std::move(input))),
      m_schema(std::make_shared<Schema>(schema)),
      m_end_reached(false),
      m_target_batch_size(target_batch_bytes) {}

CsvReader::CsvReader(const std::string& file, const Schema& schema, size_t target_batch_bytes)
    : m_impl(std::make_unique<CsvReader::Impl>(file)),
      m_schema(std::make_shared<Schema>(schema)),
      m_end_reached(false),
      m_target_batch_size(target_batch_bytes) {}

std::shared_ptr<const Schema> CsvReader::GetSchema() {
    return m_schema;
}

std::unique_ptr<Batch> CsvReader::Next() {
    if (m_end_reached) {
        return nullptr;
    }

    const size_t n_cols = m_schema->Columns().size();
    std::vector<Column> columns;

    for (size_t i = 0; i < n_cols; i++) {
        columns.push_back(Column(m_schema->Columns()[i].type));
    }

    size_t rows_read = 0;
    size_t total_bytes = 0;

    csv::CSVRow row;
    std::vector<csv::CSVRow> rows;

    const size_t n_funcs = std::thread::hardware_concurrency();

    std::vector<std::function<void()>> funcs(n_funcs);
    std::vector<std::future<void>> futures(n_funcs);

    for (size_t ind = 0; ind < n_funcs; ind++) {
        size_t beg = ind * n_cols / n_funcs;
        size_t end = (ind + 1) * n_cols / n_funcs;

        auto f = [&rows, &columns, beg, end] {
            size_t n_rows = rows.size();

            for (size_t i = beg; i < end; i++) {
                auto visitor = [&rows, i, n_rows]<TypeId id>(ColumnT<id>& col) {
                    for (size_t j = 0; j < n_rows; j++) {
                        const csv::CSVField& field = rows[j][i];
                        std::string_view token = field.get_sv();
                        col.Append(Convert<typename ReprType<id>::T>::FromString(token));
                    }
                };
                std::visit(visitor, columns[i].Values());
            }
        };

        funcs[ind] = f;
    }

    auto flush_rows = [&] {
        for (size_t i = 0; i < n_funcs; i++) {
            futures[i] = thread_pool.Push(funcs[i]);
        }
        for (size_t i = 0; i < n_funcs; i++) {
            futures[i].wait();
        }

        rows.clear();
    };

    size_t current_bytes = 0;

    while (rows_read == 0 || total_bytes < m_target_batch_size) {
        if (!m_impl->reader.read_row(row)) {
            break;
        }
        ENSURE(row.size() == n_cols);

        size_t row_bytes = row.raw_str().size();

        rows_read += 1;
        total_bytes += row_bytes;
        current_bytes += row_bytes;

        rows.push_back(std::move(row));

        if (current_bytes >= 1 * 1024 * 1024) {
            flush_rows();
            current_bytes = 0;
        }
    }

    flush_rows();

    if (rows_read == 0) {
        m_end_reached = true;
        m_impl.reset();  // should be fine
        return nullptr;
    }

    return std::make_unique<Batch>(m_schema, std::move(columns));
}

CsvReader::~CsvReader() {}

void WriteAsCsv(std::ostream& out, std::unique_ptr<BatchStream> stream) {
    auto writer = csv::make_csv_writer(out).set_auto_flush(false);

    const size_t n_cols = stream->GetSchema()->Columns().size();
    std::vector<std::string> vec(n_cols);

    for (std::unique_ptr<Batch> batch = stream->Next(); batch; batch = stream->Next()) {
        size_t n_rows = batch->NRows();
        for (size_t i = 0; i < n_rows; i++) {
            for (size_t j = 0; j < n_cols; j++) {
                vec[j] = std::visit(
                    [&]<TypeId id>(const ColumnT<id>& col) {
                        return Convert<typename ReprType<id>::T>::ToString(col[i]);
                    },
                    batch->Columns()[j].Values());
            }
            writer.write_row(vec);
        }
    }
}
void WriteAsCsv(const std::string& file, std::unique_ptr<BatchStream> stream) {
    std::ofstream fout(file);
    WriteAsCsv(fout, std::move(stream));
}
}  // namespace curse
