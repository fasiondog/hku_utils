/*
 * test_iniparser.cpp
 *
 *  Copyright (c) 2019 hikyuu.org
 *
 *  Created on: 2010-5-26
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/thread/thread.h>
#include <hikyuu/utilities/SpendTimer.h>
#include <hikyuu/utilities/Log.h>
#include <atomic>
#include <chrono>
#include <latch>
#include <thread>

using namespace hku;

#if 1

/**
 * @defgroup test_hikyuu_ThreadPool test_hikyuu_ThreadPool
 * @ingroup test_hikyuu_utilities
 * @{
 */

TEST_CASE("test_ThreadPool") {
    {
        SPEND_TIME(test_ThreadPool);
        ThreadPool tg(8);
        HKU_INFO("worker_num: {}", tg.worker_num());
        for (int i = 0; i < 10; i++) {
#if FMT_VERSION >= 90000
            tg.submit([=]() {  // fmt::print("{}: ----------------------\n", i);
                HKU_INFO("{}: ------------------- [{}]", i,
                         fmt::streamed(std::this_thread::get_id()));
            });
#else
            tg.submit([=]() {  // fmt::print("{}: ----------------------\n", i);
                HKU_INFO("{}: ------------------- [{}]", i, std::this_thread::get_id());
            });
#endif
        }
        tg.join();
    }
}

/** @par 检测点 */
TEST_CASE("test_MQThreadPool") {
    {
        SPEND_TIME(test_MQThreadPool);
        MQThreadPool tg(8);
        HKU_INFO("worker_num: {}", tg.worker_num());
        for (int i = 0; i < 10; i++) {
#if FMT_VERSION >= 90000
            tg.submit([=]() {  // fmt::print("{}: ----------------------\n", i);
                HKU_INFO("{}: ------------------- [{}]", i,
                         fmt::streamed(std::this_thread::get_id()));
            });
#else
            tg.submit([=]() {  // fmt::print("{}: ----------------------\n", i);
                HKU_INFO("{}: ------------------- [{}]", i, std::this_thread::get_id());
            });
#endif
        }
        tg.join();
    }
}

/** @par 检测点 */
TEST_CASE("test_StealThreadPool") {
    {
        SPEND_TIME(test_StealThreadPool);
        StealThreadPool tg(8);
        HKU_INFO("worker_num: {}", tg.worker_num());
        for (int i = 0; i < 10; i++) {
#if FMT_VERSION >= 90000
            tg.submit([=]() {  // fmt::print("{}: ----------------------\n", i);
                HKU_INFO("{}: ------------------- [{}]", i,
                         fmt::streamed(std::this_thread::get_id()));
            });
#else
            tg.submit([=]() {  // fmt::print("{}: ----------------------\n", i);
                HKU_INFO("{}: ------------------- [{}]", i, std::this_thread::get_id());
            });
#endif
        }
        tg.join();
    }

    {
        SPEND_TIME(parallel_for_index_void_single);
        parallel_for_index_void_single<std::function<void(size_t)>, MQStealThreadPool>(
          0, 10, [](size_t i) {  // fmt::print("{}: ----------------------\n", i);
              HKU_INFO("{}: ------------------- [{}]", i,
                       fmt::streamed(std::this_thread::get_id()));
          });
    }
}

// /** @par 检测点 */
// TEST_CASE("test_MQStealThreadPool") {
//     {
//         SPEND_TIME(test_MQStealThreadPool);
//         MQStealThreadPool tg(8, true, []() {
//             HKU_INFO("MQStealThreadPool work thread({}) stop!", std::this_thread::get_id());
//         });
//         HKU_INFO("worker_num: {}", tg.worker_num());
//         for (int i = 0; i < 10; i++) {
//             tg.submit([=]() {  // fmt::print("{}: ----------------------\n", i);
//                 HKU_INFO("{}: ------------------- [{}]", i, std::this_thread::get_id());
//             });
//         }
//         tg.join();
//     }
// }

