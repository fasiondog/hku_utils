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

    /** The number of the resources currently alive, it is used to detect leaked resources */
    static int aliveCount() {
        return counter.load();
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
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto y_result = co_await pool.asyncGet();
          CHECK_EXPECTED(y_result);
          auto y = std::move(y_result.value());
          REQUIRE(y != nullptr);
          CHECK(pool.count() == 1);
          y.reset();
          CHECK(pool.idleCount() >= 0);
      },
      boost::asio::detached);

    io_ctx.run();
}

TEST_CASE("test_ResourceAsioPool_reuse") {
    boost::asio::io_context io_ctx;
    Parameter param;
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto x1_result = co_await pool.asyncGet();
          CHECK_EXPECTED(x1_result);
          auto x1 = std::move(x1_result.value());
          int id1 = x1->getId();
          x1.reset();

          auto x2_result = co_await pool.asyncGet();
          CHECK_EXPECTED(x2_result);
          auto x2 = std::move(x2_result.value());
          int id2 = x2->getId();

          // 应该重用之前的资源
          CHECK(id1 == id2);

          x2.reset();
      },
      boost::asio::detached);

    io_ctx.run();
}

TEST_CASE("test_ResourceAsioPool_concurrent") {
    boost::asio::io_context io_ctx;
    Parameter param;
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param);

    const int num_tasks = 10;
    std::atomic<int> completed(0);

    for (int i = 0; i < num_tasks; ++i) {
        co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              auto a_result = co_await pool.asyncGet();
              CHECK_EXPECTED(a_result);
              auto a = std::move(a_result.value());
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

    CHECK(completed == num_tasks);
}

TEST_CASE("test_ResourceAsioPool_releaseIdleResource") {
    boost::asio::io_context io_ctx;
    Parameter param;
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto x1_result = co_await pool.asyncGet();
          CHECK_EXPECTED(x1_result);
          auto x1 = std::move(x1_result.value());
          auto x2_result = co_await pool.asyncGet();
          CHECK_EXPECTED(x2_result);
          auto x2 = std::move(x2_result.value());
          auto x3_result = co_await pool.asyncGet();
          CHECK_EXPECTED(x3_result);
          auto x3 = std::move(x3_result.value());

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
}

TEST_CASE("test_ResourceAsioPool_multiple_io_context_runs") {
    boost::asio::io_context io_ctx;
    Parameter param;
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto x1_result = co_await pool.asyncGet();
          CHECK_EXPECTED(x1_result);
          auto x1 = std::move(x1_result.value());
          auto x2_result = co_await pool.asyncGet();
          CHECK_EXPECTED(x2_result);
          auto x2 = std::move(x2_result.value());

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
          auto x3_result = co_await pool.asyncGet();
          CHECK_EXPECTED(x3_result);
          auto x3 = std::move(x3_result.value());
          REQUIRE(x3 != nullptr);
          x3.reset();
      },
      boost::asio::detached);

    io_ctx.run();
}

