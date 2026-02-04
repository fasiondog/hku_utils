/*
 * DuckDBStatement.h
 *
 *  Copyright (c) 2024, hikyuu.org
 *
 *  Created on: 2024-01-01
 *      Author: fasiondog
 */
#pragma once
#ifndef HIKYUU_DB_CONNECT_DUCKDB_DUCKDBSTATEMENT_H
#define HIKYUU_DB_CONNECT_DUCKDB_DUCKDBSTATEMENT_H

#include <duckdb.h>
#include "../SQLStatementBase.h"

namespace hku {

class DuckDBConnect;

/**
 * DuckDB Statement
 * @ingroup DBConnect
 */
class HKU_UTILS_API DuckDBStatement : public SQLStatementBase {
public:
    DuckDBStatement() = delete;

    /**
     * 构造函数
     * @param driver 数据库连接
     * @param sql_statement SQL语句
     */
    DuckDBStatement(DBConnectBase *driver, const std::string &sql_statement);

    /** 析构函数 */
    virtual ~DuckDBStatement() override;

    virtual void sub_exec() override;
    virtual bool sub_moveNext() override;
    virtual uint64_t sub_getLastRowid() override;

    virtual void sub_bindNull(int idx) override;
    virtual void sub_bindInt(int idx, int64_t value) override;
    virtual void sub_bindDouble(int idx, double item) override;
    virtual void sub_bindDatetime(int idx, const Datetime &item) override;
    virtual void sub_bindText(int idx, const std::string &item) override;
    virtual void sub_bindText(int idx, const char *item, size_t len) override;
    virtual void sub_bindBlob(int idx, const std::string &item) override;
    virtual void sub_bindBlob(int idx, const std::vector<char> &item) override;

    virtual int sub_getNumColumns() const override;
    virtual void sub_getColumnAsInt64(int idx, int64_t &item) override;
    virtual void sub_getColumnAsDouble(int idx, double &item) override;
    virtual void sub_getColumnAsDatetime(int idx, Datetime &item) override;
    virtual void sub_getColumnAsText(int idx, std::string &item) override;
    virtual void sub_getColumnAsBlob(int idx, std::string &item) override;
    virtual void sub_getColumnAsBlob(int idx, std::vector<char> &item) override;

private:
    void _prepare();
    void _reset();
    std::string _prepareInsertWithReturning(const std::string &sql);

private:
    duckdb_connection m_connection;
    duckdb_prepared_statement m_stmt;
    duckdb_result m_result;
    bool m_has_result;
    idx_t m_current_row;
    idx_t m_row_count;
};

}  // namespace hku

#endif /* HIKYUU_DB_CONNECT_DUCKDB_DUCKDBSTATEMENT_H */