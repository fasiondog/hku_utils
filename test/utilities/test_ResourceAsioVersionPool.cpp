/**
 *  Copyright (c) 2025 hikyuu
 *
 *  Created on: 2026-03-18
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/ResourceAsioPool.h>
#include <hikyuu/utilities/thread/ThreadPool.h>
#include <hikyuu/utilities/Log.h>
#include <boost/asio.hpp>
#include <thread>

using namespace hku;

namespace {

// 辅助函数：运行协程测试
template <typename Func>
void runCoroutineTest(boost::asio::io_context& ctx, Func&& func) {
    boost::asio::co_spawn(ctx, std::forward<Func>(func)(), boost::asio::detached);
    ctx.run();
}

}  // namespace

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
        // HKU_DEBUG("Created {} version {}", m_id, m_version);
    }

    virtual ~VersionTestResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
        // HKU_DEBUG("Destroyed {} version {}", m_id, m_version);
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

TEST_CASE("test_ResourceAsioVersionPool_Basic") {
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&]() -> boost::asio::awaitable<void> {
        Parameter param;
        param.set<std::string>("test_param", "v1");
        ResourceAsioVersionPool<VersionTestResource> pool(param);

        // 初始状态检查
        CHECK_EQ(pool.count(), 0);
        CHECK_EQ(pool.idleCount(), 0);
        CHECK_EQ(pool.getVersion(), 0);

        // 获取第一个资源
        auto res1 = co_await pool.get();
        CHECK_NE(res1, nullptr);
        CHECK_EQ(pool.count(), 1);
        CHECK_EQ(pool.idleCount(), 0);
        CHECK_EQ(res1->getVersion(), 0);
        res1->print();

        // 归还资源
        res1.reset();
        CHECK_EQ(pool.count(), 1);
        CHECK_EQ(pool.idleCount(), 1);

        // 再次获取应该复用同一个资源
        auto res2 = co_await pool.get();
        CHECK_EQ(pool.count(), 1);
        CHECK_EQ(pool.idleCount(), 0);
        CHECK_EQ(res2->getVersion(), 0);

        // 获取多个资源
        auto res3 = co_await pool.get();
        auto res4 = co_await pool.get();
        CHECK_EQ(pool.count(), 3);  // res2, res3, res4
        CHECK_EQ(pool.idleCount(), 0);

        // 归还部分资源
        res3.reset();
        CHECK_EQ(pool.count(), 3);
        CHECK_EQ(pool.idleCount(), 1);

        co_return;
    });

    ctx.run();
}

TEST_CASE("test_ResourceAsioVersionPool_VersionUpdate") {
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&]() -> boost::asio::awaitable<void> {
        Parameter param;
        param.set<std::string>("test_param", "v1");
        ResourceAsioVersionPool<VersionTestResource> pool(param);

        // 获取一些资源
        auto res1 = co_await pool.get();
        auto res2 = co_await pool.get();
        CHECK_EQ(pool.count(), 2);
        CHECK_EQ(pool.getVersion(), 0);
        CHECK_EQ(res1->getVersion(), 0);
        CHECK_EQ(res2->getVersion(), 0);

        // 更新参数（实际值变化）
        pool.setParam<std::string>("test_param", "v2");
        CHECK_EQ(pool.getVersion(), 1);
        
        // 空闲资源被释放，使用中的资源不变
        CHECK_EQ(pool.count(), 2);
        CHECK_EQ(pool.idleCount(), 0);
        CHECK_EQ(res1->getVersion(), 0);
        CHECK_EQ(res2->getVersion(), 0);

        // 归还旧版本资源，应该被拒绝回收
        res1.reset();
        CHECK_EQ(pool.count(), 1);  // 旧版本资源被销毁
        CHECK_EQ(pool.idleCount(), 0);

        // 获取新资源，应该是新版本
        auto res3 = co_await pool.get();
        CHECK_EQ(pool.count(), 2);
        CHECK_EQ(pool.idleCount(), 0);
        CHECK_EQ(res3->getVersion(), 1);
        CHECK_EQ(res3->getParam<std::string>("test_param"), "v2");

        // 归还新版本资源
        res3.reset();
        CHECK_EQ(pool.count(), 2);
        CHECK_EQ(pool.idleCount(), 1);

        // 再次更新参数但未实际变化
        pool.setParam<std::string>("test_param", "v2");
        CHECK_EQ(pool.getVersion(), 1);  // 版本号不变

        co_return;
    });

    ctx.run();
}

TEST_CASE("test_ResourceAsioVersionPool_SetParameter") {
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&]() -> boost::asio::awaitable<void> {
        Parameter param;
        param.set<std::string>("test_param", "v1");
        ResourceAsioVersionPool<VersionTestResource> pool(param);

        auto res1 = co_await pool.get();
        CHECK_EQ(res1->getVersion(), 0);

        // 使用 setParameter 整体替换参数
        Parameter new_param;
        new_param.set<std::string>("test_param", "v2");
        new_param.set<int>("count", 100);
        pool.setParameter(new_param);

        CHECK_EQ(pool.getVersion(), 1);
        
        // 旧资源仍在用
        CHECK_EQ(res1->getVersion(), 0);
        CHECK_EQ(res1->getParam<std::string>("test_param"), "v1");

        // 归还旧资源
        res1.reset();
        CHECK_EQ(pool.count(), 0);

        // 获取新资源
        auto res2 = co_await pool.get();
        CHECK_EQ(res2->getVersion(), 1);
        CHECK_EQ(res2->getParam<std::string>("test_param"), "v2");
        CHECK_EQ(res2->getParam<int>("count"), 100);

        co_return;
    });

    ctx.run();
}

TEST_CASE("test_ResourceAsioVersionPool_ReleaseIdleResource") {
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&]() -> boost::asio::awaitable<void> {
        Parameter param;
        ResourceAsioVersionPool<VersionTestResource> pool(param);

        // 创建一些资源并归还
        auto res1 = co_await pool.get();
        auto res2 = co_await pool.get();
        auto res3 = co_await pool.get();
        
        res1.reset();
        res2.reset();
        res3.reset();
        
        CHECK_EQ(pool.count(), 3);
        CHECK_EQ(pool.idleCount(), 3);

        // 释放所有空闲资源
        pool.releaseIdleResource();
        CHECK_EQ(pool.count(), 0);
        CHECK_EQ(pool.idleCount(), 0);

        co_return;
    });

    ctx.run();
}

TEST_CASE("test_ResourceAsioVersionPool_ConcurrentAccess") {
    boost::asio::io_context io_ctx;

    const int num_tasks = 20;
    std::atomic<int> completed(0);
    std::atomic<int> success_count(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    Parameter param;
    param.set<std::string>("test_param", "concurrent_test");
    
    // 在协程中创建池
    boost::asio::co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        // 单线程场景使用默认 NullLock
        ResourceAsioVersionPool<VersionTestResource> pool(param);

        for (int i = 0; i < num_tasks; ++i) {
            boost::asio::co_spawn(io_ctx, [&, i]() -> boost::asio::awaitable<void> {
                try {
                    auto res = co_await pool.get();
                    CHECK_NE(res, nullptr);
                    CHECK_GE(res->getId(), 0);
                    
                    // 模拟使用资源
                    co_await boost::asio::steady_timer(io_ctx.get_executor(), 
                                                       std::chrono::milliseconds(10))
                                        .async_wait(boost::asio::use_awaitable);
                    
                    success_count.fetch_add(1);
                } catch (const std::exception& e) {
                    HKU_WARN("Task {} failed: {}", i, e.what());
                }

                if (completed.fetch_add(1) + 1 == num_tasks) {
                    completion_promise.set_value();
                }
            }, boost::asio::detached);
        }

        co_return;
    }, boost::asio::detached);

    // 运行 io_context
    io_ctx.run();

    // 等待所有任务完成
    if (completion_future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
        HKU_ERROR("Concurrent test timeout!");
        FAIL("Test timeout");
    }

    HKU_INFO("Concurrent test completed - Success: {}/{}", success_count.load(), num_tasks);
    CHECK_EQ(success_count.load(), num_tasks);
}

TEST_CASE("test_ResourceAsioVersionPool_VersionConcurrency") {
    boost::asio::io_context io_ctx;

    const int num_tasks = 15;
    std::atomic<int> completed(0);
    std::atomic<int> old_version_count(0);
    std::atomic<int> new_version_count(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    Parameter param;
    param.set<std::string>("test_param", "v1");

    boost::asio::co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        // 单线程场景使用默认 NullLock
        ResourceAsioVersionPool<VersionTestResource> pool(param);

        // 先获取一些资源
        std::vector<std::shared_ptr<VersionTestResource>> resources;
        for (int i = 0; i < 5; ++i) {
            resources.push_back(co_await pool.get());
        }
        CHECK_EQ(pool.count(), 5);

        // 更新版本
        pool.setParam<std::string>("test_param", "v2");
        CHECK_EQ(pool.getVersion(), 1);

        // 并发获取新资源
        for (int i = 0; i < num_tasks; ++i) {
            boost::asio::co_spawn(io_ctx, [&, i]() -> boost::asio::awaitable<void> {
                try {
                    auto res = co_await pool.get();
                    CHECK_NE(res, nullptr);
                    
                    if (res->getVersion() == 0) {
                        old_version_count.fetch_add(1);
                    } else if (res->getVersion() == 1) {
                        new_version_count.fetch_add(1);
                    }
                } catch (const std::exception& e) {
                    HKU_WARN("Task {} failed: {}", i, e.what());
                }

                if (completed.fetch_add(1) + 1 == num_tasks) {
                    completion_promise.set_value();
                }
            }, boost::asio::detached);
        }

        co_return;
    }, boost::asio::detached);

    io_ctx.run();

    if (completion_future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
        HKU_ERROR("Version concurrency test timeout!");
        FAIL("Test timeout");
    }

    HKU_INFO("Version concurrency test - Old: {}, New: {}", 
             old_version_count.load(), new_version_count.load());
    
    // 所有新获取的资源都应该是新版本（旧版本会被淘汰）
    CHECK_EQ(old_version_count.load(), 0);
    CHECK_EQ(new_version_count.load(), num_tasks);
}

TEST_CASE("test_ResourceAsioVersionPool_MultithreadedAccess") {
    boost::asio::io_context io_ctx;
    auto work = boost::asio::make_work_guard(io_ctx);

    const int num_tasks = 30;
    const int num_threads = 4;
    std::atomic<int> completed(0);
    std::atomic<int> success_count(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    Parameter param;
    param.set<std::string>("test_param", "multithread_test");

    boost::asio::co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        // 多线程场景使用 std::mutex
        ResourceAsioVersionPool<VersionTestResource, std::mutex> pool(param);

        for (int i = 0; i < num_tasks; ++i) {
            boost::asio::co_spawn(io_ctx, [&, i]() -> boost::asio::awaitable<void> {
                try {
                    auto res = co_await pool.get();
                    CHECK_NE(res, nullptr);
                    CHECK_EQ(res->getVersion(), 0);
                    
                    // 模拟异步操作
                    co_await boost::asio::steady_timer(io_ctx.get_executor(),
                                                       std::chrono::milliseconds(5))
                                        .async_wait(boost::asio::use_awaitable);
                    
                    success_count.fetch_add(1);
                } catch (const std::exception& e) {
                    HKU_WARN("Task {} failed: {}", i, e.what());
                }

                if (completed.fetch_add(1) + 1 == num_tasks) {
                    completion_promise.set_value();
                }
            }, boost::asio::detached);
        }

        co_return;
    }, boost::asio::detached);

    // 创建多个线程同时运行 io_context
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&]() { io_ctx.run(); });
    }

    // 等待所有任务完成
    if (completion_future.wait_for(std::chrono::seconds(15)) == std::future_status::timeout) {
        HKU_ERROR("Multithreaded test timeout!");
    }

    // 停止 io_context
    io_ctx.stop();
    work.reset();

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    HKU_INFO("Multithreaded test completed - Success: {}/{}", success_count.load(), num_tasks);
    CHECK_EQ(completed.load(), num_tasks);
    CHECK_GT(success_count.load(), 0);
}

TEST_CASE("test_ResourceAsioVersionPool_GetParam") {
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&]() -> boost::asio::awaitable<void> {
        Parameter param;
        param.set<std::string>("name", "test_pool");
        param.set<int>("max_connections", 100);
        
        ResourceAsioVersionPool<VersionTestResource> pool(param);

        // 检查参数存在性
        CHECK_UNARY(pool.haveParam("name"));
        CHECK_UNARY(pool.haveParam("max_connections"));
        CHECK_UNARY(!pool.haveParam("non_existent"));

        // 获取参数值
        CHECK_EQ(pool.getParam<std::string>("name"), "test_pool");
        CHECK_EQ(pool.getParam<int>("max_connections"), 100);

        co_return;
    });

    ctx.run();
}

TEST_CASE("test_ResourceAsioVersionPool_IncVersion") {
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&]() -> boost::asio::awaitable<void> {
        Parameter param;
        ResourceAsioVersionPool<VersionTestResource> pool(param);

        CHECK_EQ(pool.getVersion(), 0);

        // 手动递增版本
        pool.incVersion(1);
        CHECK_EQ(pool.getVersion(), 1);

        pool.incVersion(1);
        CHECK_EQ(pool.getVersion(), 2);

        co_return;
    });

    ctx.run();
}
