/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2026-03-15
 *      Author: fasiondog
 */

#include "HttpAsyncClient.h"
#include "hikyuu/utilities/Log.h"
#include "hikyuu/utilities/os.h"
#include "hikyuu/utilities/ResourceAsioPool.h"

#include <sstream>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#if HKU_ENABLE_HTTP_CLIENT_SSL
#include <boost/asio/ssl.hpp>
#endif

#if HKU_ENABLE_HTTP_CLIENT_ZIP
#include "gzip/compress.hpp"
#include "gzip/decompress.hpp"
#endif

#if HKU_OS_OSX || HKU_OS_IOS
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace hku {

#if HKU_ENABLE_HTTP_CLIENT_SSL
namespace ssl = net::ssl;
#endif

// HttpConnection 类定义 - 用于连接池的可复用连接
struct HttpConnection {
    using SocketType = tcp::socket;
    
    Parameter params;
    std::string host;
    std::string port;
    bool is_https{false};
    std::vector<tcp::endpoint> endpoints;  // DNS 解析结果缓存
    std::chrono::steady_clock::time_point last_used_time;  // 最后使用时间
    int version{0};  // 版本号，用于检测参数是否更新
    
    // socket 在连接被获取时创建
    std::optional<SocketType> socket;
    
#if HKU_ENABLE_HTTP_CLIENT_SSL
    std::optional<ssl::stream<tcp::socket>> ssl_socket;
    
    HttpConnection(const Parameter& p) : params(p) {
        host = params.have("host") ? params.get<std::string>("host") : "";
        port = params.have("port") ? params.get<std::string>("port") : "";
        is_https = params.have("is_https") ? params.get<bool>("is_https") : false;
        last_used_time = std::chrono::steady_clock::now();
    }
    
    ~HttpConnection() {
        close();
    }
    
    void close() {
        if (ssl_socket) {
            boost::system::error_code ec;
            ssl_socket->lowest_layer().close(ec);
            ssl_socket.reset();
        }
        if (socket) {
            boost::system::error_code ec;
            socket->close(ec);
            socket.reset();
        }
    }
    
    bool is_open() const {
        if (ssl_socket) {
            return ssl_socket->lowest_layer().is_open();
        } else if (socket) {
            return socket->is_open();
        }
        return false;
    }
    
    SocketType& lowest_layer() {
        if (ssl_socket) {
            return static_cast<SocketType&>(ssl_socket->lowest_layer());
        } else if (socket) {
            return *socket;
        }
        HKU_THROW("Socket not initialized");
    }
#else
    HttpConnection(const Parameter& p) : params(p) {
        host = params.have("host") ? params.get<std::string>("host") : "";
        port = params.have("port") ? params.get<std::string>("port") : "";
        is_https = params.have("is_https") ? params.get<bool>("is_https") : false;
        last_used_time = std::chrono::steady_clock::now();
    }
    
    ~HttpConnection() {
        close();
    }
    
    void close() {
        if (socket) {
            boost::system::error_code ec;
            socket->close(ec);
            socket.reset();
        }
    }
    
    bool is_open() const {
        return socket && socket->is_open();
    }
    
    SocketType& lowest_layer() {
        if (socket) {
            return *socket;
        }
        HKU_THROW("Socket not initialized");
    }
#endif
};

#if HKU_ENABLE_HTTP_CLIENT_SSL
struct HttpAsyncClient::SslContext {
    net::ssl::context ssl_ctx;  // SSL 上下文

    SslContext() : ssl_ctx(net::ssl::context::tls_client) {
        // 配置 SSL 上下文
        ssl_ctx.set_default_verify_paths();
        ssl_ctx.set_options(net::ssl::context::default_workarounds | net::ssl::context::no_sslv2 |
                            net::ssl::context::no_sslv3 | net::ssl::context::no_tlsv1 |
                            net::ssl::context::no_tlsv1_1);
        // 使用 OpenSSL 原生 API 设置最低 TLS 版本
        SSL_CTX_set_min_proto_version(ssl_ctx.native_handle(), TLS1_2_VERSION);
    }

    /**
     * @brief 设置自定义 CA 证书文件
     * @param ca_file CA 证书文件路径（PEM 格式）
     */
    void setCaFile(const std::string& ca_file) {
        if (!ca_file.empty()) {
            ssl_ctx.load_verify_file(ca_file);
        }
    }
};
#endif

HttpAsyncClient::HttpAsyncClient()
: m_own_ctx(std::make_unique<net::io_context>()), m_ctx(m_own_ctx.get()) {
    // 创建工作守护，防止 io_context 在无任务时退出
    m_work_guard = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(
      m_own_ctx->get_executor());

#if HKU_ENABLE_HTTP_CLIENT_SSL
    // 初始化 SSL 上下文
    m_ssl_ctx = std::make_unique<SslContext>();
#endif

    // 初始化连接池参数
    Parameter pool_param;
    pool_param.set("host", m_host);
    pool_param.set("port", m_port);
    pool_param.set("is_https", m_is_https);
    pool_param.set("timeout", m_timeout.count());
    
    // 创建连接池，最大连接数为 CPU 核心数 * 2，最大空闲连接数为 CPU 核心数
    size_t max_connections = std::thread::hardware_concurrency() * 2;
    size_t max_idle = std::thread::hardware_concurrency();
    m_connection_pool = std::make_unique<ResourceAsioPool<HttpConnection>>(
        *m_ctx, pool_param, max_connections, max_idle);

    // 使用内部 io_context，启动工作线程运行事件循环
    m_worker_thread = std::make_unique<std::thread>([this] {
        HKU_INFO("Starting internal io_context thread");
        m_ctx->run();
        HKU_INFO("Internal io_context thread stopped");
    });
}

HttpAsyncClient::HttpAsyncClient(const std::string& url, std::chrono::milliseconds timeout)
: m_url(url),
  m_timeout(timeout),
  m_own_ctx(std::make_unique<net::io_context>()),
  m_ctx(m_own_ctx.get()),
  m_worker_thread(nullptr) {
    _parseUrl();
    // 在 _parseUrl() 之后初始化连接池，确保 m_host 和 m_port 已经设置好
    if (m_is_valid_url && m_ctx) {
#if HKU_ENABLE_HTTP_CLIENT_SSL
        // 初始化 SSL 上下文
        m_ssl_ctx = std::make_unique<SslContext>();
#endif

        // 创建工作守护，防止 io_context 在无任务时退出
        m_work_guard = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(
          m_own_ctx->get_executor());

        // 初始化连接池参数
        Parameter pool_param;
        pool_param.set("host", m_host);
        pool_param.set("port", m_port);
        pool_param.set("is_https", m_is_https);
        pool_param.set("timeout", m_timeout.count());
        
        // 创建连接池，最大连接数为 CPU 核心数 * 2，最大空闲连接数为 CPU 核心数
        size_t max_connections = std::thread::hardware_concurrency() * 2;
        size_t max_idle = std::thread::hardware_concurrency();
        m_connection_pool = std::make_unique<ResourceAsioPool<HttpConnection>>(
            *m_ctx, pool_param, max_connections, max_idle);

        // 启动后台线程运行 io_context
        m_worker_thread = std::make_unique<std::thread>([this]() {
            HKU_INFO("Starting internal io_context thread");
            m_ctx->run();
            HKU_INFO("Internal io_context thread stopped");
        });
    }
}

HttpAsyncClient::HttpAsyncClient(net::io_context& ctx, const std::string& url,
                                 std::chrono::milliseconds timeout)
: m_url(url),
  m_timeout(timeout),
  m_ctx(&ctx),  // 使用外部 io_context，不拥有所有权
  m_worker_thread(nullptr) {
    _parseUrl();

    if (m_is_valid_url && m_ctx) {
#if HKU_ENABLE_HTTP_CLIENT_SSL
        // 初始化 SSL 上下文
        m_ssl_ctx = std::make_unique<SslContext>();
#endif

        // 初始化连接池参数
        Parameter pool_param;
        pool_param.set("host", m_host);
        pool_param.set("port", m_port);
        pool_param.set("is_https", m_is_https);
        pool_param.set("timeout", m_timeout.count());
        
        // 创建连接池，最大连接数为 CPU 核心数 * 2，最大空闲连接数为 CPU 核心数
        size_t max_connections = std::thread::hardware_concurrency() * 2;
        size_t max_idle = std::thread::hardware_concurrency();
        m_connection_pool = std::make_unique<ResourceAsioPool<HttpConnection>>(
            *m_ctx, pool_param, max_connections, max_idle);
    }
}

HttpAsyncClient::~HttpAsyncClient() {
    if (m_own_ctx) {
        // 1. 先释放 work_guard，允许 io_context 自然退出
        m_work_guard.reset();

        // 2. 等待所有异步操作完成，让 io_context 自然处理完所有 pending 任务
        if (m_worker_thread && m_worker_thread->joinable()) {
            m_worker_thread->join();
        }

        // 3. 如果线程还在（理论上不应该），才强制 stop
        // 注意：正常情况下，release work_guard 后 io_context 会自然退出
        // 这里保留 stop() 作为最后的保护措施
        if (m_worker_thread && m_worker_thread->joinable()) {
            m_own_ctx->stop();
            m_worker_thread->join();
        }
    }
}

void HttpAsyncClient::setCaFile(const std::string& filename) {
    m_ca_file = filename;
#if HKU_ENABLE_HTTP_CLIENT_SSL
    if (m_ssl_ctx) {
        m_ssl_ctx->setCaFile(filename);
    }
#endif
}

void HttpAsyncClient::_parseUrl() noexcept {
    size_t pos = m_url.find("://");
    if (pos == std::string::npos) {
        m_is_valid_url = false;
        return;
    }

    m_is_valid_url = true;
    std::string proto = m_url.substr(0, pos);

    uint16_t port;
    if (proto == "http") {
        port = 80;
    } else if (proto == "https") {
        port = 443;
        m_is_https = true;
    } else {
        m_is_valid_url = false;
        HKU_ERROR("Invalid protocol: {}", proto);
        return;
    }

    std::string base_path;
    std::string host = m_url.substr(pos + 3);
    pos = host.find('/');
    if (pos != std::string::npos) {
        base_path = host.substr(pos);
        host = host.substr(0, pos);
    }
    pos = host.find(':');
    if (pos != std::string::npos) {
        try {
            port = std::stoi(host.substr(pos + 1));
        } catch (...) {
            m_is_valid_url = false;
            HKU_ERROR("Invalid port: {}", host.substr(pos + 1));
            return;
        }
        host = host.substr(0, pos);
    }

    // 检查 host 或 port 是否变化，如果变化则递增版本号
    bool host_changed = (m_host != host || m_port != std::to_string(port));
    
    m_base_path = std::move(base_path);
    m_host = std::move(host);
    m_port = std::to_string(port);
    
    // 如果 host 或 port 变化，递增版本号使旧连接失效
    if (host_changed && m_connection_pool) {
        m_connection_version.fetch_add(1);
        
        // 更新连接池参数
        Parameter pool_param;
        pool_param.set("host", m_host);
        pool_param.set("port", m_port);
        pool_param.set("is_https", m_is_https);
        pool_param.set("timeout", m_timeout.count());
        
        // 释放旧连接池中的空闲连接
        m_connection_pool->releaseIdleResource();
    }
}

// 异步 DNS 解析方法
net::awaitable<std::vector<tcp::endpoint>> HttpAsyncClient::_resolveDNS() {
    HKU_INFO("Starting DNS resolve for {}:{} (https={})", m_host, m_port, m_is_https);

#if HKU_OS_OSX || HKU_OS_IOS
    // macOS 使用原生 getaddrinfo 方式（beast 解析存在已知问题会卡死）
    struct addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(m_host.c_str(), m_port.c_str(), &hints, &res);
    HKU_CHECK(ret == 0, "DNS resolve failed!");

    std::vector<tcp::endpoint> dns_endpoints;
    for (struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        if (ai->ai_family == AF_INET) {
            auto* sin = reinterpret_cast<sockaddr_in*>(ai->ai_addr);
            net::ip::address_v4::bytes_type v4_bytes;
            std::memcpy(&v4_bytes, &(sin->sin_addr.s_addr), sizeof(v4_bytes));
            dns_endpoints.push_back(
              tcp::endpoint(net::ip::make_address_v4(v4_bytes), ntohs(sin->sin_port)));
        } else if (ai->ai_family == AF_INET6) {
            auto* sin6 = reinterpret_cast<sockaddr_in6*>(ai->ai_addr);
            net::ip::address_v6::bytes_type v6_bytes;
            std::memcpy(&v6_bytes, &(sin6->sin6_addr.s6_addr), sizeof(v6_bytes));
            dns_endpoints.push_back(
              tcp::endpoint(net::ip::make_address_v6(v6_bytes), ntohs(sin6->sin6_port)));
        }
    }

    freeaddrinfo(res);

    HKU_INFO("Native DNS resolve success, found {} endpoints", dns_endpoints.size());

    if (dns_endpoints.empty()) {
        HKU_THROW("No valid endpoints from DNS resolve");
    }

    co_return dns_endpoints;

#else
    // 其他平台使用 Boost.ASIO 异步 DNS 解析
    auto resolver = tcp::resolver{*m_ctx};

    struct ResolveOp {
        tcp::resolver& resolver;
        std::string host, port;
        tcp::resolver::results_type endpoints;
        boost::system::error_code ec;
        bool done = false;

        ResolveOp(tcp::resolver& r, const std::string& h, const std::string& p)
        : resolver(r), host(h), port(p) {}
    };

    auto op = std::make_shared<ResolveOp>(resolver, m_host, m_port);

    // 启动异步解析
    resolver.async_resolve(
      op->host, op->port,
      [op](const boost::system::error_code& e, tcp::resolver::results_type eps) {
          op->ec = e;
          op->endpoints = std::move(eps);
          op->done = true;
      });

    // 轮询等待操作完成或超时
    auto timer = net::steady_timer{*m_ctx};
    auto deadline = std::chrono::steady_clock::now() + m_timeout;

    while (!op->done) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                           deadline - std::chrono::steady_clock::now())
                           .count();

        if (remaining <= 0) {
            HKU_THROW_EXCEPTION(HttpTimeoutException, "DNS resolve timeout");
        }

        timer.expires_after(std::chrono::milliseconds(std::min<long long>(remaining, 50)));
        co_await timer.async_wait(net::use_awaitable);
    }

    if (op->ec) {
        HKU_THROW("DNS resolve failed: {}", op->ec.message());
    }

    // 转换为 endpoint 列表
    std::vector<tcp::endpoint> dns_endpoints;
    for (const auto& ep : op->endpoints) {
        dns_endpoints.push_back(ep.endpoint());
    }

    HKU_INFO("ASIO DNS resolve success, found {} endpoints", dns_endpoints.size());

    if (dns_endpoints.empty()) {
        HKU_THROW("No valid endpoints from DNS resolve");
    }

    co_return dns_endpoints;
