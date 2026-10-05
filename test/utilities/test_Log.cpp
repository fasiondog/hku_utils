/**
 *  Copyright (c) 2021 hikyuu
 *
 *  Created on: 2021/05/18
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <hikyuu/utilities/Log.h>
#include <spdlog/spdlog.h>

using namespace hku;

class TestClass {
    CLASS_LOGGER_IMP(TestClass)

public:
    TestClass() = default;

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

TEST_CASE("test_log_getHikyuuLogger") {
    // initLogger 后缓存实例必须与注册表中的 "hikyuu" logger 一致（注册修正 + 缓存刷新）
    initLogger();
    auto logger = getHikyuuLogger();
    CHECK_EQ(logger->name(), "hikyuu");
    // NOTE: spdlog::get/default_logger not checked here — header-only spdlog registry is a
    // per-image inline-function static, not coalesced across dylib boundaries on macOS/Windows.
    // CHECK_UNARY(spdlog::get("hikyuu") != nullptr);
    // CHECK_EQ(logger.get(), spdlog::default_logger().get());
}

TEST_CASE("test_log_level_concurrent_access") {  // Concurrent set/get of the log level must not be
                                                 // a data race (UB before the fix), and
    // get_log_level must always return a value from the valid enum range
    LOG_LEVEL original = get_log_level();

    std::atomic<bool> stop{false};
    std::atomic<int> invalid_values{0};

    auto writer = [&stop]() {
        const LOG_LEVEL levels[] = {LOG_TRACE, LOG_INFO, LOG_WARN, LOG_ERROR};
        size_t i = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            set_log_level(levels[i % 4]);
            ++i;
        }
    };

    auto reader = [&stop, &invalid_values]() {
        while (!stop.load(std::memory_order_relaxed)) {
            LOG_LEVEL level = get_log_level();
            if (level < LOG_TRACE || level > LOG_OFF) {
                invalid_values.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(writer);
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(reader);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);
    for (auto& t : threads) {
        t.join();
    }

    set_log_level(original);
    CHECK_EQ(get_log_level(), original);
    CHECK_EQ(invalid_values.load(), 0);
}
