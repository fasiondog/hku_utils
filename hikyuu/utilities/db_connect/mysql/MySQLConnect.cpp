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

#include <memory>
#include <thread>
#include <future>
#include <boost/mysql.hpp>
#include <boost/asio.hpp>
#include "hikyuu/utilities/LruCache.h"

namespace hku {

// 辅助函数：打印 diagnostics 诊断信息
static void printDiagHelper(const boost::mysql::error_code& ec,
                            const boost::mysql::diagnostics& diag, const std::string& context) {
    if (!diag.server_message().empty()) {
        HKU_ERROR("{} Server error: {}", context, diag.server_message());
    } else if (!diag.client_message().empty()) {
        HKU_ERROR("{} Client error: {}", context, diag.client_message());
    } else {
        HKU_ERROR("{} Error code {}: {}", context, ec.value(), ec.message());
    }
}

// Pimpl 实现结构体
struct MySQLConnect::Impl {
    boost::asio::io_context io_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard;
    std::thread io_thread;  // 后台线程运行 io_context
    std::unique_ptr<boost::mysql::tcp_connection> conn;
    LruCache<std::string, std::shared_ptr<boost::mysql::statement>> statement_cache{15};

    Impl()
    : work_guard(boost::asio::make_work_guard(io_context)),
      io_thread([this]() { io_context.run(); }) {}

    ~Impl() {
        work_guard.reset();  // 允许 io_context 停止
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }

    std::shared_ptr<boost::mysql::statement> get_statement(const std::string& sql,
                                                           boost::mysql::error_code& ec,
                                                           boost::mysql::diagnostics& diag) {
        std::shared_ptr<boost::mysql::statement> ret;
        if (statement_cache.tryGet(sql, ret)) {
            return ret;
        }

        // 创建 statement 并准备关闭的 lambda
        auto* connection_ptr = conn.get();
        auto deleter = [connection_ptr](boost::mysql::statement* stmt) {
            if (stmt && connection_ptr) {
                connection_ptr->close_statement(*stmt);
                // 忽略关闭时的错误，因为连接可能已经断开
            }
            delete stmt;
        };

        // 使用 async_prepare_statement + use_future 实现同步等待
        boost::mysql::diagnostics prep_diag;
        auto prepare_future =
          conn->async_prepare_statement(sql, prep_diag, boost::asio::use_future);

        try {
            // 等待准备完成，成功时返回 statement
            boost::mysql::statement stmt = prepare_future.get();

            ret = std::shared_ptr<boost::mysql::statement>(
              new boost::mysql::statement(std::move(stmt)), deleter);
            // statement_cache.insert(sql, ret);  // 临时禁用缓存
        } catch (const boost::mysql::error_with_diagnostics& e) {
            ec = e.code();
            diag = e.get_diagnostics();
            ret.reset();
        } catch (const boost::system::system_error& e) {
            ec = e.code();
            ret.reset();
        }

        return ret;
    }
};

MySQLConnect::MySQLConnect(const Parameter& param)
: DBConnectBase(param), m_impl(std::make_unique<Impl>()) {
    connect();
}

MySQLConnect::~MySQLConnect() {
    close();
}

void* MySQLConnect::getRawConnection() const noexcept {
    return m_impl->conn.get();
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

        m_impl->conn = std::make_unique<boost::mysql::tcp_connection>(m_impl->io_context);
        boost::mysql::handshake_params params(usr, pwd, database);

        // 使用 async_connect + use_future 实现同步等待
        boost::mysql::diagnostics diag;

        auto connect_future = m_impl->conn->async_connect(
          boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(host), port), params, diag,
          boost::asio::use_future);

        // 等待连接完成（后台线程已经在运行 io_context）
        try {
            connect_future.get();  // 成功时返回 void，失败时抛出异常
        } catch (const boost::mysql::error_with_diagnostics& e) {
            // 包含 diagnostics 的错误
            printDiagHelper(e.code(), e.get_diagnostics(), "MySQL connect");
            HKU_THROW("{}, {}", e.code().value(), e.code().message());
        } catch (const boost::system::system_error& e) {
            // 普通系统错误
            HKU_THROW("MySQL connect failed: {}, {}", e.code().value(), e.what());
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
    if (m_impl && m_impl->conn) {
        m_impl->statement_cache.clear();
        m_impl->conn->close();
        m_impl->conn.reset();
    }
}

bool MySQLConnect::ping() {
    HKU_ERROR_IF_RETURN((!m_impl || !m_impl->conn) && !tryConnect(), false,
                        "Failed connect to mysql!");

    try {
        // 使用 async_execute + use_future 实现同步等待
        boost::mysql::results results;
        boost::mysql::diagnostics diag;

        auto ping_future =
          m_impl->conn->async_execute("SELECT 1", results, diag, boost::asio::use_future);

        // 等待执行完成
        try {
            ping_future.get();  // 成功时返回 void
        } catch (const boost::mysql::error_with_diagnostics& e) {
            // 如果 ping 失败，尝试重连
            if (!tryConnect()) [[unlikely]] {
                printDiagHelper(e.code(), e.get_diagnostics(), "MySQL ping failed!");
                return false;
            }
            return true;
        } catch (const boost::system::system_error& e) {
            // 异常时也尝试重连
            HKU_ERROR_IF_RETURN(!tryConnect(), false, "MySQL ping exception! {}", e.what());
            return true;
        }

        return true;
    } catch (const std::exception& e) {
        // 其他异常时也尝试重连
        HKU_ERROR_IF_RETURN(!tryConnect(), false, "MySQL ping exception! {}", e.what());
        return true;
    }
}

int64_t MySQLConnect::exec(const std::string& sql_string) {
#if HKU_SQL_TRACE
    HKU_DEBUG(sql_string);
#endif

    if (!m_impl || !m_impl->conn) {
        SQL_CHECK(tryConnect(), -1, "Failed connect to mysql!");
    }

    // 使用 async_execute + use_future 实现同步等待
    boost::mysql::results results;
    boost::mysql::diagnostics diag;

    auto exec_future =
      m_impl->conn->async_execute(sql_string, results, diag, boost::asio::use_future);

    // 等待执行完成
    try {
        exec_future.get();  // 成功时返回 void
    } catch (const boost::mysql::error_with_diagnostics& e) {
        boost::mysql::error_code ec = e.code();
        boost::mysql::diagnostics err_diag = e.get_diagnostics();

        // 执行失败,尝试重连后再次执行
        if (ping()) {
            // 重新执行
            auto retry_future =
              m_impl->conn->async_execute(sql_string, results, diag, boost::asio::use_future);
            try {
                retry_future.get();
            } catch (const boost::mysql::error_with_diagnostics& retry_e) {
                ec = retry_e.code();
                err_diag = retry_e.get_diagnostics();
            } catch (const boost::system::system_error& retry_e) {
                ec = retry_e.code();
            }
        }

        if (ec) {
            printDiagHelper(ec, err_diag, "MySQL execute sql");
            SQL_THROW(ec.value(), "SQL error: {}! error msg: {}", sql_string, ec.message());
        }
    } catch (const boost::system::system_error& e) {
        boost::mysql::error_code ec = e.code();
        printDiagHelper(ec, diag, "MySQL execute sql");
        SQL_THROW(ec.value(), "SQL error: {}! error msg: {}", sql_string, ec.message());
    }

    // 获取受影响的行数
    return results.affected_rows();
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
    SQL_CHECK(count == 0, -1, "The ID cannot be reset when data is present in table({})",
              tablename);
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