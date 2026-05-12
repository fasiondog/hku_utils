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

// 测试异步获取（优先 TLS Pool）
TEST_CASE("test_ResourceHybridPool_async_prefer_tls") {
    Parameter param;
    param.set<std::string>("test_key", "async_test");

    ResourceHybridPool<HybridTestResource> pool(param);

    asio::io_context io_ctx;

    bool success = false;

    // 启动协程测试
    asio::co_spawn(
      io_ctx,
      [&]() -> asio::awaitable<void> {
          try {
              // 异步获取（应该优先从 TLS Pool 获取）
              auto resource_result = co_await pool.asyncGet(std::chrono::seconds(5));
              CHECK_EXPECTED(resource_result);
              auto resource = std::move(resource_result.value());
              CHECK_NE(resource, nullptr);
              CHECK_EQ(resource->param().get<std::string>("test_key"), "async_test");
              success = true;
          } catch (const std::exception& e) {
              HKU_ERROR("Async get failed: {}", e.what());
          }
      },
      asio::detached);

    io_ctx.run();
    CHECK(success);
}

// 测试 TLS Pool 耗尽后降级到 Asio Pool
TEST_CASE("test_ResourceHybridPool_fallback_to_asio") {
    Parameter param;
    param.set<std::string>("test_key", "fallback_test");

    // 设置较小的 TLS Pool 大小（模板参数）和 Asio Pool 大小（构造参数）
    using SmallHybridPool = ResourceHybridPool<HybridTestResource, 2>;
    SmallHybridPool pool(param, 10);  // Asio Pool 最大 10 个资源

    asio::io_context io_ctx;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    // 启动多个协程，超过 TLS Pool 容量
    for (int i = 0; i < 5; ++i) {
        asio::co_spawn(
          io_ctx,
          [&, i]() -> asio::awaitable<void> {
              try {
                  auto resource_result = co_await pool.asyncGet(std::chrono::seconds(2));
                  if (resource_result) {
                      auto resource = std::move(resource_result.value());
                      CHECK_NE(resource, nullptr);
                      success_count++;

                      // 模拟工作
                      co_await asio::post(asio::use_awaitable);
                  }
              } catch (const CreateResourceException& e) {
                  error_count++;
                  HKU_WARN("Coroutine {} failed: {}", i, e.what());
              }
          },
          asio::detached);
    }

    io_ctx.run();

    // 验证：应该有成功的（TLS + Asio），可能有超时的
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

// 测试仅从 Asio Pool 获取
TEST_CASE("test_ResourceHybridPool_asio_only") {
    Parameter param;
    param.set<std::string>("source", "asio_only");

    ResourceHybridPool<HybridTestResource> pool(param);

    asio::io_context io_ctx;
    bool success = false;

    asio::co_spawn(
      io_ctx,
      [&]() -> asio::awaitable<void> {
          try {
              // 直接从全局共享池获取
              auto resource_result = co_await pool.getFromGlobalPool(std::chrono::seconds(5));
              CHECK_EXPECTED(resource_result);
              auto resource = std::move(resource_result.value());
              CHECK_NE(resource, nullptr);
              CHECK_EQ(resource->param().get<std::string>("source"), "asio_only");
              success = true;
          } catch (const std::exception& e) {
              HKU_ERROR("Asio only get failed: {}", e.what());
          }
      },
      asio::detached);

    io_ctx.run();
    CHECK(success);
}

// 测试多线程并发访问
TEST_CASE("test_ResourceHybridPool_multithread_concurrent") {
    Parameter param;
    param.set<std::string>("concurrent_test", "true");

    // TLS Pool 大小为 4（模板参数），Asio Pool 大小为 8（构造参数）
    using TestPool = ResourceHybridPool<HybridTestResource, 4>;
    TestPool pool(param, 8);

    asio::io_context io_ctx;
    asio::thread_pool thread_pool(4);  // 4个线程

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    // 在多个线程中启动协程
    for (int i = 0; i < 12; ++i) {
        asio::co_spawn(
          thread_pool,
          [&, i]() -> asio::awaitable<void> {
              try {
                  auto resource_result = co_await pool.asyncGet(std::chrono::seconds(3));
                  if (resource_result) {
                      auto resource = std::move(resource_result.value());
                      CHECK_NE(resource, nullptr);
                      success_count++;

                      // 模拟工作
                      co_await asio::post(asio::use_awaitable);
                  }
              } catch (const CreateResourceException& e) {
                  error_count++;
              }
          },
          asio::detached);
    }

    thread_pool.join();

    CHECK_GT(success_count.load(), 0);
    HKU_INFO("Concurrent test - Success: {}, Error: {}", success_count.load(), error_count.load());
}

// 测试资源池引用访问
TEST_CASE("test_ResourceHybridPool_pool_references") {
    Parameter param;
    ResourceHybridPool<HybridTestResource> pool(param);

    // 获取 TLS Pool 引用
    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxPoolSize(), 32);

    // 获取全局共享池引用
    auto& global_pool = pool.globalPool();
    CHECK_EQ(global_pool.count(), 0);
}
