/*
 * duckdb_example.cpp
 *
 *  Copyright (c) 2024, hikyuu.org
 *
 *  Created on: 2024-01-01
 *      Author: fasiondog
 */

#include "hikyuu/utilities/db_connect/duckdb/DuckDBConnect.h"
#include "hikyuu/utilities/db_connect/duckdb/DuckDBUtil.h"
#include <iostream>

using namespace hku;

// 示例：基本数据库操作
void basic_example() {
    std::cout << "=== Basic DuckDB Example ===" << std::endl;
    
    try {
        // 创建数据库连接参数
        Parameter param;
        param.set<std::string>("db", "example.db");
        
        // 创建连接
        DuckDBConnectPtr db = std::make_shared<DuckDBConnect>(param);
        
        // 创建表
        db->exec(R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY,
                name VARCHAR(100),
                email VARCHAR(100),
                age INTEGER,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )");
        
        std::cout << "Table created successfully" << std::endl;
        
        // 插入数据
        db->transaction();
        try {
            db->exec("INSERT INTO users (name, email, age) VALUES ('Alice', 'alice@example.com', 25)");
            db->exec("INSERT INTO users (name, email, age) VALUES ('Bob', 'bob@example.com', 30)");
            db->exec("INSERT INTO users (name, email, age) VALUES ('Charlie', 'charlie@example.com', 35)");
            db->commit();
            std::cout << "Data inserted successfully" << std::endl;
        } catch (...) {
            db->rollback();
            throw;
        }
        
        // 查询数据
        auto count = db->queryNumber<int64_t>("SELECT COUNT(*) FROM users");
        std::cout << "Total users: " << count << std::endl;
        
        // 使用预编译语句查询
        SQLStatementPtr stmt = db->getStatement("SELECT name, email, age FROM users WHERE age > ?");
        stmt->bind(0, 25);
        stmt->exec();
        
        std::cout << "Users older than 25:" << std::endl;
        while (stmt->moveNext()) {
            std::string name, email;
            int age;
            stmt->getColumn(0, name);
            stmt->getColumn(1, email);
            stmt->getColumn(2, age);
            std::cout << "  " << name << " (" << email << ") - " << age << " years old" << std::endl;
        }
        
        // 显示表信息
        std::cout << "\n" << getTableInfo(db.get(), "users") << std::endl;
        
        // 显示数据库统计
        std::cout << getDatabaseStats(db.get()) << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// 示例：数据分析操作
void analytics_example() {
    std::cout << "\n=== Analytics Example ===" << std::endl;
    
    try {
        Parameter param;
        param.set<std::string>("db", "analytics.db");
        
        DuckDBConnectPtr db = std::make_shared<DuckDBConnect>(param);
        
        // 创建销售数据表
        db->exec(R"(
            CREATE TABLE IF NOT EXISTS sales (
                id INTEGER PRIMARY KEY,
                product_name VARCHAR(100),
                category VARCHAR(50),
                price DECIMAL(10,2),
                quantity INTEGER,
                sale_date DATE,
                region VARCHAR(50)
            )
        )");
        
        // 插入示例数据
        db->transaction();
        try {
            db->exec("INSERT INTO sales (product_name, category, price, quantity, sale_date, region) VALUES "
                     "('Laptop', 'Electronics', 999.99, 2, '2024-01-15', 'North')");
            db->exec("INSERT INTO sales (product_name, category, price, quantity, sale_date, region) VALUES "
                     "('Mouse', 'Electronics', 29.99, 5, '2024-01-15', 'South')");
            db->exec("INSERT INTO sales (product_name, category, price, quantity, sale_date, region) VALUES "
                     "('Keyboard', 'Electronics', 79.99, 3, '2024-01-16', 'North')");
            db->exec("INSERT INTO sales (product_name, category, price, quantity, sale_date, region) VALUES "
                     "('Monitor', 'Electronics', 299.99, 1, '2024-01-16', 'East')");
            db->exec("INSERT INTO sales (product_name, category, price, quantity, sale_date, region) VALUES "
                     "('Desk', 'Furniture', 199.99, 1, '2024-01-17', 'West')");
            db->commit();
        } catch (...) {
            db->rollback();
            throw;
        }
        
        // 数据分析查询
        
        // 1. 总销售额
        auto total_sales = db->queryNumber<double>(
            "SELECT SUM(price * quantity) FROM sales");
        std::cout << "Total Sales: $" << total_sales << std::endl;
        
        // 2. 按类别分组统计
        SQLStatementPtr category_stmt = db->getStatement(
            "SELECT category, SUM(price * quantity) as total_sales, COUNT(*) as items_sold "
            "FROM sales GROUP BY category ORDER BY total_sales DESC");
        category_stmt->exec();
        
        std::cout << "\nSales by Category:" << std::endl;
        std::cout << "Category\tTotal Sales\tItems Sold" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        while (category_stmt->moveNext()) {
            std::string category;
            double sales;
            int items;
            category_stmt->getColumn(0, category);
            category_stmt->getColumn(1, sales);
            category_stmt->getColumn(2, items);
            std::cout << category << "\t$" << sales << "\t" << items << std::endl;
        }
        
        // 3. 按地区统计
        std::cout << "\nSales by Region:" << std::endl;
        auto region_result = db->queryNumber<double>(
            "SELECT region, SUM(price * quantity) as total FROM sales GROUP BY region ORDER BY total DESC");
        // 注意：这里简化处理，实际应该使用更复杂的查询
        
        // 4. 最畅销产品
        SQLStatementPtr top_products = db->getStatement(
            "SELECT product_name, SUM(quantity) as total_quantity "
            "FROM sales GROUP BY product_name ORDER BY total_quantity DESC LIMIT 3");
        top_products->exec();
        
        std::cout << "\nTop Selling Products:" << std::endl;
        while (top_products->moveNext()) {
            std::string product;
            int quantity;
            top_products->getColumn(0, product);
            top_products->getColumn(1, quantity);
            std::cout << "  " << product << ": " << quantity << " units" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Analytics Error: " << e.what() << std::endl;
    }
}

// 示例：配置选项使用
void config_example() {
    std::cout << "\n=== Configuration Example ===" << std::endl;
    
    try {
        // 创建带配置的连接
        Parameter param;
        param.set<std::string>("db", "config_example.db");
        param.set<std::string>("access_mode", "READ_WRITE");
        param.set<bool>("allow_unsigned_extensions", false);
        
        // 添加配置选项
        std::map<std::string, std::string> config_options;
        config_options["memory_limit"] = "1GB";
        config_options["threads"] = "4";
        param.set<std::map<std::string, std::string>>("config", config_options);
        
        DuckDBConnectPtr db = std::make_shared<DuckDBConnect>(param);
        
        std::cout << "Database created with custom configuration" << std::endl;
        std::cout << "Connection valid: " << (db->ping() ? "Yes" : "No") << std::endl;
        
        // 检查数据库完整性
        std::cout << "Database integrity: " << (checkDatabaseIntegrity(db.get()) ? "OK" : "FAILED") << std::endl;
        
        // 获取所有表名
        auto tables = getAllTableNames(db.get());
        std::cout << "Existing tables: ";
        for (const auto& table : tables) {
            std::cout << table << " ";
        }
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Configuration Error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "DuckDB Driver Examples" << std::endl;
    std::cout << "======================" << std::endl;
    
    basic_example();
    analytics_example();
    config_example();
    
    std::cout << "\nAll examples completed!" << std::endl;
    return 0;
}