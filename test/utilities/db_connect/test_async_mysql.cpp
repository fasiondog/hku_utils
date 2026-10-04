/*
 * test_async_mysql.cpp
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2026-05-09
 *      Author: fasiondog
 */

#include "test_config.h"

#if ENABLE_MYSQL_TEST && HKU_ENABLE_MYSQL

#include "hikyuu/utilities/db_connect/TableMacro.h"
#include "hikyuu/utilities/db_connect/AsyncSQLResultSet.h"
#include "hikyuu/utilities/db_connect/mysql/AsyncMySQLConnect.h"
#include "hikyuu/utilities/ini_parser/IniParser.h"
#include "hikyuu/utilities/net.h"
#include "hikyuu/utilities/ResourceHybridPool.h"
#include "hikyuu/utilities/ResourceAsioPool.h"
#include <doctest/doctest.h>
#include <filesystem>

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
            std::string database = parser.get("mysql57", "database", "test");

            param.set<std::string>("host", host);
            param.set<int>("port", port);
            param.set<std::string>("usr", user);
            param.set<std::string>("pwd", parser.get("mysql57", "pwd", ""));
            param.set<std::string>("db", database);
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
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            // 验证 ping（会自动从当前协程环境获取 io_context）
            bool connected = co_await conn->ping();
            CHECK(connected == true);

            if (connected) {
                test_passed = true;
            }

        } catch (const std::exception& e) {
            MESSAGE("Exception: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);

    // 运行 io_context，最多等待 10 秒
    io_context.run_for(std::chrono::seconds(10));
}

TEST_CASE("test_async_mysql_exec") {
    // 测试异步执行 SQL
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool exec_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
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
}

TEST_CASE("test_async_mysql_statement") {
    // 测试异步预处理语句
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool stmt_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
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
}

TEST_CASE("test_async_mysql_table_exist") {
    // 测试表存在性检查
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool check_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
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
}

TEST_CASE("test_async_mysql_data_types") {
    // 测试异步各种数据类型
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                co_return;
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

            insert_stmt->bind(0, static_cast<int32_t>(2147483647));             // INT
            insert_stmt->bind(1, static_cast<int64_t>(9223372036854775807LL));  // BIGINT
            insert_stmt->bind(2, 3.14f);                                        // FLOAT
            insert_stmt->bind(3, 3.14159265358979);                             // DOUBLE
            insert_stmt->bind(4, 999999.99);                                    // DECIMAL
            insert_stmt->bind(5, std::string("VARCHAR test"));                  // VARCHAR
            insert_stmt->bind(6, std::string("TEXT content"));                  // TEXT
            insert_stmt->bind(7, std::string("2024-01-15"));                    // DATE
            insert_stmt->bind(8, std::string("2024-01-15 10:30:45"));           // DATETIME

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
            // MESSAGE("Data types test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));
}

TEST_CASE("test_async_mysql_transaction") {
    // 测试异步事务功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool transaction_success = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                co_return;
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
}

TEST_CASE("test_async_mysql_error_handling") {
    // 测试异步异常处理
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool error_handled = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                co_return;
            }

            // 测试 SQL 语法错误
            try {
                co_await conn->exec("INVALID SQL SYNTAX");
            } catch (const std::exception& e) {
                // MESSAGE("Caught expected exception: " << e.what());
                error_handled = true;
            }

            // 测试表不存在错误
            try {
                auto stmt = co_await conn->getStatement("SELECT * FROM nonexistent_table_xyz");
                co_await stmt->exec();
            } catch (const std::exception& e) {
                // MESSAGE("Caught expected exception: " << e.what());
            }

        } catch (const std::exception& e) {
            // MESSAGE("Error handling test failed: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));
}

TEST_CASE("test_async_mysql_auto_reconnect") {
    // 测试异步自动重连功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
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
            } catch (const std::out_of_range&) {
                // 预期异常
            }

            try {
                co_await results.at(1000);
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
}

