/*
 * test_duckdb.cpp
 *
 *  Copyright (c) 2024, hikyuu.org
 *
 *  Created on: 2024-01-01
 *      Author: fasiondog
 */

#include "../../../test_config.h"
#include "hikyuu/utilities/db_connect/duckdb/DuckDBConnect.h"
#include <hikyuu/utilities/db_connect/DBConnect.h>
#include "hikyuu/utilities/os.h"
#include "hikyuu/utilities/datetime/Datetime.h"
#include "hikyuu/utilities/db_connect/TableMacro.h"
#include <iostream>
#include <chrono>

using namespace hku;

/**
 * @defgroup test_duckdb DuckDB测试
 * @ingroup test_hikyuu_utilities
 */

/** @{ */

/** @par 检测起点 */
TEST_CASE("test_DuckDBConnect_simple") {
    Parameter param;
    param.set<std::string>("db", "test_simple.duckdb");

    DuckDBConnectPtr driver;

    // 测试基本连接创建
    CHECK_NOTHROW(driver = std::make_shared<DuckDBConnect>(param));
    REQUIRE(driver);

    // 测试ping功能
    CHECK_UNARY(driver->ping());

    // 先删除可能存在的表
    CHECK_NOTHROW(driver->exec("DROP TABLE IF EXISTS simple_test"));

    // 测试简单表操作
    CHECK_NOTHROW(driver->exec("CREATE TABLE simple_test (id INTEGER, name VARCHAR(50))"));
    CHECK(driver->tableExist("simple_test"));

    // 测试简单插入和查询
    CHECK_NOTHROW(driver->exec("INSERT INTO simple_test VALUES (1, 'test')"));
    auto count = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM simple_test");
    CHECK(count == 1);
}

TEST_CASE("test_DuckDBConnect_create") {
    // 先删除可能存在的数据库文件
    std::remove("test.duckdb");

    Parameter param;
    param.set<std::string>("db", "test.duckdb");

    DuckDBConnectPtr driver;

    // 测试基本连接创建 - DuckDB允许直接创建数据库文件
    CHECK_NOTHROW(driver = std::make_shared<DuckDBConnect>(param));
    REQUIRE(driver);

    // 测试ping功能
    CHECK(driver->ping());

    // 测试表存在检查
    CHECK_FALSE(driver->tableExist("nonexistent_table"));
}

TEST_CASE("test_DuckDBConnect_basic_operations") {
    Parameter param;
    param.set<std::string>("db", "test_basic.duckdb");

    DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
    REQUIRE(driver);

    // 先删除可能存在的表
    driver->exec("DROP TABLE IF EXISTS test_table");

    // 创建测试表，显式指定id为自增
    CHECK_NOTHROW(driver->exec(
      "CREATE TABLE test_table (id INTEGER PRIMARY KEY, name VARCHAR(50), age INTEGER)"));

    // 验证表创建成功
    CHECK(driver->tableExist("test_table"));

    // 插入数据，显式指定id值
    CHECK_NOTHROW(driver->exec("INSERT INTO test_table (id, name, age) VALUES (1, 'Alice', 25)"));
    CHECK_NOTHROW(driver->exec("INSERT INTO test_table (id, name, age) VALUES (2, 'Bob', 30)"));

    // 查询数据
    auto result = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM test_table");
    CHECK(result == 2);

    // 测试事务
    CHECK_NOTHROW(driver->transaction());
    CHECK_NOTHROW(driver->exec("INSERT INTO test_table (id, name, age) VALUES (3, 'Charlie', 35)"));
    CHECK_NOTHROW(driver->rollback());

    // 验证回滚成功
    result = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM test_table");
    CHECK(result == 2);

    // 测试提交事务
    CHECK_NOTHROW(driver->transaction());
    CHECK_NOTHROW(driver->exec("INSERT INTO test_table (id, name, age) VALUES (3, 'David', 40)"));
    CHECK_NOTHROW(driver->commit());

    // 验证提交成功
    result = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM test_table");
    CHECK(result == 3);
}