TEST_CASE("test_ResourceAsioPool_multithreaded_io_context") {
    boost::asio::io_context io_ctx;
    Parameter param;
    // 多线程场景使用 std::mutex
    ResourceAsioPool<TestResource, std::mutex> pool(param);

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
                  auto resource_result = co_await pool.asyncGet();
                  CHECK_EXPECTED(resource_result);
                  auto resource = std::move(resource_result.value());
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
    // 多线程场景使用 std::mutex
    ResourceAsioPool<TestResource, std::mutex> pool(param);

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
                  auto resource_result = co_await pool.asyncGet();
                  if (resource_result) {
                      auto resource = std::move(resource_result.value());
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

TEST_CASE("test_ResourceAsioPool_no_resource_leak_on_timeout_race") {
    /** @arg 超时与资源归还同时发生时,资源既不丢失也不重复释放:资源存活数与池中计数守恒 */
    /** @arg 压力过后仍可正常取得资源,槽位没有被永久占用 */
    boost::asio::io_context io_ctx;
    Parameter param;
    const size_t max_count = 1;
    ResourceAsioPool<TestResource, std::mutex> pool(param, max_count);

    const int alive_before = TestResource::aliveCount();

    // 持续借用/归还,使其与等待者的注册和超时时刻频繁重叠
    std::atomic<bool> stop{false};
    const int num_waiters = 6;
    const int rounds = 150;
    std::atomic<int> succeeded{0};
    std::atomic<int> timed_out{0};
    std::atomic<int> completed{0};
    std::promise<void> finish;
    auto finish_future = finish.get_future();

    for (int i = 0; i < num_waiters; ++i) {
        co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              for (int k = 0; k < rounds; ++k) {
                  auto result = co_await pool.asyncGet(std::chrono::microseconds(300));
                  if (result) {
                      succeeded.fetch_add(1);
                      // 持有一小段时间制造资源耗尽状态,使其他等待者频繁落到超时分支
                      co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                         std::chrono::milliseconds(1))
                        .async_wait(boost::asio::use_awaitable);
                  } else {
                      timed_out.fetch_add(1);
                  }
              }
              if (completed.fetch_add(1) + 1 == num_waiters) {
                  finish.set_value();
              }
          },
          boost::asio::detached);
    }

    // 协程都已投递之后再启动工作线程：投递前启动的话 io_context 会因无任务立即返回
    std::vector<std::thread> workers;
    for (int i = 0; i < 2; ++i) {
        workers.emplace_back([&]() { io_ctx.run(); });
    }

    std::thread borrower([&]() {
        while (!stop.load()) {
            auto result = pool.get();
            if (result) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                result.value().reset();
            } else {
                std::this_thread::yield();
            }
        }
    });

    REQUIRE(finish_future.wait_for(std::chrono::seconds(30)) != std::future_status::timeout);
    CHECK(completed.load() == num_waiters);

    stop.store(true);
    borrower.join();
    io_ctx.stop();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    /** @arg 两条分支都被走到:既发生过资源转交,也发生过等待超时 */
    CHECK(succeeded.load() > 0);
    CHECK(timed_out.load() > 0);

    // 所有借出的资源都已归位:等待解除后不存在既不在空闲队列也未被借出的资源
    CHECK(pool.count() == pool.idleCount());

    // 没有任何一个资源在总账上丢失
    pool.releaseIdleResource();
    CHECK(pool.count() == 0);
    CHECK(TestResource::aliveCount() == alive_before);
}

TEST_CASE("test_ResourceAsioPool_waiter_queue_full") {
    /** @arg 等待队列已满时立即失败返回,而不是干等自己的超时周期 */
    boost::asio::io_context io_ctx;
    Parameter param;
    ResourceAsioPool<TestResource, std::mutex> pool(param, 1, 1);  // max_count=1, max_waiters=1

    auto hold_result = pool.get();
    CHECK_EXPECTED(hold_result);
    auto hold = std::move(hold_result.value());

    // 第一个等待者:注册后立即挂起在池中
    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          (void)co_await pool.asyncGet(std::chrono::milliseconds(200));
      },
      boost::asio::detached);
    io_ctx.run_one();

    std::promise<std::string> second_result;
    auto second_future = second_result.get_future();
    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          auto start = std::chrono::steady_clock::now();
          auto result = co_await pool.asyncGet(std::chrono::seconds(5));
          auto elapsed = std::chrono::steady_clock::now() - start;
          CHECK(!result.has_value());
          CHECK(elapsed < std::chrono::seconds(1));
          second_result.set_value(result ? std::string() : result.error());
      },
      boost::asio::detached);

    io_ctx.run();

    REQUIRE(second_future.wait_for(std::chrono::seconds(5)) != std::future_status::timeout);
    CHECK(second_future.get().find("Waiter queue is full") != std::string::npos);

    hold.reset();
}

TEST_CASE("test_ResourceAsioPool_destroy_while_handing_resource_over") {
    /** @arg 资源转交与资源池析构同时进行:析构等该资源归位后才返回,资源不丢失也不重复释放 */
    boost::asio::io_context io_ctx;
    Parameter param;
    auto pool = std::make_unique<ResourceAsioPool<TestResource, std::mutex>>(param, 1);
    const int alive_before = TestResource::aliveCount();

    auto hold_result = pool->get();
    CHECK_EXPECTED(hold_result);
    auto hold = std::move(hold_result.value());

    std::atomic<bool> waiter_started{false};
    std::promise<void> waiter_done;
    auto waiter_done_future = waiter_done.get_future();
    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          waiter_started.store(true);
          auto result = co_await pool->asyncGet(std::chrono::seconds(2));
          if (result) {
              std::move(result.value()).reset();
          }
          waiter_done.set_value();
      },
      boost::asio::detached);

    // 单步推进一次:等待者必定已经注册并挂起,无需依赖时间等待
    io_ctx.run_one();
    CHECK_UNARY(waiter_started.load());

    std::thread worker([&]() { io_ctx.run(); });

    // 借用者归还资源(会转交给等待者)与析构同时进行
    std::thread returner([&]() { hold.reset(); });
    std::thread destroyer([&]() { pool.reset(); });
    returner.join();
    destroyer.join();

    REQUIRE(waiter_done_future.wait_for(std::chrono::seconds(5)) != std::future_status::timeout);
    CHECK(TestResource::aliveCount() == alive_before);

    io_ctx.stop();
    worker.join();
}

