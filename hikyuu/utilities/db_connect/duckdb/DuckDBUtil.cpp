/*
 * DuckDBUtil.cpp
 *
 *  Copyright (c) 2024, hikyuu.org
 *
 *  Created on: 2024-01-01
 *      Author: fasiondog
 */

#include "hikyuu/utilities/Log.h"
#include "DuckDBUtil.h"
#include <duckdb.h>

namespace hku {

bool isValidDuckDBFile(const std::string& filename) noexcept {
    try {
        // 尝试打开数据库文件进行验证
        duckdb_database db;
        duckdb_connection conn;
        char* error = nullptr;
        
        duckdb_state state = duckdb_open(filename.c_str(), &db);
        if (state != DuckDBSuccess) {
            return false;
        }
        
        state = duckdb_connect(db, &conn);
        if (state != DuckDBSuccess) {
            duckdb_close(&db);
            return false;
        }
        
        duckdb_result result;
        state = duckdb_query(conn, "SELECT 1", &result);
        bool valid = (state == DuckDBSuccess);
        
        duckdb_destroy_result(&result);
        duckdb_disconnect(&conn);
        duckdb_close(&db);
        
        return valid;
    } catch (...) {
        return false;
    }
}

std::string getDatabaseStats(DuckDBConnect* connect) {
    if (!connect) {
        return "Invalid connection";
    }
    
    try {
        std::ostringstream oss;
        oss << "=== Database Statistics ===\n";
        
        // 获取表数量
        duckdb_result result;
        duckdb_state state = duckdb_query(connect->getConnection(), 
            "SELECT COUNT(*) as table_count FROM information_schema.tables", &result);
        
        if (state == DuckDBSuccess) {
            idx_t row_count = duckdb_row_count(&result);
            if (row_count > 0) {
                int64_t table_count = duckdb_value_int64(&result, 0, 0);
                oss << "Total tables: " << table_count << "\n";
            }
        }
        duckdb_destroy_result(&result);
        
        return oss.str();
    } catch (const std::exception& e) {
        return std::string("Error getting stats: ") + e.what();
    }
}

std::string getTableInfo(DuckDBConnect* connect, const std::string& tablename) {
    if (!connect) {
        return "Invalid connection";
    }
    
    try {
        std::ostringstream oss;
        oss << "=== Table Info: " << tablename << " ===\n";
        
        // 获取表结构信息
        std::string query = fmt::format("DESCRIBE \"{}\"", tablename);
        duckdb_result result;
        duckdb_state state = duckdb_query(connect->getConnection(), query.c_str(), &result);
        
        if (state == DuckDBSuccess) {
            oss << "Column Name\tData Type\tNull\tKey\tDefault\tExtra\n";
            oss << "--------------------------------------------------------\n";
            
            idx_t row_count = duckdb_row_count(&result);
            for (idx_t i = 0; i < row_count; i++) {
                char* column_name = duckdb_value_varchar(&result, 0, i);
                char* data_type = duckdb_value_varchar(&result, 1, i);
                
                std::string col_name = column_name ? std::string(column_name) : "";
                std::string dtype = data_type ? std::string(data_type) : "";
                
                if (column_name) free(column_name);
                if (data_type) free(data_type);
                
                oss << col_name << "\t" << dtype << "\t\t\t\t\n";
            }
        }
        duckdb_destroy_result(&result);
        
        // 获取行数统计
        try {
            std::string count_query = fmt::format("SELECT COUNT(*) FROM \"{}\"", tablename);
            duckdb_result count_result;
            duckdb_state count_state = duckdb_query(connect->getConnection(), count_query.c_str(), &count_result);
            
            if (count_state == DuckDBSuccess) {
                idx_t count_row_count = duckdb_row_count(&count_result);
                if (count_row_count > 0) {
                    int64_t row_count = duckdb_value_int64(&count_result, 0, 0);
                    oss << "\nTotal rows: " << row_count << "\n";
                }
            }

            duckdb_destroy_result(&count_result);
        } catch (...) {
            // 忽略计数错误
        }
        
        return oss.str();
    } catch (const std::exception& e) {
        return std::string("Error getting table info: ") + e.what();
    }
}

std::vector<std::string> getAllTableNames(DuckDBConnect* connect) {
    std::vector<std::string> table_names;
    
    if (!connect) {
        return table_names;
    }
    
    try {
        duckdb_result result;
        duckdb_state state = duckdb_query(connect->getConnection(),
            "SELECT table_name FROM information_schema.tables WHERE table_schema='main'", &result);
        
        if (state == DuckDBSuccess) {
            idx_t row_count = duckdb_row_count(&result);
            for (idx_t i = 0; i < row_count; i++) {
                char* table_name = duckdb_value_varchar(&result, 0, i);
                if (table_name) {
                    table_names.push_back(std::string(table_name));
                    free(table_name);
                }
            }
        }
        duckdb_destroy_result(&result);
    } catch (const std::exception& e) {
        HKU_WARN("Failed to get table names: {}", e.what());
    }
    
    return table_names;
}

bool backupDatabase(DuckDBConnect* connect, const std::string& backup_filename) noexcept {
    if (!connect) {
        return false;
    }
    
    try {
        // DuckDB的备份可以通过EXPORT DATABASE命令实现
        std::string sql = fmt::format("EXPORT DATABASE '{}'", backup_filename);
        duckdb_result result;
        duckdb_state state = duckdb_query(connect->getConnection(), sql.c_str(), &result);
        
        bool success = (state == DuckDBSuccess);
        duckdb_destroy_result(&result);
        return success;
    } catch (const std::exception& e) {
        HKU_ERROR("Failed to backup database: {}", e.what());
        return false;
    } catch (...) {
        HKU_ERROR("Unknown error during database backup");
        return false;
    }
}

bool checkDatabaseIntegrity(DuckDBConnect* connect) noexcept {
    if (!connect) {
        return false;
    }
    
    try {
        duckdb_result result;
        duckdb_state state = duckdb_query(connect->getConnection(), 
            "SELECT COUNT(*) FROM information_schema.tables", &result);
        
        bool valid = (state == DuckDBSuccess);
        duckdb_destroy_result(&result);
        return valid;
    } catch (...) {
        return false;
    }
}

}  // namespace hku