TEST_CASE("test_DuckDBStatement_prepared_statements") {
    Parameter param;
    param.set<std::string>("db", "test_stmt.duckdb");

    DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
    REQUIRE(driver);

    // 先删除可能存在的表
    driver->exec("DROP TABLE IF EXISTS users");

    // 创建测试表，显式指定id为自增
    driver->exec("CREATE TABLE users (id INTEGER PRIMARY KEY, username VARCHAR(50), score DOUBLE)");

    // 测试预编译语句插入
    SQLStatementPtr stmt =
      driver->getStatement("INSERT INTO users (id, username, score) VALUES (?, ?, ?)");
    REQUIRE(stmt);

    stmt->bind(0, 1);
    stmt->bind(1, std::string("TestUser"));
    stmt->bind(2, 95.5);
    CHECK_NOTHROW(stmt->exec());

    // 测试查询预编译语句
    SQLStatementPtr select_stmt =
      driver->getStatement("SELECT username, score FROM users WHERE score > ?");
    REQUIRE(select_stmt);

    select_stmt->bind(0, 90.0);
    CHECK_NOTHROW(select_stmt->exec());

    if (select_stmt->moveNext()) {
        std::string username;
        double score;
        select_stmt->getColumn(0, username);
        select_stmt->getColumn(1, score);

        CHECK(username == "TestUser");
        CHECK(score == 95.5);
    }
}

TEST_CASE("test_DuckDBConnect_comprehensive") {
    Parameter param;
    param.set<std::string>("db", "test_comprehensive.duckdb");

    DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
    REQUIRE(driver);

    // 测试连接状态
    CHECK(driver->ping());

    // 测试表创建和删除
    driver->exec("DROP TABLE IF EXISTS comprehensive_test");
    driver->exec(
      "CREATE TABLE comprehensive_test (id INTEGER PRIMARY KEY, name VARCHAR(50), score DOUBLE, "
      "created TIMESTAMP)");

    CHECK(driver->tableExist("comprehensive_test"));
    CHECK_FALSE(driver->tableExist("non_existent_table"));

    // 测试数据插入 - 使用预编译语句插入Datetime
    SQLStatementPtr insert_stmt = driver->getStatement(
      "INSERT INTO comprehensive_test (id, name, score, created) VALUES (?, ?, ?, ?)");
    REQUIRE(insert_stmt);

    Datetime now = Datetime::now();
    insert_stmt->bind(0, 1);
    insert_stmt->bind(1, std::string("Alice"));
    insert_stmt->bind(2, 95.5);
    insert_stmt->bind(3, now);
    insert_stmt->exec();

    insert_stmt->bind(0, 2);
    insert_stmt->bind(1, std::string("Bob"));
    insert_stmt->bind(2, 87.2);
    insert_stmt->bind(3, now);
    insert_stmt->exec();

    // 测试数据查询
    auto count = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM comprehensive_test");
    CHECK(count == 2);

    auto avg_score = driver->queryNumber<double>("SELECT AVG(score) FROM comprehensive_test");
    CHECK(avg_score == 91.35);

    // 测试字符串查询 - 使用SQLStatement代替queryString
    SQLStatementPtr name_stmt =
      driver->getStatement("SELECT name FROM comprehensive_test ORDER BY id LIMIT 1");
    name_stmt->exec();
    if (name_stmt->moveNext()) {
        std::string first_name;
        name_stmt->getColumn(0, first_name);
        CHECK(first_name == "Alice");
    }

    // 测试事务处理
    driver->transaction();
    driver->exec(
      "INSERT INTO comprehensive_test (id, name, score, created) VALUES (3, 'Charlie', 92.0, "
      "NOW())");
    driver->rollback();

    count = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM comprehensive_test");
    CHECK(count == 2);

    // 测试提交事务
    driver->transaction();
    driver->exec(
      "INSERT INTO comprehensive_test (id, name, score, created) VALUES (3, 'David', 88.5, NOW())");
    driver->commit();

    count = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM comprehensive_test");
    CHECK(count == 3);
}

