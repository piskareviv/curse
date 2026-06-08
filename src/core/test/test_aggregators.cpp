#include <chrono>
#include <cstdint>
#include <variant>

#include "gtest/gtest.h"
#include "src/core/aggregators.hpp"
#include "src/core/types.hpp"

using namespace curse;        // NOLINT
using namespace std::chrono;  // NOLINT

TEST(Aggregators, Count1) {
    for (TypeId id : {TypeId::Int8, TypeId::Int128}) {
        Column col(id);
        for (size_t i = 0; i < 100; i++) {
            std::visit(
                [&]<TypeId id>(ColumnT<id> &cl) {
                    if constexpr (IsIntegral(id)) {
                        cl.Append(static_cast<ReprType<id>::T>(i));
                    }
                },
                col.Values());
        }

        Aggregator agg1(AggType::Count, id);
        agg1.Update(col);

        Aggregator agg2(AggType::Count, id);
        std::visit(
            [&]<TypeId id>(const ColumnT<id> &cl) {
                for (size_t i = 0; i < cl.Size(); i++) {
                    agg2.Update(Value::From<id>(cl[i]));
                }
            },
            col.Values());

        ASSERT_EQ(agg1.Get(), Value::From<TypeId::Int64>(100));
        ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int64>(100));
    }
}

TEST(Aggregators, Count2) {
    for (TypeId id : {TypeId::String, TypeId::Timestamp}) {
        Column col(id);
        for (size_t i = 0; i < 100; i++) {
            std::visit(
                [&]<TypeId id>(ColumnT<id> &cl) {
                    if constexpr (id == TypeId::String) {
                        cl.Append("meow");
                    } else if constexpr (id == TypeId::Timestamp) {
                        cl.Append(std::chrono::system_clock::now());
                    }
                },
                col.Values());
        }

        Aggregator agg1(AggType::Count, id);
        agg1.Update(col);

        Aggregator agg2(AggType::Count, id);
        std::visit(
            [&]<TypeId id>(const ColumnT<id> &cl) {
                for (size_t i = 0; i < cl.Size(); i++) {
                    agg2.Update(Value::From<id>(cl[i]));
                }
            },
            col.Values());

        ASSERT_EQ(agg1.Get(), Value::From<TypeId::Int64>(100));
        ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int64>(100));
    }
}

TEST(Aggregators, Sum) {
    EXPECT_ANY_THROW(Aggregator(AggType::Average, TypeId::Char));
    EXPECT_ANY_THROW(Aggregator(AggType::Average, TypeId::String));
    EXPECT_ANY_THROW(Aggregator(AggType::Average, TypeId::Date));
    EXPECT_ANY_THROW(Aggregator(AggType::Average, TypeId::Timestamp));

    for (TypeId id : {TypeId::Int8, TypeId::Int128}) {
        Column col(id);
        for (size_t i = 0; i < 100; i++) {
            std::visit(
                [&]<TypeId id>(ColumnT<id> &cl) {
                    if constexpr (IsIntegral(id)) {
                        cl.Append(static_cast<ReprType<id>::T>(i));
                    }
                },
                col.Values());
        }

        Aggregator agg1(AggType::Sum, id);
        agg1.Update(col);

        Aggregator agg2(AggType::Sum, id);
        std::visit(
            [&]<TypeId id>(const ColumnT<id> &cl) {
                for (size_t i = 0; i < cl.Size(); i++) {
                    agg2.Update(Value::From<id>(cl[i]));
                }
            },
            col.Values());

        ASSERT_EQ(agg1.Get(), Value::From<TypeId::Int128>(100 * 99 / 2));
        ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int128>(100 * 99 / 2));
    }
}

