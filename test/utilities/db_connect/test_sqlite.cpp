/*
 * test_mysql.cpp
 *
 *  Created on: 2020-9-26
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <string>
#include <vector>
#include <hikyuu/utilities/ResourcePool.h>
#include <hikyuu/utilities/Null.h>
#include <hikyuu/utilities/os.h>
#include <hikyuu/utilities/db_connect/DBConnect.h>

using namespace hku;

enum class MyEnum : uint16_t { V1, V2, V3 };
struct MyStruct {
    uint32_t i;
    MyEnum e;
    double f;
};

// define how object should be serialized/deserialized
template <typename Ar>
void serialize(Ar& ar, MyStruct& t) {
    ar& YAS_OBJECT_NVP("MyStruct", ("i", t.i), ("e", t.e), ("f", t.f));
}

TEST_CASE("test_sqlite") {
    createDir("测试");
    Parameter param;
    param.set<std::string>("db", "测试/测试.db");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);
    CHECK(con->ping());

    CHECK(con->tableExist("t2018") == false);
    con->exec("create table t2018 (name VARCHAR(20), age INT)");
    CHECK(con->tableExist("t2018") == true);
    con->exec("drop table t2018");
    CHECK(con->tableExist("t2018") == false);

    {
        if (!con->tableExist("t2019")) {
            con->exec(
              R"(CREATE TABLE "t2019" (
                    "id"	INTEGER UNIQUE,
                    "name"	TEXT,
                    "data_int32_t"	INTEGER,
                    "data_int64_t"	INTEGER,
                    "data_double"	REAL,
                    "data_float"	REAL,
                    "my_struct"    BLOB,
                    PRIMARY KEY("id" AUTOINCREMENT)
                );)");
        }

        struct T2019 {
            TABLE_BIND6(T2019, t2019, name, data_int32_t, data_int64_t, data_double, data_float,
                        my_struct);

            void reset() {
                name = "";
                data_int32_t = Null<int32_t>();
                data_int64_t = Null<int64_t>();
                data_double = Null<double>();
                data_float = Null<float>();
            }

            std::string name;
            int32_t data_int32_t;
            int64_t data_int64_t;
            double data_double;
            float data_float;
            MyStruct my_struct;
        };

        T2019 x;
        x.name = "Davis";
        x.data_int32_t = 32;
        x.data_int64_t = 3147483647;
        x.data_double = 3.1415926;
        x.data_float = 3.14f;
        x.my_struct = {8941, MyEnum::V2, 0.045};
        con->save(x);

        T2019 rx;
        con->load(rx);

        CHECK(rx.name == x.name);
        CHECK(rx.data_int32_t == x.data_int32_t);
        CHECK(rx.data_int64_t == x.data_int64_t);
        CHECK(std::abs(rx.data_double - x.data_double) < 0.00001);
        CHECK(std::abs(rx.data_float - x.data_float) < 0.00001);
        CHECK(rx.my_struct.i == 8941);
        CHECK(rx.my_struct.e == MyEnum::V2);
        CHECK(std::abs(rx.my_struct.f - 0.045) < 0.00001);
        con->exec("drop table t2019");
    }

    {
        struct TTT {
            TABLE_BIND4(TTT, ttt, name, age, email, other)
        public:
            TTT(const std::string& name, int age) : name(name), age(age) {}
            TTT(const std::string& name, int age, const std::string& email)
            : name(name), age(age), email(email) {}
            std::string name;
            int age;
            std::string email;
            std::string other;

            std::string to_string() {
                return fmt::format("ttt(id: {}, name: {}, age: {}, email: {}, other: {})", id(),
                                   name, age, email, other);
            }
        };

        con->exec(
          R"(CREATE TABLE "ttt" (
                "id"	INTEGER UNIQUE,
                "name"	TEXT,
                "age"	INTEGER,
                "email"	TEXT,
                "other"	TEXT,
                PRIMARY KEY("id" AUTOINCREMENT)
            );)");

        std::vector<TTT> t_list;
        t_list.push_back(TTT("aaa", 20, "aaa@x.com"));
        t_list.push_back(TTT("bbb", 30, "bbb@x.com"));
        t_list.push_back(TTT("ccc", 15, "ccc@x.com"));
        con->batchSave(t_list.begin(), t_list.end());
        /*for (auto& r : t_list) {
            HKU_INFO("{}", r.tostd::string());
        }*/

        std::vector<TTT> r_list;
        con->batchLoad(r_list, "1=1 order by name DESC");
        /*for (auto& r : r_list) {
            HKU_INFO("{}", r.tostd::string());
        }*/

        CHECK(r_list.size() == 3);
        CHECK(r_list[0].name == "ccc");
        CHECK(r_list[0].age == 15);
        CHECK(r_list[0].email == "ccc@x.com");
        CHECK(r_list[1].name == "bbb");
        CHECK(r_list[1].age == 30);
        CHECK(r_list[1].email == "bbb@x.com");
        CHECK(r_list[2].name == "aaa");
        CHECK(r_list[2].age == 20);
        CHECK(r_list[2].email == "aaa@x.com");

        TTT x;
        con->load(x, "name='bbb'");
        x.age = 100;
        con->save(x);

        TTT y;
        con->load(y, "name='bbb'");
        CHECK(y.age == 100);

        con->exec("drop table ttt");
    }

    CHECK(con->check(true));
    CHECK(con->check(false));

    /*{
        con->exec(
          R"(CREATE TABLE "perf_test" (
                "id"	INTEGER UNIQUE,
                "name"	TEXT,
                "value"	REAL,
                PRIMARY KEY("id" AUTOINCREMENT)
            );)");

        class PerformancTest {
            TABLE_BIND2(perf_test, name, value)

        public:
            PerformancTest(const std::string& name, double value) : name(name), value(value) {}

        public:
            std::string name;
            double value;
        };

        std::vector<PerformancTest> t_list;
        size_t total = 10000;
        for (auto i = 0; i < total; i++) {
            t_list.push_back(PerformancTest(std::to_std::string(i), i));
        }
        {
            SPEND_TIME_MSG(batch, "insert sqlite, total records: {}", total);
            con->batchSave(t_list.begin(), t_list.end());
        }
        con->exec("drop table perf_test");
    }*/
}

TEST_CASE("test_sqlite_check") {
    Parameter param;
    param.set<std::string>("db", "test_data/bad.db");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);
    CHECK(!con->check(true));
    CHECK(!con->check(false));

    CHECK(!con->ping());
}

TEST_CASE("test_sqlite_backup") {
    Parameter param;
    param.set<std::string>("db", "test_data/backup_test.db");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);

    CHECK(con->check(true));
    CHECK(con->check(false));

    CHECK(con->backup("test_data/tmp/backup_test.db.bak1", 5, 5));
    CHECK(con->backup("test_data/tmp/backup_test.db.bak2", -1));
}