TEST_CASE("test_async_mysql_table_macro_save_load_update") {
    // 测试使用 TableMacro 进行异步 save、load、update、batchSave、batchLoad 操作
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);

            bool connected = co_await conn->ping();
            if (!connected) {
                co_return;
            }

            // 定义测试表结构
            struct TestRecord {
                TABLE_BIND4(TestRecord, test_table_macro, name, age, score, email)

                void reset() {
                    name = "";
                    age = 0;
                    score = 0.0;
                    email = "";
                }

                std::string name;
                int age;
                double score;
                std::string email;
            };

            // 创建测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_table_macro");
            co_await conn->exec(R"(
                CREATE TABLE test_table_macro (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(100),
                    age INT,
                    score DOUBLE,
                    email VARCHAR(200)
                )
            )");

            // 测试 save（插入新记录）
            TestRecord record1;
            record1.name = "Alice";
            record1.age = 25;
            record1.score = 95.5;
            record1.email = "alice@example.com";

            co_await conn->save(record1);
            CHECK(record1.valid() == true);
            CHECK(record1.rowid() > 0);

            // 测试 load（根据条件查询）
            TestRecord loaded_record;
            // co_await conn->load(loaded_record, "name='Alice'");
            co_await conn->load(loaded_record, Field("name") == "Alice");
            CHECK(loaded_record.valid() == true);
            CHECK(loaded_record.name == "Alice");
            CHECK(loaded_record.age == 25);
            CHECK(std::abs(loaded_record.score - 95.5) < 0.001);
            CHECK(loaded_record.email == "alice@example.com");

            // 测试 update（更新已有记录）
            loaded_record.age = 26;
            loaded_record.score = 98.0;
            co_await conn->save(loaded_record);  // save 会自动判断是 insert 还是 update

            // 验证更新后的数据
            TestRecord updated_record;
            co_await conn->load(updated_record, "name='Alice'");
            CHECK(updated_record.age == 26);
            CHECK(std::abs(updated_record.score - 98.0) < 0.001);

            // 测试 batchSave（批量保存）- 不使用自动事务
            std::vector<TestRecord> records;
            TestRecord r1;
            r1.name = "Bob";
            r1.age = 30;
            r1.score = 88.5;
            r1.email = "bob@example.com";

            TestRecord r2;
            r2.name = "Charlie";
            r2.age = 28;
            r2.score = 92.0;
            r2.email = "charlie@example.com";

            records.push_back(r1);
            records.push_back(r2);

            try {
                co_await conn->batchSave(records, false);  // 不使用自动事务
                CHECK(records[0].valid() == true);
                CHECK(records[1].valid() == true);
            } catch (const hku::SQLException& e) {
                throw;
            } catch (const std::exception& e) {
                throw;
            }

            // 测试 batchLoad（批量加载）
            std::vector<TestRecord> all_records;
            try {
                co_await conn->batchLoad(all_records, "1=1 ORDER BY name");
            } catch (const std::exception& e) {
                throw;
            }
            CHECK(all_records.size() >= 3);  // Alice + Bob + Charlie

            // 清理测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_table_macro");

            test_passed = true;

        } catch (const hku::SQLException& e) {
            MESSAGE("TableMacro test failed with SQLException: " << e.what()
                                                                 << ", errcode: " << e.errcode());
        } catch (const std::exception& e) {
            MESSAGE("TableMacro test failed with exception: " << e.what());
        } catch (...) {
            MESSAGE("TableMacro test failed with unknown exception");
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    CHECK(test_passed == true);
}

