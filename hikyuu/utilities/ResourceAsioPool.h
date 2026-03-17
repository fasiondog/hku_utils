/*
 * ResourceAsioPool.h
 *
 *  Copyright (c) 2025, hikyuu.org
 *
 *  Created on: 2025-03-17
 *      Author: fasiondog
 */
#pragma once
#ifndef HKU_UTILS_RESOURCE_ASIO_POOL_H
#define HKU_UTILS_RESOURCE_ASIO_POOL_H

#include <boost/lockfree/queue.hpp>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <chrono>
#include <vector>
#include <atomic>
#include <unordered_set>
#include "Parameter.h"
#include "Log.h"
#include "ResourcePool.h"

namespace hku {

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
namespace this_coro = boost::asio::this_coro;

/**
 * 通用共享资源池 - 适用于协程环境
 * 使用 boost 无锁队列，在协程中异步获取资源
 * @ingroup Utilities
 *
 * @tparam ResourceType 资源类型，必须支持构造函数 ResourceType(const Parameter&)
 */
template <typename ResourceType>
class ResourceAsioPool {
public:
    ResourceAsioPool() = delete;
    ResourceAsioPool(const ResourceAsioPool &) = delete;
    ResourceAsioPool &operator=(const ResourceAsioPool &) = delete;

    /**
     * 构造函数
     * @param io_context Boost.Asio IO 上下文
     * @param param 连接参数
     * @param maxPoolSize 允许的最大共享资源数，为 0 表示不限制
     * @param maxIdleNum 运行的最大空闲资源数，为 0 表示用完即刻释放，无缓存
     */
    explicit ResourceAsioPool(boost::asio::io_context &io_context, const Parameter &param,
                             size_t maxPoolSize = 0, size_t maxIdleNum = 100)
    : m_io_context(io_context),
      m_maxPoolSize(maxPoolSize),
      m_maxIdleSize(maxIdleNum),
      m_count(0),
      m_idleCount(0),
      m_resourceList(128),  // 初始队列大小
      m_param(param) {}

    /**
     * 析构函数，释放所有缓存的资源
     */
    virtual ~ResourceAsioPool() {
        // 将所有已分配资源的 closer 和 pool 解绑
        for (auto iter = m_closer_set.begin(); iter != m_closer_set.end(); ++iter) {
            (*iter)->unbind();
        }

        // 释放所有空闲资源
        ResourceType *p = nullptr;
        while (m_resourceList.pop(p)) {
            if (p) {
                delete p;
            }
        }
    }

    /** 获取当前允许的最大资源数 */
    size_t maxPoolSize() const {
        return m_maxPoolSize;
    }

    /** 获取当前允许的最大空闲资源数 */
    size_t maxIdleSize() const {
        return m_maxIdleSize;
    }

    /** 设置最大资源数 */
    void maxPoolSize(size_t num) {
        m_maxPoolSize.store(num);
    }

    /** 设置允许的最大空闲资源数 */
    void maxIdleSize(size_t num) {
        m_maxIdleSize.store(num);
    }

    /** 资源实例指针类型 */
    typedef std::shared_ptr<ResourceType> ResourcePtr;

    /**
     * 协程方式获取可用资源
     * @return awaitable<ResourcePtr> 可等待的资源指针
     * @exception CreateResourceException 新资源创建可能抛出异常
     */
    awaitable<ResourcePtr> get() {
        ResourceType *p = nullptr;

        // 尝试从空闲队列获取资源
        if (m_resourceList.pop(p)) {
            m_idleCount.fetch_sub(1);  // 空闲计数减1,活动资源数不变
            co_return ResourcePtr(p, ResourceCloser(this));
        }

        // 检查是否可以创建新资源
        size_t currentCount = m_count.load();
        size_t maxPool = m_maxPoolSize.load();
        if (maxPool > 0 && currentCount >= maxPool) {
            // 等待可用资源
            auto timer = boost::asio::steady_timer(co_await this_coro::executor);
            
            // 轮询等待资源可用
            while (true) {
                if (m_resourceList.pop(p)) {
                    m_idleCount.fetch_sub(1);  // 空闲计数减1,活动资源数不变
                    co_return ResourcePtr(p, ResourceCloser(this));
                }
                
                timer.expires_after(std::chrono::milliseconds(10));
                co_await timer.async_wait(use_awaitable);
            }
        }

        // 创建新资源
        try {
            p = new ResourceType(m_param);
        } catch (const std::exception &e) {
            HKU_THROW_EXCEPTION(CreateResourceException, "Failed create a new Resource! {}", e.what());
        } catch (...) {
            HKU_THROW_EXCEPTION(CreateResourceException, "Failed create a new Resource! Unknown error!");
        }

        m_count.fetch_add(1);  // 活动资源数加1
        auto result = ResourcePtr(p, ResourceCloser(this));
        {
            std::lock_guard<std::mutex> lock(m_closer_mutex);
            m_closer_set.insert(std::get_deleter<ResourceCloser>(result));
        }
        co_return result;
    }