TEST(Aggregators, Sum_Int64Overflow) {

    ColumnT<TypeId::Int64> col;
    for (size_t i = 0; i < 100; i++) {
        col.Append(static_cast<int64_t>(i) << 55);
    }

    Aggregator agg1(AggType::Sum, TypeId::Int64);
    agg1.Update(col);

    Aggregator agg2(AggType::Sum, TypeId::Int64);
    for (size_t i = 0; i < col.Size(); i++) {
        agg2.Update(Value::From<TypeId::Int64>(col[i]));
    }

    ASSERT_EQ(agg1.Get(),
              Value::From<TypeId::Int128>(100 * 99 / 2 * (ReprType<TypeId::Int128>::T(1) << 55)));  // NOLINT
    ASSERT_EQ(agg2.Get(),
              Value::From<TypeId::Int128>(100 * 99 / 2 * (ReprType<TypeId::Int128>::T(1) << 55)));  // NOLINT
}

TEST(Aggregators, Average) {
    EXPECT_ANY_THROW(Aggregator(AggType::Average, TypeId::Char));
    EXPECT_ANY_THROW(Aggregator(AggType::Average, TypeId::String));
    EXPECT_ANY_THROW(Aggregator(AggType::Average, TypeId::Date));
    EXPECT_ANY_THROW(Aggregator(AggType::Average, TypeId::Timestamp));

    for (TypeId id : {TypeId::Int8, TypeId::Int128}) {
        Column col(id);
        for (size_t i = 0; i < 100; i++) {
            std::visit(
                [&]<TypeId id>(ColumnT<id> &cl) {
                    if constexpr (IsIntegral(id)) {
                        cl.Append(static_cast<ReprType<id>::T>(i));
                    }
                },
                col.Values());
        }

        Aggregator agg1(AggType::Average, id);
        agg1.Update(col);

        Aggregator agg2(AggType::Average, id);
        std::visit(
            [&]<TypeId id>(const ColumnT<id> &cl) {
                for (size_t i = 0; i < cl.Size(); i++) {
                    agg2.Update(Value::From<id>(cl[i]));
                }
            },
            col.Values());

        ASSERT_EQ(agg1.Get(), Value::From<TypeId::Float64>(49.5));
        ASSERT_EQ(agg2.Get(), Value::From<TypeId::Float64>(49.5));
    }
}

TEST(Aggregators, Min) {
    Aggregator agg1(AggType::Min, TypeId::Int8);
    EXPECT_ANY_THROW(agg1.Get());
    EXPECT_ANY_THROW(agg1.Update(Value::From<TypeId::Int64>(1)));

    Aggregator agg2(AggType::Min, TypeId::Int8);
    agg2.Update(Value::From<TypeId::Int8>(1));
    agg2.Update(Value::From<TypeId::Int8>(2));
    agg2.Update(Value::From<TypeId::Int8>(3));
    ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int8>(1));

    Aggregator agg3(AggType::Min, TypeId::Int64);
    agg3.Update(ColumnT<TypeId::Int64>::FromVector(std::vector<ReprType<TypeId::Int64>::T>{-1, 1, 123}));
    ASSERT_EQ(agg3.Get(), Value::From<TypeId::Int64>(-1));

    Aggregator agg4(AggType::Min, TypeId::String);
    agg4.Update(Value::From<TypeId::String>("aba"));
    agg4.Update(Value::From<TypeId::String>("abacaba"));
    agg4.Update(Value::From<TypeId::String>("abacabadaba"));
    ASSERT_EQ(agg4.Get(), Value::From<TypeId::String>("aba"));

    Aggregator agg5(AggType::Min, TypeId::Date);
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2021y, April, 5d)));
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2022y, April, 5d)));
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2023y, April, 5d)));
    ASSERT_EQ(agg5.Get(), Value::From<TypeId::Date>(std::chrono::year_month_day(2021y, April, 5d)));
}

