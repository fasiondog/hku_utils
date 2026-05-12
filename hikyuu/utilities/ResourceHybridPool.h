/*
 * ResourceHybridPool.h
 *
 *  Copyright (c) 2025, hikyuu.org
 *
 *  Created on: 2025-05-12
 *      Author: fasiondog
 */
#pragma once
#ifndef HKU_UTILS_RESOURCE_HYBRID_POOL_H
#define HKU_UTILS_RESOURCE_HYBRID_POOL_H

#include "ResourceThreadLocalPool.h"
#include "ResourceAsioPool.h"

namespace hku {

/**
 * 混合资源池 - 结合同步 TLS Pool 和异步 Asio Pool
 *
 * @details 提供两级资源获取策略：
 *          1. 优先从线程局部同步池（TLS Pool）快速获取资源（无锁、高性能）
 *          2. 如果 TLS Pool 不可用，则从异步池（Asio Pool）获取资源（支持协程等待）
 *
 *          这种设计兼顾了性能和灵活性：
 *          - 普通同步代码使用 TLS Pool，获得最佳性能
 *          - 协程代码可以使用 Asio Pool，支持超时等待
 *          - 当 TLS Pool 资源耗尽时，自动降级到 Asio Pool
 *
 * @tparam ResourceType 资源类型，必须支持构造函数 ResourceType(const Parameter&)
 * @tparam MAX_TLS_POOL_SIZE TLS 池最大大小，默认 32（编译期固定，使用 std::array 优化性能）
 * @ingroup Utilities
 *
 * @par 使用示例
 * @code
 * // 步骤1：初始化参数
 * Parameter param;
 * param.set("host", "localhost");
 * param.set("port", 3306);
 *
 * // 步骤2：创建混合池（可运行时指定全局共享池大小）
 * ResourceHybridPool<MyResource> pool(param, 64);  // 全局共享池最大 64 个资源
 *
 * // 步骤3a：同步获取（优先 TLS Pool）
 * auto result = pool.get();
 * if (result) {
 *     result.value()->doWork();
 * }
 *
 * // 步骤3b：异步获取（协程中使用）
 * co_spawn(io_ctx, [&]() -> net::awaitable<void> {
 *     auto resource = co_await pool.asyncGet(std::chrono::seconds(5));
 *     if (resource) {
 *         resource->doWork();
 *     }
 * }, net::detached);
 * @endcode
 *
 * @note 内部维护两个独立的资源池实例
 * @note TLS Pool 是线程局部的，每个线程有独立实例，大小由模板参数决定
 * @note Asio Pool 是全局共享的，支持跨线程访问，大小可在构造时指定
 */
template <typename ResourceType, size_t MAX_TLS_POOL_SIZE = 32>
class ResourceHybridPool {
public:
    /**
     * 构造函数
     *
     * @param param 资源创建参数
     * @param max_global_pool_size 全局共享池（Asio Pool）的最大资源数，默认 64
     */
    explicit ResourceHybridPool(const Parameter &param, size_t max_global_pool_size = 64)
    : m_tls_pool_param(param),
      m_asio_pool_param(param),
      m_max_global_pool_size(max_global_pool_size) {
        // 初始化 TLS Pool 的默认参数
        ResourceThreadLocalPool<ResourceType, MAX_TLS_POOL_SIZE>::init(m_tls_pool_param);

        // 创建 Asio Pool（使用运行时指定的大小）
        m_asio_pool = std::make_unique<ResourceAsioPool<ResourceType>>(m_asio_pool_param,
                                                                       m_max_global_pool_size);
    }

    /**
     * 析构函数
     */
    ~ResourceHybridPool() = default;

    /** 禁止拷贝 */
    ResourceHybridPool(const ResourceHybridPool &) = delete;
    ResourceHybridPool &operator=(const ResourceHybridPool &) = delete;

    /** 允许移动 */
    ResourceHybridPool(ResourceHybridPool &&) noexcept = default;
    ResourceHybridPool &operator=(ResourceHybridPool &&) noexcept = default;

