/*
 * test_ResourceTlsVersionPool.cpp
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
#include "hikyuu/utilities/ResourceTlsVersionPool.h"
#include "hikyuu/utilities/Parameter.h"
#include "hikyuu/utilities/ResourceAsioPool.h"  // for AsyncResourceWithVersion

using namespace hku;

// 带版本的测试资源类
class VersionTestResource : public AsyncResourceWithVersion {
    PARAMETER_SUPPORT

public:
    VersionTestResource(const Parameter& param) {
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

    virtual ~VersionTestResource() {
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

int VersionTestResource::x = 0;
std::mutex VersionTestResource::m_mutex;

// 测试默认 MAX_POOL_SIZE
TEST_CASE("test_ResourceTlsVersionPool_default_limit") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    // 使用默认的 MAX_POOL_SIZE = 32
    CHECK_NOTHROW(ResourceTlsVersionPool<VersionTestResource>::init(param));

    // 验证可以正常获取实例
    auto& pool = ResourceTlsVersionPool<VersionTestResource>::getInstance();
    CHECK_EQ(pool.maxPoolSize(), 32);
    CHECK_EQ(pool.getVersion(), 0);
}

// 测试自定义 MAX_POOL_SIZE
TEST_CASE("test_ResourceTlsVersionPool_custom_limit") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    // 使用自定义的 MAX_POOL_SIZE = 50
    using CustomPool = ResourceTlsVersionPool<VersionTestResource, 50>;

    CHECK_NOTHROW(CustomPool::init(param));

    // 验证最大资源数
    auto& pool = CustomPool::getInstance();
    CHECK_EQ(pool.maxPoolSize(), 50);
    CHECK_EQ(pool.getVersion(), 0);
}

// 测试版本同步功能
TEST_CASE("test_ResourceTlsVersionPool_syncVersion") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceTlsVersionPool<VersionTestResource>::init(param);
    auto& pool = ResourceTlsVersionPool<VersionTestResource>::getInstance();

    // 初始版本为 0
    CHECK_EQ(pool.getVersion(), 0);

    // 同步新版本
    Parameter new_param;
    new_param.set<std::string>("test_param", "v2");
    pool.syncVersion(1, new_param);

    CHECK_EQ(pool.getVersion(), 1);

    // 再次同步相同版本（应该无操作）
    pool.syncVersion(1, new_param);
    CHECK_EQ(pool.getVersion(), 1);

    // 同步更高版本
    Parameter newer_param;
    newer_param.set<std::string>("test_param", "v3");
    pool.syncVersion(2, newer_param);

    CHECK_EQ(pool.getVersion(), 2);
}

// 测试资源版本检查 - 获取时检查
TEST_CASE("test_ResourceTlsVersionPool_resourceVersionCheck_get") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceTlsVersionPool<VersionTestResource>::init(param);
    auto& pool = ResourceTlsVersionPool<VersionTestResource>::getInstance();

    // 获取第一个资源（版本 0）
    auto res1_result = pool.get();
    CHECK(res1_result.has_value());
    auto res1 = std::move(res1_result.value());
    CHECK_NE(res1, nullptr);
    CHECK_EQ(res1->getVersion(), 0);
    CHECK_EQ(pool.count(), 1);

    // 归还资源
    res1.reset();
    CHECK_EQ(pool.idleCount(), 1);

    // 同步新版本
    Parameter new_param;
    new_param.set<std::string>("test_param", "v2");
    pool.syncVersion(1, new_param);

    // 再次获取资源，旧版本应被销毁，创建新版本
    auto res2_result = pool.get();
    CHECK(res2_result.has_value());
    auto res2 = std::move(res2_result.value());
    CHECK_EQ(res2->getVersion(), 1);
    CHECK_EQ(res2->getParam<std::string>("test_param"), "v2");
    CHECK_EQ(pool.count(), 1);  // 旧资源销毁，新资源创建，总数不变
}

// 测试资源版本检查 - 归还时检查
TEST_CASE("test_ResourceTlsVersionPool_resourceVersionCheck_return") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceTlsVersionPool<VersionTestResource>::init(param);
    auto& pool = ResourceTlsVersionPool<VersionTestResource>::getInstance();

    // 获取资源（版本 0）
    auto res1_result = pool.get();
    CHECK(res1_result.has_value());
    auto res1 = std::move(res1_result.value());
    CHECK_EQ(res1->getVersion(), 0);

    // 同步新版本
    Parameter new_param;
    new_param.set<std::string>("test_param", "v2");
    pool.syncVersion(1, new_param);

    // 归还旧版本资源，应该被销毁
    res1.reset();
    CHECK_EQ(pool.count(), 0);  // 旧资源被销毁
    CHECK_EQ(pool.idleCount(), 0);

    // 获取新版本资源
    auto res2_result = pool.get();
    CHECK(res2_result.has_value());
    auto res2 = std::move(res2_result.value());
    CHECK_EQ(res2->getVersion(), 1);
}

// 测试多线程协程执行
TEST_CASE("test_ResourceTlsVersionPool_multithread_coroutine") {
    Parameter param;
    param.set<std::string>("coroutine_test", "true");

    // 初始化默认参数（使用自定义 MAX_POOL_SIZE = 5）
    using TestPool = ResourceTlsVersionPool<VersionTestResource, 5>;
    TestPool::init(param);

    asio::io_context io_ctx;
    asio::thread_pool pool(4);  // 4个线程的线程池

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    // 在多个线程中启动协程
    for (int i = 0; i < 8; ++i) {
        asio::co_spawn(
          pool,
          [&, i]() -> asio::awaitable<void> {
              // 每个线程有自己的 thread_local 实例
              auto& local_pool = TestPool::getInstance();

              // 获取资源
              auto result = co_await local_pool.asyncGet(std::chrono::seconds(2));
              if (result) {
                  auto& res = result.value();
                  CHECK_NE(res, nullptr);
                  CHECK_EQ(res->getParam<std::string>("coroutine_test"), "true");
                  success_count++;

                  // 模拟一些工作
                  co_await asio::post(asio::use_awaitable);

                  res.reset();
              } else {
                  error_count++;
              }
          },
          asio::detached);
    }

    // 等待所有协程完成
    pool.join();

    // 验证结果
    CHECK_GT(success_count.load(), 0);
    HKU_INFO("Success count: {}, Error count: {}", success_count.load(), error_count.load());
}

// 测试版本隔离性（不同线程有不同版本）
TEST_CASE("test_ResourceTlsVersionPool_threadIsolation") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceTlsVersionPool<VersionTestResource>::init(param);

    std::atomic<int> thread1_version{-1};
    std::atomic<int> thread2_version{-1};

    // 线程1：同步到版本 1
    std::thread t1([&]() {
        auto& pool = ResourceTlsVersionPool<VersionTestResource>::getInstance();

        Parameter new_param;
        new_param.set<std::string>("test_param", "v2");
        pool.syncVersion(1, new_param);

        thread1_version = pool.getVersion();
    });

    // 线程2：同步到版本 2
    std::thread t2([&]() {
        auto& pool = ResourceTlsVersionPool<VersionTestResource>::getInstance();

        Parameter new_param;
        new_param.set<std::string>("test_param", "v3");
        pool.syncVersion(2, new_param);

        thread2_version = pool.getVersion();
    });

    t1.join();
    t2.join();

    // 验证每个线程有独立的版本
    CHECK_EQ(thread1_version.load(), 1);
    CHECK_EQ(thread2_version.load(), 2);
}
