/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2026-03-15
 *      Author: fasiondog
 */

#include "HttpAsyncClient.h"
#include "hikyuu/utilities/Log.h"
#include "hikyuu/utilities/os.h"

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

struct HttpAsyncClient::Impl {
    net::io_context& ctx;
    std::chrono::milliseconds timeout;
#if HKU_ENABLE_HTTP_CLIENT_SSL
    ssl::context ssl_ctx;  // SSL 上下文

    Impl(net::io_context& io_ctx, std::chrono::milliseconds ms)
    : ctx(io_ctx), timeout(ms), ssl_ctx(ssl::context::tls_client) {
        // 配置 SSL 上下文
        ssl_ctx.set_default_verify_paths();
        ssl_ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 |
                            ssl::context::no_sslv3 | ssl::context::no_tlsv1 |
                            ssl::context::no_tlsv1_1);
        // 使用 OpenSSL 原生 API 设置最低 TLS 版本
        SSL_CTX_set_min_proto_version(ssl_ctx.native_handle(), TLS1_2_VERSION);
    }
#else
    Impl(net::io_context& io_ctx, std::chrono::milliseconds ms) : ctx(io_ctx), timeout(ms) {}
#endif
};

HttpAsyncClient::HttpAsyncClient() = default;

HttpAsyncClient::HttpAsyncClient(const std::string& url, std::chrono::milliseconds timeout)
: m_url(url), m_timeout(timeout) {
    _parseUrl();
}

HttpAsyncClient::HttpAsyncClient(net::io_context& ctx, const std::string& url,
                                 std::chrono::milliseconds timeout)
: m_url(url), m_timeout(timeout), m_ctx(&ctx), m_impl(std::make_unique<Impl>(ctx, timeout)) {
    _parseUrl();
}

HttpAsyncClient::~HttpAsyncClient() = default;

HttpAsyncClient::HttpAsyncClient(HttpAsyncClient&& rhs) noexcept
: m_impl(std::move(rhs.m_impl)),
  m_is_valid_url(rhs.m_is_valid_url),
  m_is_https(rhs.m_is_https),
  m_url(std::move(rhs.m_url)),
  m_base_path(std::move(rhs.m_base_path)),
  m_host(std::move(rhs.m_host)),
  m_port(std::move(rhs.m_port)),
  m_timeout(rhs.m_timeout),
  m_ctx(rhs.m_ctx),
  m_default_headers(std::move(rhs.m_default_headers)) {}

HttpAsyncClient& HttpAsyncClient::operator=(HttpAsyncClient&& rhs) noexcept {
    if (this != &rhs) {
        m_impl = std::move(rhs.m_impl);
        m_is_valid_url = rhs.m_is_valid_url;
        m_is_https = rhs.m_is_https;
        m_url = std::move(rhs.m_url);
        m_base_path = std::move(rhs.m_base_path);
        m_host = std::move(rhs.m_host);
        m_port = std::move(rhs.m_port);
        m_timeout = rhs.m_timeout;
        m_ctx = rhs.m_ctx;
        m_default_headers = std::move(rhs.m_default_headers);
    }
    return *this;
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
            return;
        }
        host = host.substr(0, pos);
    }

    m_base_path = std::move(base_path);
    m_host = std::move(host);
    m_port = std::to_string(port);
}

