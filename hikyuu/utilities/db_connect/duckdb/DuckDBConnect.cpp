/*
 * DuckDBConnect.cpp
 *
 *  Copyright (c) 2024, hikyuu.org
 *
 *  Created on: 2024-01-01
 *      Author: fasiondog
 */

#include "hikyuu/utilities/config.h"
#include "hikyuu/utilities/Log.h"
#include "DuckDBConnect.h"
#include "DuckDBStatement.h"

namespace hku {

DuckDBConnect::DuckDBConnect(const Parameter& param) : DBConnectBase(param) {
    try {
        // 获取数据库文件名
        m_dbname = getParam<std::string>("db");
        
        // 获取访问模式参数
        std::string access_mode = tryGetParam<std::string>("access_mode", "READ_WRITE");

        duckdb_config config;
        duckdb_create_config(&config);

        // 设置访问模式
        if (access_mode == "READ_ONLY") {
            duckdb_set_config(config, "access_mode", "READ_ONLY");
        }

        // 创建数据库
        char* error_msg = nullptr;
        duckdb_state state = duckdb_open_ext(m_dbname.c_str(), &m_db, config, &error_msg);
        duckdb_destroy_config(&config);

        if (state != DuckDBSuccess) {
            std::string err_msg = error_msg ? error_msg : "Unknown error";
            if (error_msg) {
                free(error_msg);
            }
            HKU_THROW("Failed to open DuckDB database: {}", err_msg);
        }

        // 创建连接
        state = duckdb_connect(m_db, &m_connection);
        if (state != DuckDBSuccess) {
            duckdb_close(&m_db);
            HKU_THROW("Failed to connect to DuckDB database");
        }

        // 设置自动安装和加载已知扩展
        exec("SET autoinstall_known_extensions=1;");
        exec("SET autoload_known_extensions=1;");

        HKU_TRACE("DuckDB connection established successfully");
    } catch (const std::exception& e) {
        close();
        HKU_THROW("Failed to create DuckDB connection: {}", e.what());
    }
}

DuckDBConnect::~DuckDBConnect() {
    close();
}

void DuckDBConnect::close() {
    if (m_connection) {
        duckdb_disconnect(&m_connection);
        m_connection = nullptr;
    }
    
    if (m_db) {
        duckdb_close(&m_db);
        m_db = nullptr;
    }
}

int64_t DuckDBConnect::exec(const std::string &sql_string) {
#if HKU_SQL_TRACE
    HKU_DEBUG(sql_string);
#endif
    
    duckdb_result result;
    duckdb_state state = duckdb_query(m_connection, sql_string.c_str(), &result);
    
    if (state != DuckDBSuccess) {
        const char* error_msg = duckdb_result_error(&result);
        std::string msg = error_msg ? std::string(error_msg) : "Unknown error";
        duckdb_destroy_result(&result);
        SQL_THROW(-1, "SQL error: {}! ({})", msg, sql_string);
    }
    
    // 对于INSERT/UPDATE/DELETE语句，获取影响的行数
    idx_t affected_rows = 0;
    if (duckdb_column_count(&result) == 0) {
        // 这是一个修改语句，尝试获取影响行数
        // DuckDB C API没有直接的rows_changed函数，需要通过其他方式获取
        affected_rows = 1; // 简化处理
    }
    
    duckdb_destroy_result(&result);
    return static_cast<int64_t>(affected_rows);
}

void DuckDBConnect::transaction() {
    exec("BEGIN TRANSACTION");
}

void DuckDBConnect::commit() {
    exec("COMMIT");
}

void DuckDBConnect::rollback() noexcept {
    try {
        exec("ROLLBACK");
    } catch (const std::exception &e) {
        HKU_ERROR("Failed rollback! {}", e.what());
    } catch (...) {
        HKU_ERROR("Unknown error!");
    }
}

SQLStatementPtr DuckDBConnect::getStatement(const std::string &sql_statement) {
    return std::make_shared<DuckDBStatement>(this, sql_statement);
}

bool DuckDBConnect::tableExist(const std::string &tablename) {
    try {
        // 使用PRAGMA table_info检查表是否存在
        std::string query = fmt::format("PRAGMA table_info('{}')", tablename);
        
        duckdb_result result;
        duckdb_state state = duckdb_query(m_connection, query.c_str(), &result);
        
        if (state != DuckDBSuccess) {
            duckdb_destroy_result(&result);
            return false;
        }
        
        // 检查是否有结果行
        idx_t row_count = duckdb_row_count(&result);
        bool exists = (row_count > 0);
        duckdb_destroy_result(&result);
        
        return exists;
    } catch (...) {
        return false;
    }
}

void DuckDBConnect::resetAutoIncrement(const std::string &tablename) {
    try {
        // DuckDB中重置自增ID的方式
        std::string sql = fmt::format("ALTER SEQUENCE {}_id_seq RESTART WITH 1", tablename);
        exec(sql);
    } catch (...) {
        // 如果序列不存在，忽略错误
    }
}

bool DuckDBConnect::ping() {
    try {
        // 执行一个简单的查询来测试连接
        exec("SELECT 1");
        return true;
    } catch (...) {
        return false;
    }
}

//-------------------------------------------------------------------------
// DuckDB优化的批量操作实现
//-------------------------------------------------------------------------

template <typename Iterator>
void DuckDBConnect::optimizedBatchSaveImpl(Iterator first, Iterator last, bool autotrans) {
    if (first == last) return;
    
    if (autotrans) {
        transaction();
    }
    
    try {
        // 计算数据量
        size_t count = std::distance(first, last);
        
        if (count == 0) {
            if (autotrans) commit();
            return;
        }
        
        // 对于小批量数据(< 100)，使用传统的逐行插入
        if (count < 100) {
            using ValueType = typename std::iterator_traits<Iterator>::value_type;
            // 只有当ValueType是类类型时才调用save方法
            if constexpr (std::is_class_v<ValueType>) {
                SQLStatementPtr st = getStatement(ValueType::getInsertSQL());
                for (auto iter = first; iter != last; ++iter) {
                    iter->save(st);
                    st->exec();
                    iter->rowid(st->getLastRowid());
                }
            }
        } else {
            // 对于大批量数据，使用批量插入优化
            std::vector<typename std::iterator_traits<Iterator>::value_type> items(first, last);
            performBulkInsert(items);
        }
        
        if (autotrans) {
            commit();
        }
    } catch (const std::exception& e) {
        if (autotrans) {
            rollback();
        }
        HKU_THROW("Failed to optimized batch save: {}", e.what());
    }
}

template <typename Iterator>
void DuckDBConnect::optimizedBatchUpdateImpl(Iterator first, Iterator last, bool autotrans) {
    if (first == last) return;
    
    if (autotrans) {
        transaction();
    }
    
    try {
        // 计算数据量
        size_t count = std::distance(first, last);
        
        if (count == 0) {
            if (autotrans) commit();
            return;
        }
        
        // 对于小批量数据(< 100)，使用传统的逐行更新
        if (count < 100) {
            using ValueType = typename std::iterator_traits<Iterator>::value_type;
            // 只有当ValueType是类类型时才调用update方法
            if constexpr (std::is_class_v<ValueType>) {
                SQLStatementPtr st = getStatement(ValueType::getUpdateSQL());
                for (auto iter = first; iter != last; ++iter) {
                    iter->update(st);
                    st->exec();
                }
            }
        } else {
            // 对于大批量数据，使用批量更新优化
            std::vector<typename std::iterator_traits<Iterator>::value_type> items(first, last);
            performBulkUpdate(items);
        }
        
        if (autotrans) {
            commit();
        }
    } catch (const std::exception& e) {
        if (autotrans) {
            rollback();
        }
        HKU_THROW("Failed to optimized batch update: {}", e.what());
    }
}

template <typename Container>
void DuckDBConnect::performBulkInsert(const Container& items) {
    // TODO: 实现真正的DuckDB批量插入优化
    // 这里可以使用:
    // 1. DuckDB的COPY FROM命令
    // 2. DuckDB的批量APPEND操作
    // 3. DuckDB的Arrow集成进行零拷贝批量插入
    
    HKU_INFO("DuckDB bulk insert optimization not yet implemented, using standard approach");
    
    // 临时实现：使用标准的批量保存
    using ValueType = typename Container::value_type;
    if constexpr (std::is_class_v<ValueType>) {
        SQLStatementPtr st = getStatement(ValueType::getInsertSQL());
        for (const auto& item : items) {
            item.save(st);
            st->exec();
            const_cast<ValueType&>(item).rowid(st->getLastRowid());
        }
    }
}

template <typename Container>
void DuckDBConnect::performBulkUpdate(const Container& items) {
    // TODO: 实现真正的DuckDB批量更新优化
    
    HKU_INFO("DuckDB bulk update optimization not yet implemented, using standard approach");
    
    // 临时实现：使用标准的批量更新
    using ValueType = typename Container::value_type;
    if constexpr (std::is_class_v<ValueType>) {
        SQLStatementPtr st = getStatement(ValueType::getUpdateSQL());
        for (const auto& item : items) {
            item.update(st);
            st->exec();
        }
    }
}

// 显式实例化模板方法
template void DuckDBConnect::optimizedBatchSaveImpl<std::vector<int>::iterator>(
    std::vector<int>::iterator, std::vector<int>::iterator, bool);
template void DuckDBConnect::optimizedBatchUpdateImpl<std::vector<int>::iterator>(
    std::vector<int>::iterator, std::vector<int>::iterator, bool);

} /* namespace hku */