#include "../queries.hpp"

namespace Q {

curse::Schema SubSchema(const curse::Schema& schema, std::vector<std::string> sub_schema) {
    std::vector<curse::Schema::ColumnInfo> cols;
    for (const std::string& name : sub_schema) {
        cols.push_back(schema.Columns()[schema.IndexOf(name)]);
    }
    return curse::Schema(cols);
}

}  // namespace Q
