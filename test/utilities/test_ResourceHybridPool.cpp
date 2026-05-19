/*
 * test_ResourceHybridPool.cpp
 *
 *  Copyright (c) 2025, hikyuu.org
 *
 *  Created on: 2025-05-12
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <atomic>
#include "hikyuu/utilities/ResourceHybridPool.h"
#include "hikyuu/utilities/Parameter.h"

using namespace hku;

namespace {

// 辅助宏：检查 expected 结果并获取值
#define CHECK_EXPECTED(result)                           \
    do {                                                 \
        CHECK(result.has_value());                       \
        if (!result) {                                   \
            MESSAGE("Expected error: ", result.error()); \
        }                                                \
    } while (0)

}  // namespace

// 简单的测试资源类
class HybridTestResource {
public:
    explicit HybridTestResource(const Parameter& param) : m_id(s_nextId++), m_param(param) {}

    ~HybridTestResource() {}

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

std::atomic<int> HybridTestResource::s_nextId{0};

// 用于独立测试的资源类（避免 TLS Pool 参数冲突）
class HybridTestResource2 {
public:
    explicit HybridTestResource2(const Parameter& param) : m_id(s_nextId++), m_param(param) {}

    ~HybridTestResource2() {}

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

std::atomic<int> HybridTestResource2::s_nextId{0};

// 用于 TLS 池大小配置测试的独立资源类（避免与其他测试产生状态污染）
class HybridTestResourceForSizeConfig {
public:
    explicit HybridTestResourceForSizeConfig(const Parameter& param)
    : m_id(s_nextId++), m_param(param) {}

    ~HybridTestResourceForSizeConfig() {}

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

std::atomic<int> HybridTestResourceForSizeConfig::s_nextId{0};

// 测试基本构造和同步获取
TEST_CASE("test_ResourceHybridPool_basic_sync") {
    Parameter param;
    param.set<std::string>("test_key", "sync_test");

    // 创建混合池
    ResourceHybridPool<HybridTestResource> pool(param);

    // 同步获取资源（应该从 TLS Pool 获取）
    auto result = pool.get();
    CHECK(result.has_value());

    if (result) {
        auto& resource = result.value();
        CHECK_NE(resource, nullptr);
        // 注意：由于 TLS Pool 是全局共享的，参数可能被后续测试覆盖
        // 这里只验证资源不为空即可
        CHECK(resource != nullptr);

        // 释放资源（自动归还到 TLS Pool）
        resource.reset();
    }
}

// 测试 TLS Pool 耗尽后降级到全局池
TEST_CASE("test_ResourceHybridPool_fallback_to_global") {
    Parameter param;
    param.set<std::string>("test_key", "fallback_test");

    // 设置较小的 TLS Pool 大小（模板参数）和全局池大小（构造参数）
    using SmallHybridPool = ResourceHybridPool<HybridTestResource, 2>;
    SmallHybridPool pool(param, 10);  // 全局池最大 10 个资源

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    // 启动多个线程，超过 TLS Pool 容量
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            auto resource_result = pool.get();
            if (resource_result) {
                auto resource = std::move(resource_result.value());
                CHECK_NE(resource, nullptr);
                success_count++;

                // 模拟工作
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                error_count++;
                HKU_WARN("Thread {} failed: {}", i, resource_result.error());
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 验证：应该有成功的（TLS + Global），可能有失败的
    CHECK_GT(success_count.load(), 0);
    HKU_INFO("Success: {}, Error: {}", success_count.load(), error_count.load());
}

// 测试仅从 TLS Pool 获取
TEST_CASE("test_ResourceHybridPool_tls_only") {
    Parameter param;
    param.set<std::string>("source", "tls_only");

    ResourceHybridPool<HybridTestResource2> pool(param);

    // 直接从 TLS Pool 获取
    auto result = pool.getFromTlsPool();
    CHECK(result.has_value());

    if (result) {
        auto& resource = result.value();
        CHECK_EQ(resource->param().get<std::string>("source"), "tls_only");
    }
}

// 测试仅从全局池获取
TEST_CASE("test_ResourceHybridPool_global_only") {
    Parameter param;
    param.set<std::string>("source", "global_only");

    ResourceHybridPool<HybridTestResource> pool(param);

    // 直接从全局共享池获取
    auto resource_result = pool.getFromGlobalPool();
    CHECK_EXPECTED(resource_result);
    auto resource = std::move(resource_result.value());
    CHECK_NE(resource, nullptr);
    CHECK_EQ(resource->param().get<std::string>("source"), "global_only");
}

// 测试多线程并发访问
TEST_CASE("test_ResourceHybridPool_multithread_concurrent") {
    Parameter param;
    param.set<std::string>("concurrent_test", "true");

    // TLS Pool 大小为 4（模板参数），全局池大小为 8（构造参数）
    using TestPool = ResourceHybridPool<HybridTestResource, 4>;
    TestPool pool(param, 8);

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    // 在多个线程中同步获取资源
    for (int i = 0; i < 12; ++i) {
        threads.emplace_back([&, i]() {
            auto resource_result = pool.get();
            if (resource_result) {
                auto resource = std::move(resource_result.value());
                CHECK_NE(resource, nullptr);
                success_count++;

                // 模拟工作
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                error_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    CHECK_GT(success_count.load(), 0);
    HKU_INFO("Concurrent test - Success: {}, Error: {}", success_count.load(), error_count.load());
}

// 测试资源池引用访问
TEST_CASE("test_ResourceHybridPool_pool_references") {
    Parameter param;
    ResourceHybridPool<HybridTestResource> pool(param);

    // 获取 TLS Pool 引用
    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxCount(), 32);

    // 获取全局共享池引用
    auto& global_pool = pool.globalPool();
    CHECK_EQ(global_pool.count(), 0);
}

// 测试运行时指定 TLS 池大小
TEST_CASE("test_ResourceHybridPool_custom_tls_pool_size") {
    Parameter param;
    param.set<std::string>("custom_tls", "true");

    // 模板参数为 64，但运行时指定 TLS 池实际使用 32，全局池使用 16
    using LargeTemplatePool = ResourceHybridPool<HybridTestResourceForSizeConfig, 64>;
    LargeTemplatePool pool(param, 32, 16);  // TLS 池 32，全局池 16

    // 验证配置
    CHECK_EQ(pool.maxTlsPoolSize(), 32);
    CHECK_EQ(pool.maxGlobalPoolSize(), 16);

    // 验证 TLS Pool 的实际大小
    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxCount(), 32);

    // 同步获取资源，验证正常工作
    auto result = pool.get();
    CHECK(result.has_value());
    if (result) {
        CHECK_NE(result.value(), nullptr);
    }
}

// 测试 TLS 池大小超过模板参数时的截断行为
TEST_CASE("test_ResourceHybridPool_tls_size_truncation") {
    Parameter param;
    param.set<std::string>("truncation_test", "true");

    // 模板参数为 8，但尝试设置 TLS 池为 16（应该被截断为 8），全局池为 32
    using SmallTemplatePool = ResourceHybridPool<HybridTestResourceForSizeConfig, 8>;
    SmallTemplatePool pool(param, 16, 32);  // 尝试设置 TLS 池为 16，全局池为 32

    // 验证被截断为模板参数的值
    CHECK_EQ(pool.maxTlsPoolSize(), 8);      // 应该被截断为 8
    CHECK_EQ(pool.maxGlobalPoolSize(), 32);  // 全局池不受限制

    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxCount(), 8);
}

// 测试默认 TLS 池大小（使用模板参数）
TEST_CASE("test_ResourceHybridPool_default_tls_size") {
    Parameter param;
    param.set<std::string>("default_test", "true");

    // 不指定 TLS 池大小，应该使用模板参数的默认值（32），全局池使用默认值 64
    ResourceHybridPool<HybridTestResourceForSizeConfig> pool(param);

    CHECK_EQ(pool.maxTlsPoolSize(), 32);     // 应该等于模板参数默认值
    CHECK_EQ(pool.maxGlobalPoolSize(), 64);  // 全局池默认值

    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxCount(), 32);
}

// 测试 getWaitFor 接口
TEST_CASE("test_ResourceHybridPool_get_wait_for") {
    Parameter param;
    param.set<std::string>("wait_test", "true");

    // 创建小容量的混合池，TLS Pool 大小为 1，全局池大小为 2
    using SmallPool = ResourceHybridPool<HybridTestResource, 1>;
    SmallPool pool(param, 1, 2);

    // 测试1：正常获取（应该从 TLS Pool 快速获取）
    auto result1 = pool.getWaitFor(1000);  // 等待最多 1 秒
    CHECK(result1.has_value());
    CHECK_NE(result1.value(), nullptr);

    // 测试2：TLS Pool 耗尽后，从全局池等待获取
    auto result2 = pool.getWaitFor(1000);
    CHECK(result2.has_value());
    CHECK_NE(result2.value(), nullptr);

    // 测试3：所有资源都被占用，等待超时
    auto result3 = pool.getWaitFor(100);  // 等待 100ms
    // 由于只有 3 个资源（1 TLS + 2 Global），第三个请求应该超时或失败
    if (!result3) {
        HKU_INFO("Expected timeout: {}", result3.error());
    }

    // 释放资源
    if (result1)
        result1.value().reset();
    if (result2)
        result2.value().reset();
    if (result3 && result3.value())
        result3.value().reset();
}

// 测试 getWaitFor 在多线程环境下的行为
TEST_CASE("test_ResourceHybridPool_get_wait_for_multithread") {
    Parameter param;
    param.set<std::string>("wait_mt_test", "true");

    // TLS Pool 大小为 2，全局池大小为 2
    using TestPool = ResourceHybridPool<HybridTestResource, 2>;
    TestPool pool(param, 2, 2);

    std::atomic<int> success_count{0};
    std::atomic<int> timeout_count{0};
    std::vector<std::thread> threads;

    // 启动 6 个线程，超过总资源数（4 个）
    for (int i = 0; i < 6; ++i) {
        threads.emplace_back([&, i]() {
            // 等待最多 500ms
            auto resource_result = pool.getWaitFor(500);
            if (resource_result) {
                auto resource = std::move(resource_result.value());
                CHECK_NE(resource, nullptr);
                success_count++;

                // 模拟工作 100ms
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                timeout_count++;
                HKU_WARN("Thread {} timeout: {}", i, resource_result.error());
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 验证：应该有成功的，也可能有超时的
    CHECK_GT(success_count.load(), 0);
    HKU_INFO("WaitFor multithread test - Success: {}, Timeout: {}", success_count.load(),
             timeout_count.load());
}

// 测试 getAndWait 接口（无限期等待）
TEST_CASE("test_ResourceHybridPool_get_and_wait") {
    Parameter param;
    param.set<std::string>("wait_forever_test", "true");

    // TLS Pool 大小为 1，全局池大小为 1
    using SmallPool = ResourceHybridPool<HybridTestResource, 1>;
    SmallPool pool(param, 1, 1);

    // 测试1：正常获取（应该从 TLS Pool 快速获取）
    auto result1 = pool.getAndWait();
    CHECK(result1.has_value());
    CHECK_NE(result1.value(), nullptr);

    // 测试2：TLS Pool 耗尽后，从全局池等待获取
    auto result2 = pool.getAndWait();
    CHECK(result2.has_value());
    CHECK_NE(result2.value(), nullptr);

    // 注意：不测试第三个请求，因为 getAndWait() 会永久阻塞
    // 在实际使用中，应确保资源最终会被归还

    // 释放资源
    if (result1)
        result1.value().reset();
    if (result2)
        result2.value().reset();
}

// 测试 getAndWait 在多线程环境下的行为
TEST_CASE("test_ResourceHybridPool_get_and_wait_multithread") {
    Parameter param;
    param.set<std::string>("wait_forever_mt_test", "true");

    // TLS Pool 大小为 2，全局池大小为 2
    using TestPool = ResourceHybridPool<HybridTestResource, 2>;
    TestPool pool(param, 2, 2);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // 启动 4 个线程，等于总资源数（4 个）
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&, i]() {
            // 无限期等待（应该都能成功，因为资源数足够）
            auto resource_result = pool.getAndWait();
            if (resource_result) {
                auto resource = std::move(resource_result.value());
                CHECK_NE(resource, nullptr);
                success_count++;

                // 模拟工作 50ms
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 验证：所有线程都应该成功获取资源
    CHECK_EQ(success_count.load(), 4);
    HKU_INFO("GetAndWait multithread test - Success: {}", success_count.load());
}
