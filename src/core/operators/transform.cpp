#include "transform.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "re2/re2.h"
#include "src/core/assert.hpp"
#include "src/core/hash.hpp"
#include "src/core/types.hpp"
#include "src/core/util.hpp"

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
                    res.Reserve(sz);

                    for (size_t i = 0; i < sz; i++) {
                        res.Append(vl);
                    }
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
            res.Reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                res.Append(!cl[i]);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int8; });
}

// utf8 len
Transform Transform::Strlen() {
    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::Int64> res;
            res.Reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                int64_t len = 0;
                using uchar = unsigned char;  // NOLINT
                for (char ch : cl[i]) {
                    len += static_cast<uchar>(static_cast<uchar>(ch) & static_cast<uchar>(0xc0)) !=
                           static_cast<uchar>(0x80);
                }
                res.Append(len);
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
                    res.Reserve(sz);

                    switch (how) {
                        case ComparisonType::Equal:
                            for (size_t i = 0; i < sz; i++) {
                                res.Append(cl[i] == vl);
                            }
                            break;
                        case ComparisonType::NotEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res.Append(cl[i] != vl);
                            }
                            break;
                        case ComparisonType::GreaterThan:
                            for (size_t i = 0; i < sz; i++) {
                                res.Append(cl[i] > vl);
                            }
                            break;
                        case ComparisonType::GreaterThanOrEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res.Append(cl[i] >= vl);
                            }
                            break;
                        case ComparisonType::LessThan:
                            for (size_t i = 0; i < sz; i++) {
                                res.Append(cl[i] < vl);
                            }
                            break;
                        case ComparisonType::LessThanOrEqual:
                            for (size_t i = 0; i < sz; i++) {
                                res.Append(cl[i] <= vl);
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
    std::shared_ptr<const re2::RE2> re = std::make_shared<re2::RE2>(pattern);
    ENSURE_MSG(re->ok(), "invalid regex");

    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::Int8> res;
            res.Reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                bool found = re2::RE2::PartialMatch(cl[i], *re);
                res.Append(found);
            }
            return Column(std::move(res));
        },
        [=](TypeId) { return TypeId::Int8; });
}

