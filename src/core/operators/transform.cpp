#include "transform.hpp"

#include <functional>
#include <memory>
#include <regex>
#include <unordered_map>
#include <vector>

#include "src/core/assert.hpp"
#include "src/core/types.hpp"

namespace curse {

Transform::Transform(std::function<Column(const Column&)> transform, std::function<TypeId(TypeId)> result_type)
    : m_transform(std::move(transform)), m_result_type(std::move(result_type)) {}

Transform Transform::Constant(Value value) {
    return Transform(
        [=](const Column& col) {
            return std::visit(
                [&]<TypeId id>(const ValueT<id>& val) {
                    size_t sz = col.Size();
                    const auto& vl = val.value;

                    ColumnT<id> res;
                    res.values.assign(sz, vl);
                    return Column(std::move(res));
                },
                value.value);
        },
        [=](TypeId) { return value.Type(); });
}

Transform Transform::LogicalNot() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::Int8>& cl = std::get<ColumnT<TypeId::Int8>>(col.Values());
            ColumnT<TypeId::Int8> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                res.Append(!cl[i]);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int8; });
}

Transform Transform::Strlen() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::Int64> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                res.Append(cl[i].size());
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int64; });
}

Transform Transform::Compare(ComparisonType how, Value value) {
    return Transform(
        [=](const Column& col) {
            ColumnT<TypeId::Int8> res;
            std::visit(
                [&]<TypeId id>(const ValueT<id>& val) -> void {
                    const auto& vl = val.value;

                    const ColumnT<id>& cl = std::get<ColumnT<id>>(col.Values());
                    size_t sz = cl.Size();

                    res.values.resize(sz);
                    switch (how) {
                        case ComparisonType::Equal:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] == vl;
                            }
                            break;
                        case ComparisonType::NotEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] != vl;
                            }
                            break;
                        case ComparisonType::GreaterThan:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] > vl;
                            }
                            break;
                        case ComparisonType::GreaterThanOrEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] >= vl;
                            }
                            break;
                        case ComparisonType::LessThan:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] < vl;
                            }
                            break;
                        case ComparisonType::LessThanOrEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res[i] = cl[i] <= vl;
                            }
                            break;
                    }
                },
                value.value);
            return res;
        },
        [=](TypeId) { return TypeId::Int8; });
}

Transform Transform::RegexpSearch(std::string pattern) {
    const std::regex re(pattern, std::regex::optimize);

    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::Int8> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                bool found = std::regex_search(cl[i], re);
                res.Append(found);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int8; });
}

Transform Transform::RegexpReplace(std::string pattern, std::string format) {
    const std::regex re(pattern, std::regex::optimize);

    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::String> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::string s = std::regex_replace(cl[i], re, format);
                res.Append(s);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::String; });
}

Transform Transform::ExtractMinute() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::Timestamp>& cl = std::get<ColumnT<TypeId::Timestamp>>(col.Values());
            ColumnT<TypeId::Int64> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::chrono::system_clock::time_point ts = cl.values[i];
                auto minutes = std::chrono::duration_cast<std::chrono::minutes>(ts.time_since_epoch()).count();
                res.Append(minutes % 60);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int64; });
}

Transform Transform::TruncateToMinutes() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::Timestamp>& cl = std::get<ColumnT<TypeId::Timestamp>>(col.Values());
            ColumnT<TypeId::Timestamp> res;
            res.values.reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::chrono::system_clock::time_point ts = cl.values[i];
                std::chrono::system_clock::time_point tr = std::chrono::floor<std::chrono::minutes>(ts);
                res.Append(tr);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Timestamp; });
}

ColumnOperation::ColumnOperation(std::function<Column(std::span<std::reference_wrapper<const Column>>)> transform,
                                 std::function<TypeId(std::span<TypeId>)> result_type,
                                 std::vector<std::string> input_cols, std::string output_col)
    : m_transform(std::move(transform)),
      m_result_type(std::move(result_type)),
      m_input_cols{std::move(input_cols)},
      m_output_col(std::move(output_col)) {}

ColumnOperation::ColumnOperation(Transform trs, std::string inp_col, std::string out_col)
    : m_transform([t = std::move(trs.m_transform)](std::span<std::reference_wrapper<const Column>> vec) {
          ENSURE(vec.size() == 1);
          return t(vec[0]);
      }),
      m_result_type([rt = std::move(trs.m_result_type)](std::span<TypeId> sp) {
          ENSURE(sp.size() == 1);
          return rt(sp[0]);
      }),
      m_input_cols{std::move(inp_col)},
      m_output_col(std::move(out_col)) {}

ColumnOperation ColumnOperation::LogicalAnd(std::string col1, std::string col2, std::string out_col) {
    std::function<Column(std::span<std::reference_wrapper<const Column>>)> trs =
        [](std::span<std::reference_wrapper<const Column>> sp) -> Column {
        ENSURE(sp.size() == 2);
        const Column& col1 = sp[0];
        const Column& col2 = sp[1];
        const ColumnT<TypeId::Int8>& cl1 = std::get<ColumnT<TypeId::Int8>>(col1.Values());
        const ColumnT<TypeId::Int8>& cl2 = std::get<ColumnT<TypeId::Int8>>(col2.Values());

        size_t sz = col1.Size();
        ENSURE(col2.Size() == sz);

        ColumnT<TypeId::Int8> col;
        col.values.reserve(sz);
        for (size_t i = 0; i < sz; i++) {
            col.Append(cl1[i] && cl2[i]);
        }
        return Column(std::move(col));
    };
    auto result_type = [](std::span<TypeId>) { return TypeId::Int8; };
    return ColumnOperation(std::move(trs), result_type, {col1, col2}, out_col);
}

