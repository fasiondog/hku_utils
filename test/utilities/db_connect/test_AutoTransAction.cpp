/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-10-06
 *      Author: fasiondog
 */

#include "test_config.h"
#include <string>
#include <vector>
#include <hikyuu/utilities/ResourcePool.h>
#include <hikyuu/utilities/Null.h>
#include <hikyuu/utilities/os.h>
#include <hikyuu/utilities/db_connect/DBConnect.h>
#include <hikyuu/utilities/db_connect/AutoTransAction.h>
#include <hikyuu/utilities/Log.h>

using namespace hku;

struct T2024 {
    TABLE_BIND2(T2024, t2024, name, age)
    std::string name;
    int age{0};
};

TEST_CASE("test_AutoTransAction") {
    createDir("测试");
    Parameter param;
    param.set<std::string>("db", "测试/测试.db");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);
    CHECK(con->ping());

    if (con->tableExist("t2024")) {
        con->exec("drop table t2024");
    }
    CHECK(con->tableExist("t2024") == false);
    con->exec(
      R"(create table t2024 ("id" INTEGER NOT NULL UNIQUE, name VARCHAR(20), age INT, PRIMARY KEY("id" AUTOINCREMENT));)");
    CHECK(con->tableExist("t2024") == true);

    // 无异常发生时，正常提交
    {
        AutoTransAction auto_trans(con);
        T2024 t1;
        t1.name = "t1";
        t1.age = 20;
        auto_trans.connect()->save(t1, false);

        T2024 t2;
        t2.name = "t2";
        t2.age = 22;
        auto_trans.connect()->save(t2, false);
    }

    T2024 ret_t1, ret_t2;
    con->load(ret_t1, Field("name") == "t1");
    CHECK_EQ(ret_t1.age, 20);
    con->load(ret_t2, Field("name") == "t2");
    CHECK_EQ(ret_t2.age, 22);

    con->remove(ret_t1);
    con->remove(ret_t2);

    // 异常发生时，部分数据被提交
    try {
        AutoTransAction auto_trans(con);
        T2024 t1;
        t1.name = "t1";
        t1.age = 20;
        auto_trans.connect()->save(t1, false);

        HKU_THROW("test error!");

        T2024 t2;
        t2.name = "t2";
        t2.age = 22;
        auto_trans.connect()->save(t2, false);

    } catch (...) {
    }

    ret_t1.id(0);
    ret_t1.age = 0;
    CHECK_UNARY(!ret_t1.valid());
    con->load(ret_t1, Field("name") == "t1");
    CHECK_EQ(ret_t1.age, 20);

    ret_t2.id(0);
    CHECK_UNARY(!ret_t2.valid());
    con->load(ret_t2, Field("name") == "t2");
    CHECK_UNARY(!ret_t2.valid());

    con->exec("drop table t2024");
    CHECK(con->tableExist("t2024") == false);
}

TEST_CASE("test_TransAction") {
    createDir("测试");
    Parameter param;
    param.set<std::string>("db", "测试/测试.db");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);
    CHECK(con->ping());

    if (con->tableExist("t2024")) {
        con->exec("drop table t2024");
    }
    CHECK(con->tableExist("t2024") == false);
    con->exec(
      R"(create table t2024 ("id" INTEGER NOT NULL UNIQUE, name VARCHAR(20), age INT, PRIMARY KEY("id" AUTOINCREMENT));)");
    CHECK(con->tableExist("t2024") == true);

    // 未手动提交事务
    {
        TransAction trans(con);
        T2024 t1;
        t1.name = "t1";
        t1.age = 20;
        trans.connect()->save(t1, false);

        T2024 t2;
        t2.name = "t2";
        t2.age = 22;
        trans.connect()->save(t2, false);
    }

    T2024 ret_t1, ret_t2;
    con->load(ret_t1, Field("name") == "t1");
    CHECK_UNARY(!ret_t1.valid());
    con->load(ret_t2, Field("name") == "t2");
    CHECK_UNARY(!ret_t2.valid());

    // 无异常发生时，正常提交
    {
        TransAction trans(con);
        trans.begin();
        T2024 t1;
        t1.name = "t1";
        t1.age = 20;
        trans.connect()->save(t1, false);

        T2024 t2;
        t2.name = "t2";
        t2.age = 22;
        trans.connect()->save(t2, false);
        trans.end();
    }

    con->load(ret_t1, Field("name") == "t1");
    CHECK_EQ(ret_t1.age, 20);
    con->load(ret_t2, Field("name") == "t2");
    CHECK_EQ(ret_t2.age, 22);

    con->remove(ret_t1);
    con->remove(ret_t2);

    // 异常发生时，所有数据被回滚
    try {
        TransAction trans(con);
        T2024 t1;
        t1.name = "t1";
        t1.age = 20;
        trans.connect()->save(t1, false);

        HKU_THROW("test error!");

        T2024 t2;
        t2.name = "t2";
        t2.age = 22;
        trans.connect()->save(t2, false);
        trans.end();

    } catch (...) {
    }

    ret_t1.id(0);
    ret_t1.age = 0;
    CHECK_UNARY(!ret_t1.valid());
    con->load(ret_t1, Field("name") == "t1");
    CHECK_UNARY(!ret_t1.valid());

    ret_t2.id(0);
    CHECK_UNARY(!ret_t2.valid());
    con->load(ret_t2, Field("name") == "t2");
    CHECK_UNARY(!ret_t2.valid());

    con->exec("drop table t2024");
    CHECK(con->tableExist("t2024") == false);
}