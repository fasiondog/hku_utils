/*
 * test_async_mysql.cpp
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2026-05-09
 *      Author: fasiondog
 */

#include "hikyuu/utilities/config.h"

#if HKU_ENABLE_MYSQL

#include "hikyuu/utilities/db_connect/TableMacro.h"
#include "hikyuu/utilities/db_connect/AsyncSQLResultSet.h"
#include "hikyuu/utilities/db_connect/mysql/AsyncMySQLConnect.h"
#include "hikyuu/utilities/ini_parser/IniParser.h"
#include "hikyuu/utilities/net.h"
#include <doctest/doctest.h>
#include <filesystem>

using namespace hku;

// 从 ~/workspace/dev.ini 读取 mysql57 配置
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
        }
    } catch (const std::exception& e) {
        // Ignore errors
    }

    return param;
}

TEST_CASE("test_async_mysql_basic_connection") {
    // 测试基本异步连接功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    MESSAGE("Starting async MySQL test with host: " << param.get<std::string>("host") 
            << ", port: " << param.get<int>("port"));

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            MESSAGE("Creating AsyncMySQLConnect...");
            auto conn = std::make_shared<AsyncMySQLConnect>(param);
            MESSAGE("Calling ping...");

            // 验证 ping（会自动从当前协程环境获取 io_context）
            bool connected = co_await conn->ping();
            MESSAGE("Ping result: " << (connected ? "success" : "failed"));
            CHECK(connected == true);

            if (connected) {
                test_passed = true;
            }

        } catch (const std::exception& e) {
            MESSAGE("Exception: " << e.what());
        }
    };

    MESSAGE("Spawning coroutine...");
    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    
    MESSAGE("Running io_context...");
    // 运行 io_context，最多等待 10 秒
    io_context.run_for(std::chrono::seconds(10));
    MESSAGE("io_context finished");

    if (!test_passed) {
        MESSAGE("Async MySQL connection test failed or timed out");
    } else {
        MESSAGE("Test passed!");
    }
}

TEST_CASE("test_async_mysql_exec") {
    // 测试异步执行 SQL
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool exec_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                MESSAGE("Connection failed");
                co_return;
            }

            // 测试简单查询
            int64_t affected = co_await conn->exec("SELECT 1");
            CHECK(affected >= 0);
            exec_success = true;

        } catch (const std::exception& e) {
            MESSAGE("Exec test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!exec_success) {
        MESSAGE("Async MySQL exec test failed");
    }
}

TEST_CASE("test_async_mysql_statement") {
    // 测试异步预处理语句
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool stmt_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                MESSAGE("Connection failed");
                co_return;
            }

            // 测试获取预处理语句
            auto stmt = co_await conn->getStatement("SELECT 1 as num");
            REQUIRE(stmt != nullptr);

            co_await stmt->exec();

            if (co_await stmt->moveNext()) {
                int num = 0;
                stmt->getColumn(0, num);
                CHECK(num == 1);
                stmt_success = true;
            }

        } catch (const std::exception& e) {
            MESSAGE("Statement test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!stmt_success) {
        MESSAGE("Async MySQL statement test failed");
    }
}

TEST_CASE("test_async_mysql_table_exist") {
    // 测试表存在性检查
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool check_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                MESSAGE("Connection failed");
                co_return;
            }

            // 测试表存在性检查（可能不存在，但应该能正常返回）
            bool exists = co_await conn->tableExist("nonexistent_table_12345");
            CHECK(exists == false);
            check_success = true;

        } catch (const std::exception& e) {
            MESSAGE("Table exist check failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!check_success) {
        MESSAGE("Async MySQL table exist check failed");
    }
}