TEST_CASE("test_ResourceAsioPool_destroy_with_resource_in_flight") {
    /** @arg 资源已转交给挂起的等待者、但等待者尚未取走时销毁资源池:既不泄漏也不重复释放 */
    /** @arg 挂起协程随后恢复时不再访问已经销毁的资源池 */
    boost::asio::io_context io_ctx;
    Parameter param;
    auto pool = std::make_unique<ResourceAsioPool<TestResource, std::mutex>>(param, 1);
    const int alive_before = TestResource::aliveCount();

    auto hold_result = pool->get();
    CHECK_EXPECTED(hold_result);
    auto hold = std::move(hold_result.value());

    std::atomic<bool> waiter_started{false};
    std::atomic<bool> resumed{false};
    std::atomic<bool> has_value{false};
    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          waiter_started.store(true);
          auto result = co_await pool->asyncGet(std::chrono::seconds(5));
          has_value.store(result.has_value());
          resumed.store(true);
      },
      boost::asio::detached);

    // 单步推进一次:等待者必定已经注册并挂起
    io_ctx.run_one();
    CHECK_UNARY(waiter_started.load());

    // 归还的资源转交给挂起的等待者;此刻不推进 io_context,协程仍处于挂起状态
    hold.reset();

    auto start = std::chrono::steady_clock::now();
    pool.reset();
    CHECK_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(2));

    // 让挂起的协程真正恢复:此时资源池已不存在,它不得再访问资源池的任何成员
    io_ctx.run();
    CHECK_UNARY(resumed.load());
    CHECK_UNARY(!has_value.load());
    CHECK(TestResource::aliveCount() == alive_before);
}

TEST_CASE("test_ResourceAsioPool_max_count_is_hard_limit") {
    /** @arg 多线程并发创建时 max_count 是硬上限:任何时刻观察到的 count 都不超过 max_count */
    Parameter param;
    const size_t max_count = 1;
    ResourceAsioPool<TestResource, std::mutex> pool(param, max_count);

    std::atomic<int> max_observed{0};
    const int num_threads = 8;
    const int rounds = 200;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int k = 0; k < rounds; ++k) {
                auto result = pool.get();
                if (result) {
                    // 记录观察到的最大在用资源数
                    int expected = max_observed.load();
                    while (static_cast<int>(pool.count()) > expected &&
                           !max_observed.compare_exchange_weak(expected,
                                                               static_cast<int>(pool.count()))) {
                    }
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    result.value().reset();
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    CHECK(max_observed.load() <= static_cast<int>(max_count));
    pool.releaseIdleResource();
    CHECK(pool.count() == 0);
}

TEST_CASE("test_ResourceAsioPool_multithreaded_executor") {
    boost::asio::io_context io_ctx;
    Parameter param;
    // 多线程场景使用 std::mutex
    ResourceAsioPool<TestResource, std::mutex> pool(param);

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
                  auto resource_result = co_await pool.asyncGet();
                  CHECK_EXPECTED(resource_result);
                  auto resource = std::move(resource_result.value());
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
    // 多线程场景使用 std::mutex
    ResourceAsioPool<TestResource, std::mutex> pool(param);

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
                  auto resource_result = co_await pool.asyncGet();
                  CHECK_EXPECTED(resource_result);
                  auto resource = std::move(resource_result.value());
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

TEST_CASE("test_ResourceAsioPool_max_count_limit") {
    boost::asio::io_context io_ctx;
    Parameter param;
    const size_t max_count = 3;
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param, max_count);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          // 创建达到最大数量的资源
          auto r1_result = co_await pool.asyncGet();
          CHECK_EXPECTED(r1_result);
          auto r1 = std::move(r1_result.value());
          auto r2_result = co_await pool.asyncGet();
          CHECK_EXPECTED(r2_result);
          auto r2 = std::move(r2_result.value());
          auto r3_result = co_await pool.asyncGet();
          CHECK_EXPECTED(r3_result);
          auto r3 = std::move(r3_result.value());

          REQUIRE(pool.count() == 3);

          // 尝试获取第4个资源但设置较短超时，应该返回错误
          auto r4_result = co_await pool.asyncGet(std::chrono::milliseconds(100));
          CHECK(!r4_result.has_value());

          // 释放第一个资源
          r1.reset();

          // 现在应该可以获取新的资源了
          auto r5_result = co_await pool.asyncGet(std::chrono::milliseconds(100));
          CHECK_EXPECTED(r5_result);
          auto r5 = std::move(r5_result.value());
          CHECK(pool.count() == 3);

          r2.reset();
          r3.reset();
          r5.reset();
      },
      boost::asio::detached);

    io_ctx.run();
}