TEST_CASE("test_async_sql_result_set_basic") {
    // 测试 AsyncSQLResultSet 的基本功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);
            bool connected = co_await conn->ping();
            CHECK(connected == true);

            if (!connected) {
                co_return;
            }

            // 定义测试表结构
            struct TestRecord {
                TABLE_BIND4(TestRecord, test_result_set_basic, name, age, score, email)

                void reset() {
                    name = "";
                    age = 0;
                    score = 0.0;
                    email = "";
                }

                std::string name;
                int age;
                double score;
                std::string email;
            };

            // 创建测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_result_set_basic");
            co_await conn->exec(R"(
                CREATE TABLE test_result_set_basic (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(100),
                    age INT,
                    score DOUBLE,
                    email VARCHAR(200)
                )
            )");

            // 插入测试数据
            for (int i = 1; i <= 25; ++i) {
                TestRecord record;
                record.name = "User" + std::to_string(i);
                record.age = 20 + i;
                record.score = 80.0 + i * 0.5;
                record.email = "user" + std::to_string(i) + "@example.com";
                co_await conn->save(record);
            }

            // 创建 AsyncSQLResultSet
            AsyncSQLResultSet<TestRecord, 10> result_set(conn, "");

            // 测试 size
            size_t count = co_await result_set.size();
            CHECK(count == 25);

            // 测试 empty
            bool is_empty = co_await result_set.empty();
            CHECK(is_empty == false);

            // 测试 getPageCount
            size_t page_count = co_await result_set.getPageCount();
            CHECK(page_count == 3);  // 25条记录，每页10条，共3页

            // 测试 getPage
            auto page0 = co_await result_set.getPage(0);
            CHECK(page0.size() == 10);
            CHECK(page0[0].name == "User1");

            auto page1 = co_await result_set.getPage(1);
            CHECK(page1.size() == 10);
            CHECK(page1[0].name == "User11");

            auto page2 = co_await result_set.getPage(2);
            CHECK(page2.size() == 5);  // 最后一页只有5条
            CHECK(page2[0].name == "User21");

            // 测试 at 方法
            auto first_record = co_await result_set.at(0);
            CHECK(first_record.name == "User1");
            CHECK(first_record.age == 21);

            auto last_record = co_await result_set.at(24);
            CHECK(last_record.name == "User25");
            CHECK(last_record.age == 45);

            // 清理测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_result_set_basic");

            test_passed = true;

        } catch (const hku::SQLException& e) {
            MESSAGE("AsyncSQLResultSet basic test failed: " << e.what()
                                                            << ", errcode: " << e.errcode());
        } catch (const std::exception& e) {
            MESSAGE("AsyncSQLResultSet basic test exception: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    CHECK(test_passed == true);
}

TEST_CASE("test_async_sql_result_set_with_condition") {
    // 测试 AsyncSQLResultSet 带条件的查询
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);
            bool connected = co_await conn->ping();
            CHECK(connected == true);

            if (!connected) {
                co_return;
            }

            // 定义测试表结构
            struct TestRecord {
                TABLE_BIND4(TestRecord, test_result_set_condition, name, age, score, email)

                void reset() {
                    name = "";
                    age = 0;
                    score = 0.0;
                    email = "";
                }

                std::string name;
                int age;
                double score;
                std::string email;
            };

            // 创建测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_result_set_condition");
            co_await conn->exec(R"(
                CREATE TABLE test_result_set_condition (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(100),
                    age INT,
                    score DOUBLE,
                    email VARCHAR(200)
                )
            )");

            // 插入测试数据
            for (int i = 1; i <= 20; ++i) {
                TestRecord record;
                record.name = "User" + std::to_string(i);
                record.age = 20 + i;
                record.score = 80.0 + i * 0.5;
                record.email = "user" + std::to_string(i) + "@example.com";
                co_await conn->save(record);
            }

            // 测试带 WHERE 条件的查询 - 使用 size 方法
            AsyncSQLResultSet<TestRecord, 10> result_set1(conn, "age > 30");
            size_t count1 = co_await result_set1.size();
            CHECK(count1 == 10);  // age > 30 的记录有10条（31-40）

            // 测试带 ORDER BY 的查询
            AsyncSQLResultSet<TestRecord, 10> result_set2(conn, "1=1 ORDER BY age DESC");
            size_t count2 = co_await result_set2.size();
            CHECK(count2 == 20);

            auto first_page = co_await result_set2.getPage(0);
            CHECK(first_page.size() == 10);
            CHECK(first_page[0].age == 40);  // 年龄最大的应该是40

            // 测试复合条件
            AsyncSQLResultSet<TestRecord, 10> result_set3(conn, "age > 25 AND score < 90");
            size_t count3 = co_await result_set3.size();
            CHECK(count3 == 14);  // age > 25 (6-20) 且 score < 90 (1-19)，交集是6-19，共14条

            // 清理测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_result_set_condition");

            test_passed = true;

        } catch (const hku::SQLException& e) {
            MESSAGE("AsyncSQLResultSet condition test failed: " << e.what()
                                                                << ", errcode: " << e.errcode());
        } catch (const std::exception& e) {
            MESSAGE("AsyncSQLResultSet condition test exception: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    CHECK(test_passed == true);
}

TEST_CASE("test_async_sql_result_set_pagination") {
    // 测试 AsyncSQLResultSet 的分页和迭代器功能
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    auto run_test = [&]() -> net::awaitable<void> {
        try {
            auto conn = std::make_shared<AsyncMySQLConnect>(param);
            bool connected = co_await conn->ping();
            CHECK(connected == true);

            if (!connected) {
                co_return;
            }

            // 定义测试表结构
            struct TestRecord {
                TABLE_BIND4(TestRecord, test_result_set_pagination, name, age, score, email)

                void reset() {
                    name = "";
                    age = 0;
                    score = 0.0;
                    email = "";
                }

                std::string name;
                int age;
                double score;
                std::string email;
            };

            // 创建测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_result_set_pagination");
            co_await conn->exec(R"(
                CREATE TABLE test_result_set_pagination (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(100),
                    age INT,
                    score DOUBLE,
                    email VARCHAR(200)
                )
            )");

            // 插入测试数据
            for (int i = 1; i <= 15; ++i) {
                TestRecord record;
                record.name = "User" + std::to_string(i);
                record.age = 20 + i;
                record.score = 80.0 + i * 0.5;
                record.email = "user" + std::to_string(i) + "@example.com";
                co_await conn->save(record);
            }

            // 创建 AsyncSQLResultSet，每页5条
            AsyncSQLResultSet<TestRecord, 5> result_set(conn, "");

            // 测试 size 和分页数量
            size_t total = co_await result_set.size();
            CHECK(total == 15);

            size_t page_count = co_await result_set.getPageCount();
            CHECK(page_count == 3);  // 15条记录，每页5条，共3页

            // 测试迭代器
            size_t iter_count = 0;
            auto it_begin = co_await result_set.begin();
            co_await it_begin.init();  // 初始化迭代器
            auto it_end = result_set.end();

            while (it_begin != it_end) {
                TestRecord record = *it_begin;
                CHECK(record.valid() == true);
                iter_count++;
                it_begin = co_await it_begin.operator_pre_increment();
            }
            CHECK(iter_count == 15);

            // 测试 operator[]
            auto record5 = co_await result_set.operator_bracket(4);
            CHECK(record5.name == "User5");

            auto record10 = co_await result_set.operator_bracket(9);
            CHECK(record10.name == "User10");

            auto record15 = co_await result_set.operator_bracket(14);
            CHECK(record15.name == "User15");

            // 测试跨页访问（应该自动加载对应的页）
            auto record1 = co_await result_set.operator_bracket(0);
            CHECK(record1.name == "User1");

            auto record6 = co_await result_set.operator_bracket(5);
            CHECK(record6.name == "User6");

            auto record11 = co_await result_set.operator_bracket(10);
            CHECK(record11.name == "User11");

            // 清理测试表
            co_await conn->exec("DROP TABLE IF EXISTS test_result_set_pagination");

            test_passed = true;

        } catch (const hku::SQLException& e) {
            MESSAGE("AsyncSQLResultSet pagination test failed: " << e.what()
                                                                 << ", errcode: " << e.errcode());
        } catch (const std::exception& e) {
            MESSAGE("AsyncSQLResultSet pagination test exception: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    CHECK(test_passed == true);
}

TEST_CASE("test_async_mysql_multithreaded_executor") {
    // 测试多线程环境下协程执行的正确性
    // 验证 AsyncMySQLConnect + ResourceAsioPool 在多个线程同时运行 io_context 时的线程安全性
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    const int num_tasks = 20;
    const int table_macro_tasks = 15;
    const int num_threads = 4;
    const size_t max_connections = 35;  // 支持两部分测试的并发需求
    std::atomic<int> completed(0);
    std::atomic<int> success_count(0);
    std::atomic<int> table_macro_completed(0);
    std::atomic<int> table_macro_success(0);
    std::promise<void> basic_completion_promise;
    std::future<void> basic_completion_future = basic_completion_promise.get_future();
    std::promise<void> table_macro_completion_promise;
    std::future<void> table_macro_completion_future = table_macro_completion_promise.get_future();
    std::promise<void> total_completion_promise;
    std::future<void> total_completion_future = total_completion_promise.get_future();

    // 创建资源池（默认使用 std::mutex，线程安全）
    ResourceAsioPool<AsyncMySQLConnect> pool(param, max_connections);

    // 定义 TableMacro 测试表结构
    struct MultithreadTestRecord {
        TABLE_BIND4(MultithreadTestRecord, test_table_macro_mt, name, age, score, email)

        void reset() {
            name = "";
            age = 0;
            score = 0.0;
            email = "";
        }

        std::string name;
        int age;
        double score;
        std::string email;
    };

    // 在协程中执行测试
    {
        (void)boost::asio::co_spawn(
          io_context,
          [&]() -> net::awaitable<void> {
              try {
                  // ==================== 第一部分：基本多线程协程测试 ====================
                  HKU_INFO("Starting basic multithreaded coroutine test...");

                  // 使用资源池中的连接初始化数据库和表
                  auto init_conn_result = co_await pool.asyncGet();
                  CHECK_EXPECTED(init_conn_result);
                  if (!init_conn_result) {
                      MESSAGE("Failed to get connection from pool for initialization");
                      co_return;
                  }
                  auto init_conn = std::move(init_conn_result.value());

                  bool connected = co_await init_conn->ping();
                  if (!connected) {
                      MESSAGE("Failed to connect to MySQL");
                      co_return;
                  }

                  // 创建测试表
                  co_await init_conn->exec("DROP TABLE IF EXISTS test_multithread_coroutine");
                  co_await init_conn->exec(R"(
                CREATE TABLE test_multithread_coroutine (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    task_id INT,
                    thread_info VARCHAR(100),
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            )");

                  // 归还初始化连接
                  init_conn.reset();

                  // 提交多个并发任务，每个任务从资源池获取连接
                  for (int i = 0; i < num_tasks; ++i) {
                      boost::asio::co_spawn(
                        io_context,
                        [&, i]() -> net::awaitable<void> {
                            try {
                                // 从资源池获取连接（会自动处理并发和资源复用）
                                auto conn_result = co_await pool.asyncGet();
                                if (!conn_result) {
                                    HKU_WARN("Task {} failed to get connection from pool: {}", i,
                                             conn_result.error());
                                    if (completed.fetch_add(1) + 1 == num_tasks) {
                                        basic_completion_promise.set_value();
                                    }
                                    co_return;
                                }
                                auto task_conn = std::move(conn_result.value());

                                // 每个任务插入一条记录
                                std::string sql =
                                  "INSERT INTO test_multithread_coroutine (task_id, thread_info) "
                                  "VALUES (" +
                                  std::to_string(i) + ", 'thread_pool')";
                                co_await task_conn->exec(sql);

                                // 模拟一些异步操作
                                net::steady_timer timer(co_await net::this_coro::executor);
                                timer.expires_after(std::chrono::milliseconds(10));
                                co_await timer.async_wait(net::use_awaitable);

                                // 验证插入的数据
                                auto stmt = co_await task_conn->getStatement(
                                  "SELECT COUNT(*) FROM test_multithread_coroutine WHERE task_id = "
                                  "?");
                                stmt->bind(0, i);
                                co_await stmt->exec();

                                if (co_await stmt->moveNext()) {
                                    int count = 0;
                                    stmt->getColumn(0, count);
                                    if (count == 1) {
                                        success_count.fetch_add(1);
                                    }
                                }

                                stmt.reset();

                                // 连接会在 shared_ptr 销毁时自动归还到资源池

                            } catch (const std::exception& e) {
                                HKU_WARN("Task {} failed: {}", i, e.what());
                            }

                            // 使用原子操作检查是否最后一个完成的任务
                            if (completed.fetch_add(1) + 1 == num_tasks) {
                                basic_completion_promise.set_value();
                            }
                        },
                        boost::asio::detached);
                  }

                  // 注意：不在这里等待子任务完成，让主线程通过 future 等待
                  // 这样避免阻塞事件循环

                  // 但是我们需要等待子任务完成后才能清理表
                  // 使用定时器轮询，不阻塞事件循环
                  while (completed.load() < num_tasks) {
                      net::steady_timer timer(co_await net::this_coro::executor);
                      timer.expires_after(std::chrono::milliseconds(10));
                      co_await timer.async_wait(net::use_awaitable);
                  }

                  // 清理测试表（需要再次从资源池获取连接）
                  auto cleanup_conn_result = co_await pool.asyncGet();
                  if (cleanup_conn_result) {
                      auto cleanup_conn = std::move(cleanup_conn_result.value());
                      co_await cleanup_conn->exec(
                        "DROP TABLE IF EXISTS test_multithread_coroutine");
                  }

                  HKU_INFO("Basic multithreaded test completed - Success: {}/{}, Pool count: {}",
                           success_count.load(), num_tasks, pool.count());
                  CHECK_EQ(completed.load(), num_tasks);
                  CHECK_GT(success_count.load(), 0);

                  // ==================== 第二部分：TableMacro 多线程测试 ====================
                  HKU_INFO("Starting TableMacro multithreaded test...");

                  // 获取连接创建 TableMacro 测试表
                  auto table_init_result = co_await pool.asyncGet();
                  if (!table_init_result) {
                      MESSAGE("Failed to get connection for TableMacro test initialization");
                      co_return;
                  }
                  auto table_init_conn = std::move(table_init_result.value());

                  // 创建 TableMacro 测试表
                  co_await table_init_conn->exec("DROP TABLE IF EXISTS test_table_macro_mt");
                  co_await table_init_conn->exec(R"(
                CREATE TABLE test_table_macro_mt (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(100),
                    age INT,
                    score DOUBLE,
                    email VARCHAR(200)
                )
            )");

                  table_init_conn.reset();

                  // 提交多个并发任务，使用 TableMacro 进行数据操作
                  for (int i = 0; i < table_macro_tasks; ++i) {
                      boost::asio::co_spawn(
                        io_context,
                        [&, i]() -> net::awaitable<void> {
                            try {
                                // 从资源池获取连接
                                auto conn_result = co_await pool.asyncGet();
                                if (!conn_result) {
                                    HKU_WARN("TableMacro Task {} failed to get connection", i);
                                    if (table_macro_completed.fetch_add(1) + 1 ==
                                        table_macro_tasks) {
                                        table_macro_completion_promise.set_value();
                                    }
                                    co_return;
                                }
                                auto task_conn = std::move(conn_result.value());

                                // 使用 TableMacro 保存数据
                                MultithreadTestRecord record;
                                record.name = "User_" + std::to_string(i);
                                record.age = 20 + (i % 30);
                                record.score = 80.0 + (i % 20);
                                record.email = "user" + std::to_string(i) + "@test.com";

                                co_await task_conn->save(record);
                                CHECK(record.valid() == true);

                                // 使用 TableMacro 加载数据
                                MultithreadTestRecord loaded;
                                co_await task_conn->load(loaded, Field("name") == record.name);

                                if (loaded.valid() && loaded.name == record.name &&
                                    loaded.age == record.age) {
                                    table_macro_success.fetch_add(1);
                                }

                                // 模拟异步操作
                                net::steady_timer timer(co_await net::this_coro::executor);
                                timer.expires_after(std::chrono::milliseconds(5));
                                co_await timer.async_wait(net::use_awaitable);

                            } catch (const std::exception& e) {
                                HKU_WARN("TableMacro Task {} failed: {}", i, e.what());
                            }

                            if (table_macro_completed.fetch_add(1) + 1 == table_macro_tasks) {
                                table_macro_completion_promise.set_value();
                            }
                        },
                        boost::asio::detached);
                  }

                  // 等待所有 TableMacro 任务完成
                  while (table_macro_completed.load() < table_macro_tasks) {
                      net::steady_timer timer(co_await net::this_coro::executor);
                      timer.expires_after(std::chrono::milliseconds(10));
                      co_await timer.async_wait(net::use_awaitable);
                  }

                  // 清理测试表
                  auto cleanup_result = co_await pool.asyncGet();
                  if (cleanup_result) {
                      auto cleanup_conn = std::move(cleanup_result.value());
                      co_await cleanup_conn->exec("DROP TABLE IF EXISTS test_table_macro_mt");
                  }

                  HKU_INFO(
                    "TableMacro multithreaded test completed - Success: {}/{}, Pool count: {}",
                    table_macro_success.load(), table_macro_tasks, pool.count());
                  CHECK_EQ(table_macro_completed.load(), table_macro_tasks);
                  CHECK_GT(table_macro_success.load(), 0);

              } catch (const std::exception& e) {
                  HKU_ERROR("Main coroutine failed: {}", e.what());
              }
              co_return;
          },
          boost::asio::detached);
    }

    // 创建多个线程同时运行 io_context（真正的多线程执行器）
    std::vector<std::thread> workers;
    workers.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&]() { io_context.run(); });
    }

    // 等待第一部分任务完成或超时
    if (basic_completion_future.wait_for(std::chrono::seconds(15)) == std::future_status::timeout) {
        HKU_ERROR("Basic multithreaded test timeout! Completed: {}/{}", completed.load(),
                  num_tasks);
    }

    // 等待第二部分任务完成或超时
    if (table_macro_completion_future.wait_for(std::chrono::seconds(15)) ==
        std::future_status::timeout) {
        HKU_ERROR("TableMacro multithreaded test timeout! Completed: {}/{}",
                  table_macro_completed.load(), table_macro_tasks);
    }

    // 停止 io_context 并等待所有工作线程
    io_context.stop();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    HKU_INFO("All multithreaded tests completed - Basic: {}/{}, TableMacro: {}/{}, Pool count: {}",
             success_count.load(), num_tasks, table_macro_success.load(), table_macro_tasks,
             pool.count());
}

// ============================================================================
// AsyncMySQLConnect with ResourceAsioPool 测试（验证懒加载连接 + TableMacro）
// ============================================================================

TEST_CASE("test_async_mysql_asio_pool_lazy_connect") {
    // 测试从 ResourceAsioPool 获取的连接是懒加载状态
    // 关键验证：不手动调用 connect()，而是通过 Statement 构造触发自动连接
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    {
        // 创建 Asio 资源池，最大 2 个连接
        auto pool = std::make_shared<ResourceAsioPool<AsyncMySQLConnect>>(param, 2);

        // 先使用独立连接创建测试表，避免污染测试连接的懒加载状态
        {
            auto temp_conn = std::make_shared<AsyncMySQLConnect>(param);
            bool table_created = false;

            auto create_table_coro = [&]() -> net::awaitable<void> {
                try {
                    HKU_INFO("Starting table creation...");
                    bool connected = co_await temp_conn->ping();
                    HKU_INFO("Ping result: {}", connected);
                    if (connected) {
                        co_await temp_conn->exec("DROP TABLE IF EXISTS test_lazy_connect");
                        co_await temp_conn->exec(R"(
                            CREATE TABLE test_lazy_connect (
                                id INT AUTO_INCREMENT PRIMARY KEY,
                                name VARCHAR(100),
                                value INT
                            )
                        )");
                        table_created = true;
                        HKU_INFO("Table created successfully");
                    }
                } catch (const hku::SQLException& e) {
                    HKU_ERROR("SQL error during table creation: {} (errcode: {})", e.what(),
                              e.errcode());
                } catch (const std::exception& e) {
                    HKU_ERROR("Exception during table creation: {}", e.what());
                }
                co_return;
            };

            (void)boost::asio::co_spawn(io_context, create_table_coro(), boost::asio::detached);
            io_context.run_for(std::chrono::seconds(5));
            CHECK(table_created == true);
            io_context.restart();
        }

        auto run_test = [pool, &test_passed]() -> net::awaitable<void> {
            try {
                // 从池中异步获取连接
                auto result = co_await pool->asyncGet(std::chrono::seconds(5));
                CHECK(result.has_value());

                if (result && result.value()) {
                    auto& conn = result.value();
                    CHECK(conn != nullptr);

                    // 关键验证：此时连接应该是未初始化状态（懒加载）
                    // 我们不手动调用 connect()，也不调用 ping()
                    // 直接使用 TableMacro 的 save 操作，这会触发 Statement 构造
                    // Statement 构造时会调用 _connect() 确保连接已初始化

                    struct TestRecord {
                        TABLE_BIND2(TestRecord, test_lazy_connect, name, value)

                        void reset() {
                            name = "";
                            value = 0;
                        }

                        std::string name;
                        int value;
                    };

                    // 现在使用从池中获取的懒加载连接进行 TableMacro 操作
                    // 这是首次使用该连接，应该触发自动连接

                    TestRecord record;
                    record.name = "test";
                    record.value = 42;

                    // save 操作内部会创建 Statement，Statement 构造时会触发 _connect()
                    co_await conn->save(record);
                    CHECK(record.id() > 0);

                    // 验证数据已保存
                    TestRecord loaded;
                    loaded.id(record.id());
                    co_await conn->load(loaded);
                    CHECK(loaded.name == "test");
                    CHECK(loaded.value == 42);

                    test_passed = true;

                    // 释放资源（自动归还到池中）
                    result.value().reset();
                }

            } catch (const std::exception& e) {
                MESSAGE("ResourceAsioPool lazy connect test failed: " << e.what());
            }
        };

        boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);

        // 运行 io_context 直到协程完成
        io_context.run_for(std::chrono::seconds(10));

        // 显式释放 pool
        pool.reset();

        // 清理测试表
        {
            auto temp_conn = std::make_shared<AsyncMySQLConnect>(param);
            (void)boost::asio::co_spawn(io_context, [&]() -> net::awaitable<void> {
                bool connected = co_await temp_conn->ping();
                if (connected) {
                    co_await temp_conn->exec("DROP TABLE IF EXISTS test_lazy_connect");
                }
                co_return;
            }());
            io_context.run_for(std::chrono::seconds(5));
        }
    }

    CHECK(test_passed == true);
}

TEST_CASE("test_async_mysql_asio_pool_with_tablemacro") {
    // 测试使用 ResourceAsioPool + TableMacro 的组合
    // 验证从池中获取的懒加载连接能正常使用 TableMacro 功能
    // 关键：首次使用 TableMacro 操作时，Statement 构造会触发自动连接
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;

    struct TestRecord {
        TABLE_BIND3(TestRecord, test_asio_pool_tablemacro, name, age, extra)

        void reset() {
            name = "";
            age = 0;
            extra = "";
        }

        std::string name;
        int age;
        std::string extra;
    };

    {
        // 创建 Asio 资源池
        auto pool = std::make_shared<ResourceAsioPool<AsyncMySQLConnect>>(param, 2);

        // 先使用独立连接创建测试表，避免污染测试连接的懒加载状态
        {
            auto temp_conn = std::make_shared<AsyncMySQLConnect>(param);
            bool table_created = false;

            auto create_table_coro = [&]() -> net::awaitable<void> {
                try {
                    bool connected = co_await temp_conn->ping();
                    if (connected) {
                        co_await temp_conn->exec("DROP TABLE IF EXISTS test_asio_pool_tablemacro");
                        co_await temp_conn->exec(R"(
                            CREATE TABLE test_asio_pool_tablemacro (
                                id INT AUTO_INCREMENT PRIMARY KEY,
                                name VARCHAR(100),
                                age INT,
                                extra VARCHAR(200)
                            )
                        )");
                        table_created = true;
                    }
                } catch (...) {
                    // 忽略异常，让 CHECK 断言失败
                }
                co_return;
            };

            (void)boost::asio::co_spawn(io_context, create_table_coro(), boost::asio::detached);
            io_context.run_for(std::chrono::seconds(5));
            CHECK(table_created == true);
            io_context.restart();
        }

        auto run_test = [pool, &test_passed]() -> net::awaitable<void> {
            try {
                // 从池中获取连接（此时是懒加载状态）
                auto result = co_await pool->asyncGet(std::chrono::seconds(5));
                CHECK(result.has_value());

                if (result && result.value()) {
                    auto& conn = result.value();
                    CHECK(conn != nullptr);

                    // 不手动调用 connect()，也不调用 ping/exec
                    // 直接使用 TableMacro 接口，这些操作内部会创建 Statement
                    // Statement 构造时会自动触发 _connect() 初始化连接

                    // 测试 save（单条保存）- 这会触发首次连接
                    TestRecord record1;
                    record1.name = "Alice";
                    record1.age = 25;
                    co_await conn->save(record1);
                    CHECK(record1.id() > 0);

                    // 测试批量保存
                    std::vector<TestRecord> records;
                    TestRecord record2;
                    record2.name = "Bob";
                    record2.age = 30;
                    records.push_back(record2);

                    TestRecord record3;
                    record3.name = "Charlie";
                    record3.age = 35;
                    records.push_back(record3);

                    co_await conn->batchSave(records);
                    CHECK(records[0].id() > 0);
                    CHECK(records[1].id() > 0);

                    // 测试 load（单条加载）
                    TestRecord loaded_record;
                    loaded_record.id(record1.id());
                    co_await conn->load(loaded_record);
                    CHECK(loaded_record.name == "Alice");
                    CHECK(loaded_record.age == 25);

                    // 测试 batchLoad（批量加载）
                    std::vector<TestRecord> all_records;
                    co_await conn->batchLoad(all_records, "1=1 ORDER BY name");
                    CHECK(all_records.size() == 3);
                    CHECK(all_records[0].name == "Alice");
                    CHECK(all_records[1].name == "Bob");
                    CHECK(all_records[2].name == "Charlie");

                    test_passed = true;

                    // 释放资源
                    result.value().reset();
                }

            } catch (const hku::SQLException& e) {
                MESSAGE("ResourceAsioPool TableMacro test failed: " << e.what() << ", errcode: "
                                                                    << e.errcode());
            } catch (const std::exception& e) {
                MESSAGE("ResourceAsioPool TableMacro test exception: " << e.what());
            }
        };

        boost::asio::co_spawn(io_context, run_test(), boost::asio::detached);

        // 运行 io_context 直到协程完成
        io_context.run_for(std::chrono::seconds(10));

        // 显式释放 pool
        pool.reset();

        // 清理测试表
        {
            auto temp_conn = std::make_shared<AsyncMySQLConnect>(param);
            (void)boost::asio::co_spawn(io_context, [&]() -> net::awaitable<void> {
                bool connected = co_await temp_conn->ping();
                if (connected) {
                    co_await temp_conn->exec("DROP TABLE IF EXISTS test_asio_pool_tablemacro");
                }
                co_return;
            }());
            io_context.run_for(std::chrono::seconds(5));
        }
    }

    CHECK(test_passed == true);
}

// ============================================================================
// Prepared statement released after the connection has been destroyed
// ============================================================================

TEST_CASE("test_async_mysql_statement_release_after_connect_destroyed") {
    // A statement released after its connection is destroyed must not call close_statement on the
    // freed connection (heap-use-after-free with the old deleter)
    Parameter param = loadMySQLConfig();

    if (param.empty()) {
        return;
    }

    boost::asio::io_context io_context;
    bool test_passed = false;
    AsyncSQLStatementPtr st;

    auto run_test = [&]() -> net::awaitable<void> {
        auto conn = std::make_shared<AsyncMySQLConnect>(param);

        co_await conn->exec("DROP TABLE IF EXISTS test_async_stmt_uaf");
        co_await conn->exec(
          "CREATE TABLE test_async_stmt_uaf (id INT PRIMARY KEY, name VARCHAR(50))");
        co_await conn->exec("INSERT INTO test_async_stmt_uaf VALUES (1, 'hku')");

        st = co_await conn->getStatement("SELECT id, name FROM test_async_stmt_uaf WHERE id = ?");
        st->bind(0, static_cast<int64_t>(1));
        co_await st->exec();
        REQUIRE(co_await st->moveNext());
        int64_t id;
        std::string name;
        st->getColumn(0, id);
        st->getColumn(1, name);
        CHECK_EQ(id, 1);
        CHECK_EQ(name, "hku");

        // Destroy the connection while st is still held
        conn.reset();

        /** Release the statement: it is deleted without touching the destroyed connection */
        st.reset();

        test_passed = true;
    };

    boost::asio::co_spawn(io_context, run_test, boost::asio::detached);
    io_context.run_for(std::chrono::seconds(10));

    st.reset();

    // Clean up through a fresh connection
    if (test_passed) {
        boost::asio::io_context cleanup_io;
        boost::asio::co_spawn(
          cleanup_io,
          [&]() -> net::awaitable<void> {
              auto conn = std::make_shared<AsyncMySQLConnect>(param);
              co_await conn->exec("DROP TABLE IF EXISTS test_async_stmt_uaf");
          },
          boost::asio::detached);
        cleanup_io.run_for(std::chrono::seconds(5));
    }

    CHECK(test_passed == true);
}

#endif  // HKU_ENABLE_MYSQL