TEST_CASE("test_DuckDBStatement_advanced") {
    Parameter param;
    param.set<std::string>("db", "test_stmt_advanced.duckdb");

    DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
    REQUIRE(driver);

    // 先删除可能存在的表
    driver->exec("DROP TABLE IF EXISTS advanced_test");

    driver->exec(
      "CREATE TABLE advanced_test (id INTEGER PRIMARY KEY, name VARCHAR(50), salary DOUBLE, active "
      "BOOLEAN, data BLOB)");

    // 测试各种数据类型的绑定
    SQLStatementPtr insert_stmt = driver->getStatement(
      "INSERT INTO advanced_test (id, name, salary, active, data) VALUES (?, ?, ?, ?, ?)");
    REQUIRE(insert_stmt);

    insert_stmt->bind(0, 1);
    insert_stmt->bind(1, std::string("John Doe"));
    insert_stmt->bind(2, 50000.50);
    insert_stmt->bind(3, true);
    insert_stmt->bind(4, std::string("binary data"));
    CHECK_NOTHROW(insert_stmt->exec());

    // 测试查询语句
    SQLStatementPtr select_stmt =
      driver->getStatement("SELECT id, name, salary, active, data FROM advanced_test WHERE id = ?");
    REQUIRE(select_stmt);

    select_stmt->bind(0, 1);
    CHECK_NOTHROW(select_stmt->exec());

    if (select_stmt->moveNext()) {
        int id;
        std::string name;
        double salary;
        bool active;
        std::string data;

        select_stmt->getColumn(0, id);
        select_stmt->getColumn(1, name);
        select_stmt->getColumn(2, salary);
        select_stmt->getColumn(3, active);
        select_stmt->getColumn(4, data);

        CHECK(id == 1);
        CHECK(name == "John Doe");
        CHECK(salary == 50000.50);
        CHECK(active == true);
        CHECK(data == "binary data");
    }
}

TEST_CASE("test_DuckDBConnect_with_different_configs") {
    // 测试只读模式
    removeFile("test_readonly.duckdb");
    Parameter readonly_param;
    readonly_param.set<std::string>("db", "test_readonly.duckdb");
    readonly_param.set<std::string>("access_mode", "READ_ONLY");

    // 首先创建数据库
    {
        Parameter write_param;
        write_param.set<std::string>("db", "test_readonly.duckdb");
        write_param.set<std::string>("access_mode", "READ_WRITE");
        DuckDBConnectPtr writer = std::make_shared<DuckDBConnect>(write_param);
        writer->exec("CREATE TABLE test_ro (id INTEGER PRIMARY KEY, data VARCHAR(50))");
    }

    // 然后以只读模式打开
    DuckDBConnectPtr reader = std::make_shared<DuckDBConnect>(readonly_param);
    REQUIRE(reader);
    CHECK(reader->ping());
    CHECK(reader->tableExist("test_ro"));

    // 测试只读模式下不能写入
    CHECK_THROWS_AS(reader->exec("INSERT INTO test_ro (data) VALUES ('test')"), std::exception);
}

TEST_CASE("test_DuckDB_batch_operations_performance") {
    Parameter param;
    param.set<std::string>("db", "test_batch_perf.duckdb");

    DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
    REQUIRE(driver);

    // 创建测试表 - 使用DuckDB支持的自增主键方式
    driver->exec("DROP TABLE IF EXISTS batch_test");
    driver->exec(
      "CREATE TABLE batch_test (id INTEGER PRIMARY KEY, name VARCHAR(50), value DOUBLE)");

    // 定义测试数据结构
    struct BatchTestData {
        std::string name;
        double value;

        // 必需的TABLE_BIND相关方法
        uint64_t m_id = 0;
        bool valid() const {
            return m_id != 0;
        }
        uint64_t id() const {
            return m_id;
        }
        void id(uint64_t id) {
            m_id = id;
        }
        uint64_t rowid() const {
            return m_id;
        }
        void rowid(uint64_t id) {
            m_id = id;
        }

        static std::string getTableName() {
            return "batch_test";
        }
        static const char* getInsertSQL() {
            return "INSERT INTO batch_test (id, name, value) VALUES (?, ?, ?)";
        }
        static const char* getUpdateSQL() {
            return "UPDATE batch_test SET name=?, value=? WHERE id=?";
        }

        void save(const SQLStatementPtr& st) const {
            st->bind(0, static_cast<int64_t>(m_id));
            st->bind(1, name);
            st->bind(2, value);
        }

        void update(const SQLStatementPtr& st) const {
            st->bind(0, name);
            st->bind(1, value);
            st->bind(2, static_cast<int64_t>(m_id));
        }
    };

    // 生成测试数据
    std::vector<BatchTestData> test_data;
    for (int i = 0; i < 1000; ++i) {
        BatchTestData item;
        item.id(i + 1);  // 设置唯一的id值
        item.name = fmt::format("Test Item {}", i);
        item.value = i * 1.5;
        test_data.push_back(item);
    }

    // 测试传统批量保存性能
    auto start_time = std::chrono::high_resolution_clock::now();
    driver->batchSave(test_data, true);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Traditional batch save took: " << duration.count() << " ms" << std::endl;

    // 验证数据插入成功
    auto count = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM batch_test");
    CHECK(count == 1000);

    // 清理数据准备下次测试
    driver->exec("DELETE FROM batch_test");

    std::cout << "Batch operations performance test completed!" << std::endl;
}