TEST_CASE("test_ResourceAsioPool_no_max_limit") {
    boost::asio::io_context io_ctx;
    Parameter param;
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param, 0);  // 无限制

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          // 可以创建任意数量的资源
          std::vector<ResourceAsioPool<TestResource>::ResourcePtr> resources;
          for (int i = 0; i < 10; ++i) {
              auto r_result = co_await pool.asyncGet();
              CHECK_EXPECTED(r_result);
              resources.push_back(std::move(r_result.value()));
          }

          CHECK(pool.count() == 10);

          // 释放所有资源
          resources.clear();
      },
      boost::asio::detached);

    io_ctx.run();
}

TEST_CASE("test_ResourceAsioPool_get_timeout") {
    boost::asio::io_context io_ctx;
    Parameter param;
    const size_t max_count = 2;
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param, max_count);

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          // 创建达到最大数量的资源
          auto r1_result = co_await pool.asyncGet();
          CHECK_EXPECTED(r1_result);
          auto r1 = std::move(r1_result.value());
          auto r2_result = co_await pool.asyncGet();
          CHECK_EXPECTED(r2_result);
          auto r2 = std::move(r2_result.value());

          REQUIRE(pool.count() == 2);

          // 尝试获取第3个资源，应该返回错误
          auto r3_result = co_await pool.asyncGet(std::chrono::milliseconds(50));
          CHECK(!r3_result.has_value());
          if (r3_result) {
              MESSAGE("Expected error but got value");
          } else {
              std::string msg = r3_result.error();
              CHECK(msg.find("timeout") != std::string::npos);
          }

          r1.reset();
          r2.reset();
      },
      boost::asio::detached);

    io_ctx.run();
}

TEST_CASE("test_ResourceAsioPool_get_with_timeout_success") {
    boost::asio::io_context io_ctx;
    Parameter param;
    const size_t max_count = 2;
    // 单线程协程环境，使用 NullLock 避免不必要的锁开销
    ResourceAsioPool<TestResource, rap::NullLock> pool(param, max_count);

    std::atomic<bool> test_passed{false};

    co_spawn(
      io_ctx,
      [&]() -> boost::asio::awaitable<void> {
          // 创建达到最大数量的资源
          auto r1_result = co_await pool.asyncGet();
          CHECK_EXPECTED(r1_result);
          auto r1 = std::move(r1_result.value());
          auto r2_result = co_await pool.asyncGet();
          CHECK_EXPECTED(r2_result);
          auto r2 = std::move(r2_result.value());
          REQUIRE(pool.count() == 2);

          // 延迟释放第一个资源
          co_spawn(
            io_ctx,
            [r1 = std::move(r1)]() mutable -> boost::asio::awaitable<void> {
                co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor,
                                                   std::chrono::milliseconds(50))
                  .async_wait(boost::asio::use_awaitable);
                r1.reset();
            },
            boost::asio::detached);

          // 等待第一个资源释放后，应该可以成功获取
          auto r3_result = co_await pool.asyncGet(std::chrono::milliseconds(200));
          CHECK_EXPECTED(r3_result);
          auto r3 = std::move(r3_result.value());
          CHECK(r3 != nullptr);

          r2.reset();
          r3.reset();
          test_passed.store(true);
      },
      boost::asio::detached);

    io_ctx.run();

    CHECK(test_passed.load());
}
