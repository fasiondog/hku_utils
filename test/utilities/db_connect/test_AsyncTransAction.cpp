/*
 * test_AsyncTransAction.cpp
 *
 *  Copyright (c) 2026 hikyuu.org
 *
 *  Created on: 2026-05-09
 *      Author: fasiondog
 */

#include "test_config.h"

#include "hikyuu/utilities/db_connect/AsyncTransAction.h"
#include <doctest/doctest.h>

using namespace hku;

//-----------------------------------------------------------------------------
// Mock-based tests (no real database required, run in the default build)
//-----------------------------------------------------------------------------
class MockAsyncDBConnect : public AsyncDBConnectBase {
public:
    explicit MockAsyncDBConnect(const Parameter& param = Parameter()) : AsyncDBConnectBase(param) {}

    net::awaitable<bool> ping() override {
        co_return true;
    }

    net::awaitable<void> transaction() override {
        if (transaction_throws) {
            throw std::runtime_error("mock transaction failure");
        }
        ++transaction_calls;
        co_return;
    }

    net::awaitable<void> commit() override {
        if (commit_throws) {
            throw std::runtime_error("mock commit failure");
        }
        ++commit_calls;
        co_return;
    }

    net::awaitable<void> rollback() noexcept override {
        ++rollback_calls;
        co_return;
    }

    net::awaitable<int64_t> exec(const std::string&) override {
        co_return 0;
    }

    net::awaitable<AsyncSQLStatementPtr> getStatement(const std::string&) override {
        co_return AsyncSQLStatementPtr();
    }

    net::awaitable<bool> tableExist(const std::string&) override {
        co_return false;
    }

    net::awaitable<void> resetAutoIncrement(const std::string&) override {
        co_return;
    }

    int transaction_calls = 0;
    int commit_calls = 0;
    int rollback_calls = 0;
    bool transaction_throws = false;
    bool commit_throws = false;
};

/**
 * @par 检测点
 * 1. create() 自动开启事务（transaction 调用一次）
 * 2. 析构自动提交（作用域退出无需显式 commit，detached 协程被执行）
 * 3. 提交成功后不触发回滚
 */
TEST_CASE("test_AsyncAutoTransAction_commit_on_destroy") {
    boost::asio::io_context io_context;
    auto driver = std::make_shared<MockAsyncDBConnect>();

    boost::asio::co_spawn(
      io_context,
      [&]() -> net::awaitable<void> {
          {
              auto trans = co_await AsyncAutoTransAction::create(driver);
              CHECK_EQ(driver->transaction_calls, 1);
              // No explicit commit: the destructor commits automatically
          }
          co_return;
      },
      boost::asio::detached);

    // The detached commit coroutine is spawned during the destruction and must be executed
    // before run() returns
    io_context.run();

    CHECK_EQ(driver->transaction_calls, 1);
    CHECK_EQ(driver->commit_calls, 1);
    CHECK_EQ(driver->rollback_calls, 0);
}

/**
 * @par 检测点
 * 1. commit 失败：析构派生的提交协程失败后自动回滚
 */
TEST_CASE("test_AsyncAutoTransAction_commit_failure_rolls_back_on_destroy") {
    boost::asio::io_context io_context;
    auto driver = std::make_shared<MockAsyncDBConnect>();
    driver->commit_throws = true;

    boost::asio::co_spawn(
      io_context,
      [&]() -> net::awaitable<void> {
          {
              auto trans = co_await AsyncAutoTransAction::create(driver);
          }
          co_return;
      },
      boost::asio::detached);

    io_context.run();

    CHECK_EQ(driver->transaction_calls, 1);
    CHECK_EQ(driver->commit_calls, 0);
    CHECK_EQ(driver->rollback_calls, 1);
}

/**
 * @par 检测点
 * 1. transaction() 失败：create 抛异常，事务未启动
 * 2. 事务未启动时析构不触发任何 commit/rollback
 */
TEST_CASE("test_AsyncAutoTransAction_create_failure") {
    boost::asio::io_context io_context;
    auto driver = std::make_shared<MockAsyncDBConnect>();
    driver->transaction_throws = true;

    boost::asio::co_spawn(
      io_context,
      [&]() -> net::awaitable<void> {
          bool threw = false;
          try {
              co_await AsyncAutoTransAction::create(driver);
          } catch (const std::exception&) {
              threw = true;
          }
          CHECK(threw);
          co_return;
      },
      boost::asio::detached);

    io_context.run();
    io_context.run();

    CHECK_EQ(driver->transaction_calls, 0);
    CHECK_EQ(driver->commit_calls, 0);
    CHECK_EQ(driver->rollback_calls, 0);
}

/**
 * @par 检测点
 * 1. io_context 已停止后析构：不再派生提交协程（避免协程永不执行而钉住连接），无崩溃
 */
