/**
 *  Copyright (c) 2021 hikyuu
 *
 *  Created on: 2021/11/02
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <hikyuu/utilities/Null.h>
#include "hikyuu/utilities/datetime/Datetime.h"

using namespace hku;

TEST_CASE("test_Null_size_t") {
    CHECK(std::numeric_limits<std::size_t>::max() == Null<std::size_t>());
}

/** @par 检测点 */
TEST_CASE("test_Null_integer") {
    /** @arg The explicitly specialized widths give the maximum of the type */
    CHECK_EQ(Null<int>(), (std::numeric_limits<int>::max)());
    CHECK_EQ(Null<unsigned int>(), (std::numeric_limits<unsigned int>::max)());
    CHECK_EQ(Null<long long>(), (std::numeric_limits<long long>::max)());
    CHECK_EQ(Null<unsigned long long>(), (std::numeric_limits<unsigned long long>::max)());
    CHECK_EQ(Null<int64_t>(), (std::numeric_limits<int64_t>::max)());
    CHECK_EQ(Null<uint64_t>(), (std::numeric_limits<uint64_t>::max)());

    /** @arg Narrow integer types have no specialization, the general case still yields the
     *  maximum of the type instead of a default constructed 0 */
    CHECK_EQ(Null<int8_t>(), (std::numeric_limits<int8_t>::max)());
    CHECK_EQ(Null<uint8_t>(), (std::numeric_limits<uint8_t>::max)());
    CHECK_EQ(Null<int16_t>(), (std::numeric_limits<int16_t>::max)());
    CHECK_EQ(Null<uint16_t>(), (std::numeric_limits<uint16_t>::max)());
    CHECK_EQ(Null<int32_t>(), (std::numeric_limits<int32_t>::max)());
    CHECK_EQ(Null<uint32_t>(), (std::numeric_limits<uint32_t>::max)());

    /** @arg The same fixed-size alias may name different fundamental types on different
     *  platforms (uint64_t is unsigned long on linux x86_64, unsigned long long on macos;
     *  unsigned long is 32-bit on macos/windows and 64-bit on linux), all of them must map to
     *  their own maximum */
    CHECK_EQ(Null<unsigned long>(), (std::numeric_limits<unsigned long>::max)());
    CHECK_EQ(Null<long>(), (std::numeric_limits<long>::max)());
    CHECK_EQ(Null<unsigned short>(), (std::numeric_limits<unsigned short>::max)());
}

/** @par 检测点 */
TEST_CASE("test_Null_non_numeric_type") {
    /** @arg A floating type having no specialization maps to NaN as well */
    long double ld = Null<long double>();
    CHECK_UNARY(std::isnan(ld));

    /** @arg A type with no numeric limits keeps the default constructed value as its Null;
     *  Datetime() is null by definition */
    Datetime d = Null<Datetime>();
    CHECK_UNARY(d.isNull());
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