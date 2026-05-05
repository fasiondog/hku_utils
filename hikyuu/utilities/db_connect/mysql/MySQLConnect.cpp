/*
 * MySQLConnect.cpp
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2019-8-17
 *      Author: fasiondog
 */

#include "hikyuu/utilities/config.h"
#include "MySQLConnect.h"

namespace hku {

MySQLConnect::MySQLConnect(const Parameter& param) : DBConnectBase(param) {
    connect();
}

MySQLConnect::~MySQLConnect() {
    close();
}

bool MySQLConnect::tryConnect() noexcept {
    bool success = false;
    try {
        close();
        connect();
        success = true;
    } catch (const std::exception& e) {
        HKU_WARN(e.what());
    }
    return success;
}

void MySQLConnect::connect() {
    try {
        std::string host = tryGetParam<std::string>("host", "127.0.0.1");
        std::string usr = tryGetParam<std::string>("usr", "root");
        std::string pwd = tryGetParam<std::string>("pwd", "");
        std::string database = tryGetParam<std::string>("db", "");
        unsigned short port = static_cast<unsigned short>(tryGetParam<int>("port", 3306));

        // 创建 IO context 和连接
        m_conn = std::make_unique<boost::mysql::tcp_connection>(m_io_context);
        
        // 设置连接参数
        boost::mysql::handshake_params params(
            usr,           // username
            pwd,           // password
            database       // database name
        );
        
        // 连接到 MySQL 服务器
        boost::mysql::error_code ec;
        boost::mysql::diagnostics diag;
        m_conn->connect(
            boost::asio::ip::tcp::endpoint(
                boost::asio::ip::make_address(host), 
                port
            ),
            params,
            ec,
            diag
        );
        
        if (ec) {
            HKU_THROW("Failed to connect to MySQL database! Error: {}", ec.message());
        }
        
        // 设置字符集为 utf8
        m_conn->execute("SET NAMES utf8", m_results, ec, diag);
        if (ec) {
            HKU_THROW("Failed to set character set to utf8! Error: {}", ec.message());
        }

    } catch (const hku::exception& e) {
        close();
        HKU_ERROR(e.what());
        HKU_THROW("Failed create MySQLConnect! {}", e.what());

    } catch (const std::exception& e) {
        close();
        HKU_ERROR(e.what());
        HKU_THROW("Failed create MySQLConnent instance! {}", e.what());

    } catch (...) {
        close();
        const char* errmsg = "Failed create MySQLConnect instance! Unknown error";
        HKU_ERROR(errmsg);
        HKU_THROW("{}", errmsg);
    }
}

void MySQLConnect::close() {
    if (m_conn) {
        m_conn->close();
        m_conn.reset();
    }
}

bool MySQLConnect::ping() {
    if (!m_conn) {
        return tryConnect();
    }
    
    try {
        boost::mysql::error_code ec;
        boost::mysql::diagnostics diag;
        m_conn->execute("SELECT 1", m_results, ec, diag);
        if (ec) {
            // ping 失败，尝试重连
            return tryConnect();
        }
        return true;
    } catch (...) {
        // 异常时也尝试重连
        return tryConnect();
    }
}

int64_t MySQLConnect::exec(const std::string& sql_string) {
#if HKU_SQL_TRACE
    HKU_DEBUG(sql_string);
#endif
    
    if (!m_conn) {
        HKU_CHECK(tryConnect(), "Failed connect to mysql!");
    }

    boost::mysql::error_code ec;
    boost::mysql::diagnostics diag;
    m_conn->execute(sql_string, m_results, ec, diag);
    
    if (ec) {
        // 执行失败，尝试重连后再次执行
        if (ping()) {
            m_conn->execute(sql_string, m_results, ec, diag);
        }
        
        if (ec) {
            SQL_THROW(ec.value(), "SQL error: {}! error msg: {}", sql_string, ec.message());
        }
    }

    // 获取受影响的行数
    int64_t affect_rows = m_results.affected_rows();
    
    return affect_rows;
}

SQLStatementPtr MySQLConnect::getStatement(const std::string& sql_statement) {
    return std::make_shared<MySQLStatement>(this, sql_statement);
}

bool MySQLConnect::tableExist(const std::string& tablename) {
    bool result = false;
    try {
        SQLStatementPtr st = getStatement(fmt::format("SELECT 1 FROM {} LIMIT 1;", tablename));
        st->exec();
        result = true;
    } catch (...) {
        result = false;
    }
    return result;
}

void MySQLConnect::resetAutoIncrement(const std::string& tablename) {
    int64_t count = queryNumber<int64_t>(fmt::format("select count(1) from {}", tablename));
    HKU_CHECK(count == 0, "The ID cannot be reset when data is present in table({})", tablename);
    exec(fmt::format("alter {} auto_increment=1", tablename));
}

void MySQLConnect::transaction() {
    exec("BEGIN");
}

void MySQLConnect::commit() {
    exec("COMMIT");
}

void MySQLConnect::rollback() noexcept {
    try {
        exec("ROLLBACK");
    } catch (const std::exception& e) {
        HKU_ERROR("Failed transaction! {}", e.what());
    } catch (...) {
        HKU_ERROR("Unknown error!");
    }
}

}  // namespace hku