Transform Transform::RegexpReplace(std::string pattern, std::string format) {
    std::shared_ptr<const re2::RE2> re = std::make_shared<re2::RE2>(pattern);
    ENSURE_MSG(re->ok(), "invalid regex");

    return Transform(
        [=](const Column& col) {
            const ColumnT<TypeId::String>& cl = std::get<ColumnT<TypeId::String>>(col.Values());
            ColumnT<TypeId::String> res;
            res.Reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::string s(cl[i]);
                re2::RE2::GlobalReplace(&s, *re, format);
                res.Append(std::move(s));
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
            res.Reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::chrono::system_clock::time_point ts = cl[i];
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
            res.Reserve(cl.Size());
            for (size_t i = 0; i < cl.Size(); i++) {
                std::chrono::system_clock::time_point ts = cl[i];
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

ColumnOperation ColumnOperation::LogicalOp(std::string col1, std::string col2, std::string out_col, Logical op) {
    std::function<Column(std::span<std::reference_wrapper<const Column>>)> trs =
        [=](std::span<std::reference_wrapper<const Column>> sp) -> Column {
        ENSURE(sp.size() == 2);
        const Column& col1 = sp[0];
        const Column& col2 = sp[1];
        const ColumnT<TypeId::Int8>& cl1 = std::get<ColumnT<TypeId::Int8>>(col1.Values());
        const ColumnT<TypeId::Int8>& cl2 = std::get<ColumnT<TypeId::Int8>>(col2.Values());

        size_t sz = col1.Size();
        ENSURE(col2.Size() == sz);

        ColumnT<TypeId::Int8> col;
        col.Reserve(sz);
        for (size_t i = 0; i < sz; i++) {
            switch (op) {
                case Logical::And:
                    col.Append(cl1[i] && cl2[i]);
                    break;
                case Logical::Or:
                    col.Append(cl1[i] || cl2[i]);
                    break;
            }
        }
        return Column(std::move(col));
    };
    auto result_type = [](std::span<TypeId>) { return TypeId::Int8; };
    return ColumnOperation(std::move(trs), result_type, {col1, col2}, out_col);
}

ColumnOperation ColumnOperation::LogicalAnd(std::string col1, std::string col2, std::string out_col) {
    return LogicalOp(std::move(col1), std::move(col2), std::move(out_col), Logical::And);
}

ColumnOperation ColumnOperation::LogicalOr(std::string col1, std::string col2, std::string out_col) {
    return LogicalOp(std::move(col1), std::move(col2), std::move(out_col), Logical::Or);
}

ColumnOperation ColumnOperation::LogicalNot(std::string col, std::string out_col) {
    return ColumnOperation(Transform::LogicalNot(), std::move(col), std::move(out_col));
}

ColumnOperation ColumnOperation::ArithmeticOp(std::string col1, std::string col2, std::string out_col, Arithmetic op,
                                              std::optional<TypeId> out_type_opt) {
    std::function<Column(std::span<std::reference_wrapper<const Column>>)> trs =
        [=](std::span<std::reference_wrapper<const Column>> sp) -> Column {
        ENSURE(sp.size() == 2);
        const Column& col1 = sp[0];
        const Column& col2 = sp[1];

        size_t sz = col1.Size();
        ENSURE(col2.Size() == sz);

        TypeId out_type;
        if (out_type_opt.has_value()) {
            out_type = out_type_opt.value();
        } else {
            ENSURE_MSG(col1.Type() == col2.Type(),
                       "can't do arithmetic on columns of different types uless output type is provided");
            out_type = col1.Type();
        }

        Column col(out_type);

        auto aux = [sz, op]<TypeId id1, TypeId id2, TypeId id>(const ColumnT<id1>& cl1, const ColumnT<id2>& cl2,
                                                               ColumnT<id>& cl) {
            using T = ColumnT<id>::T;

            cl.Reserve(sz);
            for (size_t i = 0; i < sz; i++) {
                switch (op) {
                    case Arithmetic::Add:
                        cl.Append(static_cast<T>(cl1[i]) + static_cast<T>(cl2[i]));
                        break;
                    case Arithmetic::Sub:
                        cl.Append(static_cast<T>(cl1[i]) - static_cast<T>(cl2[i]));
                        break;
                    case Arithmetic::Mul:
                        cl.Append(static_cast<T>(cl1[i]) * static_cast<T>(cl2[i]));
                        break;
                }
            };
        };

        std::visit(
            [&]<TypeId id1>(const ColumnT<id1>& cl1) {
                std::visit(
                    [&]<TypeId id2>(const ColumnT<id2>& cl2) {
                        auto visitor = [&]<TypeId id>(ColumnT<id>& cl) -> void {
                            if constexpr (IsIntegral(id1) && IsIntegral(id2) && IsIntegral(id)) {
                                aux(cl1, cl2, cl);
                            } else {
                                ENSURE_MSG(false, "non integral types don't support arithmetic");
                            }
                        };
                        std::visit(visitor, col.Values());
                    },
                    col2.Values());
            },
            col1.Values());

        return col;
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
                result.Reserve(sz);
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

template <TypeId id>
using HashMap = absl::flat_hash_map<typename ReprType<id>::T, size_t, MyHasher, std::equal_to<>>;

using HashMapEnum = MakeEnum<HashMap, AllTypesIds>::T;

ColumnOperation ColumnOperation::SetContains(std::vector<std::string> inp_cols, std::string out_col,
                                             std::unique_ptr<BatchStream> stream) {
    ENSURE(!inp_cols.empty());
    ENSURE(inp_cols.size() == stream->GetSchema()->Columns().size());

    struct Data {
        std::vector<HashMapEnum> maps;
        std::unordered_set<std::vector<size_t>, VecHasher<size_t>> set;
    };

    std::shared_ptr<Data> data_ptr = std::make_shared<Data>();
    {
        Data& data = *data_ptr;

        std::shared_ptr<const Schema> schema = stream->GetSchema();
        const size_t n_cols = schema->Columns().size();

        data.maps.reserve(n_cols);
        for (auto& cl : schema->Columns()) {
            ExecFor(cl.type, [&]<TypeId id> { data.maps.push_back(HashMap<id>()); });
        }

        for (std::unique_ptr<Batch> batch; (batch = stream->Next());) {
            const size_t n_rows = batch->NRows();
            std::vector<Column> cols = std::move(*batch).ExtractColumns();

            std::vector<std::vector<size_t>> map2_keys(n_rows);
            for (size_t i = 0; i < n_rows; i++) {
                map2_keys[i].resize(n_cols);
            }

            for (size_t i = 0; i < n_cols; i++) {
                std::visit(
                    [&]<TypeId id>(const ColumnT<id>& col) {
                        HashMap<id>& map = std::get<HashMap<id>>(data.maps[i]);
                        for (size_t j = 0; j < n_rows; j++) {
                            typename ReprType<id>::T val(col[j]);
                            auto [it, inserted] = map.insert({std::move(val), map.size()});
                            map2_keys[j][i] = it->second;
                        }
                    },
                    cols[i].Values());
            }

            for (size_t i = 0; i < n_rows; i++) {
                data.set.insert(std::move(map2_keys[i]));
            }
        }
    }

    auto trs = [data_ptr, sz = inp_cols.size()](std::span<std::reference_wrapper<const Column>> cols) -> Column {
        ENSURE(sz == cols.size());

        const Data& data = *data_ptr;

        const size_t n_cols = cols.size();
        const size_t n_rows = cols[0].get().Size();

        std::vector<char> not_found(n_rows);
        std::vector<std::vector<size_t>> map2_keys(n_rows);
        for (size_t i = 0; i < n_rows; i++) {
            map2_keys[i].resize(n_cols);
        }

        for (size_t i = 0; i < n_cols; i++) {
            std::visit(
                [&]<TypeId id>(const ColumnT<id>& col) {
                    const HashMap<id>& map = std::get<HashMap<id>>(data.maps[i]);
                    for (size_t j = 0; j < n_rows; j++) {
                        auto it = map.find(col[j]);
                        if (it != map.end()) {
                            map2_keys[j][i] = it->second;
                        } else {
                            not_found[j] = true;
                        }
                    }
                },
                cols[i].get().Values());
        }

        ColumnT<TypeId::Int8> result;
        for (size_t i = 0; i < n_rows; i++) {
            bool found = !not_found[i] && data.set.contains(map2_keys[i]);
            result.Append(found);
        }
        return result;
    };
    auto result_type = [sz = inp_cols.size()](std::span<TypeId> sp) {
        ENSURE(sp.size() == sz);
        return TypeId::Int8;
    };

    return ColumnOperation(trs, result_type, inp_cols, out_col);
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

        const auto& cols = sch.Columns();
        std::vector<Schema::ColumnInfo> new_cols = cols;

        std::unordered_map<std::string, size_t> map;
        for (size_t i = 0; i < new_cols.size(); i++) {
            map[new_cols[i].name] = i;
        }

        m_ops_input_cols.resize(m_ops.size());
        for (size_t i = 0; i < m_ops.size(); i++) {
            const auto& op = m_ops[i];
            ENSURE(!map.contains(op.m_output_col));

            std::vector<TypeId> types;
            for (const auto& name : op.m_input_cols) {
                ENSURE(map.contains(name));

                size_t ind = map[name];
                types.push_back(new_cols[ind].type);
                m_ops_input_cols[i].push_back(ind);
            }

            map[op.m_output_col] = cols.size() + i;
            new_cols.push_back({.name = op.m_output_col, .type = op.m_result_type(types)});
        }

        m_schema = std::make_shared<const Schema>(new_cols);
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

        std::vector<Column> cols = std::move(*batch).ExtractColumns();
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
