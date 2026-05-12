/*
 * ResourceThreadLocalPool.h
 *
 *  Copyright (c) 2025, hikyuu.org
 *
 *  Created on: 2025-03-17
 *      Author: fasiondog
 */
#pragma once
#ifndef HKU_UTILS_RESOURCE_THREAD_LOCAL_POOL_H
#define HKU_UTILS_RESOURCE_THREAD_LOCAL_POOL_H

#include <vector>
#include <memory>
#include <chrono>
#include "expected.h"
#include "Parameter.h"
#include "Log.h"
#include "exception.h"
#include "net.h"

namespace hku {

// 使用 net 命名空间中的 asio 别名
namespace asio = net::asio;

/**
 * 线程局部资源池 - 完全无锁设计
 *
 * @details 使用 thread_local 存储，每个线程拥有独立的资源池实例。
 *          适用于协程环境，同一线程内的所有协程共享该线程的资源池。
 *          由于资源完全隔离在线程内部，不需要任何锁或原子操作，性能最优。
 *
 * @tparam ResourceType 资源类型，必须支持构造函数 ResourceType(const Parameter&)
 * @tparam MAX_POOL_SIZE 最大资源池大小限制，默认值为 100
 * @ingroup Utilities
 *
 * @par 使用示例
 * @code
 * // 步骤1：初始化默认参数（程序启动时调用一次）
 * Parameter param;
 * param.set("host", "localhost");
 * param.set("port", 3306);
 * ResourceThreadLocalPool<MyResource>::init(param);
 *
 * // 步骤2：获取线程局部资源池实例
 * auto& pool = ResourceThreadLocalPool<MyResource>::getInstance();
 *
 * // 步骤3a：同步获取资源
 * auto resource = pool.get();
 * if (resource) {
 *     resource->doWork();
 * } // 离开作用域时自动归还到池
 *
 * // 步骤3b：协程中异步获取资源（带超时控制）
 * co_spawn(io_ctx, [&]() -> asio::awaitable<void> {
 *     auto& pool = ResourceThreadLocalPool<MyResource>::getInstance();
 *     auto result = co_await pool.asyncGet(std::chrono::seconds(5));
 *     if (result) {
 *         result.value()->doWork();
 *     }
 * }, asio::detached);
 * @endcode
 *
 * @note 构造函数为私有，必须通过 init() + getInstance() 模式使用
 * @note 每个线程有独立的资源池实例，线程间不共享资源
 * @note 同一线程内的所有协程共享该线程的资源池
 */
template <typename ResourceType, size_t MAX_POOL_SIZE = 32>
class ResourceThreadLocalPool {
public:
    /**
     * 初始化全局默认参数（可选）
     *
     * @param param 默认资源创建参数
     *
     * @note 应在程序启动时调用一次，设置全局默认值
     * @note 后续调用 getInstance() 无参版本时将使用这些默认值
     * @note 如果未调用此方法，将使用 Parameter{}
     * @note 最大资源数由模板参数 MAX_POOL_SIZE 决定（默认 100）
     *
     * @example
     * @code
     * // 程序启动时初始化
     * Parameter defaultParam;
     * defaultParam.set("host", "localhost");
     * defaultParam.set("port", 3306);
     * ResourceThreadLocalPool<MyResource>::init(defaultParam);
     *
     * // 后续使用无需传参
     * auto& pool = ResourceThreadLocalPool<MyResource>::getInstance();
     * @endcode
     */
    static void init(const Parameter &param) {
        ms_defaultParam = param;
    }

    /**
     * 获取当前线程的资源池实例（单例模式）
     *
     * @return 当前线程的资源池引用
     *
     * @note thread_local 保证每个线程有独立的资源池实例
     * @note 使用 init() 设置的默认参数创建实例，最大池大小由模板参数 MAX_POOL_SIZE 决定
     * @note 首次调用时创建实例，后续调用返回同一实例
     *
     * @example
     * @code
     * // 先初始化默认参数
     * ResourceThreadLocalPool<MyResource>::init(param);
     *
     * // 后续直接获取，无需传参
     * auto& pool = ResourceThreadLocalPool<MyResource>::getInstance();
     * @endcode
     */
    static ResourceThreadLocalPool &getInstance() {
        thread_local static ResourceThreadLocalPool instance(ms_defaultParam);
        return instance;
    }

