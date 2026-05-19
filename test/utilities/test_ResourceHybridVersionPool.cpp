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

using namespace hku;

namespace {

// 为每个测试定义独特的资源类型（避免测试间状态污染）

// test_ResourceHybridVersionPool_basic
class BasicTestResource {
    PARAMETER_SUPPORT
public:
    BasicTestResource(const Parameter& param) {
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
    virtual ~BasicTestResource() {
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
int BasicTestResource::x = 0;
std::mutex BasicTestResource::m_mutex;

// test_ResourceHybridVersionPool_setParam
class SetParamTestResource {
    PARAMETER_SUPPORT
public:
    SetParamTestResource(const Parameter& param) {
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
    virtual ~SetParamTestResource() {
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
int SetParamTestResource::x = 0;
std::mutex SetParamTestResource::m_mutex;

// test_ResourceHybridVersionPool_setParameter
class SetParameterTestResource {
    PARAMETER_SUPPORT
public:
    SetParameterTestResource(const Parameter& param) {
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
    virtual ~SetParameterTestResource() {
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
int SetParameterTestResource::x = 0;
std::mutex SetParameterTestResource::m_mutex;

// test_ResourceHybridVersionPool_multithreaded
class MultithreadTestResource {
    PARAMETER_SUPPORT
public:
    MultithreadTestResource(const Parameter& param) {
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
    virtual ~MultithreadTestResource() {
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
int MultithreadTestResource::x = 0;
std::mutex MultithreadTestResource::m_mutex;

// test_ResourceHybridVersionPool_versionUpdate_resourceEviction
class VersionUpdateTestResource {
    PARAMETER_SUPPORT
public:
    VersionUpdateTestResource(const Parameter& param) {
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
    virtual ~VersionUpdateTestResource() {
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
int VersionUpdateTestResource::x = 0;
std::mutex VersionUpdateTestResource::m_mutex;

// test_ResourceHybridVersionPool_fallback_strategy
class FallbackTestResource {
    PARAMETER_SUPPORT
public:
    FallbackTestResource(const Parameter& param) {
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
    virtual ~FallbackTestResource() {
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
int FallbackTestResource::x = 0;
std::mutex FallbackTestResource::m_mutex;

// test_ResourceHybridVersionPool_incVersion
class IncVersionTestResource {
    PARAMETER_SUPPORT
public:
    IncVersionTestResource(const Parameter& param) {
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
    virtual ~IncVersionTestResource() {
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
int IncVersionTestResource::x = 0;
std::mutex IncVersionTestResource::m_mutex;

}  // namespace

// 测试基本功能
TEST_CASE("test_ResourceHybridVersionPool_basic") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<BasicTestResource> pool(param, 64);

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

    ResourceHybridVersionPool<SetParamTestResource> pool(param, 64);

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

    ResourceHybridVersionPool<SetParameterTestResource> pool(param, 64);

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

// 测试版本号递增
TEST_CASE("test_ResourceHybridVersionPool_incVersion") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<IncVersionTestResource> pool(param, 64);

    CHECK_EQ(pool.getVersion(), 0);

    pool.incVersion();
    CHECK_EQ(pool.getVersion(), 1);

    pool.incVersion();
    CHECK_EQ(pool.getVersion(), 2);
}

// 测试多线程并发
TEST_CASE("test_ResourceHybridVersionPool_multithreaded") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<MultithreadTestResource> pool(param, 64);

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
                    CHECK_EQ(res->getParam<std::string>("test_param"), "v1");
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

// 测试版本更新后旧资源的淘汰
TEST_CASE("test_ResourceHybridVersionPool_versionUpdate_resourceEviction") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<VersionUpdateTestResource> pool(param, 64);

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

// 测试降级策略：TLS Pool 满时回退到全局池
TEST_CASE("test_ResourceHybridVersionPool_fallback_strategy") {
    Parameter param;
    param.set<std::string>("test_param", "v1");

    ResourceHybridVersionPool<FallbackTestResource> pool(param, 64);

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

// 测试运行时指定 TLS 池大小
TEST_CASE("test_ResourceHybridVersionPool_custom_tls_pool_size") {
    Parameter param;
    param.set<std::string>("custom_tls", "true");

    // 模板参数为 64，但运行时指定 TLS 池实际使用 16，全局池使用 32
    using LargeTemplatePool = ResourceHybridVersionPool<FallbackTestResource, 64>;
    LargeTemplatePool pool(param, 16, 32);  // TLS 池 16，全局池 32

    // 验证配置
    CHECK_EQ(pool.maxTlsPoolSize(), 16);
    CHECK_EQ(pool.maxGlobalPoolSize(), 32);

    // 验证 TLS Pool 的实际大小
    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxCount(), 16);

    // 同步获取资源，验证正常工作
    auto result = pool.get();
    CHECK(result.has_value());
    if (result) {
        CHECK_NE(result.value(), nullptr);
    }
}

// 测试 TLS 池大小超过模板参数时的截断行为
TEST_CASE("test_ResourceHybridVersionPool_tls_size_truncation") {
    Parameter param;
    param.set<std::string>("truncation_test", "true");

    // 模板参数为 8，但尝试设置 TLS 池为 16（应该被截断为 8），全局池为 32
    using SmallTemplatePool = ResourceHybridVersionPool<FallbackTestResource, 8>;
    SmallTemplatePool pool(param, 16, 32);  // 尝试设置 TLS 池为 16，全局池为 32

    // 验证被截断为模板参数的值
    CHECK_EQ(pool.maxTlsPoolSize(), 8);      // 应该被截断为 8
    CHECK_EQ(pool.maxGlobalPoolSize(), 32);  // 全局池不受限制

    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxCount(), 8);
}

// 测试默认 TLS 池大小（使用模板参数）
TEST_CASE("test_ResourceHybridVersionPool_default_tls_size") {
    Parameter param;
    param.set<std::string>("default_test", "true");

    // 不指定 TLS 池大小，应该使用模板参数的默认值（32），全局池使用默认值 64
    ResourceHybridVersionPool<FallbackTestResource> pool(param);

    CHECK_EQ(pool.maxTlsPoolSize(), 32);     // 应该等于模板参数默认值
    CHECK_EQ(pool.maxGlobalPoolSize(), 64);  // 全局池默认值

    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxCount(), 32);
}

// 测试 TLS Pool 和 Global Pool 的降级策略 (小池测试)
TEST_CASE("test_ResourceHybridVersionPool_fallback_strategy_small_pool") {
    Parameter param;
    param.set<std::string>("test_param", "fallback_test");

    // 使用小的全局池大小来测试降级
    ResourceHybridVersionPool<FallbackTestResource, 2> pool(param, 3);

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

// 测试 getWaitFor 接口
TEST_CASE("test_ResourceHybridVersionPool_get_wait_for") {
    Parameter param;
    param.set<std::string>("wait_test", "true");

    // 创建小容量的混合池，TLS Pool 大小为 1，全局池大小为 2
    using SmallPool = ResourceHybridVersionPool<FallbackTestResource, 1>;
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
TEST_CASE("test_ResourceHybridVersionPool_get_wait_for_multithread") {
    Parameter param;
    param.set<std::string>("wait_mt_test", "true");

    // TLS Pool 大小为 2，全局池大小为 2
    using TestPool = ResourceHybridVersionPool<MultithreadTestResource, 2>;
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
TEST_CASE("test_ResourceHybridVersionPool_get_and_wait") {
    Parameter param;
    param.set<std::string>("wait_forever_test", "true");

    // TLS Pool 大小为 1，全局池大小为 1
    using SmallPool = ResourceHybridVersionPool<FallbackTestResource, 1>;
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
TEST_CASE("test_ResourceHybridVersionPool_get_and_wait_multithread") {
    Parameter param;
    param.set<std::string>("wait_forever_mt_test", "true");

    // TLS Pool 大小为 2，全局池大小为 2
    using TestPool = ResourceHybridVersionPool<MultithreadTestResource, 2>;
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
