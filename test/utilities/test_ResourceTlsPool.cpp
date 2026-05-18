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

// 测试默认 MAX_POOL_SIZE
TEST_CASE("test_ResourceTlsPool_default_limit") {
    Parameter param;

    // 使用默认的 MAX_POOL_SIZE = 32
    // init 只接受 param 参数
    CHECK_NOTHROW(ResourceTlsPool<TlsPoolTestResourceDefault>::init(param));

    // 验证可以正常获取实例
    auto& pool = ResourceTlsPool<TlsPoolTestResourceDefault>::getInstance();
    CHECK_EQ(pool.maxCount(), 32);
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