    /**
     * 在指定的超时时间内获取可用资源
     * @param ms_timeout 超时时间，单位毫秒
     * @return awaitable<ResourcePtr> 可等待的资源指针
     * @exception GetResourceTimeoutException, CreateResourceException
     */
    awaitable<ResourcePtr> getWaitFor(uint64_t ms_timeout) {
        ResourceType *p = nullptr;
        auto timer = boost::asio::steady_timer(co_await this_coro::executor,
                                             std::chrono::milliseconds(ms_timeout));

        // 尝试从空闲队列获取资源
        if (m_resourceList.pop(p)) {
            m_idleCount.fetch_sub(1);  // 空闲计数减1,活动资源数不变
            co_return ResourcePtr(p, ResourceCloser(this));
        }

        // 检查是否可以创建新资源
        size_t currentCount = m_count.load();
        size_t maxPool = m_maxPoolSize.load();
        if (maxPool > 0 && currentCount >= maxPool) {
            // 等待可用资源或超时
            auto polling_timer = boost::asio::steady_timer(co_await this_coro::executor);

            while (true) {
                if (timer.expiry() <= boost::asio::steady_timer::clock_type::now()) {
                    HKU_THROW_EXCEPTION(GetResourceTimeoutException, "Failed get resource timeout!");
                }

                if (m_resourceList.pop(p)) {
                    m_idleCount.fetch_sub(1);  // 空闲计数减1,活动资源数不变
                    co_return ResourcePtr(p, ResourceCloser(this));
                }

                // 等待一小段时间再重试
                polling_timer.expires_after(std::chrono::milliseconds(10));
                try {
                    co_await polling_timer.async_wait(use_awaitable);
                } catch (const boost::system::system_error &) {
                    HKU_THROW_EXCEPTION(GetResourceTimeoutException, "Failed get resource timeout!");
                }
            }
        }

        // 创建新资源
        try {
            p = new ResourceType(m_param);
        } catch (const std::exception &e) {
            HKU_THROW_EXCEPTION(CreateResourceException, "Failed create a new Resource! {}", e.what());
        } catch (...) {
            HKU_THROW_EXCEPTION(CreateResourceException, "Failed create a new Resource! Unknown error!");
        }

        m_count.fetch_add(1);  // 活动资源数加1
        auto result = ResourcePtr(p, ResourceCloser(this));
        {
            std::lock_guard<std::mutex> lock(m_closer_mutex);
            m_closer_set.insert(std::get_deleter<ResourceCloser>(result));
        }
        co_return result;
    }

    /**
     * 获取可用资源，如超出允许的最大资源数，将阻塞等待直到获得空闲资源
     * @return awaitable<ResourcePtr> 可等待的资源指针
     * @exception CreateResourceException 新资源创建可能抛出异常
     */
    awaitable<ResourcePtr> getAndWait() {
        co_return co_await getWaitFor(0);
    }

    /** 当前活动的资源数, 即全部资源数（含空闲及被使用的资源） */
    size_t count() const {
        return m_count.load();
    }

    /** 
     * 当前空闲的资源数（精确值）
     * 使用原子计数器跟踪，避免操作队列本身
     */
    size_t idleCount() const {
        return m_idleCount.load();
    }

    /** 释放当前所有的空闲资源 */
    void releaseIdleResource() {
        ResourceType *p = nullptr;
        while (m_resourceList.pop(p)) {
            if (p) {
                m_idleCount.fetch_sub(1);  // 减少空闲计数
                delete p;
                m_count.fetch_sub(1);  // 减少计数
            }
        }
    }

private:
    boost::asio::io_context &m_io_context;
    std::atomic<size_t> m_maxPoolSize;  // 允许的最大共享资源数
    std::atomic<size_t> m_maxIdleSize;  // 允许的最大空闲资源数
    std::atomic<size_t> m_count;        // 当前活动的资源数
    std::atomic<size_t> m_idleCount;    // 当前空闲的资源数
    Parameter m_param;
    boost::lockfree::queue<ResourceType *> m_resourceList;

    class ResourceCloser {
    public:
        explicit ResourceCloser(ResourceAsioPool *pool) : m_pool(pool) {}

        void operator()(ResourceType *conn) {
            if (conn) {
                // 如果绑定了 pool，则归还资源；否则删除
                if (m_pool) {
                    m_pool->returnResource(conn, this);
                } else {
                    delete conn;
                }
            }
        }

        // 解绑资源池
        void unbind() {
            m_pool = nullptr;
        }

    private:
        ResourceAsioPool *m_pool;
    };

    /** 归还至资源池 */
    void returnResource(ResourceType *p, ResourceCloser *closer) {
        if (p) {
            size_t maxIdle = m_maxIdleSize.load();

            // 如果当前空闲资源数未达到上限，尝试归还
            if (maxIdle > 0 && m_idleCount.load() < maxIdle) {
                if (m_resourceList.push(p)) {
                    // 推入成功，增加空闲计数
                    m_idleCount.fetch_add(1);
                    // 活动资源数不变,资源仍在池中
                } else {
                    // 推入失败（队列满），删除资源
                    delete p;
                    m_count.fetch_sub(1);  // 活动资源数减1
                }
            } else {
                // 超过最大空闲数或不需要缓存，删除资源
                delete p;
                m_count.fetch_sub(1);  // 活动资源数减1
            }
        } else {
            // p为nullptr,只减少活动资源数
            m_count.fetch_sub(1);
        }

        if (closer) {
            std::lock_guard<std::mutex> lock(m_closer_mutex);
            m_closer_set.erase(closer);  // 移除该 closer
        }
    }

    std::mutex m_closer_mutex;  // 保护 closer_set 的互斥锁
    std::unordered_set<ResourceCloser *> m_closer_set;  // 占用资源的 closer
};

}  // namespace hku

#endif /* HKU_UTILS_RESOURCE_ASIO_POOL_H */