TEST(Aggregators, Max) {
    Aggregator agg1(AggType::Max, TypeId::Int8);
    EXPECT_ANY_THROW(agg1.Get());

    Aggregator agg2(AggType::Max, TypeId::Int8);
    agg2.Update(Value::From<TypeId::Int8>(1));
    agg2.Update(Value::From<TypeId::Int8>(2));
    agg2.Update(Value::From<TypeId::Int8>(3));
    ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int8>(3));

    Aggregator agg3(AggType::Max, TypeId::Int64);
    agg3.Update(ColumnT<TypeId::Int64>::FromVector(std::vector<ReprType<TypeId::Int64>::T>{-1, 1, 123}));
    ASSERT_EQ(agg3.Get(), Value::From<TypeId::Int64>(123));

    Aggregator agg4(AggType::Max, TypeId::String);
    agg4.Update(Value::From<TypeId::String>("aba"));
    agg4.Update(Value::From<TypeId::String>("abacaba"));
    agg4.Update(Value::From<TypeId::String>("abacabadaba"));
    ASSERT_EQ(agg4.Get(), Value::From<TypeId::String>("abacabadaba"));

    Aggregator agg5(AggType::Max, TypeId::Date);
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2021y, April, 5d)));
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2022y, April, 5d)));
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2023y, April, 5d)));
    ASSERT_EQ(agg5.Get(), Value::From<TypeId::Date>(std::chrono::year_month_day(2023y, April, 5d)));
}

TEST(Aggregators, CountDistinct) {
    Aggregator agg1(AggType::CountDistinct, TypeId::Int8);
    ASSERT_EQ(agg1.Get(), Value::From<TypeId::Int64>(0));

    Aggregator agg2(AggType::CountDistinct, TypeId::Int8);
    ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int64>(0));
    agg2.Update(Value::From<TypeId::Int8>(1));
    ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int64>(1));
    agg2.Update(Value::From<TypeId::Int8>(2));
    ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int64>(2));
    agg2.Update(Value::From<TypeId::Int8>(3));
    ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int64>(3));
    agg2.Update(Value::From<TypeId::Int8>(1));
    agg2.Update(Value::From<TypeId::Int8>(2));
    agg2.Update(Value::From<TypeId::Int8>(3));
    ASSERT_EQ(agg2.Get(), Value::From<TypeId::Int64>(3));

    Aggregator agg3(AggType::CountDistinct, TypeId::Int64);
    agg3.Update(ColumnT<TypeId::Int64>::FromVector(std::vector<ReprType<TypeId::Int64>::T>{-1, 1, 123}));
    ASSERT_EQ(agg3.Get(), Value::From<TypeId::Int64>(3));

    Aggregator agg4(AggType::CountDistinct, TypeId::String);
    ASSERT_EQ(agg4.Get(), Value::From<TypeId::Int64>(0));
    agg4.Update(Value::From<TypeId::String>("aba"));
    ASSERT_EQ(agg4.Get(), Value::From<TypeId::Int64>(1));
    agg4.Update(Value::From<TypeId::String>("abacaba"));
    ASSERT_EQ(agg4.Get(), Value::From<TypeId::Int64>(2));
    agg4.Update(Value::From<TypeId::String>("abacabadaba"));
    ASSERT_EQ(agg4.Get(), Value::From<TypeId::Int64>(3));
    agg4.Update(Value::From<TypeId::String>("abacaba"));
    agg4.Update(Value::From<TypeId::String>("abacabadaba"));
    ASSERT_EQ(agg4.Get(), Value::From<TypeId::Int64>(3));

    Aggregator agg5(AggType::CountDistinct, TypeId::Date);
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2021y, April, 5d)));
    ASSERT_EQ(agg5.Get(), Value::From<TypeId::Int64>(1));
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2021y, April, 5d)));
    ASSERT_EQ(agg5.Get(), Value::From<TypeId::Int64>(1));

    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2022y, April, 5d)));
    ASSERT_EQ(agg5.Get(), Value::From<TypeId::Int64>(2));
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2023y, April, 5d)));
    ASSERT_EQ(agg5.Get(), Value::From<TypeId::Int64>(3));

    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2021y, April, 5d)));
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2022y, April, 5d)));
    agg5.Update(Value::From<TypeId::Date>(std::chrono::year_month_day(2023y, April, 5d)));

    ASSERT_EQ(agg5.Get(), Value::From<TypeId::Int64>(3));
}