TEST_CASE("test_DuckDB_batch_save_or_update") {
    Parameter param;
    param.set<std::string>("db", "test_batch_save_or_update.duckdb");

    DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
    REQUIRE(driver);

    // 创建测试表
    driver->exec("DROP TABLE IF EXISTS mixed_test");
    driver->exec(
      "CREATE TABLE mixed_test (id INTEGER PRIMARY KEY, name VARCHAR(50), status INTEGER)");

    // 定义混合测试数据结构
    struct MixedTestData {
        std::string name;
        int status;

        // 必需的TABLE_BIND相关方法
        uint64_t m_id = 0;
        bool valid() const {
            return m_id != 0;
        }
        uint64_t id() const {
            return m_id;
        }
        void id(uint64_t id) {
            m_id = id;
        }
        uint64_t rowid() const {
            return m_id;
        }
        void rowid(uint64_t id) {
            m_id = id;
        }

        static std::string getTableName() {
            return "mixed_test";
        }
        static const char* getInsertSQL() {
            return "INSERT INTO mixed_test (id, name, status) VALUES (?, ?, ?)";
        }
        static const char* getUpdateSQL() {
            return "UPDATE mixed_test SET name=?, status=? WHERE id=?";
        }

        void save(const SQLStatementPtr& st) const {
            st->bind(0, static_cast<int64_t>(m_id));
            st->bind(1, name);
            st->bind(2, status);
        }

        void update(const SQLStatementPtr& st) const {
            st->bind(0, name);
            st->bind(1, status);
            st->bind(2, static_cast<int64_t>(m_id));
        }
    };

    // 创建混合数据：既有新数据又有需要更新的数据
    std::vector<MixedTestData> mixed_data;

    // 添加一些新数据
    for (int i = 0; i < 50; ++i) {
        MixedTestData item;
        item.id(i + 1);  // 设置唯一的id值
        item.name = fmt::format("New Item {}", i);
        item.status = 1;
        mixed_data.push_back(item);
    }

    // 先保存一部分数据
    std::vector<MixedTestData> initial_data(mixed_data.begin(), mixed_data.begin() + 25);
    driver->batchSave(initial_data, true);

    // 修改这些数据的状态，使其成为更新数据
    for (size_t i = 0; i < 25; ++i) {
        mixed_data[i].status = 2;  // 标记为需要更新
    }

    // 测试批量保存或更新功能
    // 注意：这里需要手动分离保存和更新的数据
    std::vector<MixedTestData> save_items, update_items;
    for (const auto& item : mixed_data) {
        // 前25个是已存在的数据（需要更新），后25个是新数据（需要插入）
        if (item.id() <= 25) {
            update_items.push_back(item);
        } else {
            save_items.push_back(item);
        }
    }

    if (!save_items.empty()) {
        driver->batchSave(save_items, true);
    }

    if (!update_items.empty()) {
        driver->batchUpdate(update_items, true);
    }

    // 验证结果
    auto total_count = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM mixed_test");
    CHECK(total_count == 50);

    auto status_1_count =
      driver->queryNumber<int64_t>("SELECT COUNT(*) FROM mixed_test WHERE status = 1");
    auto status_2_count =
      driver->queryNumber<int64_t>("SELECT COUNT(*) FROM mixed_test WHERE status = 2");

    CHECK(status_1_count == 25);
    CHECK(status_2_count == 25);

    std::cout << "Batch save or update test completed successfully!" << std::endl;
}

