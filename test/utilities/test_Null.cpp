/**
 *  Copyright (c) 2021 hikyuu
 *
 *  Created on: 2021/11/02
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <hikyuu/utilities/Null.h>

using namespace hku;

TEST_CASE("test_Null_size_t") {
    CHECK(std::numeric_limits<std::size_t>::max() == Null<std::size_t>());
}

TEST_CASE("test_Null_double_float") {
    CHECK_UNARY(std::isnan(Null<double>()));
    CHECK_UNARY(std::isnan(Null<float>()));

    double null_double = Null<double>();
    double null_float = Null<float>();

    CHECK_EQ(Null<double>(), null_double);
    CHECK_EQ(null_double, Null<double>());
    CHECK_EQ(null_double, Null<float>());
    CHECK_EQ(null_float, Null<float>());
    CHECK_EQ(null_float, Null<double>());
    CHECK_UNARY(null_double == Null<double>());
    CHECK_UNARY(Null<double>() == null_double);
    CHECK_UNARY(null_double == Null<float>());
    CHECK_UNARY(Null<float>() == null_double);
    CHECK_UNARY(null_float == Null<float>());
    CHECK_UNARY(Null<float>() == null_float);
    CHECK_UNARY(null_float == Null<double>());
    CHECK_UNARY(Null<double>() == null_float);

    CHECK_NE(Null<double>(), 0.3);
    CHECK_NE(Null<float>(), 0.3);
    CHECK_NE(Null<double>(), 0.3f);
    CHECK_NE(Null<float>(), 0.3f);
    CHECK_NE(0.3, Null<double>());
    CHECK_NE(0.3, Null<float>());
    CHECK_NE(0.3f, Null<double>());
    CHECK_NE(0.3f, Null<float>());
}