    /**
     * 析构函数，释放当前线程的所有缓存资源
     */
    virtual ~ResourceThreadLocalPool() {
        for (auto *p : m_resourceList) {
            if (p) {
                delete p;
            }
        }
        m_resourceList.clear();
    }

    /** 资源删除器，用于 unique_ptr 自动归还资源 */
    struct ResourceDeleter {
        ResourceThreadLocalPool *pool;

        void operator()(ResourceType *resource) const {
            if (resource && pool) {
                pool->returnResource(resource);
            } else if (resource) {
                delete resource;
            }
        }
    };

    /** 资源实例指针类型 */
    typedef std::unique_ptr<ResourceType, ResourceDeleter> ResourcePtr;

    /**
     * 获取可用资源
     *
     * @return ResourcePtr 的 expected 对象，成功时包含资源指针，失败时包含错误信息
     *
     * @note 完全无锁操作，性能极高
     * @note 返回的 unique_ptr 在析构时会自动归还资源到池
     * @note 如果当前资源数已达上限且无空闲资源，返回错误信息
     */
    stdx::expected<ResourcePtr, std::string> get() {
        // 1. 尝试从空闲列表获取
        if (!m_resourceList.empty()) {
            ResourceType *p = m_resourceList.back();
            m_resourceList.pop_back();
            return ResourcePtr(p, ResourceDeleter{this});
        }

        // 2. 无空闲资源，检查是否可以创建新资源
        if (m_count >= m_maxPoolSize) {
            return stdx::unexpected("No available resources and maximum pool size reached");
        }

        // 3. 创建新资源
        ResourceType *p = nullptr;
        try {
            p = new ResourceType(m_param);
        } catch (const std::exception &e) {
            return stdx::unexpected(fmt::format("Failed to create resource: {}", e.what()));
        } catch (...) {
            return stdx::unexpected("Failed to create resource: Unknown error");
        }

        m_count++;
        return ResourcePtr(p, ResourceDeleter{this});
    }

    /**
     * 创建独立管理的资源（不归入资源池）
     *
     * @return 资源指针的 expected 对象，成功时包含资源指针，失败时包含错误信息
     *
     * @note 返回的资源由调用者完全管理，不会归还到资源池
     * @note 资源析构时会直接 delete，而不是归还到池
     * @note 使用资源池内置的参数创建资源
     * @note 适用于需要临时资源或特殊配置的场景
     *
     * @example
     * @code
     * auto result = pool.createStandalone();
     * if (result) {
     *     auto res = std::move(result.value());
     *     res->doWork();
     * } // 离开作用域时自动 delete
     * @endcode
     */
    stdx::expected<std::unique_ptr<ResourceType>, std::string> createStandalone() {
        ResourceType *p = nullptr;
        try {
            p = new ResourceType(m_param);
        } catch (const std::exception &e) {
            return stdx::unexpected(
              fmt::format("Failed to create standalone resource: {}", e.what()));
        } catch (...) {
            return stdx::unexpected("Failed to create standalone resource: Unknown error");
        }

        // 返回标准的 unique_ptr，使用默认 deleter（直接 delete）
        return std::unique_ptr<ResourceType>(p);
    }

