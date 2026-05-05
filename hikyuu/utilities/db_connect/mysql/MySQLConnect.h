/*
 * MySQLConnect.h
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2019-8-17
 *      Author: fasiondog
 */

#pragma once
#ifndef HIYUU_DB_CONNECT_MYSQL_MYSQLCONNECT_H
#define HIYUU_DB_CONNECT_MYSQL_MYSQLCONNECT_H

#include "../DBConnectBase.h"
#include "MySQLStatement.h"

#include <boost/mysql.hpp>
#include <boost/asio.hpp>

namespace hku {

class HKU_UTILS_API MySQLConnect : public DBConnectBase {
public:
    explicit MySQLConnect(const Parameter &param);
    virtual ~MySQLConnect() override;

    MySQLConnect(const MySQLConnect &) = delete;
    MySQLConnect &operator=(const MySQLConnect &) = delete;

    virtual bool ping() override;

    virtual int64_t exec(const std::string &sql_string) override;
    virtual SQLStatementPtr getStatement(const std::string &sql_statement) override;
    virtual bool tableExist(const std::string &tablename) override;
    virtual void resetAutoIncrement(const std::string &tablename) override;

    virtual void transaction() override;
    virtual void commit() override;
    virtual void rollback() noexcept override;

public:
    boost::mysql::tcp_connection* getRawConnection() const noexcept {
        return m_conn.get();
    }
    
    boost::asio::io_context& getIoContext() noexcept {
        return m_io_context;
    }

private:
    bool tryConnect() noexcept;
    void connect();
    void close();

private:
    boost::asio::io_context m_io_context;
    std::unique_ptr<boost::mysql::tcp_connection> m_conn;
    boost::mysql::results m_results;
};

}  // namespace hku

#endif /* HIYUU_DB_CONNECT_MYSQL_MYSQLCONNECT_H */