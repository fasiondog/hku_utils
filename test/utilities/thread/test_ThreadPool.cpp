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

#endif

/** @} */