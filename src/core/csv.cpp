#include "src/core/csv.hpp"

#include <cstddef>
#include <fstream>
#include <memory>
#include <string_view>
#include <variant>
#include <vector>

#include "dependencies/vince/csv.hpp"
#include "src/core/convert.hpp"
#include "src/core/types.hpp"
#include "src/core/util.hpp"

namespace curse {

struct CsvReader::Impl {
    csv::CSVReader reader;
    Impl(const std::string& file) : reader(file) {}
};

CsvReader::CsvReader(const std::string& file, const Schema& schema)
    : m_impl(std::make_unique<CsvReader::Impl>(file)),
      m_schema(std::make_shared<Schema>(schema)),
      m_end_reached(false) {}

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
    while (rows_read == 0 || total_bytes < kBatchMemory) {
        if (!m_impl->reader.read_row(row)) {
            break;
        }

        ENSURE(row.size() == n_cols);

        for (size_t i = 0; i < n_cols; i++) {
            const csv::CSVField& field = row[i];
            std::string_view token = field.get_sv();

            std::visit(
                [&]<TypeId id>(ColumnT<id>& col) {
                    col.values.push_back(Convert<typename ReprType<id>::T>::FromString(token));
                },
                columns[i].Values());

            total_bytes += token.size();
        }

        rows_read += 1;
    }

    if (rows_read == 0) {
        m_end_reached = true;
        m_impl.reset();  // should be fine
        return nullptr;
    }

    return std::make_unique<Batch>(m_schema, std::move(columns));
}

CsvReader::~CsvReader() {}

void WriteAsCsv(const std::string& file, std::unique_ptr<BatchStream> stream) {
    std::ofstream fout(file);
    auto writer = csv::make_csv_writer(fout).set_auto_flush(false);

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

}  // namespace curse
