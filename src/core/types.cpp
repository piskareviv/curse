#include "src/core/types.hpp"

#include <variant>

namespace curse {

Column::Column() {}

Column::Column(TypeId id) {
    ExecFor(id, [&]<TypeId id> { m_column = ColumnT<id>(); });
}

TypeId Column::Type() const {
    return std::visit([&]<TypeId id>(const ColumnT<id>&) { return id; }, m_column);
}
size_t Column::Size() const {
    return std::visit([&](const auto& col) { return col.values.size(); }, m_column);
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

Batch::Batch(const std::shared_ptr<const Schema>& schema, std::vector<Column> columns)
    : m_schema(schema), m_columns(std::move(columns)) {
    ENSURE(schema->Columns().size() == m_columns.size());
    for (size_t i = 0; i < m_columns.size(); i++) {
        ENSURE(m_columns[i].Size() == m_columns[0].Size());
    }
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
