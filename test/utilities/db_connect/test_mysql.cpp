/*
 * test_mysql.cpp
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2026-05-06
 *      Author: fasiondog
 */

#include "hikyuu/utilities/config.h"

#if HKU_ENABLE_MYSQL

#include "hikyuu/utilities/db_connect/mysql/MySQLConnect.h"
#include "hikyuu/utilities/ini_parser/IniParser.h"
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
            // 不指定 db，让连接默认连接到无数据库状态，后续在测试中动态选择或创建
        }
    } catch (const std::exception& e) {
        // Ignore errors
    }
    
    return param;
}

TEST_CASE("test_mysql_basic_connection") {
    // 测试基本连接功能（需要本地 MySQL 服务器）
    Parameter param = loadMySQLConfig();
    
    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }
    
    try {
        MySQLConnect conn(param);
        
        // 验证 ping
        CHECK(conn.ping() == true);
        
    } catch (const std::exception& e) {
        MESSAGE("MySQL server not available: " << e.what());
    }
}

TEST_CASE("test_mysql_statement_basic") {
    Parameter param = loadMySQLConfig();
    
    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }
    
    try {
        MySQLConnect conn(param);
        
        // 先尝试使用 test 数据库，如果不存在则创建
        try {
            conn.exec("USE test");
        } catch (...) {
            conn.exec("CREATE DATABASE IF NOT EXISTS test");
            conn.exec("USE test");
        }
        
        // 创建测试表
        conn.exec("DROP TABLE IF EXISTS test_boost_mysql");
        conn.exec("CREATE TABLE test_boost_mysql (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(100), age INT)");
        
        // 测试预处理语句插入
        auto stmt = conn.getStatement("INSERT INTO test_boost_mysql (name, age) VALUES (?, ?)");
        stmt->bind(0, std::string("Alice"));
        stmt->bind(1, 25);
        stmt->exec();
        
        uint64_t last_id = stmt->getLastRowid();
        CHECK(last_id > 0);
        
        // 测试查询
        auto select_stmt = conn.getStatement("SELECT id, name, age FROM test_boost_mysql WHERE id = ?");
        select_stmt->bind(0, static_cast<int64_t>(last_id));
        select_stmt->exec();
        
        CHECK(select_stmt->moveNext() == true);
        
        int64_t id;
        std::string name;
        int64_t age;
        
        select_stmt->getColumn(0, id);
        select_stmt->getColumn(1, name);
        select_stmt->getColumn(2, age);
        
        CHECK(id == static_cast<int64_t>(last_id));
        CHECK(name == "Alice");
        CHECK(age == 25);
        
        // 清理
        conn.exec("DROP TABLE IF EXISTS test_boost_mysql");
        
    } catch (const std::exception& e) {
        MESSAGE("MySQL server not available: " << e.what());
    }
}

TEST_CASE("test_mysql_auto_reconnect") {
    Parameter param = loadMySQLConfig();
    
    if (param.empty()) {
        MESSAGE("MySQL configuration not found, skipping test");
        return;
    }
    
    try {
        MySQLConnect conn(param);
        
        // 测试 ping 方法
        CHECK(conn.ping() == true);
        
        // 执行一些查询
        int64_t result = conn.exec("SELECT 1");
        CHECK(result >= 0);
        
        // 再次 ping 验证连接仍然有效
        CHECK(conn.ping() == true);
        
    } catch (const std::exception& e) {
        MESSAGE("MySQL server not available: " << e.what());
    }
}

