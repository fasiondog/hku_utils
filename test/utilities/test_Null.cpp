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
