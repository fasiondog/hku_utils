/*
 * test_AsyncTransAction.cpp
 *
 *  Copyright (c) 2026 hikyuu.org
 *
 *  Created on: 2026-05-09
 *      Author: fasiondog
 */

#include "test_config.h"

#if ENABLE_MYSQL_TEST && HKU_ENABLE_MYSQL

#include "hikyuu/utilities/db_connect/AsyncTransAction.h"
#include "hikyuu/utilities/db_connect/mysql/AsyncMySQLConnect.h"
#include "hikyuu/utilities/ini_parser/IniParser.h"
#include "hikyuu/utilities/os.h"
#include <doctest/doctest.h>
#include <filesystem>

using namespace hku;

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

            // 测试正常提交流程
            {
                auto trans = co_await AsyncAutoTransAction::create(conn);
                co_await trans->connect()->exec(
                  "INSERT INTO test_async_auto_trans (value) VALUES (100)");
                co_await trans->commit();
            }

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

TEST_CASE("test_async_auto_trans_action_rollback_on_destroy") {
    // 测试 AsyncAutoTransAction 析构时自动回滚
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
            co_await conn->exec("DROP TABLE IF EXISTS test_async_auto_trans_rollback");
            co_await conn->exec(R"(
                CREATE TABLE test_async_auto_trans_rollback (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    value INT
                )
            )");

            // 测试未调用 commit，析构时应自动回滚
            {
                auto trans = co_await AsyncAutoTransAction::create(conn);
                co_await trans->connect()->exec(
                  "INSERT INTO test_async_auto_trans_rollback (value) VALUES (200)");
                // 注意：没有调用 commit()
            }

            // 等待一小段时间让 detached 协程执行回滚
            co_await boost::asio::steady_timer(co_await net::this_coro::executor,
                                               std::chrono::milliseconds(100))
              .async_wait(boost::asio::use_awaitable);

            // 验证数据已回滚（应该为 0）
            auto stmt =
              co_await conn->getStatement("SELECT COUNT(*) FROM test_async_auto_trans_rollback");
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
            co_await conn->exec("DROP TABLE IF EXISTS test_async_auto_trans_rollback");

        } catch (const std::exception& e) {
            MESSAGE("AsyncAutoTransAction rollback test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!rollback_success) {
        MESSAGE("AsyncAutoTransAction rollback test failed");
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