net::awaitable<HttpResponseAsync> HttpAsyncClient::request(
  const std::string& method, const std::string& path, const HttpParams& params,
  const HttpHeaders& headers, const char* body, size_t body_len, const std::string& content_type) {
    HKU_CHECK(m_is_valid_url, "Invalid url: {}", m_url);

    // 如果没有设置 io_context，从当前协程的 execution context 获取
    if (m_ctx == nullptr) {
        auto exec = co_await net::this_coro::executor;
        m_ctx = &static_cast<net::io_context&>(exec.context());
        HKU_CHECK(m_ctx != nullptr, "Cannot get io_context from execution context");
        m_impl = std::make_unique<Impl>(*m_ctx, m_timeout);
    }

#if !HKU_ENABLE_HTTP_CLIENT_SSL
    HKU_CHECK(m_is_https,
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
        // 创建 socket（使用 variant 存储普通 socket 或 SSL socket）
        struct SocketVariant {
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
        } socket_variant;

        // DNS 解析（带超时）
#if HKU_OS_OSX || HKU_OS_IOS
        // 用于 macOS 原生 DNS 解析, beast 解析存在已知问题, 会卡死
        std::vector<tcp::endpoint> dns_endpoints;
        {
            HKU_INFO("Starting DNS resolve for {}:{} (https={})", m_host, m_port, m_is_https);

            struct addrinfo hints, *res = nullptr;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            int ret = getaddrinfo(m_host.c_str(), m_port.c_str(), &hints, &res);

            if (ret != 0) {
                HKU_ERROR("getaddrinfo failed: {}", gai_strerror(ret));
                HKU_THROW("DNS resolve failed");
            }

            // 将结果转换为 endpoint 列表
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
        }
#else
        tcp::resolver::results_type dns_endpoints;
        {
            struct ResolveOp {
                tcp::resolver& resolver;
                std::string host, port;
                tcp::resolver::results_type endpoints;
                boost::system::error_code ec;
                bool done = false;

                ResolveOp(tcp::resolver& r, const std::string& h, const std::string& p)
                : resolver(r), host(h), port(p) {}
            };

            auto op = std::make_shared<ResolveOp>(resolver, host, std::to_string(port));

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
        }
#endif

        // 连接（带超时）
        boost::system::error_code connect_ec;
        bool connected = false;

        for (const auto& endpoint : dns_endpoints) {
            socket_variant.close(connect_ec);

            {
                struct ConnectOp {
                    tcp::socket& socket;
                    tcp::endpoint endpoint;
                    boost::system::error_code ec;
                    bool done = false;

                    ConnectOp(tcp::socket& s, const tcp::endpoint& e) : socket(s), endpoint(e) {}
                };

                // 先创建普通 socket
                socket_variant.plain.emplace(*m_ctx);
                auto op = std::make_shared<ConnectOp>(*socket_variant.plain, endpoint);

                socket_variant.plain->async_connect(endpoint,
                                                    [op](const boost::system::error_code& e) {
                                                        op->ec = e;
                                                        op->done = true;
                                                    });

                // 轮询等待连接完成或超时
                auto timer = net::steady_timer{*m_ctx};
                auto deadline = std::chrono::steady_clock::now() + m_timeout;

                while (!op->done && std::chrono::steady_clock::now() < deadline) {
                    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       deadline - std::chrono::steady_clock::now())
                                       .count();

                    timer.expires_after(
                      std::chrono::milliseconds(std::min<long long>(remaining, 50)));
                    co_await timer.async_wait(net::use_awaitable);
                }

                if (!op->done) {
                    continue;  // 超时，尝试下一个端点
                }

                if (!op->ec && socket_variant.plain->is_open()) {
                    connected = true;
                    break;
                }
            }
        }

        if (!connected) {
            HKU_THROW("Failed to connect to {}:{}", m_host, m_port);
        }

        // 设置 socket 选项
        socket_variant.socket().set_option(tcp::no_delay(true));

#if HKU_ENABLE_HTTP_CLIENT_SSL
        // 如果是 HTTPS，进行 SSL 握手
        if (m_is_https) {
            HKU_INFO("Performing SSL handshake with {}", m_host);

            // 移动到 SSL socket
            socket_variant.ssl.emplace(std::move(*socket_variant.plain), m_impl->ssl_ctx);
            socket_variant.plain.reset();

            // 设置 SNI（Server Name Indication）
            SSL_set_tlsext_host_name(socket_variant.ssl->native_handle(), m_host.c_str());

            // SSL 握手（带超时）
            struct SslHandshakeOp {
                ssl::stream<tcp::socket>& stream;
                boost::system::error_code ec;
                bool done = false;

                SslHandshakeOp(ssl::stream<tcp::socket>& s) : stream(s) {}
            };

            auto handshake_op = std::make_shared<SslHandshakeOp>(*socket_variant.ssl);

            socket_variant.ssl->async_handshake(ssl::stream_base::client,
                                                [handshake_op](const boost::system::error_code& e) {
                                                    handshake_op->ec = e;
                                                    handshake_op->done = true;
                                                });

            // 轮询等待 SSL 握手完成或超时
            auto timer = net::steady_timer{*m_ctx};
            auto deadline = std::chrono::steady_clock::now() + m_timeout;

            while (!handshake_op->done && std::chrono::steady_clock::now() < deadline) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   deadline - std::chrono::steady_clock::now())
                                   .count();

                timer.expires_after(std::chrono::milliseconds(std::min<long long>(remaining, 50)));
                co_await timer.async_wait(net::use_awaitable);
            }

            if (!handshake_op->done) {
                HKU_THROW_EXCEPTION(HttpTimeoutException, "SSL handshake timeout");
            }

            if (handshake_op->ec) {
                HKU_THROW("SSL handshake failed: {}", handshake_op->ec.message());
            }

            HKU_INFO("SSL handshake successful");
        }