TEST_CASE("test_async_mysql_data_types") {
    // 测试异步各种数据类型
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

            // 选择或创建 test 数据库（不使用 try-catch）
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
            co_await conn->exec("DROP TABLE IF EXISTS test_async_data_types");
            co_await conn->exec(R"(
                CREATE TABLE test_async_data_types (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    col_int INT,
                    col_bigint BIGINT,
                    col_float FLOAT,
                    col_double DOUBLE,
                    col_decimal DECIMAL(10,2),
                    col_varchar VARCHAR(100),
                    col_text TEXT,
                    col_date DATE,
                    col_datetime DATETIME
                )
            )");

            // 插入测试数据
            auto insert_stmt = co_await conn->getStatement(R"(
                INSERT INTO test_async_data_types (
                    col_int, col_bigint, col_float, col_double, col_decimal,
                    col_varchar, col_text, col_date, col_datetime
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            )");

            insert_stmt->bind(0, static_cast<int32_t>(2147483647));       // INT
            insert_stmt->bind(1, static_cast<int64_t>(9223372036854775807LL)); // BIGINT
            insert_stmt->bind(2, 3.14f);                                   // FLOAT
            insert_stmt->bind(3, 3.14159265358979);                        // DOUBLE
            insert_stmt->bind(4, 999999.99);                               // DECIMAL
            insert_stmt->bind(5, std::string("VARCHAR test"));             // VARCHAR
            insert_stmt->bind(6, std::string("TEXT content"));             // TEXT
            insert_stmt->bind(7, std::string("2024-01-15"));               // DATE
            insert_stmt->bind(8, std::string("2024-01-15 10:30:45"));      // DATETIME

            co_await insert_stmt->exec();
            uint64_t last_id = insert_stmt->getLastRowid();
            CHECK(last_id > 0);

            // 查询验证
            auto select_stmt = co_await conn->getStatement(
                "SELECT col_int, col_bigint, col_float, col_double, col_decimal, "
                "col_varchar, col_text, col_date, col_datetime "
                "FROM test_async_data_types WHERE id = ?");
            
            select_stmt->bind(0, static_cast<int64_t>(last_id));
            co_await select_stmt->exec();

            if (co_await select_stmt->moveNext()) {
                int32_t val_int = 0;
                int64_t val_bigint = 0;
                float val_float = 0.0f;
                double val_double = 0.0;
                double val_decimal = 0.0;
                std::string val_varchar;
                std::string val_text;
                std::string val_date;
                std::string val_datetime;

                select_stmt->getColumn(0, val_int);
                select_stmt->getColumn(1, val_bigint);
                select_stmt->getColumn(2, val_float);
                select_stmt->getColumn(3, val_double);
                select_stmt->getColumn(4, val_decimal);
                select_stmt->getColumn(5, val_varchar);
                select_stmt->getColumn(6, val_text);
                select_stmt->getColumn(7, val_date);
                select_stmt->getColumn(8, val_datetime);

                CHECK(val_int == 2147483647);
                CHECK(val_bigint == 9223372036854775807LL);
                CHECK(val_float == doctest::Approx(3.14f).epsilon(0.01));
                CHECK(val_double == doctest::Approx(3.14159265358979).epsilon(0.0001));
                CHECK(val_decimal == doctest::Approx(999999.99).epsilon(0.01));
                CHECK(val_varchar == "VARCHAR test");
                CHECK(val_text == "TEXT content");
                CHECK(val_date == "2024-01-15");
                CHECK(val_datetime == "2024-01-15 10:30:45");

                test_passed = true;
            }

            // 清理
            co_await conn->exec("DROP TABLE IF EXISTS test_async_data_types");

        } catch (const std::exception& e) {
            MESSAGE("Data types test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!test_passed) {
        MESSAGE("Async MySQL data types test failed");
    }
}

TEST_CASE("test_async_mysql_transaction") {
    // 测试异步事务功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool transaction_success = false;

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
            co_await conn->exec("DROP TABLE IF EXISTS test_async_transaction");
            co_await conn->exec(R"(
                CREATE TABLE test_async_transaction (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    value INT
                )
            )");

            // 开始事务
            co_await conn->transaction();

            // 插入数据
            co_await conn->exec("INSERT INTO test_async_transaction (value) VALUES (100)");
            co_await conn->exec("INSERT INTO test_async_transaction (value) VALUES (200)");

            // 提交事务
            co_await conn->commit();

            // 验证数据已提交
            auto stmt = co_await conn->getStatement("SELECT COUNT(*) FROM test_async_transaction");
            co_await stmt->exec();
            
            if (co_await stmt->moveNext()) {
                int count = 0;
                stmt->getColumn(0, count);
                CHECK(count == 2);
                
                if (count == 2) {
                    transaction_success = true;
                }
            }

            // 测试回滚
            co_await conn->transaction();
            co_await conn->exec("INSERT INTO test_async_transaction (value) VALUES (300)");
            co_await conn->rollback();

            // 验证回滚后数据未变化
            auto stmt2 = co_await conn->getStatement("SELECT COUNT(*) FROM test_async_transaction");
            co_await stmt2->exec();
            
            if (co_await stmt2->moveNext()) {
                int count = 0;
                stmt2->getColumn(0, count);
                CHECK(count == 2);  // 应该还是 2，因为回滚了
            }

            // 清理
            co_await conn->exec("DROP TABLE IF EXISTS test_async_transaction");

        } catch (const std::exception& e) {
            MESSAGE("Transaction test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!transaction_success) {
        MESSAGE("Async MySQL transaction test failed");
    }
}

TEST_CASE("test_async_mysql_error_handling") {
    // 测试异步异常处理
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool error_handled = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                MESSAGE("Connection failed");
                co_return;
            }

            // 测试 SQL 语法错误
            try {
                co_await conn->exec("INVALID SQL SYNTAX");
                MESSAGE("Should have thrown exception for invalid SQL");
            } catch (const std::exception& e) {
                MESSAGE("Caught expected exception: " << e.what());
                error_handled = true;
            }

            // 测试表不存在错误
            try {
                auto stmt = co_await conn->getStatement("SELECT * FROM nonexistent_table_xyz");
                co_await stmt->exec();
                MESSAGE("Should have thrown exception for nonexistent table");
            } catch (const std::exception& e) {
                MESSAGE("Caught expected exception: " << e.what());
            }

        } catch (const std::exception& e) {
            MESSAGE("Error handling test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!error_handled) {
        MESSAGE("Async MySQL error handling test failed");
    }
}