#endif
}

// 从连接池获取已连接的连接（带版本检查和 DNS 缓存）
net::awaitable<std::pair<std::shared_ptr<HttpConnection>, bool>> HttpAsyncClient::_getConnection() {
    HKU_CHECK(m_connection_pool != nullptr, "Connection pool not initialized");
    
    // 从池中获取连接
    auto conn_ptr = co_await m_connection_pool->get();
    HKU_CHECK(conn_ptr != nullptr, "Failed to get connection from pool");
    
    int current_version = m_connection_version.load();
    bool is_new_connection = false;
    
    // 检查连接是否需要重新创建
    if (!conn_ptr->is_open() || conn_ptr->version != current_version) {
        // 连接已关闭或版本不匹配，需要重新创建
        is_new_connection = true;
        HKU_INFO("Creating new connection - is_open={}, version_mismatch={}", 
                 conn_ptr->is_open(), 
                 conn_ptr->version != current_version);
        
        // 如果 DNS 缓存为空或超时（5 分钟），重新解析 DNS
        auto now = std::chrono::steady_clock::now();
        bool need_dns_resolve = conn_ptr->endpoints.empty();
        
        if (!need_dns_resolve) {
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
                now - conn_ptr->last_used_time).count();
            if (elapsed > 5) {
                need_dns_resolve = true;
            }
        }
        
        if (need_dns_resolve) {
            // DNS 解析（带超时）
            conn_ptr->endpoints = co_await _resolveDNS();
        }
        
        // 关闭旧连接（如果有）
        conn_ptr->close();
        
        // 创建新 socket 并连接
#if HKU_ENABLE_HTTP_CLIENT_SSL
        if (m_is_https) {
            conn_ptr->ssl_socket.emplace(*m_ctx, m_ssl_ctx->ssl_ctx);
            SSL_set_tlsext_host_name(conn_ptr->ssl_socket->native_handle(), m_host.c_str());
            
            // 连接到服务器
            boost::system::error_code connect_ec;
            bool connected = false;
            
            for (const auto& endpoint : conn_ptr->endpoints) {
                conn_ptr->close();
                
                auto timer = net::steady_timer{*m_ctx};
                timer.expires_after(m_timeout);
                
                struct ConnectOp {
                    tcp::socket* socket;
                    tcp::endpoint endpoint;
                    
                    net::awaitable<boost::system::error_code> run() {
                        auto [ec] = co_await socket->async_connect(
                            endpoint, 
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return ec ? ec : (socket->is_open() 
                            ? boost::system::errc::make_error_code(boost::system::errc::success)
                            : boost::system::errc::make_error_code(boost::system::errc::not_connected));
                    }
                };
                
                ConnectOp connect_op{&conn_ptr->ssl_socket->next_layer(), endpoint};
                HKU_INFO("Trying to connect to {}:{}", endpoint.address().to_string(), endpoint.port());
                auto connect_result = co_await connect_op.run();
                HKU_INFO("Connect result: ec={}, socket.is_open={}", connect_result.message(), conn_ptr->socket->is_open());
                
                if (!timer.cancel()) {
                    HKU_INFO("Timer expired, trying next endpoint");
                    continue;
                }
                
                if (!connect_result && conn_ptr->ssl_socket->lowest_layer().is_open()) {
                    connected = true;
                    break;
                }
            }
            
            if (!connected) {
                HKU_THROW_EXCEPTION(HttpTimeoutException, "Connect timeout to {}:{}", m_host, m_port);
            }
            
            // 设置 socket 选项
            conn_ptr->ssl_socket->lowest_layer().set_option(tcp::no_delay(true));
            
            // SSL 握手
            HKU_INFO("Performing SSL handshake with {}", m_host);
            auto timer = net::steady_timer{*m_ctx};
            timer.expires_after(m_timeout);
            
            struct SslHandshakeOp {
                ssl::stream<tcp::socket>* stream;
                
                net::awaitable<boost::system::error_code> run() {
                    auto [ec] = co_await stream->async_handshake(
                        ssl::stream_base::client,
                        net::as_tuple(net::use_awaitable)
                    );
                    co_return ec;
                }
            };
            
            SslHandshakeOp handshake_op{&conn_ptr->ssl_socket.value()};
            auto handshake_result = co_await handshake_op.run();
            
            if (!timer.cancel()) {
                HKU_THROW_EXCEPTION(HttpTimeoutException, "SSL handshake timeout");
            }
            
            if (handshake_result) {
                HKU_THROW("SSL handshake failed: {}", handshake_result.message());
            }
            
            HKU_INFO("SSL handshake successful");
        } else {
#endif
            // 普通 HTTP 连接
            conn_ptr->socket.emplace(*m_ctx);
            
            boost::system::error_code connect_ec;
            bool connected = false;
            
            for (const auto& endpoint : conn_ptr->endpoints) {
                auto timer = net::steady_timer{*m_ctx};
                timer.expires_after(m_timeout);
                
                struct ConnectOp {
                    tcp::socket* socket;
                    tcp::endpoint endpoint;
                    
                    net::awaitable<boost::system::error_code> run() {
                        auto [ec] = co_await socket->async_connect(
                            endpoint, 
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return ec ? ec : (socket->is_open() 
                            ? boost::system::errc::make_error_code(boost::system::errc::success)
                            : boost::system::errc::make_error_code(boost::system::errc::not_connected));
                    }
                };
                
                ConnectOp connect_op{&conn_ptr->socket.value(), endpoint};
                HKU_INFO("Trying to connect to {}:{}", endpoint.address().to_string(), endpoint.port());
                auto connect_result = co_await connect_op.run();
                HKU_INFO("Connect result: ec={}, socket.is_open={}", connect_result.message(), conn_ptr->socket->is_open());
                
                // 先检查连接是否成功
                if (!connect_result && conn_ptr->socket->is_open()) {
                    connected = true;
                    HKU_INFO("Connected successfully");
                    break;
                }
                
                // 连接失败，检查是否超时
                if (!timer.cancel()) {
                    HKU_INFO("Timer expired, trying next endpoint");
                    // 超时后不再尝试其他 endpoint
                    break;
                }
                
                // 连接失败但未超时，继续尝试下一个 endpoint
                HKU_INFO("Connect failed, trying next endpoint");

                // 连接失败，关闭 socket 以便下一次尝试
                boost::system::error_code ec;
                conn_ptr->socket->close(ec);
            }
            
            if (!connected) {
                HKU_THROW_EXCEPTION(HttpTimeoutException, "Connect timeout to {}:{}", m_host, m_port);
            }
            
            // 设置 socket 选项
            conn_ptr->socket->set_option(tcp::no_delay(true));
#if HKU_ENABLE_HTTP_CLIENT_SSL
        }
#endif
        
        // 更新版本号和使用时间
        conn_ptr->version = current_version;
        conn_ptr->last_used_time = now;
    } else {
        // 复用已有连接，更新时间并检查连接是否有效
        auto now = std::chrono::steady_clock::now();
        
        // 检查连接是否仍然打开
        if (!conn_ptr->is_open()) {
            HKU_INFO("Connection closed, need reconnect");
            
            // 连接已关闭，需要重新创建 - 直接在这里处理重连
            conn_ptr->close();
            
#if HKU_ENABLE_HTTP_CLIENT_SSL
            if (m_is_https) {
                conn_ptr->ssl_socket.emplace(*m_ctx, m_ssl_ctx->ssl_ctx);
#else
            {
#endif
                
                struct ConnectOp {
                    tcp::socket* socket;
                    tcp::endpoint endpoint;
                    
                    net::awaitable<boost::system::error_code> run() {
                        auto [ec] = co_await socket->async_connect(
                            endpoint, 
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return ec ? ec : (socket->is_open() 
                            ? boost::system::errc::make_error_code(boost::system::errc::success)
                            : boost::system::errc::make_error_code(boost::system::errc::not_connected));
                    }
                };
                
                bool connected = false;
                for (const auto& endpoint : conn_ptr->endpoints) {
                    auto timer = net::steady_timer{*m_ctx};
                    timer.expires_after(m_timeout);
                    
                    ConnectOp connect_op{
#if HKU_ENABLE_HTTP_CLIENT_SSL
                        m_is_https ? &conn_ptr->ssl_socket->next_layer() : 
#endif
                        &conn_ptr->socket.value(), 
                        endpoint
                    };
                    auto connect_result = co_await connect_op.run();
                    
                    if (!timer.cancel()) {
                        continue;
                    }
                    
#if HKU_ENABLE_HTTP_CLIENT_SSL
                    if (!connect_result && (m_is_https ? conn_ptr->ssl_socket->lowest_layer().is_open() : conn_ptr->socket->is_open())) {
#else
                    if (!connect_result && conn_ptr->socket->is_open()) {
#endif
                        connected = true;
                        break;
                    }
                }
                
                if (!connected) {
                    HKU_THROW_EXCEPTION(HttpTimeoutException, "Reconnect timeout to {}:{}", m_host, m_port);
                }
                
#if HKU_ENABLE_HTTP_CLIENT_SSL
                if (m_is_https) {
                    conn_ptr->ssl_socket->lowest_layer().set_option(tcp::no_delay(true));
                    
                    // SSL 握手
                    auto timer = net::steady_timer{*m_ctx};
                    timer.expires_after(m_timeout);
                    
                    struct SslHandshakeOp {
                        ssl::stream<tcp::socket>* stream;
                        
                        net::awaitable<boost::system::error_code> run() {
                            auto [ec] = co_await stream->async_handshake(
                                ssl::stream_base::client,
                                net::as_tuple(net::use_awaitable)
                            );
                            co_return ec;
                        }
                    };
                    
                    SslHandshakeOp handshake_op{&conn_ptr->ssl_socket.value()};
                    auto handshake_result = co_await handshake_op.run();
                    
                    if (!timer.cancel()) {
                        HKU_THROW_EXCEPTION(HttpTimeoutException, "SSL handshake timeout");
                    }
                    
                    if (handshake_result) {
                        HKU_THROW("SSL handshake failed: {}", handshake_result.message());
                    }
                } else {
                    conn_ptr->socket->set_option(tcp::no_delay(true));
                }
#else
                conn_ptr->socket->set_option(tcp::no_delay(true));
#endif
            }
            
            conn_ptr->version = current_version;
        }
        
        conn_ptr->last_used_time = now;
    }

    // 设置 socket 选项以确保连接活跃
    if (conn_ptr->socket) {
        boost::system::error_code ec;
        conn_ptr->socket->set_option(tcp::no_delay(true), ec);
    }
#if HKU_ENABLE_HTTP_CLIENT_SSL
    if (conn_ptr->ssl_socket) {
        boost::system::error_code ec;
        conn_ptr->ssl_socket->lowest_layer().set_option(tcp::no_delay(true), ec);
    }
#endif
    
    co_return std::make_pair(conn_ptr, is_new_connection);
}

// 创建 socket（使用 variant 存储普通 socket 或 SSL socket）
struct HttpAsyncClient::SocketVariant {
    std::optional<tcp::socket> plain;
#if HKU_ENABLE_HTTP_CLIENT_SSL
    std::optional<ssl::stream<tcp::socket>> ssl;

    void close(boost::system::error_code& ec) {
        if (plain) {
            plain->close(ec);
            plain.reset();
        }
        if (ssl) {
            ssl->lowest_layer().close(ec);
            ssl.reset();
        }
    }

    bool is_ssl() const {
        return ssl.has_value();
    }

    tcp::socket& socket() {
        if (ssl) {
            return ssl->next_layer();
        } else if (plain) {
            return *plain;
        }
        HKU_THROW("Socket not initialized");
    }
#else
    void close(boost::system::error_code& ec) {
        if (plain) {
            plain->close(ec);
            plain.reset();
        }
    }

    bool is_ssl() const {
        return false;
    }

    tcp::socket& socket() {
        if (plain) {
            return *plain;
        }
        HKU_THROW("Socket not initialized");
    }
#endif
};

net::awaitable<void> HttpAsyncClient::_connect(SocketVariant& socket_variant,
                                               const std::vector<tcp::endpoint>& dns_endpoints) {
    // 连接（带超时）
    boost::system::error_code connect_ec;
    bool connected = false;

    for (const auto& endpoint : dns_endpoints) {
        socket_variant.close(connect_ec);

        {
            // 先创建普通 socket
            socket_variant.plain.emplace(*m_ctx);
            
            // 使用真正的事件驱动异步连接配合超时
        auto timer = net::steady_timer{*m_ctx};
        timer.expires_after(m_timeout);
        
        // 直接 co_await async_connect，这是真正的事件驱动
        struct ConnectOp {
            tcp::socket* socket;
            tcp::endpoint endpoint;
            
            net::awaitable<boost::system::error_code> run() {
                auto [ec] = co_await socket->async_connect(
                    endpoint, 
                    net::as_tuple(net::use_awaitable)
                );
                co_return ec ? ec : (socket->is_open() 
                    ? boost::system::errc::make_error_code(boost::system::errc::success)
                    : boost::system::errc::make_error_code(boost::system::errc::not_connected));
            }
        };
        
        ConnectOp connect_op{&socket_variant.plain.value(), endpoint};
        
        // 等待连接完成（如果超时，协程会被中断）
        auto connect_result = co_await connect_op.run();
        
        // 检查是否超时（通过检查 timer 状态）
        if (!timer.cancel()) {
            // timer 已经触发，说明超时了
            continue;  // 尝试下一个端点
        }
        
        if (!connect_result && socket_variant.plain->is_open()) {
            connected = true;
            break;
        }
        }
    }

    if (!connected) {
        HKU_THROW_EXCEPTION(HttpTimeoutException, "Connect timeout to {}:{}", m_host, m_port);
    }

    // 设置 socket 选项
    socket_variant.socket().set_option(tcp::no_delay(true));

#if HKU_ENABLE_HTTP_CLIENT_SSL
    // 如果是 HTTPS，进行 SSL 握手（带超时）
    if (m_is_https) {
        HKU_INFO("Performing SSL handshake with {}", m_host);

        // 移动到 SSL socket
#if HKU_ENABLE_HTTP_CLIENT_SSL
        socket_variant.ssl.emplace(std::move(*socket_variant.plain), m_ssl_ctx->ssl_ctx);
#else
        socket_variant.ssl.emplace(std::move(*socket_variant.plain));
#endif
        socket_variant.plain.reset();

        // 设置 SNI（Server Name Indication）
        SSL_set_tlsext_host_name(socket_variant.ssl->native_handle(), m_host.c_str());

        // 使用事件驱动的 SSL 握手配合超时
        auto timer = net::steady_timer{*m_ctx};
        timer.expires_after(m_timeout);
        
        struct SslHandshakeOp {
            ssl::stream<tcp::socket>* stream;
            
            net::awaitable<boost::system::error_code> run() {
                auto [ec] = co_await stream->async_handshake(
                    ssl::stream_base::client,
                    net::as_tuple(net::use_awaitable)
                );
                co_return ec;
            }
        };
        
        SslHandshakeOp handshake_op{&socket_variant.ssl.value()};
        auto handshake_result = co_await handshake_op.run();
        
        // 检查是否超时
        if (!timer.cancel()) {
            HKU_THROW_EXCEPTION(HttpTimeoutException, "SSL handshake timeout");
        }
        
        if (handshake_result) {
            HKU_THROW("SSL handshake failed: {}", handshake_result.message());
        }

        HKU_INFO("SSL handshake successful");
    }
#endif

    co_return;
}

net::awaitable<HttpResponseAsync> HttpAsyncClient::request(
  const std::string& method, const std::string& path, const HttpParams& params,
  const HttpHeaders& headers, const char* body, size_t body_len, const std::string& content_type) {
    HKU_CHECK(m_is_valid_url, "Invalid url: {}", m_url);

    // 确保 io_context 已设置（默认构造函数可能没有初始化）
    if (m_ctx == nullptr) {
        auto exec = co_await net::this_coro::executor;
        m_ctx = &static_cast<net::io_context&>(exec.context());
        HKU_CHECK(m_ctx != nullptr, "Cannot get io_context from execution context");
    }

#if !HKU_ENABLE_HTTP_CLIENT_SSL
    HKU_CHECK(!m_is_https,
              "HTTPS is not supported. Please enable SSL support with --http_client_ssl=y");
#endif

    // 构建完整的 URI
    std::ostringstream uri_stream;
    uri_stream << m_base_path;
    if (!path.empty()) {
        if (m_base_path.back() != '/' && path.front() != '/') {
            uri_stream << '/';
        }
        uri_stream << path;
    }

    // 添加查询参数
    bool first = true;
    for (const auto& [key, value] : params) {
        uri_stream << (first ? "?" : "&") << key << "=" << value;
        first = false;
    }

    std::string uri = uri_stream.str();

    HttpResponseAsync response;

    try {
        // 从连接池获取连接（自动处理 DNS 缓存和连接复用）
    auto [conn, is_new] = co_await _getConnection();
    HKU_CHECK(conn != nullptr, "Failed to get connection from pool");
    
    HKU_INFO("Got connection - is_new={}, socket_has_value={}, ssl_socket_has_value={}", 
             is_new, 
             conn->socket.has_value(),
#if HKU_ENABLE_HTTP_CLIENT_SSL
             conn->ssl_socket.has_value()
#else
             false
#endif
    );

    // 创建 HTTP 请求
    http::request<http::string_body> req;
        req.method(http::string_to_verb(method));
        req.target(uri);
        req.version(11);  // HTTP/1.1

        // 添加默认头
        for (const auto& [key, value] : m_default_headers) {
            req.set(key, value);
        }

        // 添加额外头
        for (const auto& [key, value] : headers) {
            req.set(key, value);
        }

        // 添加 User-Agent
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::host, m_host);
        // 注意：不使用 "close"，允许连接复用

        // 添加请求体
        if (body != nullptr && body_len > 0) {
#if HKU_ENABLE_HTTP_CLIENT_ZIP
            auto content_type = req["Content-Type"];
            if (content_type == "gzip") {
                gzip::Compressor comp(Z_DEFAULT_COMPRESSION);
                std::string output;
                comp.compress(output, body, body_len);
                req.body() = std::move(output);
            } else {
                req.set(http::field::content_type, content_type);
                req.body() = std::string(body, body_len);
                req.prepare_payload();
            }
#else
            req.set(http::field::content_type, content_type);
            req.body() = std::string(body, body_len);
            req.prepare_payload();
#endif
        }

        // 发送请求（带超时）
        {
#if HKU_ENABLE_HTTP_CLIENT_SSL
            if (conn->ssl_socket) {
                // SSL 连接：使用 SSL stream 发送
                struct WriteOp {
                    ssl::stream<tcp::socket>& stream;
                    http::request<http::string_body>& req;
                    
                    net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                        auto [ec, bytes] = co_await http::async_write(
                            stream, req,
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return std::make_pair(ec, bytes);
                    }
                };
                
                auto write_op = WriteOp{*conn->ssl_socket, req};
                auto [write_ec, bytes_transferred] = co_await write_op.run();
                
                if (write_ec) {
                    HKU_THROW("HTTP write failed: {}", write_ec.message());
                }
            } else {
#endif
                // 普通连接：使用 socket 发送
                struct WriteOp {
                    tcp::socket& sock;
                    http::request<http::string_body>& req;
                    
                    net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                        auto [ec, bytes] = co_await http::async_write(
                            sock, req,
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return std::make_pair(ec, bytes);
                    }
                };
                
                auto write_op = WriteOp{conn->socket.value(), req};
                auto [write_ec, bytes_transferred] = co_await write_op.run();
                
                if (write_ec) {
                    HKU_THROW("HTTP write failed: {}", write_ec.message());
                }
#if HKU_ENABLE_HTTP_CLIENT_SSL
            }
#endif
        }

        // 读取响应（带超时）
        beast::flat_buffer buffer;
        http::response<http::string_body> res;

        {
#if HKU_ENABLE_HTTP_CLIENT_SSL
            if (conn->ssl_socket) {
                // SSL 连接：使用 SSL stream 读取
                struct ReadOp {
                    ssl::stream<tcp::socket>& stream;
                    beast::flat_buffer& buffer;
                    http::response<http::string_body>& response;
                    
                    net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                        auto [ec, bytes] = co_await http::async_read(
                            stream, buffer, response,
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return std::make_pair(ec, bytes);
                    }
                };
                
                auto read_op = ReadOp{*conn->ssl_socket, buffer, res};
                auto [read_ec, bytes_transferred] = co_await read_op.run();
                
                if (read_ec) {
                    HKU_THROW("HTTP read failed: {}", read_ec.message());
                }
            } else {
#endif
                // 普通连接：使用 socket 读取
                struct ReadOp {
                    tcp::socket& sock;
                    beast::flat_buffer& buffer;
                    http::response<http::string_body>& response;
                    
                    net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                        auto [ec, bytes] = co_await http::async_read(
                            sock, buffer, response,
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return std::make_pair(ec, bytes);
                    }
                };
                
                auto read_op = ReadOp{conn->socket.value(), buffer, res};
                
                auto [read_ec, bytes_transferred] = co_await read_op.run();
                
                if (read_ec) {
                    HKU_THROW("HTTP read failed: {}", read_ec.message());
                }
#if HKU_ENABLE_HTTP_CLIENT_SSL
            }
#endif
        }

        // 填充响应对象
        response.m_status = res.result_int();
        response.m_reason = std::string(res.reason());

#if HKU_ENABLE_HTTP_CLIENT_ZIP
        // 正确获取 Content-Encoding 头部
        auto encoding_it = res.find("Content-Encoding");
        if (encoding_it != res.end() && encoding_it->value() == "gzip") {
            response.m_body = gzip::decompress(res.body().data(), res.body().size());
        } else {
            response.m_body = std::move(res.body());
        }
#else
        response.m_body = std::move(res.body());
#endif

        for (auto it = res.begin(); it != res.end(); ++it) {
            response.m_headers.emplace(std::string(it->name_string()), std::string(it->value()));
        }

        // 不关闭连接，让连接池自动管理（连接会被归还到池中）

    } catch (const boost::system::system_error& e) {
        HKU_ERROR("HTTP request system error! {}", e.what());
        throw;
    } catch (const std::exception& e) {
        HKU_ERROR("HTTP request failed! {}", e.what());
        throw;
    }

    co_return response;
}

net::awaitable<HttpStreamResponseAsync> HttpAsyncClient::requestStream(
  const std::string& method, const std::string& path, const HttpParams& params,
  const HttpHeaders& headers, const char* body, size_t body_len, const std::string& content_type,
  const HttpChunkCallback& chunk_callback) {
    HKU_CHECK(m_is_valid_url, "Invalid url: {}", m_url);
    HKU_CHECK(chunk_callback != nullptr, "Chunk callback must not be null");

    // 确保 io_context 已设置（默认构造函数可能没有初始化）
    if (m_ctx == nullptr) {
        auto exec = co_await net::this_coro::executor;
        m_ctx = &static_cast<net::io_context&>(exec.context());
        HKU_CHECK(m_ctx != nullptr, "Cannot get io_context from execution context");
    }

#if !HKU_ENABLE_HTTP_CLIENT_SSL
    HKU_CHECK(!m_is_https,
              "HTTPS is not supported. Please enable SSL support with --http_client_ssl=y");
#endif

    // 构建完整的 URI
    std::ostringstream uri_stream;
    uri_stream << m_base_path;
    if (!path.empty()) {
        if (m_base_path.back() != '/' && path.front() != '/') {
            uri_stream << '/';
        }
        uri_stream << path;
    }

    // 添加查询参数
    bool first = true;
    for (const auto& [key, value] : params) {
        uri_stream << (first ? "?" : "&") << key << "=" << value;
        first = false;
    }

    std::string uri = uri_stream.str();

    HttpStreamResponseAsync response;

    try {
        // 从连接池获取连接（自动处理 DNS 缓存和连接复用）
        auto [conn, is_new] = co_await _getConnection();
        HKU_CHECK(conn != nullptr, "Failed to get connection from pool");

        // 创建 HTTP 请求
        http::request<http::string_body> req;
        req.method(http::string_to_verb(method));
        req.target(uri);
        req.version(11);

        for (const auto& [key, value] : m_default_headers) {
            req.set(key, value);
        }

        for (const auto& [key, value] : headers) {
            req.set(key, value);
        }

        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::host, m_host);
        req.set(http::field::connection, "close");

        if (body != nullptr && body_len > 0) {
#if HKU_ENABLE_HTTP_CLIENT_ZIP
            auto content_type = req["Content-Type"];
            if (content_type == "gzip") {
                gzip::Compressor comp(Z_DEFAULT_COMPRESSION);
                std::string output;
                comp.compress(output, body, body_len);
                req.body() = std::move(output);
            } else {
                req.set(http::field::content_type, content_type);
                req.body() = std::string(body, body_len);
                req.prepare_payload();
            }
#else
            req.set(http::field::content_type, content_type);
            req.body() = std::string(body, body_len);
            req.prepare_payload();
#endif
        }

        // 发送请求 - 使用事件驱动方式
        {
            auto timer = net::steady_timer{*m_ctx};
            timer.expires_after(m_timeout);
            
#if HKU_ENABLE_HTTP_CLIENT_SSL
            if (conn->ssl_socket) {
                struct WriteOp {
                    ssl::stream<tcp::socket>& stream;
                    http::request<http::string_body>& req;
                    
                    net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                        auto [ec, bytes] = co_await http::async_write(
                            stream, req,
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return std::make_pair(ec, bytes);
                    }
                };
                
                auto write_op = WriteOp{*conn->ssl_socket, req};
                auto [write_ec, bytes_transferred] = co_await write_op.run();
                
                if (!timer.cancel()) {
                    HKU_THROW_EXCEPTION(HttpTimeoutException, "HTTP write timeout");
                }
                
                if (write_ec) {
                    HKU_THROW("HTTP write failed: {}", write_ec.message());
                }
            } else {
#endif
                struct WriteOp {
                    tcp::socket& sock;
                    http::request<http::string_body>& req;
                    
                    net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                        auto [ec, bytes] = co_await http::async_write(
                            sock, req,
                            net::as_tuple(net::use_awaitable)
                        );
                        co_return std::make_pair(ec, bytes);
                    }
                };
                
                auto write_op = WriteOp{conn->socket.value(), req};
                auto [write_ec, bytes_transferred] = co_await write_op.run();
                
                if (!timer.cancel()) {
                    HKU_THROW_EXCEPTION(HttpTimeoutException, "HTTP write timeout");
                }
                
                if (write_ec) {
                    HKU_THROW("HTTP write failed: {}", write_ec.message());
                }
#if HKU_ENABLE_HTTP_CLIENT_SSL
            }
#endif
        }

        // 流式读取响应 - 使用 buffer_body
        beast::flat_buffer buffer;
        http::response_parser<http::buffer_body> parser;

        // 设置缓冲区大小（8KB 块）
        constexpr size_t BUFFER_SIZE = 8192;
        std::vector<char> chunk_buffer(BUFFER_SIZE);

        parser.get().body().data = chunk_buffer.data();
        parser.get().body().size = BUFFER_SIZE;

        {
            // 读取响应头 - 使用事件驱动方式
            {
                auto timer = net::steady_timer{*m_ctx};
                timer.expires_after(m_timeout);
                
#if HKU_ENABLE_HTTP_CLIENT_SSL
                if (conn->ssl_socket) {
                    struct ReadHeaderOp {
                        ssl::stream<tcp::socket>& stream;
                        beast::flat_buffer& buffer;
                        http::response_parser<http::buffer_body>& parser;
                        
                        net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                            auto [ec, bytes] = co_await http::async_read_header(
                                stream, buffer, parser,
                                net::as_tuple(net::use_awaitable)
                            );
                            co_return std::make_pair(ec, bytes);
                        }
                    };
                    
                    auto read_header_op = ReadHeaderOp{*conn->ssl_socket, buffer, parser};
                    auto [read_ec, bytes_transferred] = co_await read_header_op.run();
                    
                    if (!timer.cancel()) {
                        HKU_THROW_EXCEPTION(HttpTimeoutException, "HTTP read header timeout");
                    }
                    
                    if (read_ec) {
                        HKU_THROW("HTTP read header failed: {}", read_ec.message());
                    }
                } else {
#endif
                    struct ReadHeaderOp {
                        tcp::socket& sock;
                        beast::flat_buffer& buffer;
                        http::response_parser<http::buffer_body>& parser;
                        
                        net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                            auto [ec, bytes] = co_await http::async_read_header(
                                sock, buffer, parser,
                                net::as_tuple(net::use_awaitable)
                            );
                            co_return std::make_pair(ec, bytes);
                        }
                    };
                    
                    auto read_header_op = ReadHeaderOp{conn->socket.value(), buffer, parser};
                    auto [read_ec, bytes_transferred] = co_await read_header_op.run();
                    
                    if (!timer.cancel()) {
                        HKU_THROW_EXCEPTION(HttpTimeoutException, "HTTP read header timeout");
                    }
                    
                    if (read_ec) {
                        HKU_THROW("HTTP read header failed: {}", read_ec.message());
                    }
#if HKU_ENABLE_HTTP_CLIENT_SSL
                }
#endif
            }

            // 填充响应对象
            response.m_status = static_cast<int>(parser.get().result_int());
            response.m_reason = std::string(parser.get().reason());
            for (auto it = parser.get().begin(); it != parser.get().end(); ++it) {
                response.m_headers.emplace(std::string(it->name_string()),
                                           std::string(it->value()));
            }

            // 循环读取响应体数据块
            while (!parser.is_done()) {
                auto timer = net::steady_timer{*m_ctx};
                timer.expires_after(m_timeout);
                
                std::size_t bytes_transferred = 0;
                boost::system::error_code read_ec;
                
#if HKU_ENABLE_HTTP_CLIENT_SSL
                if (conn->ssl_socket) {
                    struct ReadOp {
                        ssl::stream<tcp::socket>& stream;
                        beast::flat_buffer& buffer;
                        http::response_parser<http::buffer_body>& parser;
                        
                        net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                            auto [ec, bytes] = co_await http::async_read(
                                stream, buffer, parser,
                                net::as_tuple(net::use_awaitable)
                            );
                            co_return std::make_pair(ec, bytes);
                        }
                    };
                    
                    auto read_op = ReadOp{*conn->ssl_socket, buffer, parser};
                    auto [ec, bytes] = co_await read_op.run();
                    read_ec = ec;
                    bytes_transferred = bytes;
                    
                    if (!timer.cancel()) {
                        HKU_THROW_EXCEPTION(HttpTimeoutException, "HTTP read body timeout");
                    }
                    
                    if (read_ec && read_ec != http::error::need_buffer) {
                        HKU_THROW("HTTP read body failed: {}", read_ec.message());
                    }
                } else {
#endif
                    struct ReadOp {
                        tcp::socket& sock;
                        beast::flat_buffer& buffer;
                        http::response_parser<http::buffer_body>& parser;
                        
                        net::awaitable<std::pair<boost::system::error_code, std::size_t>> run() {
                            auto [ec, bytes] = co_await http::async_read(
                                sock, buffer, parser,
                                net::as_tuple(net::use_awaitable)
                            );
                            co_return std::make_pair(ec, bytes);
                        }
                    };
                    
                    auto read_op = ReadOp{conn->socket.value(), buffer, parser};
                    auto [ec, bytes] = co_await read_op.run();
                    read_ec = ec;
                    bytes_transferred = bytes;
                    
                    if (!timer.cancel()) {
                        HKU_THROW_EXCEPTION(HttpTimeoutException, "HTTP read body timeout");
                    }
                    
                    if (read_ec && read_ec != http::error::need_buffer) {
                        HKU_THROW("HTTP read body failed: {}", read_ec.message());
                    }
#if HKU_ENABLE_HTTP_CLIENT_SSL
                }
#endif
                
                // 调用回调处理数据块
                if (bytes_transferred > 0) {
                    response.m_total_bytes_read += bytes_transferred;
                    chunk_callback(chunk_buffer.data(), bytes_transferred);
                }
                
                // 重置缓冲区供下一次读取使用
                parser.get().body().data = chunk_buffer.data();
                parser.get().body().size = BUFFER_SIZE;
            }
        }

        // 不关闭连接，让连接池自动管理（连接会被归还到池中）

    } catch (const boost::system::system_error& e) {
        HKU_ERROR("HTTP stream request system error! {}", e.what());
        throw;
    } catch (const std::exception& e) {
        HKU_ERROR("HTTP stream request failed! {}", e.what());
        throw;
    }

    co_return response;
}

}  // namespace hku