/**
 * @par 检测点
 * 1. Recursive submit: a running task submits sub-tasks while the main thread calls join().
 *    join() must not set m_done while tasks are still in flight, otherwise submit() throws
 *    std::logic_error and the sub-tasks are lost (H3).
 * 2. All recursively submitted tasks must be executed (no lost tasks).
 */
template <typename Pool>
static void recursive_submit_must_not_lose_tasks() {
    const int root_count = 4;
    const int sub_count = 200;
    std::atomic<int> executed{0};
    Pool pool(8);

    for (int i = 0; i < root_count; ++i) {
        pool.submit([&pool, &executed]() {
            // Widen the window where the task queues appear empty while this task is still
            // in flight. Without an in-flight counter join() would set m_done here and the
            // submit() calls below would throw std::logic_error.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            for (int j = 0; j < sub_count; ++j) {
                pool.submit([&executed]() { executed.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }

    pool.join();
    CHECK(executed.load() == root_count * sub_count);
}

TEST_CASE("test_StealThreadPool_recursive_submit") {
    recursive_submit_must_not_lose_tasks<StealThreadPool>();
}

TEST_CASE("test_GlobalStealThreadPool_recursive_submit") {
    recursive_submit_must_not_lose_tasks<GlobalStealThreadPool>();
}

TEST_CASE("test_MQStealThreadPool_recursive_submit") {
    recursive_submit_must_not_lose_tasks<MQStealThreadPool>();
}

/**
 * @par 检测点
 * join()（until_empty 模式）必须等待池排空：长任务执行期间 joiner 阻塞等待
 * （条件变量睡眠而非自旋占核），join 在慢任务完成后才返回。
 */
template <typename PoolType>
static void join_waits_for_long_task() {
    PoolType pool(2);
    std::atomic<int> executed{0};
    auto start = std::chrono::steady_clock::now();
    pool.submit([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        executed.fetch_add(1, std::memory_order_relaxed);
    });
    pool.join();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count();
    CHECK_EQ(executed.load(), 1);
    CHECK_UNARY(elapsed >= 100);
}

TEST_CASE("test_StealThreadPool_join_waits_for_long_task") {
    join_waits_for_long_task<StealThreadPool>();
}

TEST_CASE("test_GlobalStealThreadPool_join_waits_for_long_task") {
    join_waits_for_long_task<GlobalStealThreadPool>();
}

TEST_CASE("test_MQStealThreadPool_join_waits_for_long_task") {
    join_waits_for_long_task<MQStealThreadPool>();
}

/**
 * @par 检测点
 * 1. Multiple non-worker producer threads submit concurrently: the round-robin index
 *    m_current_index must be updated atomically, otherwise it is a data race (H4).
 * 2. Every submitted task is executed exactly once (no lost or duplicated tasks).
 */
TEST_CASE("test_MQStealThreadPool_multi_producer_submit") {
    const int producer_count = 8;
    const int per_producer = 500;
    std::atomic<int> executed{0};
    MQStealThreadPool pool(4);

    std::vector<std::thread> producers;
    producers.reserve(producer_count);
    for (int p = 0; p < producer_count; ++p) {
        producers.emplace_back([&pool, &executed, per_producer]() {
            for (int i = 0; i < per_producer; ++i) {
                pool.submit([&executed]() { executed.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }

    pool.join();
    CHECK(executed.load() == producer_count * per_producer);
}

/**
 * @brief test MQStealThreadPool stop semantics
 * 1. stop() must not run tasks that are still queued: the workers exit as soon as the
 *    currently running task finishes, the queued tasks are discarded.
 * 2. A blocked worker must be woken up by the stop signal itself even when the tail of its
 *    queue is a null sentinel that cannot be stolen.
 * 3. Submitting after stop() must throw instead of being silently dropped.
 */
TEST_CASE("test_MQStealThreadPool_stop_discards_queued_tasks") {
    MQStealThreadPool pool(1);
    std::latch blocker_started(1);
    std::latch release_blocker(1);
    std::atomic<int> executed{0};

    /** Occupy the only worker so that further tasks stay queued */
    pool.submit([&]() {
        blocker_started.count_down();
        release_blocker.wait();
    });
    blocker_started.wait();

    for (int i = 0; i < 4; ++i) {
        pool.submit([&]() { executed.fetch_add(1, std::memory_order_relaxed); });
    }

    /** stop() joins the worker, so release the blocker from the test thread after the stop
        flags and sentinels had time to be installed */
    std::thread stopper([&]() { pool.stop(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    release_blocker.count_down();
    stopper.join();

    /** The queued tasks must have been discarded, not executed */
    CHECK_EQ(executed.load(), 0);

    /** Submitting to a stopped pool must throw */
    CHECK_THROWS_AS(pool.submit([]() {}), std::logic_error);
}

/**
 * @brief test concurrent join()/stop() on the steal pools
 * 1. Two concurrent join() calls must not join the same worker thread twice (UB); every
 *    submitted task is executed exactly once.
 * 2. join() racing with stop() must complete without crash or hang; tasks still queued when
 *    stop() wins may be discarded, so no assertion is made on the executed count there.
 */
TEST_CASE("test_StealThreadPool_concurrent_join_stop") {
    for (int iter = 0; iter < 10; ++iter) {
        /** Concurrent join: all tasks must run exactly once */
        StealThreadPool pool(4);
        std::atomic<int> executed{0};
        for (int i = 0; i < 20; ++i) {
            pool.submit([&]() { executed.fetch_add(1, std::memory_order_relaxed); });
        }
        std::thread t1([&]() { pool.join(); });
        std::thread t2([&]() { pool.join(); });
        t1.join();
        t2.join();
        CHECK_EQ(executed.load(), 20);

        /** join racing stop: must not crash or hang */
        StealThreadPool pool2(4);
        for (int i = 0; i < 20; ++i) {
            pool2.submit([]() { std::this_thread::yield(); });
        }
        std::thread t3([&]() { pool2.join(); });
        std::thread t4([&]() { pool2.stop(); });
        t3.join();
        t4.join();
        CHECK_UNARY(pool2.done());
    }
}

TEST_CASE("test_GlobalStealThreadPool_concurrent_join_stop") {
    for (int iter = 0; iter < 10; ++iter) {
        GlobalStealThreadPool pool(4);
        std::atomic<int> executed{0};
        for (int i = 0; i < 20; ++i) {
            pool.submit([&]() { executed.fetch_add(1, std::memory_order_relaxed); });
        }
        std::thread t1([&]() { pool.join(); });
        std::thread t2([&]() { pool.join(); });
        t1.join();
        t2.join();
        CHECK_EQ(executed.load(), 20);

        GlobalStealThreadPool pool2(4);
        for (int i = 0; i < 20; ++i) {
            pool2.submit([]() { std::this_thread::yield(); });
        }
        std::thread t3([&]() { pool2.join(); });
        std::thread t4([&]() { pool2.stop(); });
        t3.join();
        t4.join();
        CHECK_UNARY(pool2.done());
    }
}

TEST_CASE("test_MQStealThreadPool_concurrent_join_stop") {
    for (int iter = 0; iter < 10; ++iter) {
        MQStealThreadPool pool(4);
        std::atomic<int> executed{0};
        for (int i = 0; i < 20; ++i) {
            pool.submit([&]() { executed.fetch_add(1, std::memory_order_relaxed); });
        }
        std::thread t1([&]() { pool.join(); });
        std::thread t2([&]() { pool.join(); });
        t1.join();
        t2.join();
        CHECK_EQ(executed.load(), 20);

        MQStealThreadPool pool2(4);
        for (int i = 0; i < 20; ++i) {
            pool2.submit([]() { std::this_thread::yield(); });
        }
        std::thread t3([&]() { pool2.join(); });
        std::thread t4([&]() { pool2.stop(); });
        t3.join();
        t4.join();
        CHECK_UNARY(pool2.done());
    }
}

#endif

/** @} */