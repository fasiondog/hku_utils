/**
 *  Copyright (c) 2021 hikyuu
 *
 *  Created on: 2021/05/18
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <hikyuu/utilities/Log.h>

using namespace hku;

class TestClass {
    CLASS_LOGGER_IMP(TestClass)

    // public:
    //     TestClass() = default;

    void operator()() {
        HKU_TRACE("trace");
        HKU_DEBUG("debug");
        HKU_INFO("info");
        HKU_WARN("warn");
        HKU_ERROR("error");
        HKU_FATAL("fatal");
        CLS_TRACE("cls trace");
        CLS_DEBUG("cls debug");
        CLS_INFO("cls info");
        CLS_WARN("cls info");
        CLS_ERROR("cls error");
        CLS_FATAL("cls fatal");
    }
};

TEST_CASE("test_log") {
    TestClass x;
    x();
}
