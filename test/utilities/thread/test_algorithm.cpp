/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-01-25
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/thread/thread.h>
#include <hikyuu/utilities/cppdef.h>
#include <hikyuu/utilities/thread/ThreadPool.h>
#include <hikyuu/utilities/SpendTimer.h>
#include <hikyuu/utilities/thread/algorithm.h>
#include <hikyuu/utilities/net.h>
#include <atomic>
#include <latch>
#include <thread>

using namespace hku;
namespace asio = hku::net::asio;

#if CPP_STANDARD >= CPP_STANDARD_20

#if !defined(__GNUC__) || defined(__clang__) || (__GNUC__ > 12)
/**
 * @brief 辅助协程函数：测试 co_run 基本功能
 */
static asio::awaitable<void> test_co_run_basic_helper(ThreadPool& pool) {
    // 测试有返回值的情况
    int result = co_await co_run(pool.executor(), []() -> int { return 42; });
    CHECK_EQ(result, 42);

    // 测试 void 返回类型
    bool executed = false;
    co_await co_run(pool.executor(), [&]() { executed = true; });
    CHECK_UNARY(executed);
}

TEST_CASE("test_co_run_basic") {
    ThreadPool pool(4);
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_basic_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run_ec 基本功能（error_code 模式）
 */
static asio::awaitable<void> test_co_run_ec_basic_helper(ThreadPool& pool) {
    // 测试有返回值的情况
    int result = co_await co_run_ec(pool.executor(), []() -> int { return 42; });
    CHECK_EQ(result, 42);

    // 测试 void 返回类型
    bool executed = false;
    co_await co_run_ec(pool.executor(), [&]() { executed = true; });
    CHECK_UNARY(executed);
}

TEST_CASE("test_co_run_ec_basic") {
    ThreadPool pool(4);
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_basic_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试延迟执行
 */
static asio::awaitable<void> test_co_run_delayed_helper(ThreadPool& pool) {
    std::atomic<bool> started{false};

    int result = co_await co_run(pool.executor(), [&started]() -> int {
        started.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    });

    CHECK_UNARY(started.load());
    CHECK_EQ(result, 42);
}

TEST_CASE("test_co_run_delayed_wait") {
    ThreadPool pool(4);
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_delayed_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run_ec 的延迟执行
 */
static asio::awaitable<void> test_co_run_ec_delayed_helper(ThreadPool& pool) {
    std::atomic<bool> started{false};

    int result = co_await co_run_ec(pool.executor(), [&started]() -> int {
        started.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    });

    CHECK_UNARY(started.load());
    CHECK_EQ(result, 42);
}

TEST_CASE("test_co_run_ec_delayed_wait") {
    ThreadPool pool(4);
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_delayed_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief Helper: co_run keeps the original exception type (no error_code mapping)
 */
static asio::awaitable<void> test_co_run_exception_helper(ThreadPool& pool) {
    /** co_run must rethrow the original exception thrown by func */
    bool exception_caught = false;
    try {
        co_await co_run(pool.executor(), []() -> int {
            throw std::runtime_error("Test exception from co_run");
            return 0;
        });
        CHECK_FALSE("Should have thrown runtime_error");
    } catch (const std::runtime_error& e) {
        exception_caught = true;
        CHECK_EQ(std::string(e.what()), "Test exception from co_run");
    } catch (...) {
        CHECK_FALSE("Unexpected exception type: co_run must propagate the original exception");
    }
    CHECK_UNARY(exception_caught);

    /** A successful task must return its value without throwing */
    try {
        int result = co_await co_run(pool.executor(), []() -> int { return 42; });
        CHECK_EQ(result, 42);
    } catch (...) {
        CHECK_FALSE("Should not throw for a successful operation");
    }
}

TEST_CASE("test_co_run_exception") {
    ThreadPool pool(4);
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_exception_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief Helper: co_run_ec maps arbitrary func exceptions to a system_error
 */
static asio::awaitable<void> test_co_run_ec_exception_helper(ThreadPool& pool) {
    /** co_run_ec converts the func exception into a non-empty error_code */
    bool exception_caught = false;
    try {
        co_await co_run_ec(pool.executor(), []() -> int {
            throw std::runtime_error("Test exception from co_run_ec");
            return 0;
        });
        CHECK_FALSE("Should have thrown system_error");
    } catch (const boost::system::system_error& e) {
        exception_caught = true;
        CHECK_NE(e.code().value(), 0);
        HKU_INFO("Caught expected system_error: {}", e.what());
    }
    CHECK_UNARY(exception_caught);

    /** A successful task must return its value without throwing */
    try {
        int result = co_await co_run_ec(pool.executor(), []() -> int { return 42; });
        CHECK_EQ(result, 42);
    } catch (...) {
        CHECK_FALSE("Should not throw for a successful operation");
    }
}

TEST_CASE("test_co_run_ec_exception") {
    ThreadPool pool(4);
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_exception_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run 的异常穿透（非 void 类型）
 */
static asio::awaitable<void> test_co_run_non_void_helper(ThreadPool& pool) {
    // 测试异常情况 - 应该能够捕获原始异常类型
    bool exception_caught = false;

    try {
        int result = co_await co_run(pool.executor(), []() -> int {
            throw std::runtime_error("Test exception from co_run");
            return 0;
        });
        CHECK_FALSE("Should have thrown exception");
    } catch (const std::runtime_error& e) {
        exception_caught = true;
        HKU_INFO("Caught runtime_error: {}", e.what());
        CHECK_EQ(std::string(e.what()), "Test exception from co_run");
    } catch (...) {
        HKU_ERROR("Caught unexpected exception type");
    }

    CHECK(exception_caught);

    // 测试正常情况（没有异常）
    try {
        int result = co_await co_run(pool.executor(), []() -> int { return 42; });
        CHECK_EQ(result, 42);
        HKU_INFO("Normal case passed, result: {}", result);
    } catch (...) {
        CHECK_FALSE("Should not throw for successful operation");
    }
}

TEST_CASE("test_co_run_non_void") {
    ThreadPool pool(4);
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_non_void_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run 的异常穿透（void 类型）
 */
static asio::awaitable<void> test_co_run_void_helper(ThreadPool& pool) {
    bool exception_caught = false;

    try {
        co_await co_run(pool.executor(),
                        []() { throw std::logic_error("Void function exception from co_run"); });
        CHECK_FALSE("Should have thrown exception");
    } catch (const std::logic_error& e) {
        exception_caught = true;
        HKU_INFO("Caught logic_error: {}", e.what());
        CHECK_EQ(std::string(e.what()), "Void function exception from co_run");
    } catch (...) {
        HKU_ERROR("Caught unexpected exception type");
    }

    CHECK(exception_caught);

    // 测试正常情况（没有异常）
    bool executed = false;
    co_await co_run(pool.executor(), [&executed]() { executed = true; });
    CHECK_UNARY(executed);
    HKU_INFO("Normal void case passed");
}

TEST_CASE("test_co_run_void") {
    ThreadPool pool(4);
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_void_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run_ec 的 lambda 捕获
 */
static asio::awaitable<void> test_co_run_ec_lambda_capture_helper(ThreadPool& pool) {
    int value1 = 100;
    std::string value2 = "hello";

    auto result = co_await co_run_ec(pool.executor(), [value1, value2 = std::move(value2)]() {
        return std::make_pair(value1, value2);
    });

    CHECK_EQ(result.first, 100);
    CHECK_EQ(result.second, "hello");
}

TEST_CASE("test_co_run_ec_lambda_capture") {
    ThreadPool pool(4);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_lambda_capture_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run 返回字符串
 */
static asio::awaitable<void> test_co_run_string_return_helper(ThreadPool& pool) {
    std::string expected = "Hello, co_run!";

    auto result =
      co_await co_run(pool.executor(), [expected]() -> std::string { return expected; });

    CHECK_EQ(result, expected);
}

TEST_CASE("test_co_run_string_return") {
    ThreadPool pool(4);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_string_return_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run_ec 返回字符串
 */
static asio::awaitable<void> test_co_run_ec_string_return_helper(ThreadPool& pool) {
    std::string expected = "Hello, co_run_ec!";

    auto result =
      co_await co_run_ec(pool.executor(), [expected]() -> std::string { return expected; });

    CHECK_EQ(result, expected);
}

TEST_CASE("test_co_run_ec_string_return") {
    ThreadPool pool(4);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_string_return_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run 返回复杂类型
 */
static asio::awaitable<void> test_co_run_complex_type_helper(ThreadPool& pool) {
    struct TestData {
        int value;
        std::string text;
        double number;
    };

    auto result =
      co_await co_run(pool.executor(), []() -> TestData { return TestData{42, "test", 3.14}; });

    CHECK_EQ(result.value, 42);
    CHECK_EQ(result.text, "test");
    CHECK_EQ(result.number, 3.14);
}

TEST_CASE("test_co_run_complex_type") {
    ThreadPool pool(4);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_complex_type_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run_ec 返回复杂类型
 */
static asio::awaitable<void> test_co_run_ec_complex_type_helper(ThreadPool& pool) {
    struct TestData {
        int value;
        std::string text;
        double number;
    };

    auto result =
      co_await co_run_ec(pool.executor(), []() -> TestData { return TestData{42, "test", 3.14}; });

    CHECK_EQ(result.value, 42);
    CHECK_EQ(result.text, "test");
    CHECK_EQ(result.number, 3.14);
}

TEST_CASE("test_co_run_ec_complex_type") {
    ThreadPool pool(4);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_complex_type_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run 返回仅移动类型
 */
static asio::awaitable<void> test_co_run_move_only_type_helper(ThreadPool& pool) {
    auto result = co_await co_run(
      pool.executor(), []() -> std::unique_ptr<int> { return std::make_unique<int>(100); });

    CHECK_NE(result, nullptr);
    CHECK_EQ(*result, 100);
}

TEST_CASE("test_co_run_move_only_type") {
    ThreadPool pool(4);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_move_only_type_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run_ec 返回仅移动类型
 */
static asio::awaitable<void> test_co_run_ec_move_only_type_helper(ThreadPool& pool) {
    auto result = co_await co_run_ec(
      pool.executor(), []() -> std::unique_ptr<int> { return std::make_unique<int>(100); });

    CHECK_NE(result, nullptr);
    CHECK_EQ(*result, 100);
}

TEST_CASE("test_co_run_ec_move_only_type") {
    ThreadPool pool(4);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_move_only_type_helper(pool), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run 使用多个执行器
 */
static asio::awaitable<void> test_co_run_multiple_executors_helper(ThreadPool& pool1,
                                                                   ThreadPool& pool2) {
    std::atomic<int> counter{0};

    co_await co_run(pool1.executor(),
                    [&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });

    co_await co_run(pool2.executor(),
                    [&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });

    CHECK_EQ(counter.load(), 2);
}

TEST_CASE("test_co_run_multiple_executors") {
    ThreadPool pool1(2);
    ThreadPool pool2(2);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_multiple_executors_helper(pool1, pool2), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run_ec 使用多个执行器
 */
static asio::awaitable<void> test_co_run_ec_multiple_executors_helper(ThreadPool& pool1,
                                                                      ThreadPool& pool2) {
    std::atomic<int> counter{0};

    co_await co_run_ec(pool1.executor(),
                       [&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });

    co_await co_run_ec(pool2.executor(),
                       [&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });

    CHECK_EQ(counter.load(), 2);
}

TEST_CASE("test_co_run_ec_multiple_executors") {
    ThreadPool pool1(2);
    ThreadPool pool2(2);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_multiple_executors_helper(pool1, pool2), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run 压力测试
 */
static asio::awaitable<void> test_co_run_stress_test_helper(ThreadPool& pool) {
    const int num_tasks = 100;
    std::vector<int> results;
    results.reserve(num_tasks);

    for (int i = 0; i < num_tasks; ++i) {
        auto result = co_await co_run(pool.executor(), [i]() -> int { return i * 2; });
        results.push_back(result);
    }

    for (int i = 0; i < num_tasks; ++i) {
        CHECK_EQ(results[i], i * 2);
    }
}

TEST_CASE("test_co_run_stress_test") {
    ThreadPool pool(8);

    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_stress_test_helper(pool), asio::detached);
    ctx.run();
}

// ============================================================================
// 协程中的使用示例测试
// ============================================================================

/**
 * @brief 辅助协程函数：测试 await_future 在协程中的基本使用
 */
static asio::awaitable<void> test_await_future_in_coro_helper() {
    ThreadPool pool(4);

    // 提交任务到线程池，获取 std::future
    std::future<int> fut = pool.submit([]() -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 42;
    });

    // 在协程中异步等待 future
    int result = co_await await_future(std::move(fut));
    CHECK_EQ(result, 42);
}

TEST_CASE("test_await_future_in_coroutine") {
    asio::io_context ctx;
    asio::co_spawn(ctx, test_await_future_in_coro_helper(), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试混合使用异步模式
 */
static asio::awaitable<void> test_mixed_async_in_coro_helper() {
    ThreadPool pool(4);

    // 1. 使用 co_run 提交需要等待的任务（直接 co_await）
    int result = co_await co_run(pool.executor(), []() -> int { return 10; });

    // 2. 使用另一个 co_run 模拟后台任务，不等待
    std::atomic<bool> background_done{false};
    auto background_fut = pool.submit([&background_done]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        background_done.store(true, std::memory_order_relaxed);
    });

    // 3. 验证 co_run 的结果
    CHECK_EQ(result, 10);

    // 4. 等待后台任务完成（使用 await_future 异步等待）
    co_await await_future(std::move(background_fut));
    CHECK_UNARY(background_done.load());
}

TEST_CASE("test_mixed_async_patterns_in_coroutine") {
    asio::io_context ctx;
    asio::co_spawn(ctx, test_mixed_async_in_coro_helper(), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试并发执行多个异步任务
 */
static asio::awaitable<void> test_concurrent_tasks_in_coro_helper() {
    ThreadPool pool(8);

    // 并发提交多个任务，直接获取结果
    std::vector<int> results(5);
    for (int i = 0; i < 5; ++i) {
        results[i] = co_await co_run(pool.executor(), [i]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * i;
        });
    }

    // 验证结果
    CHECK_EQ(results.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        CHECK_EQ(results[i], i * i);
    }
}

TEST_CASE("test_concurrent_co_run_tasks_in_coroutine") {
    asio::io_context ctx;
    asio::co_spawn(ctx, test_concurrent_tasks_in_coro_helper(), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试 co_run 在协程中的基本使用
 */
static asio::awaitable<void> test_co_run_in_coro_helper() {
    ThreadPool pool(4);

    // 在协程中直接使用 co_await co_run
    int result = co_await co_run(pool.executor(), []() -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 100;
    });

    CHECK_EQ(result, 100);
}

TEST_CASE("test_co_run_in_coroutine") {
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_in_coro_helper(), asio::detached);
    ctx.run();
}

/**
 * @brief 辅助协程函数：测试并发执行多个 co_run 任务
 */
static asio::awaitable<void> test_concurrent_co_run_helper() {
    ThreadPool pool(8);

    // 并发执行多个 co_run 任务
    std::vector<asio::awaitable<int>> run_tasks;
    for (int i = 0; i < 5; ++i) {
        run_tasks.push_back(co_run(pool.executor(), [i]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * i;
        }));
    }

    // 并发等待所有结果
    std::vector<int> results;
    for (auto& task : run_tasks) {
        results.push_back(co_await std::move(task));
    }

    // 验证结果
    CHECK_EQ(results.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        CHECK_EQ(results[i], i * i);
    }
}

TEST_CASE("test_concurrent_co_run_in_coroutine") {
    asio::io_context ctx;
    asio::co_spawn(ctx, test_concurrent_co_run_helper(), asio::detached);
    ctx.run();
}

// ============================================================================
// co_run/co_run_ec lifetime and cancellation semantics
// ============================================================================

/**
 * @brief The home io_context is destroyed while func is still running on the worker
 *
 * The completion must never be posted back to a destroyed io_context; the func result is
 * simply discarded and the coroutine never resumes.
 */
TEST_CASE("test_co_run_destroy_home_io_context") {
    ThreadPool pool(2);
    std::latch func_started(1);
    std::latch release_func(1);
    std::atomic<bool> resumed{false};

    {
        asio::io_context ctx;
        asio::co_spawn(
          ctx,
          [&]() -> asio::awaitable<void> {
              co_await co_run(pool.executor(), [&]() -> void {
                  func_started.count_down();
                  release_func.wait();
              });
              resumed.store(true, std::memory_order_release);
          },
          asio::detached);

        std::thread runner([&] { ctx.run(); });
        func_started.wait();

        /** func is still running: abandon the coroutine and destroy its io_context */
        ctx.stop();
        runner.join();
    }

    /** Let the worker finish after the home io_context is already gone */
    release_func.count_down();
    CHECK_FALSE(resumed.load(std::memory_order_acquire));
}

/**
 * @brief A queued (never started) task is dropped when its home io_context dies first and the
 * pool stops afterwards; neither the task nor the coroutine may touch the dead io_context
 */
TEST_CASE("test_co_run_dropped_queued_task") {
    /** until_empty=false: stop() discards tasks still waiting in the queue */
    ThreadPool pool(1, false);
    std::latch blocker_started(1);
    std::latch release_blocker(1);
    pool.submit([&]() {
        blocker_started.count_down();
        release_blocker.wait();
    });
    blocker_started.wait();

    std::atomic<int> progress{0};
    {
        asio::io_context ctx;
        asio::co_spawn(ctx,
                       co_run(pool.executor(), [&]() -> int { return progress.fetch_add(1) + 1; }),
                       asio::detached);
        /** Run the coroutine until it first suspends, which guarantees the task is enqueued */
        ctx.run_one();
    }

    /** stop() joins the blocked worker from another thread; once m_done is visible, the worker
        exits without touching the queued task, which is then cleared */
    std::thread stopper([&] { pool.stop(); });
    while (!pool.done()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    release_blocker.count_down();
    stopper.join();

    CHECK_EQ(progress.load(), 0);
}

/**
 * @brief Helper: cancelling a co_run/co_run_ec wait aborts the co_await immediately while func
 * keeps running on the worker until it is released
 */
template <bool UseErrorCode>
static asio::awaitable<void> test_co_run_cancel_helper(ThreadPool& pool, std::latch& func_started,
                                                       std::latch& release_func,
                                                       std::atomic<bool>& aborted) {
    auto func = [&]() -> int {
        func_started.count_down();
        release_func.wait();
        return 42;
    };

    try {
        if constexpr (UseErrorCode) {
            (void)co_await co_run_ec(pool.executor(), func);
        } else {
            (void)co_await co_run(pool.executor(), func);
        }
    } catch (const boost::system::system_error& e) {
        aborted.store(e.code() == asio::error::operation_aborted, std::memory_order_release);
    }
}

TEST_CASE("test_co_run_cancellation") {
    ThreadPool pool(2);
    asio::io_context ctx;
    std::latch func_started(1);
    std::latch release_func(1);
    std::atomic<bool> aborted{false};
    asio::cancellation_signal signal;

    asio::co_spawn(ctx, test_co_run_cancel_helper<false>(pool, func_started, release_func, aborted),
                   asio::bind_cancellation_slot(signal.slot(), asio::detached));

    std::thread runner([&] { ctx.run(); });
    func_started.wait();
    signal.emit(asio::cancellation_type::terminal);
    runner.join();

    /** The wait is aborted immediately even though func cannot be force-interrupted */
    CHECK_UNARY(aborted.load(std::memory_order_acquire));
    release_func.count_down();
}

TEST_CASE("test_co_run_ec_cancellation") {
    ThreadPool pool(2);
    asio::io_context ctx;
    std::latch func_started(1);
    std::latch release_func(1);
    std::atomic<bool> aborted{false};
    asio::cancellation_signal signal;

    asio::co_spawn(ctx, test_co_run_cancel_helper<true>(pool, func_started, release_func, aborted),
                   asio::bind_cancellation_slot(signal.slot(), asio::detached));

    std::thread runner([&] { ctx.run(); });
    func_started.wait();
    signal.emit(asio::cancellation_type::terminal);
    runner.join();

    CHECK_UNARY(aborted.load(std::memory_order_acquire));
    release_func.count_down();
}

/**
 * @brief Helpers: when the executor rejects the task (the pool is already stopped), the
 * rejection surfaces as an exception instead of hanging the coroutine forever
 */
static asio::awaitable<void> test_co_run_stopped_executor_helper(ThreadPool& pool,
                                                                 std::atomic<bool>& caught) {
    try {
        (void)co_await co_run(pool.executor(), []() -> int { return 42; });
    } catch (const std::logic_error&) {
        caught.store(true, std::memory_order_release);
    }
}

static asio::awaitable<void> test_co_run_ec_stopped_executor_helper(ThreadPool& pool,
                                                                    std::atomic<bool>& caught) {
    try {
        (void)co_await co_run_ec(pool.executor(), []() -> int { return 42; });
    } catch (const boost::system::system_error& e) {
        caught.store(
          e.code() == boost::system::errc::make_error_code(boost::system::errc::io_error),
          std::memory_order_release);
    }
}

TEST_CASE("test_co_run_stopped_executor") {
    ThreadPool pool(1, false);
    pool.stop();

    std::atomic<bool> caught{false};
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_stopped_executor_helper(pool, caught), asio::detached);
    ctx.run();
    CHECK_UNARY(caught.load(std::memory_order_acquire));
}

TEST_CASE("test_co_run_ec_stopped_executor") {
    ThreadPool pool(1, false);
    pool.stop();

    std::atomic<bool> caught{false};
    asio::io_context ctx;
    asio::co_spawn(ctx, test_co_run_ec_stopped_executor_helper(pool, caught), asio::detached);
    ctx.run();
    CHECK_UNARY(caught.load(std::memory_order_acquire));
}

#endif  // co_run coroutine tests (GCC > 12, clang and other C++20 compilers)
#endif  // CPP_STANDARD >= CPP_STANDARD_20

/**
 * @defgroup test_hikyuu_algorithm test_hikyuu_algorithm
 * @ingroup test_hikyuu_utilities
 * @{
 */

/** @par 检测点 */
TEST_CASE("test_parallelIndexRange") {
    /** @arg start=0, end=0 */
    std::vector<range_t> ranges = parallelIndexRange(0, 0);
    CHECK_UNARY(ranges.empty());

    /** @arg start=2, end=2 */
    ranges = parallelIndexRange(2, 2);
    CHECK_UNARY(ranges.empty());

    /** @arg start=1, end=0 */
    ranges = parallelIndexRange(1, 0);
    CHECK_UNARY(ranges.empty());

    /** @arg start=0, end=3 */
    ranges = parallelIndexRange(0, 3);
    CHECK_UNARY(ranges.size() > 0);

    std::vector<size_t> expect(3);
    for (size_t i = 0; i < expect.size(); ++i) {
        expect[i] = i;
    }

    std::vector<size_t> result;
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }

    /** @arg start=0, end=100 */
    ranges = parallelIndexRange(0, 100);
    CHECK_UNARY(ranges.size() > 0);

    expect.resize(100);
    for (size_t i = 0; i < expect.size(); ++i) {
        expect[i] = i;
    }

    result.clear();
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }

    /** @arg start=1, end=100 */
    ranges = parallelIndexRange(1, 100);
    CHECK_UNARY(ranges.size() > 0);

    expect.resize(99);
    for (size_t i = 0; i < expect.size(); ++i) {
        expect[i] = i + 1;
    }

    result.clear();
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }

    /** @arg start=99, end=100 */
    ranges = parallelIndexRange(99, 100);
    CHECK_UNARY(ranges.size() > 0);

    expect.resize(1);
    expect[0] = 99;

    result.clear();
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }

    /** @arg start=99, end=100 */
    ranges = parallelIndexRange(17, 50);
    CHECK_UNARY(ranges.size() > 0);

    expect.resize(50 - 17);
    for (size_t i = 0; i < expect.size(); ++i) {
        expect[i] = i + 17;
    }

    result.clear();
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }
}

TEST_CASE("test_parallelIndexRange2") {
    std::vector<std::pair<size_t, size_t>> expect{{0, 1}, {1, 2}};
    auto result = parallelIndexRange(0, 2);
    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0, len = expect.size(); i < len; i++) {
        CHECK_EQ(result[i].first, expect[i].first);
        CHECK_EQ(result[i].second, expect[i].second);
    }

    size_t cpu_num = std::thread::hardware_concurrency();
    if (cpu_num == 32) {
        result = parallelIndexRange(0, 100);
        expect = {{0, 3},   {3, 6},   {6, 9},   {9, 12},  {12, 15}, {15, 18}, {18, 21}, {21, 24},
                  {24, 27}, {27, 30}, {30, 33}, {33, 36}, {36, 39}, {39, 42}, {42, 45}, {45, 48},
                  {48, 51}, {51, 54}, {54, 57}, {57, 60}, {60, 63}, {63, 66}, {66, 69}, {69, 72},
                  {72, 75}, {75, 78}, {78, 81}, {81, 84}, {84, 87}, {87, 90}, {90, 93}, {93, 96},
                  {96, 97}, {97, 98}, {98, 99}, {99, 100}};
        CHECK_EQ(result.size(), expect.size());
        for (size_t i = 0, len = expect.size(); i < len; i++) {
            CHECK_EQ(result[i].first, expect[i].first);
            CHECK_EQ(result[i].second, expect[i].second);
        }

    } else if (cpu_num == 8) {
        result = parallelIndexRange(0, 35);
        expect = {{0, 4},   {4, 8},   {8, 12},  {12, 16}, {16, 20}, {20, 24},
                  {24, 28}, {28, 32}, {32, 33}, {33, 34}, {34, 35}};
        CHECK_EQ(result.size(), expect.size());
        for (size_t i = 0, len = expect.size(); i < len; i++) {
            CHECK_EQ(result[i].first, expect[i].first);
            CHECK_EQ(result[i].second, expect[i].second);
        }
    }
}

TEST_CASE("test_parallel_for_index") {
    std::vector<size_t> values(100);
    for (size_t i = 0, len = values.size(); i < len; i++) {
        values[i] = i;
    }

    auto result = parallel_for_index(0, values.size(), [](size_t i) { return i + 1; });

    std::vector<size_t> expect(100);
    for (size_t i = 0, len = expect.size(); i < len; i++) {
        expect[i] = i + 1;
    }
    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0, len = expect.size(); i < len; i++) {
        CHECK_EQ(result[i], expect[i]);
    }
}

TEST_CASE("test_parallel_for_index_single") {
    std::vector<size_t> values(100);
    for (size_t i = 0, len = values.size(); i < len; i++) {
        values[i] = i;
    }

    auto result = parallel_for_index_single(0, values.size(), [](size_t i) { return i + 1; });
    // auto result = parallel_for_index_single<std::function<size_t(size_t)>, MQStealThreadPool>(
    //   0, values.size(), [](size_t i) { return i + 1; });

    std::vector<size_t> expect(100);
    for (size_t i = 0, len = expect.size(); i < len; i++) {
        expect[i] = i + 1;
    }
    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0, len = expect.size(); i < len; i++) {
        CHECK_EQ(result[i], expect[i]);
    }
}

/** @} */
