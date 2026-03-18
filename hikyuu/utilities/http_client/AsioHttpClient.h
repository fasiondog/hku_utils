/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2026-03-15
 *      Author: fasiondog
 */

#pragma once
#ifndef HKU_UTILS_ASIO_HTTP_CLIENT_H
#define HKU_UTILS_ASIO_HTTP_CLIENT_H

#include "hikyuu/utilities/config.h"
#if !HKU_ENABLE_HTTP_CLIENT
#error "Don't enable http client, please config with --http_client=y"
#endif

#include <string>
#include <map>
#include <functional>
#include <thread>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "hikyuu/utilities/Log.h"
#include "hikyuu/utilities/Parameter.h"
#include "HttpException.h"

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

namespace hku {

using json = nlohmann::json;

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

using HttpHeaders = std::map<std::string, std::string>;
using HttpParams = std::map<std::string, std::string>;

/**
 * @brief HTTP 数据块回调函数类型
 *
 * 用于流式响应处理，每次接收到数据块时调用
 * @param data 数据块指针
 * @param size 数据块大小
 */
using HttpChunkCallback = std::function<void(const char* data, size_t size)>;

class HKU_UTILS_API AsioHttpClient;

// HttpConnection 前向声明
struct HttpConnection;

// 连接池类型前向声明（避免在头文件中暴露完整模板定义）
template <typename T>
class ResourceAsioVersionPool;

class HKU_UTILS_API AsioHttpResponse final {
    friend class HKU_UTILS_API AsioHttpClient;

public:
    AsioHttpResponse() = default;
    ~AsioHttpResponse() = default;

    AsioHttpResponse(const AsioHttpResponse&) = delete;
    AsioHttpResponse& operator=(const AsioHttpResponse&) = delete;

    AsioHttpResponse(AsioHttpResponse&& rhs) = default;
    AsioHttpResponse& operator=(AsioHttpResponse&& rhs) = default;

    const std::string& body() const noexcept {
        return m_body;
    }

    hku::json json() const {
        return json::parse(m_body);
    }

    int status() const noexcept {
        return m_status;
    }

    std::string reason() const noexcept {
        return m_reason;
    }

    std::string getHeader(const std::string& key) const noexcept {
        auto it = m_headers.find(key);
        return it != m_headers.end() ? it->second : std::string();
    }

