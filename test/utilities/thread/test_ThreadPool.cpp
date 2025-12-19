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

#endif

/** @} */