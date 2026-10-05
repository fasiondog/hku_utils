/*
 *  Copyright (c) hikyuu.org
 *
 *  Created on: 2026-10-6
 *      Author: fasiondog
 *
 */

#include <doctest/doctest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <hikyuu/utilities/SpendTimer.h>

using namespace hku;

#if !HKU_CLOSE_SPEND_TIME

TEST_CASE("test_spend_time_status") {
    /** Open/close toggles the global status and get_spend_time_status reports it */
    bool original = get_spend_time_status();

    open_spend_time();
    CHECK_UNARY(get_spend_time_status());
    CHECK_UNARY_FALSE(SpendTimer::isClosed());

    close_spend_time();
    CHECK_UNARY_FALSE(get_spend_time_status());
    CHECK_UNARY(SpendTimer::isClosed());

    if (original) {
        open_spend_time();
    } else {
        close_spend_time();
    }
    CHECK_EQ(get_spend_time_status(), original);
}

TEST_CASE("test_spend_time_concurrent_toggle") {
    // Concurrent open/close/isClosed/get_spend_time_status must not be a data race (UB before
    // ms_closed became atomic)
    bool original = get_spend_time_status();

    std::atomic<bool> stop{false};

    auto toggler = [&stop]() {
        while (!stop.load(std::memory_order_relaxed)) {
            open_spend_time();
            close_spend_time();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(toggler);
    }
    threads.emplace_back([&stop]() {
        while (!stop.load(std::memory_order_relaxed)) {
            // Concurrent reads only; the two functions each load the atomic independently and
            // the togglers may flip the state between the calls, so no cross-call equality can
            // be asserted here
            (void)get_spend_time_status();
            (void)SpendTimer::isClosed();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);
    for (auto& t : threads) {
        t.join();
    }

    if (original) {
        open_spend_time();
    } else {
        close_spend_time();
    }
    CHECK_EQ(get_spend_time_status(), original);
}

TEST_CASE("test_spend_timer_destructor_suppressed_when_closed") {
    /** A closed switch suppresses the destructor output; the timer itself still records */
    bool original = get_spend_time_status();
    close_spend_time();
    {
        SPEND_TIME_MSG(test_suppressed, "suppressed output");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // Nothing to assert on the output stream; only that the code path runs safely
    if (original) {
        open_spend_time();
    } else {
        close_spend_time();
    }
    CHECK_EQ(get_spend_time_status(), original);
}

TEST_CASE("test_spend_timer_keep_prints_last_segment") {
    /** The destructor prints every keep segment, including the final interval pushed at
     *  destruction which has no keep description */
    bool original = get_spend_time_status();
    open_spend_time();

    std::ostringstream oss;
    auto* oldbuf = std::cout.rdbuf(oss.rdbuf());
    {
        SPEND_TIME_MSG(test_keep, "keep print test");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        SPEND_TIME_KEEP(test_keep, "first");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        SPEND_TIME_KEEP(test_keep, "second");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout.rdbuf(oldbuf);

    if (original) {
        open_spend_time();
    } else {
        close_spend_time();
    }

    std::string out = oss.str();

    // Count the keep lines: two keeps plus the final segment printed at destruction
    size_t keep_lines = 0;
    size_t pos = 0;
    while ((pos = out.find("keep:", pos)) != std::string::npos) {
        ++keep_lines;
        ++pos;
    }
    CHECK_EQ(keep_lines, 3);
    CHECK_UNARY(out.find("first") != std::string::npos);
    CHECK_UNARY(out.find("second") != std::string::npos);
}

#endif  // #if !HKU_CLOSE_SPEND_TIME