TEST_CASE("test_DuckDB_parameter_handling") {
    SUBCASE("默认参数配置") {
        Parameter param;
        param.set<std::string>("db", "test_params.duckdb");

        DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
        REQUIRE(driver);
        CHECK(driver->ping());

        // 验证数据库文件创建成功
        std::ifstream file("test_params.duckdb");
        CHECK(file.good());
        file.close();
    }

    SUBCASE("只读模式配置") {
        Parameter param;
        param.set<std::string>("db", "test_readonly.duckdb");
        param.set<std::string>("access_mode", "READ_ONLY");

        // 先创建数据库文件
        {
            Parameter create_param;
            create_param.set<std::string>("db", "test_readonly.duckdb");
            DuckDBConnectPtr creator = std::make_shared<DuckDBConnect>(create_param);
            creator->exec("DROP TABLE IF EXISTS test_ro");
            creator->exec("CREATE TABLE IF NOT EXISTS test_table (id INTEGER)");
        }

        // 然后以只读模式连接
        DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
        REQUIRE(driver);
        CHECK(driver->ping());

        // 只读模式下应该不能执行写操作
        CHECK_THROWS_AS(driver->exec("CREATE TABLE readonly_test (id INTEGER)"), hku::exception);
    }

    SUBCASE("内存模式配置") {
        Parameter param;
        param.set<std::string>("db", ":memory:");

        DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
        REQUIRE(driver);
        CHECK(driver->ping());

        // 内存模式下创建表应该成功
        CHECK_NOTHROW(driver->exec("CREATE TABLE memory_test (id INTEGER)"));
        CHECK(driver->tableExist("memory_test"));
    }

    std::cout << "Parameter handling test completed successfully!" << std::endl;
}

