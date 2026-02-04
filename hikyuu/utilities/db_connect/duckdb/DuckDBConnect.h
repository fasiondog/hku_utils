/*
 * DuckDBConnect.h
 *
 *  Copyright (c) 2024, hikyuu.org
 *
 *  Created on: 2024-01-01
 *      Author: fasiondog
 */
#pragma once
#ifndef HIKYUU_DB_CONNECT_DUCKDB_DUCKDBCONNECT_H
#define HIKYUU_DB_CONNECT_DUCKDB_DUCKDBCONNECT_H

#include <duckdb.h>
#include "../DBConnectBase.h"
#include "DuckDBStatement.h"

namespace hku {

/**
 * @defgroup DuckDB DuckDB driver DuckDB 数据驱动
 * @ingroup DBConnect
 */

/**
 * DuckDB连接
 * @ingroup DuckDB
 */
class HKU_UTILS_API DuckDBConnect : public DBConnectBase {
public:
    /**
     * 构造函数
     * @param param 数据库连接参数,支持如下参数：
     * <pre>
     * string db - 数据库文件名
     * string access_mode - 访问模式："READ_WRITE"(默认) 或 "READ_ONLY"
     * </pre>
     */
    explicit DuckDBConnect(const Parameter& param);

    /** 析构函数 */
    virtual ~DuckDBConnect() override;

    /** 如果数据库连接有效，返回true */
    virtual bool ping() override;

    /** 开始事务，失败时抛出异常 */
    virtual void transaction() override;

    /** 提交事务，失败时抛出异常 */
    virtual void commit() override;

    /** 回滚事务 */
    virtual void rollback() noexcept override;

    /**
     * 执行无返回结果的 SQL
     * @note duckdb 无法返回 INSERT/UPDATE/DELETE 语句的受影响函数，这里成功返回1
     */
    virtual int64_t exec(const std::string& sql_string) override;

    /** 获取 SQLStatement */
    virtual SQLStatementPtr getStatement(const std::string& sql_statement) override;

    /** 判断表是否存在 */
    virtual bool tableExist(const std::string& tablename) override;

    /**
     * 重置含自增 id 的表中的 id 从 1开始
     * @param tablename 待重置id的表名
     * @exception 表中仍旧含有数据时，抛出异常
     */
    virtual void resetAutoIncrement(const std::string& tablename) override;

    //-------------------------------------------------------------------------
    // DuckDB特化的批量操作方法
    //-------------------------------------------------------------------------

    /**
     * DuckDB优化的批量保存 - 根据数据量选择合适的策略
     * @param container 支持迭代器的容器
     * @param autotrans 是否自动管理事务
     */
    template <typename Container>
    void optimizedBatchSave(Container& container, bool autotrans = true) {
        optimizedBatchSaveImpl(container.begin(), container.end(), autotrans);
    }

    /**
     * DuckDB优化的批量更新 - 根据数据量选择合适的策略
     * @param container 支持迭代器的容器
     * @param autotrans 是否自动管理事务
     */
    template <typename Container>
    void optimizedBatchUpdate(Container& container, bool autotrans = true) {
        optimizedBatchUpdateImpl(container.begin(), container.end(), autotrans);
    }

    /**
     * DuckDB优化的批量加载 - 利用列式特性提高性能
     * @param container 支持push_back的容器
     * @param where 查询条件
     */
    template <typename Container>
    void optimizedBatchLoad(Container& container, const std::string& where = "");

    /**
     * DuckDB优化的批量加载 - 利用列式特性提高性能
     * @param container 支持push_back的容器
     * @param cond 查询条件
     */
    template <typename Container>
    void optimizedBatchLoad(Container& container, const DBCondition& cond);

    /**
     * DuckDB优化的批量加载 - 利用列式特性提高性能
     * @param container 支持push_back的容器
     * @param sql 查询语句
     */
    template <typename Container>
    void optimizedBatchLoadView(Container& container, const std::string& sql);

private:
    // 私有辅助方法
    template <typename Iterator>
    void optimizedBatchSaveImpl(Iterator first, Iterator last, bool autotrans);

    template <typename Iterator>
    void optimizedBatchUpdateImpl(Iterator first, Iterator last, bool autotrans);

    template <typename Container>
    void performBulkInsert(const Container& items);

    template <typename Container>
    void performBulkUpdate(const Container& items);

    void close();

private:
    friend class DuckDBStatement;
    std::string m_dbname;            // 数据库文件名
    duckdb_database m_db;            // 数据库句柄
    duckdb_connection m_connection;  // 连接句柄
};

typedef std::shared_ptr<DuckDBConnect> DuckDBConnectPtr;

}  // namespace hku

#endif /* HIKYUU_DB_CONNECT_DUCKDB_DUCKDBCONNECT_H */