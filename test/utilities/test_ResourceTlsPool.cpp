/*
 * test_ResourceTlsPool.cpp
 *
 *  Copyright (c) 2025, hikyuu.org
 *
 *  Created on: 2025-03-17
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <atomic>
#include <future>
#include "hikyuu/utilities/ResourceTlsPool.h"
#include "hikyuu/utilities/Parameter.h"

using namespace hku;

// 用于默认限制测试的资源类
class TlsPoolTestResourceDefault {
public:
    explicit TlsPoolTestResourceDefault(const Parameter& param)
    : m_id(s_nextId++), m_param(param) {}

    ~TlsPoolTestResourceDefault() {}

    int id() const {
        return m_id;
    }

    const Parameter& param() const {
        return m_param;
    }

private:
    int m_id;
    Parameter m_param;
    static std::atomic<int> s_nextId;
};

std::atomic<int> TlsPoolTestResourceDefault::s_nextId{0};

// 用于自定义限制测试的资源类（避免与默认测试产生状态污染）
class TlsPoolTestResourceCustom {
public:
    explicit TlsPoolTestResourceCustom(const Parameter& param) : m_id(s_nextId++), m_param(param) {}

    ~TlsPoolTestResourceCustom() {}

    int id() const {
        return m_id;
    }

    const Parameter& param() const {
        return m_param;
    }

private:
    int m_id;
    Parameter m_param;
    static std::atomic<int> s_nextId;
};

std::atomic<int> TlsPoolTestResourceCustom::s_nextId{0};

// 用于多线程测试的资源类（避免与其他测试产生状态污染）
class TlsPoolTestResourceMultiThread {
public:
    explicit TlsPoolTestResourceMultiThread(const Parameter& param)
    : m_id(s_nextId++), m_param(param) {}

    ~TlsPoolTestResourceMultiThread() {}

    int id() const {
        return m_id;
    }

    const Parameter& param() const {
        return m_param;
    }

private:
    int m_id;
    Parameter m_param;
    static std::atomic<int> s_nextId;
};

std::atomic<int> TlsPoolTestResourceMultiThread::s_nextId{0};

// Resource class tracking its live instances (for the destructor leak and the cross-thread tests)
class TlsPoolTestResourceLifetime {
public:
    explicit TlsPoolTestResourceLifetime(const Parameter& param) : m_param(param) {
        s_alive.fetch_add(1, std::memory_order_relaxed);
    }

    ~TlsPoolTestResourceLifetime() {
        s_alive.fetch_sub(1, std::memory_order_relaxed);
    }

    static std::atomic<int> s_alive;

private:
    Parameter m_param;
};

std::atomic<int> TlsPoolTestResourceLifetime::s_alive{0};

// 测试默认 MAX_POOL_SIZE
TEST_CASE("test_ResourceTlsPool_default_limit") {
    Parameter param;

    // 使用默认的 MAX_POOL_SIZE = 2
    // init 只接受 param 参数
    CHECK_NOTHROW(ResourceTlsPool<TlsPoolTestResourceDefault>::init(param));

    // 验证可以正常获取实例
    auto& pool = ResourceTlsPool<TlsPoolTestResourceDefault>::getInstance();
    CHECK_EQ(pool.maxCount(), 2);  // 默认模板参数为 2
}

// 测试自定义 MAX_POOL_SIZE_LIMIT
TEST_CASE("test_ResourceTlsPool_custom_limit") {
    Parameter param;

    // 使用自定义的 MAX_POOL_SIZE_LIMIT = 50
    using CustomPool = ResourceTlsPool<TlsPoolTestResourceCustom, 50>;

    CHECK_NOTHROW(CustomPool::init(param));

    // 验证最大资源数
    auto& pool = CustomPool::getInstance();
    CHECK_EQ(pool.maxCount(), 50);
}

// 测试多线程资源获取
TEST_CASE("test_ResourceTlsPool_multithread") {
    Parameter param;
    param.set<std::string>("thread_test", "true");

    // 初始化默认参数（使用自定义 MAX_POOL_SIZE = 5）
    using TestPool = ResourceTlsPool<TlsPoolTestResourceMultiThread, 5>;
    TestPool::init(param);

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    // 在多个线程中获取资源
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&, i]() {
            // 每个线程有自己的 thread_local 实例
            auto& local_pool = TestPool::getInstance();

            // 获取资源（同步调用）
            try {
                auto result = local_pool.get();
                if (result) {
                    auto& res = result.value();
                    CHECK_NE(res, nullptr);
                    // 注意：由于 thread_local 单例的特性，不同线程可能在 init() 之前
                    // 就创建了实例，导致参数不一致。因此这里只验证资源有效性，
                    // 不检查特定参数值，避免测试间状态污染。
                    success_count++;

                    // 模拟一些工作
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));

                    // 资源会自动归还（通过 shared_ptr 析构）
                } else {
                    error_count++;
                    HKU_ERROR("Thread {} failed to get resource: {}", i, result.error());
                }
            } catch (const std::exception& e) {
                error_count++;
                HKU_ERROR("Thread {} exception: {}", i, e.what());
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 验证结果
    CHECK_GT(success_count.load(), 0);
    HKU_INFO("Success count: {}, Error count: {}", success_count.load(), error_count.load());
}

/**
 * @par 检测点
 * 1. 析构时按环形分布 (m_head + i) % LIMIT 释放全部空闲槽位资源，不再按 [0, m_freeCount)
 *    连续下标错误释放（旧实现会漏删 wrap-around 的槽位导致泄漏）
 * 2. 线程退出后所有资源均被释放（存活计数归零）
 */
