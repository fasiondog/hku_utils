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
#include "hikyuu/utilities/Parameter.h"

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
            co_await conn->exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");
            
            // 测试插入数据
            int64_t affected = co_await conn->exec("INSERT INTO users (name, age) VALUES ('Alice', 25)");
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
            co_await conn->exec("CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price REAL)");
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
    std::string db_file = "/tmp/test_async_sqlite_mt.db";
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
                    co_await conn->exec("CREATE TABLE IF NOT EXISTS " + table_name + 
                                      " (id INTEGER PRIMARY KEY, thread_id INTEGER, seq INTEGER)");
                    
                    // 插入数据
                    for (int j = 0; j < ops_per_thread; ++j) {
                        std::string sql = fmt::format(
                            "INSERT INTO {} (thread_id, seq) VALUES ({}, {})",
                            table_name, i, j
                        );
                        co_await conn->exec(sql);
                    }
                    
                    // 验证数据
                    int count = co_await conn->queryInt(
                        fmt::format("SELECT COUNT(*) FROM {}", table_name), 0
                    );
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
                            "INSERT INTO {} (value) VALUES ('data_{}_{}')",
                            table_name, i, j
                        );
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