TEST_CASE("test_AsyncAutoTransAction_stopped_io_context") {
    boost::asio::io_context io_context;
    auto driver = std::make_shared<MockAsyncDBConnect>();
    std::shared_ptr<AsyncAutoTransAction> trans;

    boost::asio::co_spawn(
      io_context,
      [&]() -> net::awaitable<void> {
          trans = co_await AsyncAutoTransAction::create(driver);
          co_return;
      },
      boost::asio::detached);

    io_context.run();
    CHECK(trans != nullptr);
    CHECK_EQ(driver->commit_calls, 0);

    // Simulate the application shutdown: stop the io_context before the transaction is released
    io_context.stop();
    CHECK(io_context.stopped());

    // Destroy the transaction on the stopped io_context: no commit coroutine is spawned, no crash
    trans.reset();
    CHECK_EQ(driver->commit_calls, 0);
    CHECK_EQ(driver->rollback_calls, 0);
}

//-----------------------------------------------------------------------------
// Tests against the real MySQL server (require the external environment)
//-----------------------------------------------------------------------------
#if ENABLE_MYSQL_TEST && HKU_ENABLE_MYSQL

#include "hikyuu/utilities/db_connect/mysql/AsyncMySQLConnect.h"
#include "hikyuu/utilities/ini_parser/IniParser.h"
#include "hikyuu/utilities/os.h"
#include <filesystem>

static Parameter loadMySQLConfig() {
    Parameter param;

    // 获取用户主目录
    const char* home = std::getenv("HOME");
    if (!home) {
        return param;
    }

    std::string ini_path = std::string(home) + "/workspace/dev.ini";

    // 检查文件是否存在
    if (!std::filesystem::exists(ini_path)) {
        return param;
    }

    try {
        IniParser parser;
        parser.read(ini_path);

        // 读取 mysql57 配置段
        if (parser.hasSection("mysql57")) {
            std::string host = parser.get("mysql57", "host", "127.0.0.1");
            int port = parser.getInt("mysql57", "port", "3306");
            std::string user = parser.get("mysql57", "user", "root");

            param.set<std::string>("host", host);
            param.set<int>("port", port);
            param.set<std::string>("usr", user);
            param.set<std::string>("pwd", parser.get("mysql57", "pwd", ""));
            param.set<std::string>("database", parser.get("mysql57", "database", "test"));
        }
    } catch (const std::exception& e) {
        // Ignore errors
    }

    return param;
}

TEST_CASE("test_async_auto_trans_action_basic") {
    // 测试 AsyncAutoTransAction 基本功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                MESSAGE("Connection failed");
                co_return;
            }

            // 选择或创建 test 数据库
            bool use_success = true;
            try {
                co_await conn->exec("USE test");
            } catch (...) {
                use_success = false;
            }

            if (!use_success) {
                co_await conn->exec("CREATE DATABASE IF NOT EXISTS test");
                co_await conn->exec("USE test");
            }

            // 创建测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_async_auto_trans");
            co_await conn->exec(R"(
                CREATE TABLE test_async_auto_trans (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    value INT
                )
            )");

            // 测试正常提交流程：作用域退出后析构自动提交
            {
                auto trans = co_await AsyncAutoTransAction::create(conn);
                co_await trans->connect()->exec(
                  "INSERT INTO test_async_auto_trans (value) VALUES (100)");
                // 注意：没有显式 commit，依赖析构自动提交
            }

            // 等待一小段时间让 detached 协程执行提交
            co_await boost::asio::steady_timer(co_await net::this_coro::executor,
                                               std::chrono::milliseconds(100))
              .async_wait(boost::asio::use_awaitable);

            // 验证数据已提交
            auto stmt = co_await conn->getStatement("SELECT COUNT(*) FROM test_async_auto_trans");
            co_await stmt->exec();

            if (co_await stmt->moveNext()) {
                int count = 0;
                stmt->getColumn(0, count);
                CHECK(count == 1);

                if (count == 1) {
                    test_passed = true;
                }
            }

            // 清理
            co_await conn->exec("DROP TABLE IF EXISTS test_async_auto_trans");

        } catch (const std::exception& e) {
            MESSAGE("AsyncAutoTransAction basic test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!test_passed) {
        MESSAGE("AsyncAutoTransAction basic test failed");
    }
}

