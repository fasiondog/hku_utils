/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-06-04
 *      Author: fasiondog
 */

#pragma once
#ifndef HIYUU_DB_CONNECT_TDENGINE_STATEMENT_H
#define HIYUU_DB_CONNECT_TDENGINE_STATEMENT_H

#include <string>
#include <vector>
#include <boost/any.hpp>
#include "../SQLStatementBase.h"

#include <taos.h>

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

namespace hku {

class HKU_UTILS_API TDengineStatement : public SQLStatementBase {
public:
    TDengineStatement() = delete;
    TDengineStatement(DBConnectBase *driver, const std::string &sql_statement);
    virtual ~TDengineStatement();

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
    void _prepare(DBConnectBase *driver);

private:
    TAOS *m_taos{nullptr};
    TAOS_STMT *m_stmt{nullptr};

    std::vector<TAOS_MULTI_BIND> m_param_bind;

    // private:
    //     void _prepare(DBConnectBase *driver);
    //     void _reset();
    //     void _bindResult();

    // private:
    //     MYSQL *m_db;
    //     MYSQL_STMT *m_stmt;
    //     MYSQL_RES *m_meta_result;
    //     bool m_needs_reset;
    //     bool m_has_bind_result;
    //     std::vector<MYSQL_BIND> m_param_bind;
    //     std::vector<MYSQL_BIND> m_result_bind;
    //     std::vector<boost::any> m_param_buffer;
    //     std::vector<boost::any> m_result_buffer;
    //     std::vector<unsigned long> m_result_length;
    //     std::vector<char> m_result_is_null;
    //     std::vector<char> m_result_error;
};

}  // namespace hku

#endif /* HIYUU_DB_CONNECT_TDENGINE_STATEMENT_H */