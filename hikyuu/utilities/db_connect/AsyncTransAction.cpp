/*
 * AsyncTransAction.cpp
 *
 *  Copyright (c) 2026, hikyuu.org
 *
 *  Created on: 2026-05-09
 *      Author: fasiondog
 */

#include "AsyncTransAction.h"
#include "../Log.h"

namespace hku {

// ============================================================================
// AsyncAutoTransAction 实现
// ============================================================================

AsyncAutoTransAction::AsyncAutoTransAction(const AsyncDBConnectPtr& driver)
: m_driver(driver), m_io_context(nullptr), m_committed(false) {
    HKU_CHECK(m_driver, "Null AsyncDBConnectPtr!");
}

net::awaitable<std::shared_ptr<AsyncAutoTransAction>> 
AsyncAutoTransAction::create(const AsyncDBConnectPtr& driver) {
    // 使用 shared_ptr 直接构造，绕过私有构造函数限制
    auto action = std::shared_ptr<AsyncAutoTransAction>(new AsyncAutoTransAction(driver));
    co_await action->startTransaction();
    co_return action;
}

net::awaitable<void> AsyncAutoTransAction::startTransaction() {
    // 获取当前协程环境的 io_context
    auto exec = co_await net::this_coro::executor;
    m_io_context = &static_cast<boost::asio::io_context&>(exec.context());
    
    // 启动事务
    co_await m_driver->transaction();
}

net::awaitable<void> AsyncAutoTransAction::commit() {
    if (m_committed) {
        co_return;  // 已经提交过
    }
    
    bool rollback_needed = false;
    std::exception_ptr original_exception;
    
    try {
        co_await m_driver->commit();
        m_committed = true;
    } catch (...) {
        rollback_needed = true;
        original_exception = std::current_exception();
    }
    
    // 在 try-catch 外部处理回滚
    if (rollback_needed) {
        try {
            co_await m_driver->rollback();
        } catch (...) {
            HKU_ERROR("AsyncAutoTransAction: rollback failed after commit failure!");
        }
        
        // 重新抛出原始异常
        std::rethrow_exception(original_exception);
    }
}

AsyncAutoTransAction::~AsyncAutoTransAction() {
    if (!m_committed && m_driver && m_io_context) {
        // 启动一个 detached 协程来回滚（即发即忘）
        boost::asio::co_spawn(*m_io_context,
            [driver=m_driver]() -> net::awaitable<void> {
                try {
                    co_await driver->rollback();
                } catch (...) {
                    HKU_WARN("AsyncAutoTransAction: rollback in destructor failed");
                }
            },
            boost::asio::detached);
    }
}

// ============================================================================
// AsyncTransAction 实现
// ============================================================================

AsyncTransAction::AsyncTransAction(const AsyncDBConnectPtr& driver)
: m_driver(driver), m_io_context(nullptr), m_committed(false), m_started(false) {
    HKU_CHECK(m_driver, "Null AsyncDBConnectPtr!");
}

net::awaitable<std::shared_ptr<AsyncTransAction>> 
AsyncTransAction::create(const AsyncDBConnectPtr& driver) {
    // 使用 shared_ptr 直接构造，绕过私有构造函数限制
    auto action = std::shared_ptr<AsyncTransAction>(new AsyncTransAction(driver));
    co_await action->begin();
    co_return action;
}

net::awaitable<void> AsyncTransAction::begin() {
    if (!m_started) {
        // 获取当前协程环境的 io_context
        auto exec = co_await net::this_coro::executor;
        m_io_context = &static_cast<boost::asio::io_context&>(exec.context());
        
        // 启动事务
        co_await m_driver->transaction();
        m_started = true;
        m_committed = false;
    }
}

net::awaitable<void> AsyncTransAction::end() {
    HKU_CHECK(m_started, "No transaction has started!");
    
    if (!m_committed) {
        co_await m_driver->commit();
        m_committed = true;
        m_started = false;
    }
}

net::awaitable<void> AsyncTransAction::rollback() {
    if (m_started && !m_committed) {
        co_await m_driver->rollback();
        m_started = false;
        m_committed = false;
    }
}

AsyncTransAction::~AsyncTransAction() {
    // 如果没有主动提交事务，视为需要回滚
    if (m_started && !m_committed && m_driver && m_io_context) {
        HKU_WARN("AsyncTransAction: The transaction is rolled back in destructor!");
        
        // 启动一个 detached 协程来回滚（即发即忘）
        boost::asio::co_spawn(*m_io_context,
            [driver=m_driver]() -> net::awaitable<void> {
                try {
                    co_await driver->rollback();
                } catch (...) {
                    HKU_WARN("AsyncTransAction: rollback in destructor failed");
                }
            },
            boost::asio::detached);
    }
}

}  // namespace hku
