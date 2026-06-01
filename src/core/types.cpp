#include "src/core/types.hpp"

#include <memory>
#include <set>
#include <utility>
#include <variant>
#include <vector>

namespace curse {

Column::Column() {}

Column::Column(TypeId id) {
    ExecFor(id, [&]<TypeId id> { m_column = ColumnT<id>(); });
}
Column::Column(Value val) {
    std::visit([&]<TypeId id>(ValueT<id> vl) { m_column = ColumnT<id>{.values = {std::move(vl.value)}}; },
               std::move(val.value));
}

TypeId Column::Type() const {
    return std::visit([&]<TypeId id>(const ColumnT<id>&) { return id; }, m_column);
}
size_t Column::Size() const {
    return std::visit([&](const auto& col) { return col.values.size(); }, m_column);
}

void Column::Append(Value value) {
    std::visit([&]<TypeId id>(ColumnT<id>& col) { col.Append(std::get<ValueT<id>>(value.value).value); }, m_column);
}

Column::ColumnEnum& Column::Values() {
    return m_column;
}
const Column::ColumnEnum& Column::Values() const {
    return m_column;
}

Schema::Schema(std::vector<ColumnInfo> columns) : m_columns(std::move(columns)) {
    std::set<std::string> set;
    for (auto& [name, type] : m_columns) {
        set.insert(name);
    }
    ENSURE_MSG(set.size() == m_columns.size(), "column names must be distinct");
}

const std::vector<Schema::ColumnInfo>& Schema::Columns() const {
    return m_columns;
}

size_t Schema::IndexOf(std::string_view column_name) const {
    for (size_t i = 0; i < m_columns.size(); i++) {
        if (m_columns[i].name == column_name) {
            return i;
        }
    }
    ENSURE_MSG(false, "unknown column name");
}
// TypeId Schema::TypeOf(size_t ind) const {
//     return m_columns[ind].type;
// }
// TypeId Schema::TypeOf(std::string_view column_name) const {
//     return m_columns[IndexOf(column_name)].type;
// }

std::shared_ptr<const Schema> AddColumn(const Schema& schema, const Schema::ColumnInfo& info) {
    std::vector<Schema::ColumnInfo> columns = schema.Columns();
    columns.push_back(info);
    return std::make_shared<const Schema>(std::move(columns));
}
std::shared_ptr<const Schema> AddColumns(const Schema& schema, const std::vector<Schema::ColumnInfo>& cols) {
    std::vector<Schema::ColumnInfo> columns = schema.Columns();
    columns.insert(columns.end(), cols.begin(), cols.end());
    return std::make_shared<const Schema>(std::move(columns));
}

Batch::Batch(const std::shared_ptr<const Schema>& schema, std::vector<Column> columns)
    : m_schema(schema), m_columns(std::move(columns)) {
    ENSURE(schema->Columns().size() == m_columns.size());
    for (size_t i = 0; i < m_columns.size(); i++) {
        ENSURE(m_columns[i].Size() == m_columns[0].Size());
    }
}

std::vector<Column> Batch::ExtractColumns() {
    m_schema = nullptr;
    return std::exchange(m_columns, std::vector<Column>());
}

size_t Batch::NRows() const {
    if (m_columns.empty()) {
        return 0;
    } else {
        return m_columns[0].Size();
    }
}
const std::vector<Column>& Batch::Columns() const {
    return m_columns;
}
std::shared_ptr<const Schema> Batch::GetSchema() const {
    return m_schema;
}

std::unique_ptr<BatchStream> operator>=(std::unique_ptr<BatchStream> stream, const Operator& op) {
    return op.Transform(std::move(stream));
}

}  // namespace curse
