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