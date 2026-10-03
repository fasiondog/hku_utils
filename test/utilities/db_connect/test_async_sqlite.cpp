/*
 * test_AsyncSQLiteConnect.cpp
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2026-05-09
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <boost/asio.hpp>
#include "hikyuu/utilities/db_connect/sqlite/AsyncSQLiteConnect.h"
#include "hikyuu/utilities/db_connect/AsyncSQLResultSet.h"
#include "hikyuu/utilities/db_connect/TableMacro.h"
#include "hikyuu/utilities/Parameter.h"
#include "hikyuu/utilities/ResourceAsioPool.h"

namespace net = hku::net;
using namespace hku;

// ============================================================================
// 测试夹具：用于创建测试用的数据库连接
// ============================================================================

class SQLiteTestHelper {
public:
    static std::shared_ptr<AsyncSQLiteConnect> createMemoryConnection() {
        Parameter param;
        param.set("db", std::string(":memory:"));
        return std::make_shared<AsyncSQLiteConnect>(param);
    }

    static std::shared_ptr<AsyncSQLiteConnect> createFileConnection(const std::string& filename) {
        Parameter param;
        param.set("db", filename);
        return std::make_shared<AsyncSQLiteConnect>(param);
    }
};

// ============================================================================
// 基础功能测试
// ============================================================================

TEST_CASE("test_async_sqlite_basic_connection") {
    boost::asio::io_context io_context;

    bool test_passed = false;
    std::exception_ptr captured_exception;

    auto test_coro = [&]() -> net::awaitable<void> {
        try {
            auto conn = SQLiteTestHelper::createMemoryConnection();

            // 测试 ping
            bool connected = co_await conn->ping();
            CHECK(connected == true);

            // 测试创建表
            co_await conn->exec(
              "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");

            // 测试插入数据
            int64_t affected =
              co_await conn->exec("INSERT INTO users (name, age) VALUES ('Alice', 25)");
            CHECK(affected == 1);

            // 测试查询
            auto stmt = co_await conn->getStatement("SELECT id, name, age FROM users");
            co_await stmt->exec();

            int row_count = 0;
            while (co_await stmt->moveNext()) {
                int id, age;
                std::string name;
                stmt->getColumn(0, id);
                stmt->getColumn(1, name);
                stmt->getColumn(2, age);

                CHECK(id == 1);
                CHECK(name == "Alice");
                CHECK(age == 25);
                row_count++;
            }
            CHECK(row_count == 1);

            test_passed = true;
        } catch (...) {
            captured_exception = std::current_exception();
        }
    };

    net::co_spawn(io_context.get_executor(), test_coro());
    io_context.run();

    if (captured_exception) {
        std::rethrow_exception(captured_exception);
    }
    CHECK(test_passed);
}

// ============================================================================
// 参数化查询测试
// ============================================================================

TEST_CASE("test_async_sqlite_parameterized_query") {
    boost::asio::io_context io_context;

    bool test_passed = false;

    auto test_coro = [&]() -> net::awaitable<void> {
        try {
            auto conn = SQLiteTestHelper::createMemoryConnection();
            co_await conn->exec(
              "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price REAL)");
            co_await conn->exec("INSERT INTO products (name, price) VALUES ('Apple', 1.5)");
            co_await conn->exec("INSERT INTO products (name, price) VALUES ('Banana', 0.8)");
            co_await conn->exec("INSERT INTO products (name, price) VALUES ('Orange', 1.2)");

            // 测试参数绑定
            auto stmt = co_await conn->getStatement("SELECT * FROM products WHERE price > ?");
            stmt->bind(0, 1.0);
            co_await stmt->exec();

            int count = 0;
            while (co_await stmt->moveNext()) {
                count++;
            }
            CHECK(count == 2);  // Apple and Orange

            test_passed = true;
        } catch (...) {
            // 忽略异常
        }
    };

    net::co_spawn(io_context.get_executor(), test_coro());
    io_context.run();
    CHECK(test_passed);
}

// ============================================================================
// 事务测试
// ============================================================================

TEST_CASE("test_async_sqlite_transaction") {
    boost::asio::io_context io_context;

    bool test_passed = false;

    auto test_coro = [&]() -> net::awaitable<void> {
        try {
            auto conn = SQLiteTestHelper::createMemoryConnection();
            co_await conn->exec("CREATE TABLE accounts (id INTEGER PRIMARY KEY, balance REAL)");
            co_await conn->exec("INSERT INTO accounts (balance) VALUES (1000)");
            co_await conn->exec("INSERT INTO accounts (balance) VALUES (2000)");

            // 测试成功的事务
            co_await conn->transaction();

            std::exception_ptr saved_exception;
            try {
                co_await conn->exec("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
                co_await conn->exec("UPDATE accounts SET balance = balance + 100 WHERE id = 2");
            } catch (...) {
                saved_exception = std::current_exception();
            }

            if (saved_exception) {
                try {
                    co_await conn->rollback();
                } catch (...) {
                    // 忽略回滚异常
                }
                std::rethrow_exception(saved_exception);
            } else {
                co_await conn->commit();
            }

            // 验证结果
            auto stmt = co_await conn->getStatement("SELECT balance FROM accounts WHERE id = 1");
            co_await stmt->exec();
            CHECK(co_await stmt->moveNext());
            double balance;
            stmt->getColumn(0, balance);
            CHECK(balance == 900.0);

            test_passed = true;
        } catch (...) {
            // 忽略异常
        }
    };

    net::co_spawn(io_context.get_executor(), test_coro());
    io_context.run();
    CHECK(test_passed);
}

// ============================================================================
// 表存在性检查测试
// ============================================================================

TEST_CASE("test_async_sqlite_table_exist") {
    boost::asio::io_context io_context;

    bool test_passed = false;

    auto test_coro = [&]() -> net::awaitable<void> {
        try {
            auto conn = SQLiteTestHelper::createMemoryConnection();

            // 表不存在
            bool exists = co_await conn->tableExist("nonexistent");
            CHECK(exists == false);

            // 创建表后存在
            co_await conn->exec("CREATE TABLE test_table (id INTEGER)");
            exists = co_await conn->tableExist("test_table");
            CHECK(exists == true);

            test_passed = true;
        } catch (...) {
            // 忽略异常
        }
    };

    net::co_spawn(io_context.get_executor(), test_coro());
    io_context.run();
    CHECK(test_passed);
}

// ============================================================================
// 查询数值测试
// ============================================================================

struct AsyncOrderRecord {
    TABLE_BIND3(AsyncOrderRecord, async_order_test, order_date, amount_limit, memo)

    std::string order_date;
    int amount_limit = 0;
    std::string memo;
};

TEST_CASE("test_async_sqlite_condition_query") {
    boost::asio::io_context io_context;

    bool test_passed = false;
    std::exception_ptr captured_exception;

    auto test_coro = [&]() -> net::awaitable<void> {
        try {
            auto conn = SQLiteTestHelper::createMemoryConnection();
            co_await conn->exec(
              "CREATE TABLE async_order_test (id INTEGER PRIMARY KEY AUTOINCREMENT, order_date "
              "TEXT, amount_limit INTEGER, memo TEXT)");

            for (int i = 1; i <= 6; i++) {
                AsyncOrderRecord record;
                record.order_date = "2024-01-0" + std::to_string(i);
                record.amount_limit = i;
                record.memo = i % 2 == 0 ? "even" : "odd";
                co_await conn->save(record);
            }

            // 列名含 order 的条件不被误切，值走绑定
            auto results = conn->query<AsyncOrderRecord, 2>(Field("order_date") >= "2024-01-03");
            CHECK(co_await results.size() == 4);
            auto first = co_await results.at(0);
            CHECK(first.order_date == "2024-01-03");

            // 带 order-by 尾部的条件分页：排序子句参与内外两层查询
            auto ordered =
              conn->query<AsyncOrderRecord, 2>((Field("memo") == "even") + DESC("order_date"));
            CHECK(co_await ordered.size() == 3);
            auto top = co_await ordered.at(0);
            CHECK(top.order_date == "2024-01-06");

            // 条件的 limit 限制总行数，超出的页为空
            auto capped =
              conn->query<AsyncOrderRecord, 2>((Field("order_date") >= "2024-01-01") + LIMIT(3));
            CHECK(co_await capped.size() == 3);
            auto page = co_await capped.getPage(1);
            CHECK(page.size() == 1);
            auto beyond = co_await capped.getPage(2);
            CHECK(beyond.empty());

            // 非分页的条件加载
            std::vector<AsyncOrderRecord> rows;
            co_await conn->batchLoad(rows, Field("memo") == "odd");
            CHECK(rows.size() == 3);

            AsyncOrderRecord one;
            co_await conn->load(one, Field("order_date") == "2024-01-05");
            CHECK(one.amount_limit == 5);

            // 含引号的载荷作为值绑定，不能拓宽条件
            std::vector<AsyncOrderRecord> evil_rows;
            co_await conn->batchLoad(evil_rows, Field("memo") == "x\" or \"1\"=\"1");
            CHECK(evil_rows.empty());

            // 按条件删除走预处理语句 + 绑定值
            AsyncOrderRecord evil;
            evil.order_date = "2024-02-01";
            evil.amount_limit = 99;
            evil.memo = "drop";
            co_await conn->save(evil);
            co_await conn->remove("async_order_test", Field("order_date") == "2024-02-01");
            auto rest = conn->query<AsyncOrderRecord>();
            CHECK(co_await rest.size() == 6);

            test_passed = true;
        } catch (...) {
            captured_exception = std::current_exception();
        }
        co_return;
    };

    net::co_spawn(io_context.get_executor(), test_coro());
    io_context.run();

    if (captured_exception) {
        std::rethrow_exception(captured_exception);
    }
    CHECK(test_passed);
}

// ============================================================================
// 查询数值测试（原有）
// ============================================================================

TEST_CASE("test_async_sqlite_query_number") {
    boost::asio::io_context io_context;

    bool test_passed = false;

    auto test_coro = [&]() -> net::awaitable<void> {
        try {
            auto conn = SQLiteTestHelper::createMemoryConnection();
            co_await conn->exec("CREATE TABLE items (id INTEGER, value INTEGER)");
            co_await conn->exec("INSERT INTO items VALUES (1, 10)");
            co_await conn->exec("INSERT INTO items VALUES (2, 20)");
            co_await conn->exec("INSERT INTO items VALUES (3, 30)");

            // 测试 COUNT
            int count = co_await conn->queryInt("SELECT COUNT(*) FROM items", 0);
            CHECK(count == 3);

            // 测试 SUM
            int sum = co_await conn->queryNumber<int>("SELECT SUM(value) FROM items", 0);
            CHECK(sum == 60);

            test_passed = true;
        } catch (...) {
            // 忽略异常
        }
    };

    net::co_spawn(io_context.get_executor(), test_coro());
    io_context.run();
    CHECK(test_passed);
}

// ============================================================================
// 多线程并发测试
// ============================================================================

TEST_CASE("test_async_sqlite_multithreaded_concurrent") {
    const int thread_count = 4;
    const int ops_per_thread = 10;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    // 使用文件数据库以便多线程访问
    std::string db_file = "test_data/tmp/test_async_sqlite_mt.db";
    std::remove(db_file.c_str());  // 清理旧文件

    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i]() {
            try {
                boost::asio::io_context io_context;

                auto test_coro = [&]() -> net::awaitable<void> {
                    auto conn = SQLiteTestHelper::createFileConnection(db_file);

                    // 每个线程创建自己的表
                    std::string table_name = "table_" + std::to_string(i);
                    co_await conn->exec(
                      "CREATE TABLE IF NOT EXISTS " + table_name +
                      " (id INTEGER PRIMARY KEY, thread_id INTEGER, seq INTEGER)");

                    // 插入数据
                    for (int j = 0; j < ops_per_thread; ++j) {
                        std::string sql = fmt::format(
                          "INSERT INTO {} (thread_id, seq) VALUES ({}, {})", table_name, i, j);
                        co_await conn->exec(sql);
                    }

                    // 验证数据
                    int count = co_await conn->queryInt(
                      fmt::format("SELECT COUNT(*) FROM {}", table_name), 0);
                    CHECK(count == ops_per_thread);
                };

                net::co_spawn(io_context.get_executor(), test_coro());
                io_context.run();
                success_count++;

            } catch (const std::exception& e) {
                std::cerr << "Thread " << i << " error: " << e.what() << std::endl;
                error_count++;
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    CHECK(success_count == thread_count);
    CHECK(error_count == 0);

    // 清理测试文件
    std::remove(db_file.c_str());
}

// ============================================================================
// 压力测试：大量并发操作
// ============================================================================

TEST_CASE("test_async_sqlite_stress_concurrent") {
    const int connection_count = 10;
    const int ops_per_connection = 50;
    std::atomic<int> total_ops{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < connection_count; ++i) {
        threads.emplace_back([&, i]() {
            try {
                boost::asio::io_context io_context;

                auto test_coro = [&]() -> net::awaitable<void> {
                    auto conn = SQLiteTestHelper::createMemoryConnection();

                    std::string table_name = "stress_table_" + std::to_string(i);
                    co_await conn->exec("CREATE TABLE " + table_name +
                                        " (id INTEGER PRIMARY KEY, value TEXT)");

                    for (int j = 0; j < ops_per_connection; ++j) {
                        std::string sql = fmt::format(
                          "INSERT INTO {} (value) VALUES ('data_{}_{}')", table_name, i, j);
                        co_await conn->exec(sql);
                        total_ops++;
                    }
                };

                net::co_spawn(io_context.get_executor(), test_coro());
                io_context.run();

            } catch (const std::exception& e) {
                std::cerr << "Stress test thread " << i << " error: " << e.what() << std::endl;
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    CHECK(total_ops == connection_count * ops_per_connection);
}

// ============================================================================
// 异常处理测试
// ============================================================================

TEST_CASE("test_async_sqlite_error_handling") {
    boost::asio::io_context io_context;

    bool exception_caught = false;

    auto test_coro = [&]() -> net::awaitable<void> {
        try {
            auto conn = SQLiteTestHelper::createMemoryConnection();

            // 测试无效 SQL
            try {
                co_await conn->exec("INVALID SQL STATEMENT");
            } catch (const SQLException& e) {
                exception_caught = true;
            }
            CHECK(exception_caught == true);

        } catch (...) {
            // 忽略其他异常
        }
    };

    net::co_spawn(io_context.get_executor(), test_coro());
    io_context.run();
}

// ============================================================================
// AsyncSQLiteConnect with ResourceAsioPool 测试（验证懒加载连接 + TableMacro）
// ============================================================================

TEST_CASE("test_async_sqlite_asio_pool_lazy_connect") {
    // 测试从 ResourceAsioPool 获取的连接是懒加载状态
    // 关键验证：不手动调用 connect()，而是通过 Statement 构造触发自动连接
    boost::asio::io_context io_context;
    bool test_passed = false;

    // 使用临时文件数据库
    std::string db_path =
      "test_data/tmp/test_async_sqlite_pool_" + std::to_string(std::time(nullptr)) + ".db";

    Parameter param;
    param.set("db", db_path);

    // 创建 Asio 资源池，最大 2 个连接
    auto pool = std::make_shared<ResourceAsioPool<AsyncSQLiteConnect>>(param, 2);

    auto run_test = [pool, &test_passed, db_path]() -> net::awaitable<void> {
        try {
            // 先创建测试表（使用独立连接）
            {
                Parameter temp_param;
                temp_param.set("db", db_path);
                auto temp_conn = std::make_shared<AsyncSQLiteConnect>(temp_param);
                bool connected = co_await temp_conn->ping();
                if (connected) {
                    co_await temp_conn->exec(
                      "CREATE TABLE IF NOT EXISTS test_lazy_connect ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "name TEXT, "
                      "value INTEGER)");
                }
            }

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

        } catch (const hku::SQLException& e) {
            MESSAGE("ResourceAsioPool lazy connect test failed: " << e.what()
                                                                  << ", errcode: " << e.errcode());
        } catch (const std::exception& e) {
            MESSAGE("ResourceAsioPool lazy connect test exception: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context.get_executor(), run_test(), boost::asio::detached);

    // 运行 io_context 直到协程完成
    io_context.run_for(std::chrono::seconds(10));

    // 显式释放 pool
    pool.reset();

    // 清理临时文件
    std::remove(db_path.c_str());

    CHECK(test_passed == true);
}

TEST_CASE("test_async_sqlite_asio_pool_with_tablemacro") {
    // 测试使用 ResourceAsioPool + TableMacro 的组合
    // 验证从池中获取的懒加载连接能正常使用 TableMacro 功能
    // 关键：首次使用 TableMacro 操作时，Statement 构造会触发自动连接
    boost::asio::io_context io_context;
    bool test_passed = false;

    // 使用临时文件数据库
    std::string db_path =
      "test_data/tmp/test_async_sqlite_pool2_" + std::to_string(std::time(nullptr)) + ".db";

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

    Parameter param;
    param.set("db", db_path);

    // 创建 Asio 资源池
    auto pool = std::make_shared<ResourceAsioPool<AsyncSQLiteConnect>>(param, 2);

    auto run_test = [pool, &test_passed, db_path]() -> net::awaitable<void> {
        try {
            // 先创建测试表（使用独立连接）
            {
                Parameter temp_param;
                temp_param.set("db", db_path);
                auto temp_conn = std::make_shared<AsyncSQLiteConnect>(temp_param);
                bool connected = co_await temp_conn->ping();
                if (connected) {
                    co_await temp_conn->exec(
                      "CREATE TABLE IF NOT EXISTS test_asio_pool_tablemacro ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "name TEXT, "
                      "age INTEGER, "
                      "extra TEXT)");
                }
            }

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
            MESSAGE("ResourceAsioPool TableMacro test failed: " << e.what()
                                                                << ", errcode: " << e.errcode());
        } catch (const std::exception& e) {
            MESSAGE("ResourceAsioPool TableMacro test exception: " << e.what());
        }
    };

    boost::asio::co_spawn(io_context.get_executor(), run_test(), boost::asio::detached);

    // 运行 io_context 直到协程完成
    io_context.run_for(std::chrono::seconds(10));

    // 显式释放 pool
    pool.reset();

    // 清理临时文件
    std::remove(db_path.c_str());

    CHECK(test_passed == true);
}