TEST_CASE("test_DuckDB_Datetime") {
    Parameter param;
    param.set<std::string>("db", "test_datetime.duckdb");

    DuckDBConnectPtr driver = std::make_shared<DuckDBConnect>(param);
    REQUIRE(driver);

    // 先删除可能存在的表
    driver->exec("DROP TABLE IF EXISTS datetime_test");

    // 创建测试表，包含TIMESTAMP类型的字段
    driver->exec(
      "CREATE TABLE datetime_test (id INTEGER PRIMARY KEY, name VARCHAR(50), "
      "created_at TIMESTAMP, "
      "updated_at TIMESTAMP)");

    driver->exec("PRAGMA table_info('datetime_test')");

    // 测试Datetime插入 - 使用预编译语句
    SQLStatementPtr stmt = driver->getStatement(
      "INSERT INTO datetime_test (id, name, created_at, updated_at) VALUES (?, ?, ?, ?)");
    REQUIRE(stmt);

    // 创建几个Datetime对象
    Datetime dt1 = Datetime(2024, 1, 1, 12, 30, 45);
    Datetime dt2 = Datetime(2024, 6, 15, 8, 15, 30);
    Datetime dt3 = Datetime::now();

    // 插入数据
    stmt->bind(0, 1);
    stmt->bind(1, std::string("Test 1"));
    stmt->bind(2, dt1);
    stmt->bind(3, dt2);
    CHECK_NOTHROW(stmt->exec());

    stmt->bind(0, 2);
    stmt->bind(1, std::string("Test 2"));
    stmt->bind(2, dt2);
    stmt->bind(3, dt3);
    CHECK_NOTHROW(stmt->exec());

    // 验证插入的数据数量
    auto count = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM datetime_test");
    CHECK(count == 2);

    // 测试Datetime查询
    SQLStatementPtr select_stmt = driver->getStatement(
      "SELECT id, name, created_at, updated_at FROM datetime_test ORDER BY id");
    REQUIRE(select_stmt);
    CHECK_NOTHROW(select_stmt->exec());

    // 验证第一条记录
    CHECK(select_stmt->moveNext());
    int64_t id1;
    std::string name1;
    Datetime created_at1, updated_at1;
    select_stmt->getColumn(0, id1);
    select_stmt->getColumn(1, name1);
    select_stmt->getColumn(2, created_at1);
    select_stmt->getColumn(3, updated_at1);

    CHECK(id1 == 1);
    CHECK(name1 == "Test 1");
    CHECK(created_at1.year() == 2024);
    CHECK(created_at1.month() == 1);
    CHECK(created_at1.day() == 1);
    CHECK(created_at1.hour() == 12);
    CHECK(created_at1.minute() == 30);
    CHECK(created_at1.second() == 45);

    CHECK(updated_at1.year() == 2024);
    CHECK(updated_at1.month() == 6);
    CHECK(updated_at1.day() == 15);

    // 验证第二条记录
    CHECK(select_stmt->moveNext());
    int64_t id2;
    std::string name2;
    Datetime created_at2, updated_at2;
    select_stmt->getColumn(0, id2);
    select_stmt->getColumn(1, name2);
    select_stmt->getColumn(2, created_at2);
    select_stmt->getColumn(3, updated_at2);

    CHECK(id2 == 2);
    CHECK(name2 == "Test 2");
    CHECK(created_at2.year() == 2024);
    CHECK(created_at2.month() == 6);
    CHECK(created_at2.day() == 15);

    // 测试Datetime的批量操作
    // struct DateTimeTestData {
    //     TABLE_BIND3(DateTimeTestData, datetime_test, name, created_at, updated_at)
    //     std::string name;
    //     Datetime created_at;
    //     Datetime updated_at;
    // };
    struct DateTimeTestData {
        std::string name;
        Datetime created_at;
        Datetime updated_at;

        uint64_t m_id = 0;
        bool valid() const {
            return m_id != 0;
        }
        uint64_t id() const {
            return m_id;
        }
        void id(uint64_t id) {
            m_id = id;
        }
        uint64_t rowid() const {
            return m_id;
        }
        void rowid(uint64_t id) {
            m_id = id;
        }

        static std::string getTableName() {
            return "datetime_test";
        }
        static const char* getInsertSQL() {
            return "INSERT INTO datetime_test (id, name, created_at, updated_at) VALUES (?, ?, "
                   "?,?)";
        }
        static const char* getUpdateSQL() {
            return "UPDATE datetime_test SET name=?, created_at=?, updated_at=? WHERE id=?";
        }
        static const char* getSelectSQL() {
            return "SELECT id, name, created_at, updated_at FROM datetime_test";
        }

        void save(const SQLStatementPtr& st) const {
            st->bind(0, static_cast<int64_t>(m_id));
            st->bind(1, name);
            st->bind(2, created_at);
            st->bind(3, updated_at);
        }

        void update(const SQLStatementPtr& st) const {
            st->bind(0, name);
            st->bind(1, created_at);
            st->bind(2, updated_at);
            st->bind(3, static_cast<int64_t>(m_id));
        }

        void load(const SQLStatementPtr& st) {
            st->getColumn(0, m_id);
            st->getColumn(1, name);
            st->getColumn(2, created_at);
            st->getColumn(3, updated_at);
        }
    };

    // 创建批量测试数据
    std::vector<DateTimeTestData> batch_data;
    for (int i = 0; i < 10; ++i) {
        DateTimeTestData item;
        item.id(i + 10);
        item.name = fmt::format("Batch Item {}", i);
        item.created_at = Datetime(2024, 1, 1 + i, i * 2, i * 3, i * 4);
        item.updated_at = Datetime::now();
        batch_data.push_back(item);
    }

    // 执行批量插入
    CHECK_NOTHROW(driver->batchSave(batch_data, true));

    // 验证批量插入的数据
    auto total_count = driver->queryNumber<int64_t>("SELECT COUNT(*) FROM datetime_test");
    CHECK(total_count == 12);

    // 使用batchLoad读出并验证批量插入的数据
    std::vector<DateTimeTestData> loaded_data;
    driver->batchLoad(loaded_data, "id >= 10 ORDER BY id");

    CHECK(loaded_data.size() == 10);
    for (int i = 0; i < 10; ++i) {
        CHECK(loaded_data[i].id() == i + 10);
        CHECK(loaded_data[i].name == fmt::format("Batch Item {}", i));
        CHECK(loaded_data[i].created_at.year() == 2024);
        CHECK(loaded_data[i].created_at.month() == 1);
        CHECK(loaded_data[i].created_at.day() == 1 + i);
        CHECK(loaded_data[i].created_at.hour() == i * 2);
        CHECK(loaded_data[i].created_at.minute() == i * 3);
        CHECK(loaded_data[i].created_at.second() == i * 4);
    }

    // 测试Datetime的比较查询
    auto count_after_date = driver->queryNumber<int64_t>(
      "SELECT COUNT(*) FROM datetime_test WHERE created_at > '2024-01-05 00:00:00'");
    CHECK(count_after_date > 0);

    std::cout << "Datetime test completed successfully!" << std::endl;
}

/** @} */