    /**
     * 异步获取可用资源（带超时控制）
     *
     * @param timeout 超时时间，默认 5 秒
     * @return ResourcePtr 的 expected 对象，成功时包含资源指针，失败时包含错误信息
     *
     * @note 在协程中使用：
     * @code
     * auto result = co_await pool.asyncGet(std::chrono::seconds(5));
     * if (result) {
     *     auto& res = result.value();
     *     res->doWork();
     * } else {
     *     HKU_ERROR("Failed to get resource: {}", result.error());
     * }
     * @endcode
     * @note 如果超时时仍无法获取资源，返回错误信息
     * @note 此方法适用于资源池已满且无空闲资源的场景，会等待其他协程归还资源
     */
    asio::awaitable<stdx::expected<ResourcePtr, std::string>> asyncGet(
      std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        // 从当前协程上下文获取 executor
        auto executor = co_await asio::this_coro::executor;

        // 1. 尝试立即获取（快速路径）
        if (!m_resourceList.empty()) {
            ResourceType *p = m_resourceList.back();
            m_resourceList.pop_back();
            co_return ResourcePtr(p, ResourceDeleter{this});
        }

        // 2. 如果可以创建新资源，直接创建
        if (m_count < m_maxPoolSize) {
            ResourceType *p = nullptr;
            try {
                p = new ResourceType(m_param);
            } catch (const std::exception &e) {
                co_return stdx::unexpected(fmt::format("Failed to create resource: {}", e.what()));
            } catch (...) {
                co_return stdx::unexpected("Failed to create resource: Unknown error");
            }

            m_count++;
            co_return ResourcePtr(p, ResourceDeleter{this});
        }

        // 3. 资源池已满，等待超时或其他协程归还资源
        // 使用定时器等待，超时后返回空指针
        asio::steady_timer timer(executor);
        timer.expires_after(timeout);

        net::error_code ec;
        co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));

        // 超时后再次尝试获取
        if (!m_resourceList.empty()) {
            ResourceType *p = m_resourceList.back();
            m_resourceList.pop_back();
            co_return ResourcePtr(p, ResourceDeleter{this});
        }

        // 仍然没有资源，返回错误
        co_return stdx::unexpected("Timeout waiting for available resource");
    }

    /** 当前活动的资源数（含空闲及被使用的资源） */
    size_t count() const {
        return m_count;
    }

    /** 当前空闲的资源数 */
    size_t idleCount() const {
        return m_resourceList.size();
    }

    /** 获取允许的最大资源数 */
    size_t maxPoolSize() const {
        return m_maxPoolSize;
    }

    /** 设置最大资源数 */
    void maxPoolSize(size_t num) {
        m_maxPoolSize = num;
    }

    /** 释放当前所有的空闲资源 */
    void releaseIdleResource() {
        for (auto *p : m_resourceList) {
            if (p) {
                delete p;
            }
        }
        m_count -= m_resourceList.size();
        m_resourceList.clear();
    }

private:
    ResourceThreadLocalPool(const ResourceThreadLocalPool &) = delete;
    ResourceThreadLocalPool &operator=(const ResourceThreadLocalPool &) = delete;

    /**
     * 构造函数（私有，仅通过 getInstance() 访问）
     *
     * @param param 资源创建参数
     */
    explicit ResourceThreadLocalPool(const Parameter &param)
    : m_maxPoolSize(MAX_POOL_SIZE), m_count(0), m_param(param) {}

    /**
     * 默认构造函数（私有，仅通过 getInstance() 访问）
     */
    ResourceThreadLocalPool() : m_maxPoolSize(MAX_POOL_SIZE), m_count(0) {}

private:
    /** 归还资源到池 */
    void returnResource(ResourceType *p) {
        if (p) {
            m_resourceList.push_back(p);
        }
    }

private:
    size_t m_maxPoolSize;                        // 允许的最大资源数，由模板参数 MAX_POOL_SIZE 决定
    size_t m_count;                              // 当前活动的资源数
    Parameter m_param;                           // 资源创建参数
    std::vector<ResourceType *> m_resourceList;  // 空闲资源列表

private:
    static Parameter ms_defaultParam;
};

// Static member initialization
template <typename ResourceType, size_t MAX_POOL_SIZE>
Parameter ResourceThreadLocalPool<ResourceType, MAX_POOL_SIZE>::ms_defaultParam{};

}  // namespace hku

#endif /* HKU_UTILS_RESOURCE_THREAD_LOCAL_POOL_H */
