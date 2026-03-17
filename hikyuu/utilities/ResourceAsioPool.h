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
#include <boost/asio/awaitable.hpp>
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
     * @param param 连接参数
     */
    explicit ResourceAsioPool(const Parameter &param)
    : m_count(0),
      m_idleCount(0),
      m_resourceList(256),  // 初始队列大小
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
            m_idleCount.fetch_sub(1);  // 空闲计数减 1,活动资源数不变
            co_return ResourcePtr(p, ResourceCloser(this));
        }

        // 创建新资源
        try {
            p = new ResourceType(m_param);
        } catch (const std::exception &e) {
            HKU_THROW_EXCEPTION(CreateResourceException, "Failed create a new Resource! {}",
                                e.what());
        } catch (...) {
            HKU_THROW_EXCEPTION(CreateResourceException,
                                "Failed create a new Resource! Unknown error!");
        }

        m_count.fetch_add(1);  // 活动资源数加 1
        auto result = ResourcePtr(p, ResourceCloser(this));
        {
            std::lock_guard<std::mutex> lock(m_closer_mutex);
            m_closer_set.insert(std::get_deleter<ResourceCloser>(result));
        }
        co_return result;
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
    std::atomic<size_t> m_count;      // 当前活动的资源数
    std::atomic<size_t> m_idleCount;  // 当前空闲的资源数
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
            // 始终将资源归还到池中，不限制空闲数量
            if (m_resourceList.push(p)) {
                // 推入成功，增加空闲计数
                m_idleCount.fetch_add(1);
                // 活动资源数不变，资源仍在池中
            } else {
                // 推入失败（极罕见），删除资源
                delete p;
                m_count.fetch_sub(1);  // 活动资源数减 1
            }
        } else {
            // p 为 nullptr，只减少活动资源数
            m_count.fetch_sub(1);
        }

        if (closer) {
            std::lock_guard<std::mutex> lock(m_closer_mutex);
            m_closer_set.erase(closer);  // 移除该 closer
        }
    }

    std::mutex m_closer_mutex;                          // 保护 closer_set 的互斥锁
    std::unordered_set<ResourceCloser *> m_closer_set;  // 占用资源的 closer
};

}  // namespace hku

#endif /* HKU_UTILS_RESOURCE_ASIO_POOL_H */
