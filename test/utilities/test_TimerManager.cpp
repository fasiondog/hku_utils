/*
 *  Copyright(C) 2021 hikyuu.org
 *
 *  Create on: 2021-01-09
 *     Author: fasiondog
 */

#include "../test_config.h"

#if defined(HKU_SUPPORT_DATETIME)
#include <hikyuu/utilities/TimerManager.h>
#include <hikyuu/utilities/thread/thread.h>
#include <hikyuu/utilities/Log.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace hku;

/**
 * @defgroup test_hikyuu_TimerManager test_hikyuu_TimerManager
 * @ingroup test_hikyuu_utilities
 * @{
 */

static void hello_test() {
    HKU_TRACE("hello");
}

TEST_CASE("test_TimerManager") {
    TimerManager tm;
    tm.start();

    CHECK_THROWS_AS(tm.addDurationFunc(1, TimeDelta(), hello_test), hku::exception);
    CHECK_THROWS_AS(tm.addDurationFunc(0, TimeDelta(1), hello_test), hku::exception);
    CHECK_THROWS_AS(tm.addDurationFunc(-1, TimeDelta(1), hello_test), hku::exception);

    CHECK_THROWS_AS(tm.addDelayFunc(TimeDelta(0), hello_test), hku::exception);
    CHECK_THROWS_AS(tm.addDelayFunc(TimeDelta(-1), hello_test), hku::exception);

    CHECK_THROWS_AS(tm.addFuncAtTime(Datetime::min(), hello_test), hku::exception);

    CHECK_THROWS_AS(tm.addFuncAtTimeEveryDay(TimeDelta(-1), hello_test), hku::exception);
    CHECK_THROWS_AS(tm.addFuncAtTimeEveryDay(TimeDelta(1), hello_test), hku::exception);

    CHECK_THROWS_AS(
      tm.addFuncAtTimeEveryDay(Datetime::min(), Datetime::max(), TimeDelta(-1), hello_test),
      hku::exception);
    CHECK_THROWS_AS(
      tm.addFuncAtTimeEveryDay(Datetime::min(), Datetime::max(), TimeDelta(1), hello_test),
      hku::exception);
    CHECK_THROWS_AS(
      tm.addFuncAtTimeEveryDay(Datetime::max(), Datetime::min(), TimeDelta(0, 1), hello_test),
      hku::exception);
    CHECK_THROWS_AS(
      tm.addFuncAtTimeEveryDay(Datetime::min(), Datetime(), TimeDelta(0, 1), hello_test),
      hku::exception);
    CHECK_THROWS_AS(
      tm.addFuncAtTimeEveryDay(Datetime(), Datetime::max(), TimeDelta(0, 1), hello_test),
      hku::exception);
}

/**
 * @par 检测点
 * 1. 外部线程池被提前停止后，到期定时器 submit 抛 logic_error，检测线程必须捕获而非逃逸导致
 * std::terminate (H5)
 * 2. 该定时器被移除，任务不会执行
 */
TEST_CASE("test_TimerManager_external_pool_stopped") {
    auto pool = std::make_unique<ThreadPool>(2);
    TimerManager tm(pool.get());

    std::atomic<bool> executed{false};
    tm.addDelayFunc(TimeDelta(0, 0, 0, 50), [&executed]() { executed.store(true); });

    // Stop the external pool before the timer fires; the detection thread must not terminate.
    pool->stop();

    // Wait long enough for the timer's scheduled point to pass and be processed.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    CHECK_FALSE(executed.load());
}

/** @} */

#endif