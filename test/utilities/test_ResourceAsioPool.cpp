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
    ResourceAsioPool<TestResource> pool(param);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto y = co_await pool.get();
          REQUIRE(y != nullptr);
          CHECK(pool.count() == 1);
          y.reset();
          CHECK(pool.idleCount() >= 0);
      },
      boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_reuse") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(param);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto x1 = co_await pool.get();
          int id1 = x1->getId();
          x1.reset();

          auto x2 = co_await pool.get();
          int id2 = x2->getId();

          // 应该重用之前的资源
          CHECK(id1 == id2);

          x2.reset();
      },
      boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_concurrent") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(param);

    const int num_tasks = 10;
    std::atomic<int> completed(0);

    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              auto a = co_await pool.get();
              REQUIRE(a != nullptr);
              a->print();
              co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                 std::chrono::milliseconds(200))
                .async_wait(boost::asio::use_awaitable);
              a.reset();
              completed++;
          },
          boost::asio::detached);
    }

    io_ctx.run();
    io_ctx.restart();

    CHECK(completed == num_tasks);
}

TEST_CASE("test_ResourceAsioPool_releaseIdleResource") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(param);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto x1 = co_await pool.get();
          auto x2 = co_await pool.get();
          auto x3 = co_await pool.get();

          REQUIRE(pool.count() == 3);

          x1.reset();
          x2.reset();

          REQUIRE(pool.count() >= 1);

          // 释放所有空闲资源
          pool.releaseIdleResource();

          CHECK(pool.count() == 1);  // 只有 x3 还在使用

          x3.reset();
      },
      boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_multiple_io_context_runs") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(param);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto x1 = co_await pool.get();
          auto x2 = co_await pool.get();

          REQUIRE(pool.count() == 2);

          x1.reset();
          x2.reset();
      },
      boost::asio::detached);

    io_ctx.run();

    // 第二次运行
    io_ctx.restart();
    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto x3 = co_await pool.get();
          REQUIRE(x3 != nullptr);
          x3.reset();
      },
      boost::asio::detached);

    io_ctx.run();
    io_ctx.restart();
}

TEST_CASE("test_ResourceAsioPool_multithreaded_io_context") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(param);

    const int num_tasks = 50;
    std::atomic<int> completed(0);

    // 使用单线程运行 io_context
    std::thread worker([&]() { io_ctx.run(); });

    // 提交多个并发任务
    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              try {
                  auto resource = co_await pool.get();
                  REQUIRE(resource != nullptr);

                  // 模拟一些异步操作
                  co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                     std::chrono::milliseconds(10))
                    .async_wait(boost::asio::use_awaitable);

                  resource->print();
                  resource.reset();
              } catch (const std::exception& e) {
                  HKU_ERROR("Task failed: {}", e.what());
              }
              completed++;
          },
          boost::asio::detached);
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

TEST_CASE("test_ResourceAsioPool_stress_test") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(param);

    const int num_tasks = 200;
    std::atomic<int> completed(0);
    std::atomic<int> success_count(0);

    // 使用单线程避免多线程竞争导致的复杂性
    std::thread worker([&]() { io_ctx.run(); });

    // 高并发压力测试
    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              try {
                  auto resource = co_await pool.get();
                  if (resource) {
                      success_count.fetch_add(1);
                      // 快速归还
                      resource.reset();
                  }
              } catch (const std::exception& e) {
                  HKU_ERROR("Stress test task failed: {}", e.what());
              }
              completed.fetch_add(1);
          },
          boost::asio::detached);
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

TEST_CASE("test_ResourceAsioPool_multithreaded_executor") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(param);

    const int num_tasks = 50;
    const int num_threads = 4;
    std::atomic<int> completed(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    // 提交多个并发任务
    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              try {
                  auto resource = co_await pool.get();
                  REQUIRE(resource != nullptr);

                  // 模拟一些异步操作
                  co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                     std::chrono::milliseconds(10))
                    .async_wait(boost::asio::use_awaitable);

                  resource->print();
                  resource.reset();
              } catch (const std::exception& e) {
                  HKU_ERROR("Task failed: {}", e.what());
              }

              // 使用原子操作检查是否最后一个完成的任务
              if (completed.fetch_add(1) + 1 == num_tasks) {
                  completion_promise.set_value();
              }
          },
          boost::asio::detached);
    }

    // 创建多个线程同时运行 io_context（真正的多线程执行器）
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&]() { io_ctx.run(); });
    }

    // 等待所有任务完成或超时
    if (completion_future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
        HKU_ERROR("Test timeout! Completed: {}/{}", completed.load(), num_tasks);
    }

    // 停止 io_context 并等待所有工作线程
    io_ctx.stop();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    CHECK(completed == num_tasks);
}

TEST_CASE("test_ResourceAsioPool_multithreaded_executor_stress") {
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource> pool(param);

    const int num_tasks = 100;
    const int num_threads = 8;
    std::atomic<int> completed(0);
    std::atomic<int> max_concurrent_observed(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    // 提交大量并发任务，测试多线程调度
    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              try {
                  auto resource = co_await pool.get();
                  REQUIRE(resource != nullptr);

                  // 记录当前并发数
                  size_t current_count = pool.count();
                  int expected = max_concurrent_observed.load();
                  while (current_count > static_cast<size_t>(expected)) {
                      max_concurrent_observed.compare_exchange_weak(
                        expected, static_cast<int>(current_count));
                  }

                  // 模拟业务处理
                  co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                     std::chrono::milliseconds(5))
                    .async_wait(boost::asio::use_awaitable);

                  resource.reset();
              } catch (const std::exception& e) {
                  HKU_ERROR("Stress test task failed: {}", e.what());
              }

              if (completed.fetch_add(1) + 1 == num_tasks) {
                  completion_promise.set_value();
              }
          },
          boost::asio::detached);
    }

    // 创建 8 个线程同时运行 io_context，模拟高并发场景
    std::vector<std::thread> workers;
    workers.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&]() { io_ctx.run(); });
    }

    // 等待所有任务完成或超时
    if (completion_future.wait_for(std::chrono::seconds(15)) == std::future_status::timeout) {
        HKU_ERROR("Stress test timeout! Completed: {}/{}", completed.load(), num_tasks);
    }

    // 验证最大并发数被正确观察（在协程环境下，并发数可能超过线程数）
    CHECK(max_concurrent_observed.load() > 0);
    CHECK(max_concurrent_observed.load() <= num_tasks);

    // 停止 io_context 并等待所有工作线程
    io_ctx.stop();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    CHECK(completed == num_tasks);
}
