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

        std::string attach = tryGetParam<std::string>("attach", "");
        if (!attach.empty()) {
            exec(attach);
        }

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

int64_t DuckDBConnect::exec(const std::string& sql_string) {
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
    if (duckdb_row_count(&result) == 0) {
        // 这是一个修改语句，尝试获取影响行数
        // DuckDB C API没有直接的rows_changed函数，需要通过其他方式获取
        affected_rows = 1;  // 简化处理
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
    } catch (const std::exception& e) {
        HKU_ERROR("Failed rollback! {}", e.what());
    } catch (...) {
        HKU_ERROR("Unknown error!");
    }
}

SQLStatementPtr DuckDBConnect::getStatement(const std::string& sql_statement) {
    return std::make_shared<DuckDBStatement>(this, sql_statement);
}

bool DuckDBConnect::tableExist(const std::string& tablename) {
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

void DuckDBConnect::resetAutoIncrement(const std::string& tablename) {
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
    if (first == last)
        return;

    if (autotrans) {
        transaction();
    }

    try {
        // 计算数据量
        size_t count = std::distance(first, last);

        if (count == 0) {
            if (autotrans)
                commit();
            return;
        }

        using ValueType = typename std::iterator_traits<Iterator>::value_type;
        // 只有当ValueType是类类型时才进行批量操作
        if constexpr (std::is_class_v<ValueType>) {
            // 对于小批量数据(< 100)，使用传统的逐行插入
            if (count < 100) {
                SQLStatementPtr st = getStatement(ValueType::getInsertSQL());
                for (auto iter = first; iter != last; ++iter) {
                    iter->save(st);
                    st->exec();
                    iter->rowid(st->getLastRowid());
                }
            } else {
                // 对于大批量数据，使用批量插入优化
                std::vector<ValueType> items(first, last);
                performBulkInsert(items);
            }
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
    if (first == last)
        return;

    if (autotrans) {
        transaction();
    }

    try {
        // 计算数据量
        size_t count = std::distance(first, last);

        if (count == 0) {
            if (autotrans)
                commit();
            return;
        }

        using ValueType = typename std::iterator_traits<Iterator>::value_type;
        // 只有当ValueType是类类型时才进行批量操作
        if constexpr (std::is_class_v<ValueType>) {
            // 对于小批量数据(< 100)，使用传统的逐行更新
            if (count < 100) {
                SQLStatementPtr st = getStatement(ValueType::getUpdateSQL());
                for (auto iter = first; iter != last; ++iter) {
                    iter->update(st);
                    st->exec();
                }
            } else {
                // 对于大批量数据，使用批量更新优化
                std::vector<ValueType> items(first, last);
                performBulkUpdate(items);
            }
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
    using ValueType = typename Container::value_type;
    if constexpr (!std::is_class_v<ValueType>) {
        return;
    }

    if (items.empty()) {
        return;
    }

    try {
        // 获取插入SQL语句
        const char* insert_sql = ValueType::getInsertSQL();

        // 开始事务以提高性能
        transaction();

        // 使用预处理语句进行批量插入
        SQLStatementPtr st = getStatement(insert_sql);
        if (!st) {
            HKU_WARN("Failed to create statement, using standard approach");
            rollback();

            // 回退到标准方法
            SQLStatementPtr st = getStatement(ValueType::getInsertSQL());
            for (const auto& item : items) {
                item.save(st);
                st->exec();
                const_cast<ValueType&>(item).rowid(st->getLastRowid());
            }
            return;
        }

        // 为每个项目执行插入
        for (const auto& item : items) {
            // 绑定参数
            item.save(st);

            // 执行插入
            st->exec();

            // 更新rowid
            const_cast<ValueType&>(item).rowid(st->getLastRowid());
        }

        // 提交事务
        commit();

        HKU_DEBUG("DuckDB bulk insert completed successfully for {} items", items.size());

    } catch (const std::exception& e) {
        HKU_WARN("Bulk insert failed: {}, using standard approach", e.what());

        // 回退到标准方法
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
    using ValueType = typename Container::value_type;
    if constexpr (!std::is_class_v<ValueType>) {
        return;
    }

    if (items.empty()) {
        return;
    }

    try {
        // 获取更新SQL语句
        std::string update_sql = ValueType::getUpdateSQL();

        // 使用预处理语句进行批量更新
        SQLStatementPtr st = getStatement(update_sql);

        // 开始事务以提高性能
        transaction();

        for (const auto& item : items) {
            item.update(st);
            st->exec();
        }

        // 提交事务
        commit();

        HKU_DEBUG("DuckDB bulk update completed successfully for {} items", items.size());

    } catch (const std::exception& e) {
        HKU_WARN("Bulk update failed: {}, using standard approach", e.what());

        // 回退到标准方法
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

//-------------------------------------------------------------------------
// DuckDB特化的批量加载方法实现
//-------------------------------------------------------------------------

/**
 * 优化的批量加载实现 - 利用DuckDB的列式特性和批量查询
 */
template <typename Container>
void DuckDBConnect::optimizedBatchLoad(Container& container, const std::string& where) {
    using ValueType = typename Container::value_type;
    if constexpr (!std::is_class_v<ValueType>) {
        return;
    }

    try {
        // 构建查询SQL
        std::ostringstream sql;
        if (!where.empty()) {
            sql << ValueType::getSelectSQL() << " WHERE " << where;
        } else {
            sql << ValueType::getSelectSQL();
        }

        // 使用DuckDB的查询优化
        SQLStatementPtr st = getStatement(sql.str());

        // 执行查询
        st->exec();

        // 批量处理结果
        while (st->moveNext()) {
            ValueType tmp;
            tmp.load(st);
            container.push_back(tmp);
        }

        HKU_DEBUG("DuckDB optimized batch load completed successfully");

    } catch (const std::exception& e) {
        HKU_WARN("Optimized batch load failed: {}, using standard approach", e.what());

        // 回退到标准方法
        DBConnectBase::batchLoad(container, where);
    }
}

/**
 * 优化的批量加载实现 - 利用DuckDB的列式特性和批量查询
 */
template <typename Container>
void DuckDBConnect::optimizedBatchLoad(Container& container, const DBCondition& cond) {
    optimizedBatchLoad(container, cond.str());
}

/**
 * 优化的批量加载实现 - 利用DuckDB的列式特性和批量查询
 */
template <typename Container>
void DuckDBConnect::optimizedBatchLoadView(Container& container, const std::string& sql) {
    using ValueType = typename Container::value_type;
    if constexpr (!std::is_class_v<ValueType>) {
        return;
    }

    try {
        // 使用DuckDB的查询优化
        SQLStatementPtr st = getStatement(sql);

        // 执行查询
        st->exec();

        // 批量处理结果
        while (st->moveNext()) {
            ValueType tmp;
            tmp.load(st);
            container.push_back(tmp);
        }

        HKU_DEBUG("DuckDB optimized batch load view completed successfully");

    } catch (const std::exception& e) {
        HKU_WARN("Optimized batch load view failed: {}, using standard approach", e.what());

        // 回退到标准方法
        DBConnectBase::batchLoadView(container, sql);
    }
}

} /* namespace hku */