TEST_CASE("test_mysql_data_types") {
    Parameter param = loadMySQLConfig();
    
    if (param.empty()) {
        return;
    }
    
    MySQLConnect conn(param);
    
    // 选择或创建 test 数据库（USE 可能失败，但 CREATE DATABASE IF NOT EXISTS 不会）
    try {
        conn.exec("USE test");
    } catch (...) {
        conn.exec("CREATE DATABASE IF NOT EXISTS test");
        conn.exec("USE test");
    }
    
    // 创建包含各种数据类型的测试表
    conn.exec("DROP TABLE IF EXISTS test_data_types");
    conn.exec(R"(
            CREATE TABLE test_data_types (
                id INT AUTO_INCREMENT PRIMARY KEY,
                col_tinyint TINYINT,
                col_smallint SMALLINT,
                col_mediumint MEDIUMINT,
                col_int INT,
                col_bigint BIGINT,
                col_float FLOAT,
                col_double DOUBLE,
                col_decimal DECIMAL(10,2),
                col_char CHAR(10),
                col_varchar VARCHAR(100),
                col_text TEXT,
                col_date DATE,
                col_datetime DATETIME,
                col_timestamp TIMESTAMP,
                col_time TIME,
                col_year YEAR,
                col_bool BOOLEAN,
                col_binary BINARY(16),
                col_varbinary VARBINARY(100)
            )
        )");
    
    // 插入测试数据（使用字符串格式）
    auto insert_stmt = conn.getStatement(R"(
        INSERT INTO test_data_types (
            col_tinyint, col_smallint, col_mediumint, col_int, col_bigint,
            col_float, col_double, col_decimal,
            col_char, col_varchar, col_text,
            col_date, col_datetime, col_timestamp, col_time, col_year,
            col_bool, col_binary, col_varbinary
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    // 绑定各种类型的参数
    insert_stmt->bind(0, static_cast<int8_t>(127));           // TINYINT
    insert_stmt->bind(1, static_cast<int16_t>(32767));        // SMALLINT
    insert_stmt->bind(2, static_cast<int32_t>(8388607));      // MEDIUMINT
    insert_stmt->bind(3, static_cast<int32_t>(2147483647));   // INT
    insert_stmt->bind(4, static_cast<int64_t>(9223372036854775807LL)); // BIGINT
    insert_stmt->bind(5, 3.14f);                               // FLOAT
    insert_stmt->bind(6, 3.14159265358979);                    // DOUBLE
    insert_stmt->bind(7, 999999.99);                           // DECIMAL
    insert_stmt->bind(8, std::string("CHAR"));                 // CHAR
    insert_stmt->bind(9, std::string("VARCHAR test"));         // VARCHAR
    insert_stmt->bind(10, std::string("TEXT content"));        // TEXT
    insert_stmt->bind(11, std::string("2024-01-15"));          // DATE
    insert_stmt->bind(12, std::string("2024-01-15 10:30:45")); // DATETIME
    insert_stmt->bind(13, std::string("2024-01-15 10:30:45")); // TIMESTAMP
    insert_stmt->bind(14, std::string("10:30:45"));            // TIME
    insert_stmt->bind(15, static_cast<int16_t>(2024));         // YEAR
    insert_stmt->bind(16, static_cast<int8_t>(1));             // BOOLEAN (true)
    
    // BINARY 数据
    std::vector<char> binary_data(16, 'A');
    insert_stmt->bindBlob(17, binary_data);
    
    // VARBINARY 数据
    std::vector<char> varbinary_data = {'H', 'e', 'l', 'l', 'o'};
    insert_stmt->bindBlob(18, varbinary_data);
    
    insert_stmt->exec();
    uint64_t last_id = insert_stmt->getLastRowid();
    CHECK(last_id > 0);
    
    // 测试使用 hku::Datetime 类型直接绑定
    auto insert_dt_stmt = conn.getStatement(R"(
        INSERT INTO test_data_types (col_date, col_datetime, col_timestamp) 
        VALUES (?, ?, ?)
    )");
    
    Datetime dt1(2024, 2, 20);                        // DATE
    Datetime dt2(2024, 2, 20, 14, 30, 45);            // DATETIME
    Datetime dt3(2024, 2, 20, 15, 45, 30);            // TIMESTAMP
    
    insert_dt_stmt->bind(0, dt1);
    insert_dt_stmt->bind(1, dt2);
    insert_dt_stmt->bind(2, dt3);
    insert_dt_stmt->exec();
    
    uint64_t dt_last_id = insert_dt_stmt->getLastRowid();
    CHECK(dt_last_id > 0);
    
    // 验证 Datetime 绑定后的数据
    auto select_dt_stmt = conn.getStatement("SELECT col_date, col_datetime, col_timestamp FROM test_data_types WHERE id = ?");
    select_dt_stmt->bind(0, static_cast<int64_t>(dt_last_id));
    select_dt_stmt->exec();
    CHECK(select_dt_stmt->moveNext() == true);
    
    // 创建新的查询语句来测试 Datetime 类型
    auto select_dt_stmt2 = conn.getStatement("SELECT col_date, col_datetime, col_timestamp FROM test_data_types WHERE id = ?");
    select_dt_stmt2->bind(0, static_cast<int64_t>(dt_last_id));
    select_dt_stmt2->exec();
    CHECK(select_dt_stmt2->moveNext() == true);
    
    Datetime read_date, read_datetime, read_timestamp;
    
    select_dt_stmt2->getColumn(0, read_date);
    select_dt_stmt2->getColumn(1, read_datetime);
    select_dt_stmt2->getColumn(2, read_timestamp);
    
    CHECK(read_date.year() == 2024);
    CHECK(read_date.month() == 2);
    CHECK(read_date.day() == 20);
    
    CHECK(read_datetime.year() == 2024);
    CHECK(read_datetime.month() == 2);
    CHECK(read_datetime.day() == 20);
    CHECK(read_datetime.hour() == 14);
    CHECK(read_datetime.minute() == 30);
    CHECK(read_datetime.second() == 45);
    
    CHECK(read_timestamp.year() == 2024);
    CHECK(read_timestamp.month() == 2);
    CHECK(read_timestamp.day() == 20);
    CHECK(read_timestamp.hour() == 15);
    CHECK(read_timestamp.minute() == 45);
    CHECK(read_timestamp.second() == 30);
    
    // 查询并验证数据
    auto select_stmt = conn.getStatement("SELECT * FROM test_data_types WHERE id = ?");
    select_stmt->bind(0, static_cast<int64_t>(last_id));
    select_stmt->exec();
    
    CHECK(select_stmt->moveNext() == true);
    
    // 验证整数类型
    int8_t tinyint_val;
    int16_t smallint_val;
    int32_t mediumint_val, int_val;
    int64_t bigint_val;
    
    select_stmt->getColumn(1, tinyint_val);
    select_stmt->getColumn(2, smallint_val);
    select_stmt->getColumn(3, mediumint_val);
    select_stmt->getColumn(4, int_val);
    select_stmt->getColumn(5, bigint_val);
    
    CHECK(tinyint_val == 127);
    CHECK(smallint_val == 32767);
    CHECK(mediumint_val == 8388607);
    CHECK(int_val == 2147483647);
    CHECK(bigint_val == 9223372036854775807LL);
    
    // 验证浮点类型
    float float_val;
    double double_val;
    double decimal_val;
    
    select_stmt->getColumn(6, float_val);
    select_stmt->getColumn(7, double_val);
    select_stmt->getColumn(8, decimal_val);
    
    CHECK(float_val == doctest::Approx(3.14f).epsilon(0.01));
    CHECK(double_val == doctest::Approx(3.14159265358979).epsilon(0.0001));
    CHECK(decimal_val == doctest::Approx(999999.99).epsilon(0.01));
    
    // 验证字符串类型
    std::string char_val, varchar_val, text_val;
    
    select_stmt->getColumn(9, char_val);
    select_stmt->getColumn(10, varchar_val);
    select_stmt->getColumn(11, text_val);
    
    CHECK(char_val == "CHAR");
    CHECK(varchar_val == "VARCHAR test");
    CHECK(text_val == "TEXT content");
    
    // 验证日期时间类型（作为字符串返回）
    std::string date_val, datetime_val, timestamp_val, time_val;
    int16_t year_val;
    
    select_stmt->getColumn(12, date_val);
    select_stmt->getColumn(13, datetime_val);
    select_stmt->getColumn(14, timestamp_val);
    select_stmt->getColumn(15, time_val);
    select_stmt->getColumn(16, year_val);
    
    CHECK(date_val.find("2024-01-15") != std::string::npos);
    CHECK(datetime_val.find("2024-01-15") != std::string::npos);
    CHECK(timestamp_val.find("2024-01-15") != std::string::npos);
    CHECK(time_val.find("10:30:45") != std::string::npos);
    CHECK(year_val == 2024);
    
    // 验证 hku::Datetime 类型的直接支持
    Datetime dt_date, dt_datetime, dt_timestamp;
    select_stmt->getColumn(12, dt_date);      // DATE -> Datetime
    select_stmt->getColumn(13, dt_datetime);  // DATETIME -> Datetime
    select_stmt->getColumn(14, dt_timestamp); // TIMESTAMP -> Datetime
    
    CHECK(dt_date.year() == 2024);
    CHECK(dt_date.month() == 1);
    CHECK(dt_date.day() == 15);
    
    CHECK(dt_datetime.year() == 2024);
    CHECK(dt_datetime.month() == 1);
    CHECK(dt_datetime.day() == 15);
    CHECK(dt_datetime.hour() == 10);
    CHECK(dt_datetime.minute() == 30);
    CHECK(dt_datetime.second() == 45);
    
    CHECK(dt_timestamp.year() == 2024);
    CHECK(dt_timestamp.month() == 1);
    CHECK(dt_timestamp.day() == 15);
    
    // 验证布尔类型
    int8_t bool_val;
    select_stmt->getColumn(17, bool_val);
    CHECK(bool_val == 1);
    
    // 验证二进制类型
    std::vector<char> binary_result, varbinary_result;
    select_stmt->getColumn(18, binary_result);
    select_stmt->getColumn(19, varbinary_result);
    
    CHECK(binary_result.size() == 16);
    CHECK(varbinary_result.size() == 5);
    CHECK(varbinary_result[0] == 'H');
    CHECK(varbinary_result[4] == 'o');
    
    // 清理
    conn.exec("DROP TABLE IF EXISTS test_data_types");
}

#else

#include <doctest/doctest.h>

TEST_CASE("test_mysql_not_supported") {
    MESSAGE("MySQL support is not enabled");
}

#endif // HKU_ENABLE_MYSQL
