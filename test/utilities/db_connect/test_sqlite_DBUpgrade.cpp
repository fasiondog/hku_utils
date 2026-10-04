/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-09-28
 *      Author: fasiondog
 */

#include "test_config.h"
#include <string>
#include <vector>
#include <hikyuu/utilities/ResourcePool.h>
#include <hikyuu/utilities/db_connect/DBConnect.h>
#include <hikyuu/utilities/os.h>

using namespace hku;
namespace net = hku::net;

TEST_CASE("test_sqlite_DBUpgrade") {
    removeFile("测试/test.db");

    const char *create_script = R"(
        CREATE TABLE test_table (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name VARCHAR(10)
        );
    )";

    Parameter param;
    param.set<std::string>("db", "测试/test.db");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);

    REQUIRE(con->tableExist("test_table") == false);
    DBUpgrade(con, "test", {}, 2, create_script);
    CHECK_UNARY(con->tableExist("test_table"));
    CHECK_EQ(con->queryInt("select version from module_version where module='test'", 0), 1);

    std::vector<std::string> upgrade_scripts = {
      R"(CREATE TABLE test_table2 (id INTEGER PRIMARY KEY AUTOINCREMENT, name VARCHAR(10));)",
    };
    DBUpgrade(con, "test", upgrade_scripts, 2, nullptr);
    CHECK_EQ(con->queryInt("select version from module_version where module='test'", 0), 2);
}

TEST_CASE("test_async_sqlite_DBUpgrade") {
    removeFile("测试/test_async.db");

    boost::asio::io_context io_context;

    bool test_passed = false;
    std::exception_ptr captured_exception;

    boost::asio::co_spawn(
      io_context,
      [&]() -> net::awaitable<void> {
          try {
              const char *create_script = R"(
                CREATE TABLE test_table (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name VARCHAR(10)
                );
            )";

              Parameter param;
              param.set<std::string>("db", "测试/test_async.db");
              param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
              auto con = std::make_shared<AsyncSQLiteConnect>(param);

              // 测试创建数据库
              REQUIRE(!(co_await con->tableExist("test_table")));
              co_await DBUpgrade(con, "test", {}, 2, create_script);
              CHECK_UNARY(co_await con->tableExist("test_table"));
              CHECK_EQ(
                co_await con->queryInt("select version from module_version where module='test'", 0),
                1);

              // 测试升级数据库
              std::vector<std::string> upgrade_scripts = {
                R"(CREATE TABLE test_table2 (id INTEGER PRIMARY KEY AUTOINCREMENT, name VARCHAR(10));)",
              };
              co_await DBUpgrade(con, "test", upgrade_scripts, 2, nullptr);
              CHECK_EQ(
                co_await con->queryInt("select version from module_version where module='test'", 0),
                2);

              test_passed = true;
          } catch (...) {
              captured_exception = std::current_exception();
          }
          co_return;
      },
      boost::asio::detached);

    io_context.run();

    if (captured_exception) {
        std::rethrow_exception(captured_exception);
    }
    CHECK(test_passed);
}

TEST_CASE("test_sqlite_DBUpgrade_module_name_escaped") {
    // The module name carries a double quote (an injection payload); it must be treated as a plain
    // name rather than breaking out of the string literal
    removeFile("测试/test_escape.db");

    const char *evil_name = "evil\" OR \"1\"=\"1";

    const char *create_script = R"(
        CREATE TABLE evil_table (id INTEGER PRIMARY KEY AUTOINCREMENT);
    )";

    Parameter param;
    param.set<std::string>("db", "测试/test_escape.db");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);

    /** Create with the quote-bearing name: the version SELECT must not match any row */
    DBUpgrade(con, evil_name, {}, 2, create_script);
    CHECK_EQ(con->queryInt(fmt::format("select version from module_version where module={}",
                                       sqlStringLiteral(evil_name)),
                           0),
             1);

    /** Upgrade the same module; the UPDATE must hit its own row only */
    std::vector<std::string> upgrade_scripts = {
      R"(CREATE TABLE evil_table2 (id INTEGER PRIMARY KEY);)",
    };
    DBUpgrade(con, evil_name, upgrade_scripts, 2, nullptr);
    CHECK_EQ(con->queryInt(fmt::format("select version from module_version where module={}",
                                       sqlStringLiteral(evil_name)),
                           0),
             2);

    /** A different module is not confused with the escaped one */
    CHECK_EQ(con->queryInt("select count(*) from module_version", 0), 1);
}

TEST_CASE("test_async_sqlite_DBUpgrade_module_name_escaped") {
    // Asynchronous counterpart of the quote-bearing module name test
    removeFile("测试/test_async_escape.db");

    const char *evil_name = "evil\" OR \"1\"=\"1";
    const char *create_script = R"(
        CREATE TABLE evil_table (id INTEGER PRIMARY KEY AUTOINCREMENT);
    )";

    boost::asio::io_context io_context;
    bool test_passed = false;
    std::exception_ptr captured_exception;

    boost::asio::co_spawn(
      io_context,
      [&]() -> net::awaitable<void> {
          try {
              Parameter param;
              param.set<std::string>("db", "测试/test_async_escape.db");
              param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
              auto con = std::make_shared<AsyncSQLiteConnect>(param);

              co_await DBUpgrade(con, evil_name, {}, 2, create_script);
              CHECK_EQ(co_await con->queryInt(
                         fmt::format("select version from module_version where module={}",
                                     sqlStringLiteral(evil_name)),
                         0),
                       1);

              std::vector<std::string> upgrade_scripts = {
                R"(CREATE TABLE evil_table2 (id INTEGER PRIMARY KEY);)",
              };
              co_await DBUpgrade(con, evil_name, upgrade_scripts, 2, nullptr);
              CHECK_EQ(co_await con->queryInt(
                         fmt::format("select version from module_version where module={}",
                                     sqlStringLiteral(evil_name)),
                         0),
                       2);

              CHECK_EQ(co_await con->queryInt("select count(*) from module_version", 0), 1);
              test_passed = true;
          } catch (...) {
              captured_exception = std::current_exception();
          }
          co_return;
      },
      boost::asio::detached);

    io_context.run();

    if (captured_exception) {
        std::rethrow_exception(captured_exception);
    }
    CHECK(test_passed);
}