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

using namespace hku;

namespace {

// 为每个测试定义独特的资源类型（避免测试间状态污染）

// test_ResourceTlsVersionPool_default_limit
class DefaultLimitResource {
    PARAMETER_SUPPORT
public:
    DefaultLimitResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        m_id = fmt::format("Resource_{}", i);
        if (param.have("test_param")) {
            setParam<std::string>("test_param", param.get<std::string>("test_param"));
        }
    }
    virtual ~DefaultLimitResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
    }
    int getId() const {
        return i;
    }
    std::string getIdStr() const {
        return m_id;
    }
    int getVersion() const {
        return m_version;
    }
    void setVersion(int version) {
        m_version = version;
    }

private:
    static int x;
    static std::mutex m_mutex;
    int i = 0;
    int m_version = 0;
    std::string m_id;
};
int DefaultLimitResource::x = 0;
std::mutex DefaultLimitResource::m_mutex;

// test_ResourceTlsVersionPool_custom_limit
class CustomLimitResource {
    PARAMETER_SUPPORT
public:
    CustomLimitResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        m_id = fmt::format("Resource_{}", i);
        if (param.have("test_param")) {
            setParam<std::string>("test_param", param.get<std::string>("test_param"));
        }
    }
    virtual ~CustomLimitResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
    }
    int getId() const {
        return i;
    }
    std::string getIdStr() const {
        return m_id;
    }
    int getVersion() const {
        return m_version;
    }
    void setVersion(int version) {
        m_version = version;
    }

private:
    static int x;
    static std::mutex m_mutex;
    int i = 0;
    int m_version = 0;
    std::string m_id;
};
int CustomLimitResource::x = 0;
std::mutex CustomLimitResource::m_mutex;

// test_ResourceTlsVersionPool_syncVersion
class SyncVersionResource {
    PARAMETER_SUPPORT
public:
    SyncVersionResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        m_id = fmt::format("Resource_{}", i);
        if (param.have("test_param")) {
            setParam<std::string>("test_param", param.get<std::string>("test_param"));
        }
    }
    virtual ~SyncVersionResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
    }
    int getId() const {
        return i;
    }
    std::string getIdStr() const {
        return m_id;
    }
    int getVersion() const {
        return m_version;
    }
    void setVersion(int version) {
        m_version = version;
    }

private:
    static int x;
    static std::mutex m_mutex;
    int i = 0;
    int m_version = 0;
    std::string m_id;
};
int SyncVersionResource::x = 0;
std::mutex SyncVersionResource::m_mutex;

// test_ResourceTlsVersionPool_resourceVersionCheck_get
class VersionCheckGetResource {
    PARAMETER_SUPPORT
public:
    VersionCheckGetResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        m_id = fmt::format("Resource_{}", i);
        if (param.have("test_param")) {
            setParam<std::string>("test_param", param.get<std::string>("test_param"));
        }
    }
    virtual ~VersionCheckGetResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
    }
    int getId() const {
        return i;
    }
    std::string getIdStr() const {
        return m_id;
    }
    int getVersion() const {
        return m_version;
    }
    void setVersion(int version) {
        m_version = version;
    }

private:
    static int x;
    static std::mutex m_mutex;
    int i = 0;
    int m_version = 0;
    std::string m_id;
};
int VersionCheckGetResource::x = 0;
std::mutex VersionCheckGetResource::m_mutex;

// test_ResourceTlsVersionPool_resourceVersionCheck_return
class VersionCheckReturnResource {
    PARAMETER_SUPPORT
public:
    VersionCheckReturnResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        m_id = fmt::format("Resource_{}", i);
        if (param.have("test_param")) {
            setParam<std::string>("test_param", param.get<std::string>("test_param"));
        }
    }
    virtual ~VersionCheckReturnResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
    }
    int getId() const {
        return i;
    }
    std::string getIdStr() const {
        return m_id;
    }
    int getVersion() const {
        return m_version;
    }
    void setVersion(int version) {
        m_version = version;
    }

private:
    static int x;
    static std::mutex m_mutex;
    int i = 0;
    int m_version = 0;
    std::string m_id;
};
int VersionCheckReturnResource::x = 0;
std::mutex VersionCheckReturnResource::m_mutex;

// test_ResourceTlsVersionPool_multithread_coroutine
class MultithreadCoroutineResource {
    PARAMETER_SUPPORT
public:
    MultithreadCoroutineResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        m_id = fmt::format("Resource_{}", i);
        if (param.have("coroutine_test")) {
            setParam<std::string>("coroutine_test", param.get<std::string>("coroutine_test"));
        }
    }
    virtual ~MultithreadCoroutineResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
    }
    int getId() const {
        return i;
    }
    std::string getIdStr() const {
        return m_id;
    }
    int getVersion() const {
        return m_version;
    }
    void setVersion(int version) {
        m_version = version;
    }

private:
    static int x;
    static std::mutex m_mutex;
    int i = 0;
    int m_version = 0;
    std::string m_id;
};
int MultithreadCoroutineResource::x = 0;
std::mutex MultithreadCoroutineResource::m_mutex;

