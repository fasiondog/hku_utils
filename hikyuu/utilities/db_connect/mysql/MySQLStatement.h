/*
 * MySQLStatement.h
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2019-8-17
 *      Author: fasiondog
 */

#pragma once
#ifndef HIYUU_DB_CONNECT_MYSQL_MYSQLSTATEMENT_H
#define HIYUU_DB_CONNECT_MYSQL_MYSQLSTATEMENT_H

#include <string>
#include <vector>
#include <boost/any.hpp>
#include "../SQLStatementBase.h"

#include <boost/mysql.hpp>

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

namespace hku {

class HKU_UTILS_API MySQLStatement : public SQLStatementBase {
public:
    MySQLStatement() = delete;
    MySQLStatement(DBConnectBase *driver, const std::string &sql_statement);
    virtual ~MySQLStatement() override;

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

private:
    boost::mysql::tcp_connection *m_conn{nullptr};
    boost::mysql::statement m_stmt;
    boost::mysql::results m_results;
    std::vector<boost::mysql::field> m_params;
    size_t m_current_row{0};
    bool m_has_result{false};
    bool m_needs_reset{false};
};

inline uint64_t MySQLStatement::sub_getLastRowid() {
    return m_results.last_insert_id();
}

}  // namespace hku

#endif /* HIYUU_DB_CONNECT_MYSQL_MYSQLSTATEMENT_H */