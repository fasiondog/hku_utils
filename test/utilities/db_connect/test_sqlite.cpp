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

TEST_CASE("test_batchSaveOrUpdate") {
    createDir("测试");
    Parameter param;
    param.set<std::string>("db", "测试/测试_batch_save_or_update.db");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);
    CHECK(con->ping());

    struct BatchItem {
        TABLE_BIND2(BatchItem, t_batch_save_or_update, name, age)
        std::string name;
        int age{0};
    };

    if (con->tableExist("t_batch_save_or_update")) {
        con->exec("drop table t_batch_save_or_update");
    }
    con->exec(
      R"(create table t_batch_save_or_update ("id" INTEGER NOT NULL UNIQUE, name VARCHAR(20), age INT, PRIMARY KEY("id" AUTOINCREMENT));)");

    /** Pre-insert two rows so that their rowids are known for the update part */
    std::vector<BatchItem> seed(2);
    seed[0].name = "seed0";
    seed[0].age = 0;
    seed[1].name = "seed1";
    seed[1].age = 0;
    con->batchSave(seed.begin(), seed.end(), true);
    CHECK_EQ(seed[0].rowid(), 1);
    CHECK_EQ(seed[1].rowid(), 2);

    /** Two elements to update (valid) and three to save (invalid), mixed in one range */
    std::vector<BatchItem> items(5);
    items[0] = seed[0];
    items[0].age = 10;
    items[1] = seed[1];
    items[1].age = 20;
    items[2].name = "new0";
    items[3].name = "new1";
    items[4].name = "new2";
    con->batchSaveOrUpdate(items.begin(), items.end(), true);

    /** The saved elements must get their rowids written back, in line with batchSave */
    CHECK_EQ(items[2].rowid(), 3);
    CHECK_EQ(items[3].rowid(), 4);
    CHECK_EQ(items[4].rowid(), 5);

    /** The updated rows hold the new values, the saved rows hold the original values */
    int64_t total = con->queryNumber<int64_t>("select count(1) from t_batch_save_or_update");
    CHECK_EQ(total, 5);
    BatchItem loaded;
    con->load(loaded, Field("name") == "seed0");
    CHECK_EQ(loaded.age, 10);
    con->load(loaded, Field("name") == "seed1");
    CHECK_EQ(loaded.age, 20);
    con->load(loaded, Field("name") == "new0");
    CHECK_EQ(loaded.age, 0);

    /** A second run over the now all-valid range must update in place instead of duplicating rows
     */
    items[2].age = 30;
    con->batchSaveOrUpdate(items.begin(), items.end(), true);
    total = con->queryNumber<int64_t>("select count(1) from t_batch_save_or_update");
    CHECK_EQ(total, 5);
    con->load(loaded, Field("name") == "new0");
    CHECK_EQ(loaded.age, 30);

    /** autotrans=false leaves the transaction handling to the caller */
    items[3].age = 40;
    con->batchSaveOrUpdate(items.begin(), items.end(), false);
    con->load(loaded, Field("name") == "new1");
    CHECK_EQ(loaded.age, 40);
}

/**
 * @brief test integer boundary handling
 * 1. int64 boundary values (INT64_MIN/INT64_MAX) roundtrip exactly
 * 2. uint64 values within the int64 range roundtrip exactly
 * 3. Binding a uint64 value above INT64_MAX to SQLite (int64-only storage) must throw
 * 4. Reading an int64 column into a narrower signed integer must throw on overflow
 * 5. Reading a negative value into an unsigned integer must throw
 */
TEST_CASE("test_sqlite_integer_boundary") {
    Parameter param;
    param.set<std::string>("db", ":memory:");
    param.set<int>("flags", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    auto con = std::make_shared<SQLiteConnect>(param);

    con->exec("CREATE TABLE t_int (v INTEGER)");

    /** int64 boundary values roundtrip exactly */
    {
        auto st = con->getStatement("INSERT INTO t_int (v) VALUES (?)");
        st->bind(0, std::numeric_limits<int64_t>::max());
        st->exec();
        st = con->getStatement("INSERT INTO t_int (v) VALUES (?)");
        st->bind(0, std::numeric_limits<int64_t>::min());
        st->exec();

        auto query = con->getStatement("SELECT v FROM t_int ORDER BY v DESC");
        query->exec();
        CHECK_UNARY(query->moveNext());
        int64_t val = 0;
        query->getColumn(0, val);
        CHECK_EQ(val, std::numeric_limits<int64_t>::max());
        CHECK_UNARY(query->moveNext());
        query->getColumn(0, val);
        CHECK_EQ(val, std::numeric_limits<int64_t>::min());
        con->exec("DELETE FROM t_int");
    }

    /** uint64 values within the int64 range roundtrip exactly */
    {
        uint64_t uval = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        auto st = con->getStatement("INSERT INTO t_int (v) VALUES (?)");
        st->bind(0, uval);
        st->exec();

        auto query = con->getStatement("SELECT v FROM t_int");
        query->exec();
        CHECK_UNARY(query->moveNext());
        uint64_t rval = 0;
        query->getColumn(0, rval);
        CHECK_EQ(rval, uval);
        con->exec("DELETE FROM t_int");
    }

    /** Binding a uint64 value above INT64_MAX must throw instead of wrapping to negative */
    {
        auto st = con->getStatement("INSERT INTO t_int (v) VALUES (?)");
        uint64_t too_big = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
        CHECK_THROWS(st->bind(0, too_big));
        uint64_t umax = std::numeric_limits<uint64_t>::max();
        CHECK_THROWS(st->bind(0, umax));
    }

    /** Reading an int64 column into a narrower signed integer must throw on overflow */
    {
        auto st = con->getStatement("INSERT INTO t_int (v) VALUES (?)");
        st->bind(0, static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1);
        st->exec();

        auto query = con->getStatement("SELECT v FROM t_int");
        query->exec();
        CHECK_UNARY(query->moveNext());
        int32_t narrow = 0;
        CHECK_THROWS(query->getColumn(0, narrow));
        int64_t wide = 0;
        CHECK_NOTHROW(query->getColumn(0, wide));
        CHECK_EQ(wide, static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1);
        con->exec("DELETE FROM t_int");
    }

    /** Reading a negative value into an unsigned integer must throw */
    {
        auto st = con->getStatement("INSERT INTO t_int (v) VALUES (?)");
        st->bind(0, static_cast<int64_t>(-1));
        st->exec();

        auto query = con->getStatement("SELECT v FROM t_int");
        query->exec();
        CHECK_UNARY(query->moveNext());
        uint64_t uval = 0;
        CHECK_THROWS(query->getColumn(0, uval));
        uint32_t unarrow = 0;
        CHECK_THROWS(query->getColumn(0, unarrow));
        con->exec("DELETE FROM t_int");
    }
}