    /**
     * 同步获取资源（两级策略）
     *
     * @return ResourcePtr 的 expected 对象，成功时包含资源指针，失败时包含错误信息
     *
     * @note 获取策略：
     *       1. 首先尝试从 TLS Pool 获取（快速路径，无锁）
     *       2. 如果 TLS Pool 失败（资源耗尽），返回错误
     *       3. 如需异步等待，请使用 asyncGet()
     *
     * @example
     * @code
     * auto result = pool.get();
     * if (result) {
     *     auto& resource = result.value();
     *     resource->doWork();
     * } else {
     *     HKU_ERROR("Failed to get resource: {}", result.error());
     * }
     * @endcode
     */
    stdx::expected<std::shared_ptr<ResourceType>, std::string> get() {
        // 优先从 TLS Pool 获取（快速路径）
        auto tls_result =
          ResourceThreadLocalPool<ResourceType, MAX_TLS_POOL_SIZE>::getInstance().get();
        if (tls_result) {
            return tls_result;  // TLS Pool 成功，直接返回 shared_ptr
        }

        // TLS Pool 失败，返回错误信息
        return stdx::unexpected(fmt::format("TLS Pool exhausted: {}", tls_result.error()));
    }

    /**
     * 异步获取资源（优先从 TLS Pool，失败则从 Asio Pool）
     *
     * @param timeout 超时时间
     * @return expected<shared_ptr<ResourceType>, string>
     */
    net::awaitable<stdx::expected<std::shared_ptr<ResourceType>, std::string>> asyncGet(
      std::chrono::steady_clock::duration timeout = std::chrono::seconds(5)) {
        // 1. 首先尝试从 TLS Pool 获取（快速路径）
        auto tls_result =
          ResourceThreadLocalPool<ResourceType, MAX_TLS_POOL_SIZE>::getInstance().get();
        if (tls_result) {
            // TLS Pool 成功，直接返回
            co_return tls_result;
        }

        // 2. TLS Pool 失败，从 Asio Pool 获取（支持协程等待）
        auto asio_result = co_await m_asio_pool->asyncGet(timeout);
        co_return asio_result;
    }

    /**
     * 仅从 TLS Pool 获取资源（同步）
     *
     * @return ResourcePtr 的 expected 对象
     */
    stdx::expected<std::shared_ptr<ResourceType>, std::string> getFromTlsPool() {
        return ResourceThreadLocalPool<ResourceType, MAX_TLS_POOL_SIZE>::getInstance().get();
    }

    /**
     * 仅从 Asio Pool 获取资源（异步）
     *
     * @param timeout 超时时间
     * @return awaitable<expected<shared_ptr<ResourceType>, string>>
     */
    net::awaitable<stdx::expected<std::shared_ptr<ResourceType>, std::string>> getFromAsioPool(
      std::chrono::steady_clock::duration timeout = std::chrono::seconds(5)) {
        auto result = co_await m_asio_pool->asyncGet(timeout);
        co_return result;
    }

    /** 获取 TLS Pool 引用 */
    ResourceThreadLocalPool<ResourceType, MAX_TLS_POOL_SIZE> &tlsPool() {
        return ResourceThreadLocalPool<ResourceType, MAX_TLS_POOL_SIZE>::getInstance();
    }

    /** 获取 Asio Pool 引用 */
    ResourceAsioPool<ResourceType> &asioPool() {
        return *m_asio_pool;
    }

private:
    Parameter m_tls_pool_param;         // TLS Pool 参数
    Parameter m_asio_pool_param;        // Asio Pool 参数
    size_t m_max_global_pool_size{64};  // 全局共享池（Asio Pool）最大资源数
    std::unique_ptr<ResourceAsioPool<ResourceType>> m_asio_pool;  // Asio Pool 实例
};

}  // namespace hku

#endif /* RESOURCE_HYBRID_POOL_H */