#endif

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
        req.set(http::field::connection, "close");

        // 添加请求体
        if (body != nullptr && body_len > 0) {
            req.set(http::field::content_type, content_type);
            req.body() = std::string(body, body_len);
            req.prepare_payload();
        }

        // 发送请求（带超时）
        {
            struct WriteOp {
                boost::system::error_code ec;
                std::size_t bytes_transferred;
                bool done = false;
            };

            auto op = std::make_shared<WriteOp>();

#if HKU_ENABLE_HTTP_CLIENT_SSL
            if (socket_variant.is_ssl()) {
                // SSL 连接：使用 SSL stream 发送
                http::async_write(*socket_variant.ssl, req,
                                  [op](const boost::system::error_code& e, std::size_t n) {
                                      op->ec = e;
                                      op->bytes_transferred = n;
                                      op->done = true;
                                  });
            } else {
#endif
                // 普通连接：使用 socket 发送
                http::async_write(socket_variant.socket(), req,
                                  [op](const boost::system::error_code& e, std::size_t n) {
                                      op->ec = e;
                                      op->bytes_transferred = n;
                                      op->done = true;
                                  });
#if HKU_ENABLE_HTTP_CLIENT_SSL
            }
#endif

            // 轮询等待发送完成或超时
            auto timer = net::steady_timer{*m_ctx};
            auto deadline = std::chrono::steady_clock::now() + m_timeout;

            while (!op->done && std::chrono::steady_clock::now() < deadline) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   deadline - std::chrono::steady_clock::now())
                                   .count();

                timer.expires_after(std::chrono::milliseconds(std::min<long long>(remaining, 50)));
                co_await timer.async_wait(net::use_awaitable);
            }

            if (!op->done) {
                HKU_THROW_EXCEPTION(HttpTimeoutException, "HTTP write timeout");
            }

            if (op->ec) {
                HKU_THROW("HTTP write failed: {}", op->ec.message());
            }
        }

        // 读取响应（带超时）
        beast::flat_buffer buffer;
        http::response<http::string_body> res;

        {
            struct ReadOp {
                boost::system::error_code ec;
                std::size_t bytes_transferred;
                bool done = false;
            };

            auto op = std::make_shared<ReadOp>();

#if HKU_ENABLE_HTTP_CLIENT_SSL
            if (socket_variant.is_ssl()) {
                // SSL 连接：使用 SSL stream 读取
                http::async_read(*socket_variant.ssl, buffer, res,
                                 [op](const boost::system::error_code& e, std::size_t n) {
                                     op->ec = e;
                                     op->bytes_transferred = n;
                                     op->done = true;
                                 });
            } else {
#endif
                // 普通连接：使用 socket 读取
                http::async_read(socket_variant.socket(), buffer, res,
                                 [op](const boost::system::error_code& e, std::size_t n) {
                                     op->ec = e;
                                     op->bytes_transferred = n;
                                     op->done = true;
                                 });
#if HKU_ENABLE_HTTP_CLIENT_SSL
            }
#endif

            // 轮询等待读取完成或超时
            auto timer = net::steady_timer{*m_ctx};
            auto deadline = std::chrono::steady_clock::now() + m_timeout;

            while (!op->done && std::chrono::steady_clock::now() < deadline) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   deadline - std::chrono::steady_clock::now())
                                   .count();

                timer.expires_after(std::chrono::milliseconds(std::min<long long>(remaining, 50)));
                co_await timer.async_wait(net::use_awaitable);
            }

            if (!op->done) {
                HKU_THROW_EXCEPTION(HttpTimeoutException, "HTTP read timeout");
            }

            if (op->ec) {
                HKU_THROW("HTTP read failed: {}", op->ec.message());
            }
        }

        // 填充响应对象
        response.m_status = res.result_int();
        response.m_reason = std::string(res.reason());
        response.m_body = std::move(res.body());
#if HKU_ENABLE_HTTP_CLIENT_ZIP
        if (res["Content-Encoding"] == "gzip") {
            response.m_body = gzip::decompress(res.body(), res.body().size());
        } else {
            response.m_body = std::move(res.body());
        }
#endif

        for (auto it = res.begin(); it != res.end(); ++it) {
            response.m_headers.emplace(std::string(it->name_string()), std::string(it->value()));
        }

        // 关闭连接
        beast::error_code shutdown_ec;
        socket_variant.socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
        socket_variant.close(shutdown_ec);  // 确保所有资源被清理

    } catch (const boost::system::system_error& e) {
        HKU_ERROR("HTTP request system error: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        HKU_ERROR("HTTP request failed: {}", e.what());
        throw;
    }

    co_return response;
}

}  // namespace hku