ColumnOperation ColumnOperation::LogicalOr(std::string col1, std::string col2, std::string out_col) {
    std::function<Column(std::span<std::reference_wrapper<const Column>>)> trs =
        [](std::span<std::reference_wrapper<const Column>> sp) -> Column {
        ENSURE(sp.size() == 2);
        const Column& col1 = sp[0];
        const Column& col2 = sp[1];
        const ColumnT<TypeId::Int8>& cl1 = std::get<ColumnT<TypeId::Int8>>(col1.Values());
        const ColumnT<TypeId::Int8>& cl2 = std::get<ColumnT<TypeId::Int8>>(col2.Values());

        size_t sz = col1.Size();
        ENSURE(col2.Size() == sz);

        ColumnT<TypeId::Int8> col;
        col.values.reserve(sz);
        for (size_t i = 0; i < sz; i++) {
            col.Append(cl1[i] || cl2[i]);
        }
        return Column(std::move(col));
    };
    auto result_type = [](std::span<TypeId>) { return TypeId::Int8; };
    return ColumnOperation(std::move(trs), result_type, {col1, col2}, out_col);
}

ColumnOperation ColumnOperation::Select(std::string mask_col, std::string col1, std::string col2, std::string out_col) {
    std::function<Column(std::span<std::reference_wrapper<const Column>>)> trs =
        [](std::span<std::reference_wrapper<const Column>> sp) -> Column {
        ENSURE(sp.size() == 3);
        const Column& col1 = sp[0];
        const Column& col2 = sp[1];
        const Column& col3 = sp[2];

        size_t sz = col1.Size();
        const ColumnT<TypeId::Int8>& mask = std::get<ColumnT<TypeId::Int8>>(col1.Values());

        return std::visit(
            [&]<TypeId id>(const ColumnT<id>& cl2) -> Column {
                ColumnT<id> cl3 = std::get<ColumnT<id>>(col3.Values());
                ENSURE(cl2.Size() == sz && cl3.Size() == sz);

                ColumnT<id> result;
                result.values.reserve(sz);
                for (size_t i = 0; i < sz; i++) {
                    result.Append(mask[i] ? cl2[i] : cl3[i]);
                }
                return Column(std::move(result));
            },
            col2.Values());
    };
    auto result_type = [](std::span<TypeId> sp) {
        ENSURE(sp.size() == 3);
        return sp[1];
    };
    return ColumnOperation(std::move(trs), result_type, {mask_col, col1, col2}, out_col);
}

TransformOperator::TransformOperator(std::vector<ColumnOperation> ops) : m_ops(std::move(ops)) {}

class TransformOperatorStream : public BatchStream {
private:
    std::unique_ptr<BatchStream> m_stream;
    std::shared_ptr<const Schema> m_schema;

    std::vector<ColumnOperation> m_ops;
    std::vector<std::vector<size_t>> m_ops_input_cols;

    struct Secret {};
    friend class TransformOperator;

public:
    TransformOperatorStream(Secret, std::vector<ColumnOperation> ops, std::unique_ptr<BatchStream> stream)
        : m_stream(std::move(stream)), m_ops(std::move(ops)) {

        const Schema& sch = *m_stream->GetSchema();

        std::vector<Schema::ColumnInfo> cols = sch.Columns();

        std::unordered_map<std::string, size_t> map;
        for (size_t i = 0; i < cols.size(); i++) {
            map[cols[i].name] = i;
        }

        m_ops_input_cols.resize(m_ops.size());
        for (size_t i = 0; i < m_ops.size(); i++) {
            const auto& op = m_ops[i];
            ENSURE(!map.contains(op.m_output_col));

            std::vector<TypeId> types;
            for (const auto& name : op.m_input_cols) {
                ENSURE(map.contains(name));

                size_t ind = map[name];
                types.push_back(cols[ind].type);
                m_ops_input_cols[i].push_back(ind);
            }

            cols.push_back({.name = op.m_output_col, .type = op.m_result_type(types)});
            map[op.m_output_col] = cols.size() + i;
        }

        m_schema = std::make_shared<const Schema>(cols);
    }

    std::unique_ptr<Batch> Next() override {
        if (!m_stream) {
            return nullptr;
        }
        std::unique_ptr<Batch> batch = m_stream->Next();
        if (batch == nullptr) {
            m_stream = nullptr;
            return nullptr;
        }

        std::vector<Column> cols = batch->ExtractColumns();
        std::vector<std::reference_wrapper<const Column>> vec;

        for (size_t i = 0; i < m_ops.size(); i++) {
            const auto& op = m_ops[i];

            vec.clear();
            for (size_t ind : m_ops_input_cols[i]) {
                vec.push_back(std::cref(cols[ind]));
            }

            Column col = op.m_transform(vec);
            cols.push_back(std::move(col));
        }

        return std::make_unique<Batch>(m_schema, cols);
    }

    std::shared_ptr<const Schema> GetSchema() override {
        return m_schema;
    }
};

std::unique_ptr<BatchStream> TransformOperator::Transform(std::unique_ptr<BatchStream> stream) const {
    return std::make_unique<TransformOperatorStream>(TransformOperatorStream::Secret(), m_ops, std::move(stream));
}

}  // namespace curse
