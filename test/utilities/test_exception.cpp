/*
 *  Copyright (c) 2019~2021, hikyuu
 *
 *  Created on: 2021/12/16
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <hikyuu/utilities/exception.h>

TEST_CASE("test_exception") {
    CHECK_THROWS(throw hku::exception("test exception"));
}
