/**
 *  Copyright (c) 2021 hikyuu
 *
 *  Created on: 2021/05/18
 *      Author: fasiondog
 */

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

#include <hikyuu/utilities/SpendTimer.h>
#include <hikyuu/utilities/ResourcePool.h>
#include <hikyuu/utilities/version.h>
#include <hikyuu/utilities/os.h>

#include <dlfcn.h>

int main(int argc, char** argv) {
#if defined(_WIN32)
    // Windows 下设置控制台程序输出代码页为 UTF8
    auto old_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
#endif

    doctest::Context context;

    // !!! THIS IS JUST AN EXAMPLE SHOWING HOW DEFAULTS/OVERRIDES ARE SET !!!

    // defaults
    // context.addFilter("test-case-exclude", "*math*");  // exclude test cases with "math" in their
    // name
    context.setOption("abort-after", 5);    // stop test execution after 5 failed assertions
    context.setOption("order-by", "name");  // sort the test cases by their name

    context.applyCommandLine(argc, argv);

    // overrides
    context.setOption("no-breaks", true);  // don't break in the debugger when assertions fail

    HKU_INFO("before init log");

    hku::initLogger();

    HKU_INFO("current utils version: {}", hku::utils::getVersion());
    HKU_INFO("current utils build version: {}", hku::utils::getVersionWithBuild());

    OPEN_SPEND_TIME;
    hku::createDir("test_data/tmp");

    auto m_handle = dlopen("/usr/local/lib/libtaos.dylib", RTLD_LAZY);

    int res = 0;
    {
        SPEND_TIME_MSG(total_test_run, "Total test time");
        res = context.run();  // run
        std::cout << std::endl;
    }

    std::cout << std::endl;

    dlclose(m_handle);

    if (context.shouldExit())  // important - query flags (and --exit) rely on the user doing this
        return res;            // propagate the result of the tests

    int client_stuff_return_code = 0;
    // your program - if the testing framework is integrated in your production code

#if defined(_WIN32)
    SetConsoleOutputCP(old_cp);
#endif

    return res + client_stuff_return_code;  // the result from doctest is propagated here as well
}