// test_ResourceTlsVersionPool_threadIsolation
class ThreadIsolationResource {
    PARAMETER_SUPPORT
public:
    ThreadIsolationResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        m_id = fmt::format("Resource_{}", i);
        if (param.have("test_param")) {
            setParam<std::string>("test_param", param.get<std::string>("test_param"));
        }
    }
    virtual ~ThreadIsolationResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
    }
    int getId() const {
        return i;
    }
    std::string getIdStr() const {
        return m_id;
    }
    int getVersion() const {
        return m_version;
    }
    void setVersion(int version) {
        m_version = version;
    }

private:
    static int x;
    static std::mutex m_mutex;
    int i = 0;
    int m_version = 0;
    std::string m_id;
};
int ThreadIsolationResource::x = 0;
std::mutex ThreadIsolationResource::m_mutex;

}  // namespace

// 测试默认限制
TEST_CASE("test_ResourceTlsVersionPool_default_limit") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    using TestPool = ResourceTlsVersionPool<DefaultLimitResource>;
    TestPool::init(param);

    // 验证可以正常获取实例
    auto& pool = TestPool::getInstance();
    CHECK_EQ(pool.maxCount(), 32);
    CHECK_EQ(pool.getVersion(), 0);
}

// 测试自定义限制
TEST_CASE("test_ResourceTlsVersionPool_custom_limit") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    using TestPool = ResourceTlsVersionPool<CustomLimitResource, 5>;
    TestPool::init(param);

    // 验证最大资源数
    auto& pool = TestPool::getInstance();
    CHECK_EQ(pool.maxCount(), 5);
    CHECK_EQ(pool.getVersion(), 0);
}

// 测试同步版本更新
TEST_CASE("test_ResourceTlsVersionPool_syncVersion") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    using TestPool = ResourceTlsVersionPool<SyncVersionResource>;
    TestPool::init(param);

    // 初始版本为 0
    auto& pool = TestPool::getInstance();
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

// 测试资源版本检查 - get
TEST_CASE("test_ResourceTlsVersionPool_resourceVersionCheck_get") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    using TestPool = ResourceTlsVersionPool<VersionCheckGetResource>;
    TestPool::init(param);

    // 获取第一个资源（版本 0）
    auto res1_result = TestPool::getInstance().get();
    CHECK(res1_result.has_value());
    auto res1 = std::move(res1_result.value());
    CHECK_NE(res1, nullptr);
    CHECK_EQ(res1->getVersion(), 0);
    CHECK_EQ(TestPool::getInstance().count(), 1);

    // 归还资源
    res1.reset();
    CHECK_EQ(TestPool::getInstance().idleCount(), 1);

    // 同步新版本
    Parameter new_param;
    new_param.set<std::string>("test_param", "v2");
    TestPool::getInstance().syncVersion(1, new_param);

    // 再次获取资源，旧版本应被销毁，创建新版本
    auto res2_result = TestPool::getInstance().get();
    CHECK(res2_result.has_value());
    auto res2 = std::move(res2_result.value());
    CHECK_EQ(res2->getVersion(), 1);
    CHECK_EQ(res2->getParam<std::string>("test_param"), "v2");
    CHECK_EQ(TestPool::getInstance().count(), 1);  // 旧资源销毁，新资源创建，总数不变
}

// 测试资源版本检查 - 归还时检查
TEST_CASE("test_ResourceTlsVersionPool_resourceVersionCheck_return") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    using TestPool = ResourceTlsVersionPool<VersionCheckReturnResource>;
    TestPool::init(param);

    // 获取资源（版本 0）
    auto res1_result = TestPool::getInstance().get();
    CHECK(res1_result.has_value());
    auto res1 = std::move(res1_result.value());
    CHECK_EQ(res1->getVersion(), 0);

    // 同步新版本
    Parameter new_param;
    new_param.set<std::string>("test_param", "v2");
    TestPool::getInstance().syncVersion(1, new_param);

    // 归还旧版本资源，应该被销毁
    res1.reset();
    CHECK_EQ(TestPool::getInstance().count(), 0);  // 旧资源被销毁
    CHECK_EQ(TestPool::getInstance().idleCount(), 0);

    // 获取新版本资源
    auto res2_result = TestPool::getInstance().get();
    CHECK(res2_result.has_value());
    auto res2 = std::move(res2_result.value());
    CHECK_EQ(res2->getVersion(), 1);
}

// 测试多线程资源获取（注意：ResourceTlsVersionPool 是 thread_local 的，
// 每个线程有独立的池实例，此测试主要验证线程隔离性）
TEST_CASE("test_ResourceTlsVersionPool_multithread") {
    Parameter param;
    param.set<std::string>("thread_test", "true");

    // 初始化默认参数（使用自定义 MAX_POOL_SIZE = 5）
    using TestPool = ResourceTlsVersionPool<MultithreadCoroutineResource, 5>;
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

// 测试线程隔离性
TEST_CASE("test_ResourceTlsVersionPool_threadIsolation") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    using TestPool = ResourceTlsVersionPool<ThreadIsolationResource>;
    TestPool::init(param);

    std::atomic<int> thread1_version{-1};
    std::atomic<int> thread2_version{-1};

    // 线程1：同步到版本 1
    std::thread t1([&]() {
        auto& pool = TestPool::getInstance();

        Parameter new_param;
        new_param.set<std::string>("test_param", "v2");
        pool.syncVersion(1, new_param);

        thread1_version = pool.getVersion();
    });

    // 线程2：同步到版本 2
    std::thread t2([&]() {
        auto& pool = TestPool::getInstance();

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