    size_t getContentLength() const noexcept {
        auto it = m_headers.find("Content-Length");
        if (it != m_headers.end() && !it->second.empty()) {
            try {
                return std::stoull(it->second);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }

private:
    int m_status{0};
    std::string m_reason;
    std::string m_body;
    std::map<std::string, std::string> m_headers;
};

/**
 * @brief 流式 HTTP 响应类
 *
 * 用于处理大响应体的流式下载，避免一次性加载到内存
 * 支持 Content-Length 和 Transfer-Encoding: chunked 两种模式
 */
class HKU_UTILS_API AsioHttpStreamResponse final {
    friend class HKU_UTILS_API AsioHttpClient;

public:
    AsioHttpStreamResponse() = default;
    ~AsioHttpStreamResponse() = default;

    AsioHttpStreamResponse(const AsioHttpStreamResponse&) = delete;
    AsioHttpStreamResponse& operator=(const AsioHttpStreamResponse&) = delete;

    AsioHttpStreamResponse(AsioHttpStreamResponse&& rhs) = default;
    AsioHttpStreamResponse& operator=(AsioHttpStreamResponse&& rhs) = default;

    int status() const noexcept {
        return m_status;
    }

    std::string reason() const noexcept {
        return m_reason;
    }

    std::string getHeader(const std::string& key) const noexcept {
        auto it = m_headers.find(key);
        return it != m_headers.end() ? it->second : std::string();
    }

    size_t getContentLength() const noexcept {
        auto it = m_headers.find("Content-Length");
        if (it != m_headers.end() && !it->second.empty()) {
            try {
                return std::stoull(it->second);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }

    bool isChunked() const noexcept {
        auto it = m_headers.find("Transfer-Encoding");
        return it != m_headers.end() && it->second == "chunked";
    }

    uint64_t totalBytesRead() const noexcept {
        return m_total_bytes_read;
    }

private:
    int m_status{0};
    std::string m_reason;
    std::map<std::string, std::string> m_headers;
    uint64_t m_total_bytes_read{0};
};

class HKU_UTILS_API AsioHttpClient {
public:
    using executor_type = net::any_io_executor;

    AsioHttpClient();
    explicit AsioHttpClient(const std::string& url,
                            std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    explicit AsioHttpClient(net::io_context& ctx, const std::string& url,
                            std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    virtual ~AsioHttpClient();

    AsioHttpClient(const AsioHttpClient&) = delete;
    AsioHttpClient& operator=(const AsioHttpClient&) = delete;

    // 禁用移动操作，因为管理后台线程和 io_context 的生命周期不安全
    AsioHttpClient(AsioHttpClient&&) = delete;
    AsioHttpClient& operator=(AsioHttpClient&&) = delete;

    bool valid() const noexcept {
        return !m_url.empty();
    }

    const std::string& url() const noexcept {
        return m_url;
    }

    void setUrl(const std::string& url);
    void setTimeout(std::chrono::milliseconds ms);

    std::chrono::milliseconds getTimeout() const noexcept {
        return m_timeout;
    }

    /**
     * @brief 获取 io_context 的执行器
     *
     * 用于在 AsioHttpClient 的内部 io_context 上启动协程或其他异步操作
     * @return net::any_io_executor io_context 的执行器
     */
    executor_type get_executor() const noexcept {
        return m_ctx->get_executor();
    }

    void setDefaultHeaders(std::map<std::string, std::string>&& headers) {
        m_default_headers = std::move(headers);
    }

    /**
     * @brief 设置自定义 CA 证书文件路径
     *
     * 用于 HTTPS 连接时验证服务器证书
     * @param filename CA 证书文件路径（PEM 格式）
     */
    void setCaFile(const std::string& filename);

    void setDefaultHeaders(const HttpHeaders& headers) {
        m_default_headers = headers;
    }

    // 异步请求方法 - 返回 net::awaitable
    net::awaitable<AsioHttpResponse> request(const std::string& method, const std::string& path,
                                             const HttpParams& params, const HttpHeaders& headers,
                                             const char* body, size_t body_len,
                                             const std::string& content_type);

    net::awaitable<AsioHttpResponse> get(const std::string& path, const HttpHeaders& headers = {}) {
        co_return co_await request("GET", path, {}, headers, nullptr, 0, "");
    }

    net::awaitable<AsioHttpResponse> get(const std::string& path, const HttpParams& params,
                                         const HttpHeaders& headers) {
        co_return co_await request("GET", path, params, headers, nullptr, 0, "");
    }

    net::awaitable<AsioHttpResponse> post(const std::string& path, const HttpParams& params,
                                          const HttpHeaders& headers, const char* body, size_t len,
                                          const std::string& content_type) {
        co_return co_await request("POST", path, params, headers, body, len, content_type);
    }

    net::awaitable<AsioHttpResponse> post(const std::string& path, const HttpHeaders& headers,
                                          const char* body, size_t len,
                                          const std::string& content_type) {
        co_return co_await request("POST", path, {}, headers, body, len, content_type);
    }

    net::awaitable<AsioHttpResponse> post(const std::string& path, const HttpParams& params,
                                          const HttpHeaders& headers, const std::string& content,
                                          const std::string& content_type = "text/plain") {
        co_return co_await post(path, params, headers, content.data(), content.size(),
                                content_type);
    }

    net::awaitable<AsioHttpResponse> post(const std::string& path, const HttpHeaders& headers,
                                          const std::string& content,
                                          const std::string& content_type = "text/plain") {
        co_return co_await post(path, {}, headers, content, content_type);
    }

    net::awaitable<AsioHttpResponse> post(const std::string& path, const HttpParams& params,
                                          const HttpHeaders& headers, const json& body) {
        co_return co_await post(path, params, headers, body.dump(), "application/json");
    }

    net::awaitable<AsioHttpResponse> post(const std::string& path, const HttpHeaders& headers,
                                          const json& body) {
        co_return co_await post(path, {}, headers, body);
    }

    net::awaitable<AsioHttpResponse> post(const std::string& path, const json& body) {
        co_return co_await post(path, {}, body);
    }

    /**
     * @brief 流式 HTTP 请求（支持大文件下载）
     *
     * 使用回调函数处理响应数据块，避免一次性加载到内存
     * 自动支持 Content-Length 和 Transfer-Encoding: chunked 两种模式
     *
     * @param method HTTP 方法 (GET, POST 等)
     * @param path 请求路径
     * @param params URL 查询参数
     * @param headers HTTP 请求头
     * @param body 请求体指针
     * @param body_len 请求体长度
     * @param content_type 内容类型
     * @param chunk_callback 数据块回调函数，每次接收到数据时调用
     * @return AsioHttpStreamResponse 流式响应对象
     */
    net::awaitable<AsioHttpStreamResponse> requestStream(
      const std::string& method, const std::string& path, const HttpParams& params,
      const HttpHeaders& headers, const char* body, size_t body_len,
      const std::string& content_type, const HttpChunkCallback& chunk_callback);

    /**
     * @brief 流式 GET 请求
     *
     * @param path 请求路径
     * @param params URL 查询参数
     * @param headers HTTP 请求头
     * @param chunk_callback 数据块回调函数
     * @return AsioHttpStreamResponse 流式响应对象
     */
    net::awaitable<AsioHttpStreamResponse> getStream(const std::string& path,
                                                     const HttpParams& params,
                                                     const HttpHeaders& headers,
                                                     const HttpChunkCallback& chunk_callback) {
        co_return co_await requestStream("GET", path, params, headers, nullptr, 0, "",
                                         chunk_callback);
    }

    net::awaitable<AsioHttpStreamResponse> getStream(const std::string& path,
                                                     const HttpHeaders& headers,
                                                     const HttpChunkCallback& chunk_callback) {
        co_return co_await requestStream("GET", path, {}, headers, nullptr, 0, "", chunk_callback);
    }

    /**
     * @brief 流式 POST 请求
     *
     * @param path 请求路径
     * @param params URL 查询参数
     * @param headers HTTP 请求头
     * @param body 请求体指针
     * @param body_len 请求体长度
     * @param content_type 内容类型
     * @param chunk_callback 数据块回调函数
     * @return AsioHttpStreamResponse 流式响应对象
     */
    net::awaitable<AsioHttpStreamResponse> postStream(const std::string& path,
                                                      const HttpParams& params,
                                                      const HttpHeaders& headers, const char* body,
                                                      size_t body_len,
                                                      const std::string& content_type,
                                                      const HttpChunkCallback& chunk_callback) {
        co_return co_await requestStream("POST", path, params, headers, body, body_len,
                                         content_type, chunk_callback);
    }

    net::awaitable<AsioHttpStreamResponse> postStream(const std::string& path,
                                                      const HttpHeaders& headers, const char* body,
                                                      size_t body_len,
                                                      const std::string& content_type,
                                                      const HttpChunkCallback& chunk_callback) {
        co_return co_await requestStream("POST", path, {}, headers, body, body_len, content_type,
                                         chunk_callback);
    }

private:
    void _parseUrl() noexcept;

    net::awaitable<std::vector<tcp::endpoint>> _resolveDNS();

    struct SocketVariant;
    net::awaitable<void> _connect(SocketVariant& socket_variant,
                                  const std::vector<tcp::endpoint>& dns_endpoints);

    // 从连接池获取已连接的 socket（带版本检查）
    net::awaitable<std::pair<std::shared_ptr<HttpConnection>, bool>> _getConnection();

private:
#if HKU_ENABLE_HTTP_CLIENT_SSL
    struct SslContext;
    std::unique_ptr<SslContext> m_ssl_ctx;  // SSL 上下文（仅在启用 SSL 时使用）
#endif

    bool m_is_valid_url{false};
    bool m_is_https{false};
    std::string m_url;
    std::string m_base_path;
    std::string m_host;
    std::string m_port;
    std::chrono::milliseconds m_timeout{30000};
    std::map<std::string, std::string> m_default_headers;
    std::string m_ca_file;  // 自定义 CA 证书文件路径

    // 连接池相关成员
    std::unique_ptr<ResourceAsioVersionPool<HttpConnection>> m_connection_pool;  // 带版本的连接池

    // io_context 管理
    std::unique_ptr<net::io_context> m_own_ctx;    // 内部 io_context
    net::io_context* m_ctx{nullptr};               // 当前使用的 io_context
    std::unique_ptr<std::thread> m_worker_thread;  // 后台运行 io_context 的线程
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>>
      m_work_guard;  // 防止 io_context 在无任务时退出
};

}  // namespace hku

#endif
