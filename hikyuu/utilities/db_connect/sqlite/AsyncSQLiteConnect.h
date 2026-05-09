/*
 * AsyncSQLiteConnect.h
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2026-05-09
 *      Author: fasiondog
 */
#pragma once
#ifndef HIYUU_DB_CONNECT_SQLITE_ASYNCSQLITECONNECT_H
#define HIYUU_DB_CONNECT_SQLITE_ASYNCSQLITECONNECT_H

#include "../AsyncDBConnectBase.h"
#include "AsyncSQLiteStatement.h"

#include <memory>

namespace hku {

/**
 * SQLite 异步连接
 * @ingroup SQLite
 * 
 * 基于 SQLite3 的异步数据库连接实现。
 * 每个连接内部持有独立的 ThreadPool(1) 单线程池，通过 co_run 将同步操作转换为异步接口。
 */
class HKU_UTILS_API AsyncSQLiteConnect : public AsyncDBConnectBase {
public:
    explicit AsyncSQLiteConnect(const Parameter &param);
    virtual ~AsyncSQLiteConnect() override;

    AsyncSQLiteConnect(const AsyncSQLiteConnect &) = delete;
    AsyncSQLiteConnect &operator=(const AsyncSQLiteConnect &) = delete;

    virtual net::awaitable<bool> ping() override;
    virtual net::awaitable<int64_t> exec(const std::string &sql_string) override;
    virtual net::awaitable<AsyncSQLStatementPtr> getStatement(
      const std::string &sql_statement) override;
    virtual net::awaitable<bool> tableExist(const std::string &tablename) override;
    virtual net::awaitable<void> resetAutoIncrement(const std::string &tablename) override;

    virtual net::awaitable<void> transaction() override;
    virtual net::awaitable<void> commit() override;
    virtual net::awaitable<void> rollback() noexcept override;

private:
    friend class AsyncSQLiteStatement;

    // 提供给 AsyncSQLiteStatement 访问原始连接的方法
    void *getRawConnection() const noexcept;

    // 内部辅助方法
    net::awaitable<void> connect();
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace hku

#endif /* HIYUU_DB_CONNECT_SQLITE_ASYNCSQLITECONNECT_H */
