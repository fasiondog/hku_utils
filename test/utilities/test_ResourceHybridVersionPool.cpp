/*
 * test_ResourceHybridVersionPool.cpp
 *
 *  Copyright (c) 2025, hikyuu.org
 *
 *  Created on: 2025-05-13
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <atomic>
#include "hikyuu/utilities/ResourceHybridVersionPool.h"
#include "hikyuu/utilities/Parameter.h"
#include "hikyuu/utilities/ResourceAsioPool.h"  // for AsyncResourceWithVersion

using namespace hku;

// 带版本的测试资源类（复用）
class HybridVersionTestResource : public AsyncResourceWithVersion {
    PARAMETER_SUPPORT

public:
    HybridVersionTestResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        m_id = fmt::format("Resource_{}", i);
        if (param.have("test_param")) {
            setParam<std::string>("test_param", param.get<std::string>("test_param"));
        }
        if (param.have("count")) {
            setParam<int>("count", param.get<int>("count"));
        }
    }

    virtual ~HybridVersionTestResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
    }

    void print() {
        printf("%s version: %d\n", m_id.c_str(), m_version);
    }

    int getId() const {
        return i;
    }

    std::string getIdStr() const {
        return m_id;
    }

private:
    static int x;
    static std::mutex m_mutex;
    int i = 0;
    std::string m_id;
};

int HybridVersionTestResource::x = 0;
std::mutex HybridVersionTestResource::m_mutex;

// 测试基本功能
TEST_CASE("test_ResourceHybridVersionPool_basic") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<HybridVersionTestResource> pool(param, 64);

    // 初始状态检查
    CHECK_EQ(pool.getVersion(), 0);
    CHECK_UNARY(pool.haveParam("test_param"));
    CHECK_EQ(pool.getParam<std::string>("test_param"), "v1");

    // 同步获取资源
    auto result = pool.get();
    CHECK(result.has_value());
    auto res = std::move(result.value());
    CHECK_NE(res, nullptr);
    CHECK_EQ(res->getVersion(), 0);
    CHECK_EQ(res->getParam<std::string>("test_param"), "v1");

    // 归还资源
    res.reset();
}

// 测试参数更新和版本递增
TEST_CASE("test_ResourceHybridVersionPool_setParam") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<HybridVersionTestResource> pool(param, 64);

    // 初始版本为 0
    CHECK_EQ(pool.getVersion(), 0);

    // 修改参数
    pool.setParam<std::string>("test_param", "v2");
    CHECK_EQ(pool.getVersion(), 1);
    CHECK_EQ(pool.getParam<std::string>("test_param"), "v2");

    // 再次修改参数
    pool.setParam<int>("count", 100);
    CHECK_EQ(pool.getVersion(), 2);
    CHECK_EQ(pool.getParam<int>("count"), 100);

    // 获取资源，应该是新版本
    auto result = pool.get();
    CHECK(result.has_value());
    auto res = std::move(result.value());
    CHECK_EQ(res->getVersion(), 2);
    CHECK_EQ(res->getParam<std::string>("test_param"), "v2");
    CHECK_EQ(res->getParam<int>("count"), 100);
}

// 测试整体参数替换
TEST_CASE("test_ResourceHybridVersionPool_setParameter") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<HybridVersionTestResource> pool(param, 64);

    // 获取旧版本资源
    auto res1_result = pool.get();
    CHECK(res1_result.has_value());
    auto res1 = std::move(res1_result.value());
    CHECK_EQ(res1->getVersion(), 0);

    // 整体替换参数
    Parameter new_param;
    new_param.set<std::string>("test_param", "v2");
    new_param.set<int>("count", 200);
    pool.setParameter(new_param);

    CHECK_EQ(pool.getVersion(), 1);

    // 归还旧资源
    res1.reset();

    // 获取新版本资源
    auto res2_result = pool.get();
    CHECK(res2_result.has_value());
    auto res2 = std::move(res2_result.value());
    CHECK_EQ(res2->getVersion(), 1);
    CHECK_EQ(res2->getParam<std::string>("test_param"), "v2");
    CHECK_EQ(res2->getParam<int>("count"), 200);
}

// 测试手动递增版本
TEST_CASE("test_ResourceHybridVersionPool_incVersion") {
    Parameter param;
    ResourceHybridVersionPool<HybridVersionTestResource> pool(param, 64);

    CHECK_EQ(pool.getVersion(), 0);

    pool.incVersion();
    CHECK_EQ(pool.getVersion(), 1);

    pool.incVersion();
    CHECK_EQ(pool.getVersion(), 2);
}

// 测试多线程并发访问
TEST_CASE("test_ResourceHybridVersionPool_multithreaded") {
    Parameter param;
    param.set<std::string>("test_param", "concurrent_test");

    ResourceHybridVersionPool<HybridVersionTestResource> pool(param, 64);

    const int num_threads = 8;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&pool, &success_count, &error_count, i]() {
            try {
                auto result = pool.get();
                if (result) {
                    auto res = std::move(result.value());
                    CHECK_NE(res, nullptr);
                    CHECK_EQ(res->getParam<std::string>("test_param"), "concurrent_test");
                    success_count++;

                    // 模拟一些工作
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } else {
                    error_count++;
                }
            } catch (const std::exception& e) {
                HKU_WARN("Thread {} failed: {}", i, e.what());
                error_count++;
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    CHECK_GT(success_count.load(), 0);
    HKU_INFO("Multithreaded test - Success: {}, Error: {}", success_count.load(),
             error_count.load());
}

// 测试协程异步获取
TEST_CASE("test_ResourceHybridVersionPool_asyncGet") {
    boost::asio::io_context ctx;

    boost::asio::co_spawn(
      ctx,
      [&]() -> boost::asio::awaitable<void> {
          Parameter param;
          param.set<std::string>("test_param", "async_test");

          ResourceHybridVersionPool<HybridVersionTestResource> pool(param, 64);

          // 异步获取资源
          auto result = co_await pool.asyncGet(std::chrono::seconds(5));
          CHECK(result.has_value());
          auto res = std::move(result.value());
          CHECK_NE(res, nullptr);
          CHECK_EQ(res->getVersion(), 0);
          CHECK_EQ(res->getParam<std::string>("test_param"), "async_test");

          co_return;
      },
      boost::asio::detached);

    ctx.run();
}

// 测试版本更新后旧资源的淘汰
TEST_CASE("test_ResourceHybridVersionPool_versionUpdate_resourceEviction") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<HybridVersionTestResource> pool(param, 64);

    // 获取一些资源
    auto res1_result = pool.get();
    CHECK(res1_result.has_value());
    auto res1 = std::move(res1_result.value());
    CHECK_EQ(res1->getVersion(), 0);

    auto res2_result = pool.get();
    CHECK(res2_result.has_value());
    auto res2 = std::move(res2_result.value());
    CHECK_EQ(res2->getVersion(), 0);

    // 更新参数（触发版本递增）
    pool.setParam<std::string>("test_param", "v2");
    CHECK_EQ(pool.getVersion(), 1);

    // 归还旧版本资源
    res1.reset();
    res2.reset();

    // 获取新资源，应该是新版本
    auto res3_result = pool.get();
    CHECK(res3_result.has_value());
    auto res3 = std::move(res3_result.value());
    CHECK_EQ(res3->getVersion(), 1);
    CHECK_EQ(res3->getParam<std::string>("test_param"), "v2");
}

// 测试 TLS Pool 和 Global Pool 的降级策略
TEST_CASE("test_ResourceHybridVersionPool_fallback_strategy") {
    Parameter param;
    param.set<std::string>("test_param", "fallback_test");

    // 使用小的全局池大小来测试降级
    ResourceHybridVersionPool<HybridVersionTestResource, 2> pool(param, 3);

    // 获取多个资源填满 TLS Pool
    auto res1_result = pool.get();
    CHECK(res1_result.has_value());
    auto res1 = std::move(res1_result.value());

    auto res2_result = pool.get();
    CHECK(res2_result.has_value());
    auto res2 = std::move(res2_result.value());

    // TLS Pool 已满，继续获取应该降级到 Global Pool
    auto res3_result = pool.get();
    CHECK(res3_result.has_value());
    auto res3 = std::move(res3_result.value());

    // 验证所有资源都有效
    CHECK_NE(res1, nullptr);
    CHECK_NE(res2, nullptr);
    CHECK_NE(res3, nullptr);
}