TEST_CASE("test_ResourceTlsPool_destructor_releases_ring_slots") {
    using TestPool = ResourceTlsPool<TlsPoolTestResourceLifetime, 3>;
    Parameter param;
    TestPool::init(param);

    int alive_before = TlsPoolTestResourceLifetime::s_alive.load();

    std::thread worker([]() {
        auto& pool = TestPool::getInstance();

        // Build the ring state: after the sequence below the idle slots are 1 and 2 while
        // slot 0 is null (head=1, freeCount=2). The old destructor iterating [0, freeCount)
        // deleted slot 0 (null) and slot 1, leaking the resource stored at slot 2.
        auto r1 = pool.get();
        auto r2 = pool.get();
        auto r3 = pool.get();
        REQUIRE(r1.has_value());
        REQUIRE(r2.has_value());
        REQUIRE(r3.has_value());
        CHECK_EQ(pool.count(), 3);
        CHECK_EQ(pool.idleCount(), 0);

        r1.value().reset();  // list[0] = r1, tail=1, freeCount=1
        r2.value().reset();  // list[1] = r2, tail=2, freeCount=2

        auto r4 = pool.get();  // dequeues list[0] (=r1), head=1, freeCount=1
        REQUIRE(r4);
        r4.value().reset();  // list[2] = r1, tail=0, freeCount=2; idle slots are 1 and 2

        CHECK_EQ(pool.count(), 3);
        CHECK_EQ(pool.idleCount(), 2);

        // r3 is returned by its destructor at scope end; the thread-local pool is destroyed
        // at thread exit and must release everything it still holds
    });
    worker.join();

    CHECK_EQ(TlsPoolTestResourceLifetime::s_alive.load(), alive_before);
}

/**
 * @par 检测点
 * 1. 跨线程归还：资源在异线程被安全删除，不触碰 owner 线程的 thread_local 池状态（无 UB）
 * 2. 文档化限制（见类注释 Cross-thread safety check mechanism）：池记账仅由 owner 线程维护，
 *    跨线程归还后容量不恢复（count 不递减）
 */
TEST_CASE("test_ResourceTlsPool_cross_thread_return") {
    using TestPool = ResourceTlsPool<TlsPoolTestResourceLifetime, 2>;
    using ResourcePtr = typename TestPool::ResourcePtr;
    Parameter param;
    TestPool::init(param);

    int alive_before = TlsPoolTestResourceLifetime::s_alive.load();

    std::promise<ResourcePtr> to_consumer;
    std::promise<void> done;
    std::future<void> done_future = done.get_future();

    std::thread producer([&]() {
        auto& pool = TestPool::getInstance();
        auto r1 = pool.get();
        REQUIRE(r1.has_value());
        CHECK_EQ(pool.count(), 1);

        // Hand the resource over to the other thread
        to_consumer.set_value(std::move(r1.value()));
        done_future.wait();

        // Documented limitation: the count is maintained by the owner thread only, the
        // cross-thread return does not recover the consumed capacity
        CHECK_EQ(pool.count(), 1);
    });

    std::thread consumer([&]() {
        auto r1 = to_consumer.get_future().get();

        // Destroying in a different thread triggers the cross-thread return path: the
        // resource is deleted here and the owner pool state is not touched
        r1.reset();

        done.set_value();
    });

    producer.join();
    consumer.join();

    // The resource handed abroad must have been deleted
    CHECK_EQ(TlsPoolTestResourceLifetime::s_alive.load(), alive_before);
}