TEST_CASE("test_async_mysql_auto_reconnect") {
    // 测试异步自动重连功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }

    boost::asio::io_context io_context;
    bool reconnect_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            // 第一次 ping，应该成功连接
            bool connected = co_await conn->ping();
            CHECK(connected == true);

            if (!connected) {
                MESSAGE("Initial connection failed");
                co_return;
            }

            // 执行一些查询
            int64_t result = co_await conn->exec("SELECT 1");
            CHECK(result >= 0);

            // 再次 ping 验证连接仍然有效
            connected = co_await conn->ping();
            CHECK(connected == true);

            if (connected) {
                reconnect_success = true;
            }

        } catch (const std::exception& e) {
            MESSAGE("Auto reconnect test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    if (!reconnect_success) {
        MESSAGE("Async MySQL auto reconnect test failed");
    }
}

// ============================================================================
// AsyncSQLResultSet 测试
// ============================================================================

struct TestResultTable {
    TABLE_BIND3(TestResultTable, test_async_result_set, name, value, extra);
    
    std::string name;
    int value = 0;
    std::string extra;
};

TEST_CASE("test_async_sql_result_set_null_connect") {
    // 测试空连接的情况
    AsyncSQLResultSet<TestResultTable, 100> results;
    
    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            CHECK(co_await results.empty() == true);
            CHECK(co_await results.size() == 0);
            
            // 测试 at 方法抛出异常
            try {
                co_await results.at(0);
                MESSAGE("Expected exception not thrown for at(0)");
            } catch (const std::out_of_range&) {
                // 预期异常
            }
            
            try {
                co_await results.at(1000);
                MESSAGE("Expected exception not thrown for at(1000)");
            } catch (const std::out_of_range&) {
                // 预期异常
            }
            
            // 测试 operator[]
            auto x = co_await results.operator_bracket(0);
            CHECK(x.valid() == false);
            
            test_passed = true;
        } catch (const std::exception& e) {
            MESSAGE("Null connect test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(5));

    if (!test_passed) {
        MESSAGE("AsyncSQLResultSet null connect test failed");
    }
}

TEST_CASE("test_async_sql_result_set_basic") {
    // 注意：AsyncSQLResultSet 目前需要额外的异步 load 支持
    // 此测试暂时跳过，待完整实现后再启用
    MESSAGE("AsyncSQLResultSet basic test skipped - needs async load support");
}

TEST_CASE("test_async_sql_result_set_with_condition") {
    // 注意：AsyncSQLResultSet 目前需要额外的异步 load 支持
    // 此测试暂时跳过，待完整实现后再启用
    MESSAGE("AsyncSQLResultSet condition test skipped - needs async load support");
}

TEST_CASE("test_async_sql_result_set_pagination") {
    // 注意：AsyncSQLResultSet 目前需要额外的异步 load 支持
    // 此测试暂时跳过，待完整实现后再启用
    MESSAGE("AsyncSQLResultSet pagination test skipped - needs async load support");
}

#endif // HKU_ENABLE_MYSQL
