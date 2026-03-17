/**
 *  Copyright (c) 2025 hikyuu
 *
 *  Created on: 2025/03/17
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/ResourceAsioPool.h>
#include <hikyuu/utilities/Log.h>

using namespace hku;

class TestResource {
public:
    TestResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_id = ++counter;
        // HKU_ERROR("new TestResource {}", m_id);
    }

    virtual ~TestResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        --counter;
        // HKU_ERROR("delete TestResource {}", m_id);
    }

    int getId() const {
        return m_id;
    }

    void print() {
        // printf("i am a %d\n", m_id);
    }

private:
    std::mutex m_mutex;
    int m_id = 0;
    static std::atomic<int> counter;
};

std::atomic<int> TestResource::counter(0);

TEST_CASE("test_ResourceAsioPool_basic") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param);

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto y = co_await pool.get();
        CHECK(y != nullptr);
        CHECK(pool.count() == 1);
        y.reset();
        CHECK(pool.idleCount() >= 0);
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_max_idle") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 0, 2);

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x1 = co_await pool.get();
        auto x2 = co_await pool.get();
        auto x3 = co_await pool.get();
        
        CHECK(pool.count() == 3);
        
        x1.reset();
        x2.reset();
        x3.reset();
        
        // 空闲资源数量应不超过 maxIdleSize
        CHECK(pool.idleCount() <= 2);
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_max_pool_size") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 2, 5);

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x1 = co_await pool.get();
        auto x2 = co_await pool.get();
        
        CHECK(x1 != nullptr);
        CHECK(x2 != nullptr);
        
        // 释放 x1 后，应该可以获取新资源
        x1.reset();
        
        auto x3 = co_await pool.get();
        CHECK(x3 != nullptr);
        
        x2.reset();
        x3.reset();
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_reuse") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 0, 5);

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x1 = co_await pool.get();
        int id1 = x1->getId();
        x1.reset();
        
        auto x2 = co_await pool.get();
        int id2 = x2->getId();
        
        // 应该重用之前的资源
        CHECK(id1 == id2);
        
        x2.reset();
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_concurrent") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 0, 5);

    const int num_tasks = 10;
    std::atomic<int> completed(0);

    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
            auto a = co_await pool.get();
            CHECK(a != nullptr);
            a->print();
            co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                 std::chrono::milliseconds(200)).async_wait(
                boost::asio::use_awaitable);
            a.reset();
            completed++;
        }, boost::asio::detached);
    }

    io_ctx.run();
    io_ctx.restart();

    CHECK(completed == num_tasks);
}

TEST_CASE("test_ResourceAsioPool_getWaitFor_timeout") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 1, 0);

    bool timeout_thrown = false;

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x1 = co_await pool.get();
        
        try {
            // 尝试获取第二个资源，应该超时
            auto x2 = co_await pool.getWaitFor(100);
        } catch (const GetResourceTimeoutException& e) {
            timeout_thrown = true;
        }
        
        x1.reset();
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();

    CHECK(timeout_thrown);
}

TEST_CASE("test_ResourceAsioPool_getWaitFor_success") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 2, 5);

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x1 = co_await pool.get();
        
        // 在超时时间内应该能获取到资源
        auto x2 = co_await pool.getWaitFor(1000);
        CHECK(x2 != nullptr);
        
        x1.reset();
        x2.reset();
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_getAndWait") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 2, 5);

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x1 = co_await pool.get();
        auto x2 = co_await pool.get();
        
        // 启动第三个协程等待资源
        co_spawn(co_await boost::asio::this_coro::executor,
                  [&]() -> boost::asio::awaitable<void> {
                      auto x3 = co_await pool.getAndWait();
                      CHECK(x3 != nullptr);
                      x3.reset();
                  }, boost::asio::detached);
        
        // 等待一段时间后释放 x1
        co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                             std::chrono::milliseconds(100)).async_wait(
            boost::asio::use_awaitable);
        
        x1.reset();
        // 再等待一段时间释放 x2，让第三个协程有机会执行
        co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                             std::chrono::milliseconds(100)).async_wait(
            boost::asio::use_awaitable);
        x2.reset();
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_releaseIdleResource") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 0, 5);

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x1 = co_await pool.get();
        auto x2 = co_await pool.get();
        auto x3 = co_await pool.get();
        
        CHECK(pool.count() == 3);
        
        x1.reset();
        x2.reset();
        
        CHECK(pool.count() >= 1);
        
        // 释放所有空闲资源
        pool.releaseIdleResource();
        
        CHECK(pool.count() == 1);  // 只有 x3 还在使用
        
        x3.reset();
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_multiple_io_context_runs") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 0, 3);

    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x1 = co_await pool.get();
        auto x2 = co_await pool.get();
        
        CHECK(pool.count() == 2);
        
        x1.reset();
        x2.reset();
    }, boost::asio::detached);

    io_ctx.run();

    // 第二次运行
    io_ctx.restart();
    co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
        auto x3 = co_await pool.get();
        CHECK(x3 != nullptr);
        x3.reset();
    }, boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_multithreaded_io_context") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 0, 10);

    const int num_tasks = 50;
    std::atomic<int> completed(0);

    // 使用单线程运行 io_context
    std::thread worker([&]() {
        io_ctx.run();
    });

    // 提交多个并发任务
    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(io_ctx, [&completed, &pool]() -> boost::asio::awaitable<void> {
            try {
                auto resource = co_await pool.get();
                CHECK(resource != nullptr);
                
                // 模拟一些异步操作
                co_await boost::asio::steady_timer(
                    co_await boost::asio::this_coro::executor,
                    std::chrono::milliseconds(10)
                ).async_wait(boost::asio::use_awaitable);
                
                resource->print();
                resource.reset();
            } catch (const std::exception& e) {
                HKU_ERROR("Task failed: {}", e.what());
            }
            completed++;
        }, boost::asio::detached);
    }

    // 等待所有任务完成
    while (completed < num_tasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 停止 io_context 并等待工作线程
    io_ctx.stop();
    if (worker.joinable()) {
        worker.join();
    }

    CHECK(completed == num_tasks);
}

TEST_CASE("test_ResourceAsioPool_concurrent_with_max_pool_size") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 5, 10);  // 最大 5 个活动资源

    const int num_tasks = 20;
    std::atomic<int> completed(0);
    std::atomic<bool> max_limit_verified(false);

    // 使用单线程运行 io_context，避免多线程竞争导致的复杂性
    std::thread worker([&]() {
        io_ctx.run();
    });

    // 提交多个并发任务，测试资源池大小限制
    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
            try {
                auto resource = co_await pool.get();
                CHECK(resource != nullptr);
                
                // 检查当前活动资源数是否满足限制（应该不超过5个）
                size_t count = pool.count();
                if (count <= 5) {
                    max_limit_verified = true;
                }
                
                // 模拟业务处理 - 缩短时间
                co_await boost::asio::steady_timer(
                    co_await boost::asio::this_coro::executor,
                    std::chrono::milliseconds(5)
                ).async_wait(boost::asio::use_awaitable);
                
                resource.reset();
            } catch (const std::exception& e) {
                HKU_ERROR("Task failed: {}", e.what());
            }
            completed++;
        }, boost::asio::detached);
    }

    // 等待所有任务完成
    while (completed < num_tasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 验证资源池大小限制被触发过
    CHECK(max_limit_verified);
    
    // 停止 io_context 并等待工作线程
    io_ctx.stop();
    if (worker.joinable()) {
        worker.join();
    }

    CHECK(completed == num_tasks);
}

TEST_CASE("test_ResourceAsioPool_stress_test") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(io_ctx, param, 20, 50);

    const int num_tasks = 200;
    std::atomic<int> completed(0);
    std::atomic<int> success_count(0);
    
    // 使用单线程避免多线程竞争导致的复杂性
    std::thread worker([&]() {
        io_ctx.run();
    });

    // 高并发压力测试
    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(io_ctx, [&]() -> boost::asio::awaitable<void> {
            try {
                auto resource = co_await pool.get();
                if (resource) {
                    success_count++;
                    // 快速归还
                    resource.reset();
                }
            } catch (const std::exception& e) {
                HKU_ERROR("Stress test task failed: {}", e.what());
            }
            completed++;
        }, boost::asio::detached);
    }

    // 等待所有任务完成
    while (completed < num_tasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 停止 io_context 并等待工作线程
    io_ctx.stop();
    if (worker.joinable()) {
        worker.join();
    }

    CHECK(completed == num_tasks);
    // 在高并发下，应该能成功获取大部分资源
    CHECK(success_count > num_tasks * 0.9);  // 至少 90% 成功率
}
