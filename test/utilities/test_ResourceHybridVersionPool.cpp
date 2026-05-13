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

// test_ResourceHybridVersionPool_asyncGet
class AsyncGetTestResource {
    PARAMETER_SUPPORT
public:
    AsyncGetTestResource(const Parameter& param) {
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
    virtual ~AsyncGetTestResource() {
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
int AsyncGetTestResource::x = 0;
std::mutex AsyncGetTestResource::m_mutex;

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

// 测试协程异步获取
TEST_CASE("test_ResourceHybridVersionPool_asyncGet") {
    boost::asio::io_context ctx;

    boost::asio::co_spawn(
      ctx,
      [&]() -> boost::asio::awaitable<void> {
          Parameter param;
          param.set<std::string>("test_param", "async_test");

          ResourceHybridVersionPool<AsyncGetTestResource> pool(param, 64);

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

    // 不指定 TLS 池大小，应该使用模板参数的默认值（64），全局池使用默认值 64
    ResourceHybridVersionPool<FallbackTestResource> pool(param);

    CHECK_EQ(pool.maxTlsPoolSize(), 64);     // 应该等于模板参数默认值
    CHECK_EQ(pool.maxGlobalPoolSize(), 64);  // 全局池默认值

    auto& tls_pool = pool.tlsPool();
    CHECK_EQ(tls_pool.maxCount(), 64);
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