TEST_CASE("test_async_auto_trans_action_commit_on_destroy") {
    // 测试 AsyncAutoTransAction 作用域退出（无显式 commit）时析构自动提交
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool commit_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                MESSAGE("Connection failed");
                co_return;
            }

            // 选择或创建 test 数据库
            bool use_success = true;
            try {
                co_await conn->exec("USE test");
            } catch (...) {
                use_success = false;
            }

            if (!use_success) {
                co_await conn->exec("CREATE DATABASE IF NOT EXISTS test");
                co_await conn->exec("USE test");
            }

            // 创建测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_async_auto_trans_commit");
            co_await conn->exec(R"(
                CREATE TABLE test_async_auto_trans_commit (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    value INT
                )
            )");

            // 测试未显式调用 commit，析构时应自动提交
            {
                auto trans = co_await AsyncAutoTransAction::create(conn);
                co_await trans->connect()->exec(
                  "INSERT INTO test_async_auto_trans_commit (value) VALUES (200)");
                // 注意：没有调用 commit()
            }

            // 等待一小段时间让 detached 协程执行提交
            co_await boost::asio::steady_timer(co_await net::this_coro::executor,
                                               std::chrono::milliseconds(100))
              .async_wait(boost::asio::use_awaitable);

            // 验证数据已提交（应该为 1）
            auto stmt =
              co_await conn->getStatement("SELECT COUNT(*) FROM test_async_auto_trans_commit");
            co_await stmt->exec();

            if (co_await stmt->moveNext()) {
                int count = 0;
                stmt->getColumn(0, count);
                CHECK(count == 1);

                if (count == 1) {
                    commit_success = true;
                }
            }

            // 清理
            co_await conn->exec("DROP TABLE IF EXISTS test_async_auto_trans_commit");

        } catch (const std::exception& e) {
            MESSAGE("AsyncAutoTransAction commit-on-destroy test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!commit_success) {
        MESSAGE("AsyncAutoTransAction commit-on-destroy test failed");
    }
}

TEST_CASE("test_async_trans_action_basic") {
    // 测试 AsyncTransAction 基本功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                MESSAGE("Connection failed");
                co_return;
            }

            // 选择或创建 test 数据库
            bool use_success = true;
            try {
                co_await conn->exec("USE test");
            } catch (...) {
                use_success = false;
            }

            if (!use_success) {
                co_await conn->exec("CREATE DATABASE IF NOT EXISTS test");
                co_await conn->exec("USE test");
            }

            // 创建测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_async_trans");
            co_await conn->exec(R"(
                CREATE TABLE test_async_trans (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    value INT
                )
            )");

            // 测试正常提交流程
            {
                auto trans = co_await AsyncTransAction::create(conn);
                co_await trans->connect()->exec(
                  "INSERT INTO test_async_trans (value) VALUES (300)");
                co_await trans->end();  // 使用 end() 提交
            }

            // 验证数据已提交
            auto stmt = co_await conn->getStatement("SELECT COUNT(*) FROM test_async_trans");
            co_await stmt->exec();

            if (co_await stmt->moveNext()) {
                int count = 0;
                stmt->getColumn(0, count);
                CHECK(count == 1);

                if (count == 1) {
                    test_passed = true;
                }
            }

            // 清理
            co_await conn->exec("DROP TABLE IF EXISTS test_async_trans");

        } catch (const std::exception& e) {
            MESSAGE("AsyncTransAction basic test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!test_passed) {
        MESSAGE("AsyncTransAction basic test failed");
    }
}

TEST_CASE("test_async_trans_action_manual_rollback") {
    // 测试 AsyncTransAction 手动回滚
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool rollback_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                MESSAGE("Connection failed");
                co_return;
            }

            // 选择或创建 test 数据库
            bool use_success = true;
            try {
                co_await conn->exec("USE test");
            } catch (...) {
                use_success = false;
            }

            if (!use_success) {
                co_await conn->exec("CREATE DATABASE IF NOT EXISTS test");
                co_await conn->exec("USE test");
            }

            // 创建测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_async_trans_rollback");
            co_await conn->exec(R"(
                CREATE TABLE test_async_trans_rollback (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    value INT
                )
            )");

            // 测试手动回滚
            {
                auto trans = co_await AsyncTransAction::create(conn);
                co_await trans->connect()->exec(
                  "INSERT INTO test_async_trans_rollback (value) VALUES (400)");
                co_await trans->rollback();  // 手动回滚
            }

            // 验证数据已回滚
            auto stmt =
              co_await conn->getStatement("SELECT COUNT(*) FROM test_async_trans_rollback");
            co_await stmt->exec();

            if (co_await stmt->moveNext()) {
                int count = 0;
                stmt->getColumn(0, count);
                CHECK(count == 0);

                if (count == 0) {
                    rollback_success = true;
                }
            }

            // 清理
            co_await conn->exec("DROP TABLE IF EXISTS test_async_trans_rollback");

        } catch (const std::exception& e) {
            MESSAGE("AsyncTransAction manual rollback test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!rollback_success) {
        MESSAGE("AsyncTransAction manual rollback test failed");
    }
}

#endif
