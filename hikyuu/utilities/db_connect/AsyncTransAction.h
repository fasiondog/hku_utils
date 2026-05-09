/*
 * AsyncTransAction.h
 *
 *  Copyright (c) 2026, hikyuu.org
 *
 *  Created on: 2026-05-09
 *      Author: fasiondog
 */
#pragma once
#ifndef HIKYUU_DB_CONNECT_ASYNC_TRANSACTION_H
#define HIKYUU_DB_CONNECT_ASYNC_TRANSACTION_H

#include <memory>
#include <boost/asio.hpp>
#include "AsyncDBConnectBase.h"

namespace hku {

/**
 * 异步自动事务处理，在代码块中自动启动事务，并在代码块退出后自动提交
 * @note 当有多个数据更改时，如果在程序处理中间发送异常时，可能导致数据被部分提交
 * @details commit() 失败时会自动回滚，析构函数中如果未提交则自动回滚（使用 detached 协程）
 * @ingroup DBConnect
 */
class AsyncAutoTransAction {
public:
    /**
     * 工厂方法：创建实例并自动启动事务
     * @param driver 数据库连接指针
     * @return 异步事务对象
     */
    static net::awaitable<std::shared_ptr<AsyncAutoTransAction>> create(
      const AsyncDBConnectPtr& driver);

    /** 获取数据库连接 */
    const AsyncDBConnectPtr& connect() const {
        return m_driver;
    }

    /**
     * 提交事务
     * @note 如果提交失败，会自动尝试回滚，然后重新抛出原始异常
     */
    net::awaitable<void> commit();

    /** 析构函数：如果未提交则自动回滚 */
    ~AsyncAutoTransAction();

private:
    /** 私有构造函数 */
    explicit AsyncAutoTransAction(const AsyncDBConnectPtr& driver);

    /** 内部方法：启动事务 */
    net::awaitable<void> startTransaction();

private:
    AsyncDBConnectPtr m_driver;
    boost::asio::io_context* m_io_context = nullptr;
    bool m_committed = false;
};

/**
 * 异步手动事务处理，允许嵌套启动事务，必须手工启动和提交事务
 * @details 必须有一次有效的手工启动事务，多次嵌套启动事务将被视为一次事务处理。
 *          手工提交一次事务后，如有新的事务处理，须再次手工启动事务。
 * @note 析构时如果已启动但未提交，则自动回滚（使用 detached 协程）
 * @ingroup DBConnect
 */
class AsyncTransAction {
public:
    /**
     * 工厂方法：创建实例并自动启动事务
     * @param driver 数据库连接指针
     * @return 异步事务对象
     */
    static net::awaitable<std::shared_ptr<AsyncTransAction>> create(
      const AsyncDBConnectPtr& driver);

    /** 获取数据库连接 */
    const AsyncDBConnectPtr& connect() const {
        return m_driver;
    }

    /**
     * 启动事务（支持嵌套）
     * @note 如果已经启动，则不重复启动
     */
    net::awaitable<void> begin();

    /**
     * 结束并提交事务
     * @note 必须先调用 begin() 启动事务
     */
    net::awaitable<void> end();

    /** 回滚事务 */
    net::awaitable<void> rollback();

    /** 析构函数：如果已启动但未提交，则自动回滚 */
    ~AsyncTransAction();

private:
    /** 私有构造函数 */
    explicit AsyncTransAction(const AsyncDBConnectPtr& driver);

private:
    AsyncDBConnectPtr m_driver;
    boost::asio::io_context* m_io_context = nullptr;
    bool m_committed = false;
    bool m_started = false;
};

}  // namespace hku

#endif /* HIKYUU_DB_CONNECT_ASYNC_TRANSACTION_H */
