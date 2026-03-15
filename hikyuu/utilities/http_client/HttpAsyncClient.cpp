/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2026-03-15
 *      Author: fasiondog
 */

#include "HttpAsyncClient.h"

#include <sstream>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include "hikyuu/utilities/Log.h"
#include "hikyuu/utilities/os.h"

#ifdef __APPLE__
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace hku {

struct HttpAsyncClient::Impl {
    net::io_context& ctx;
    std::chrono::milliseconds timeout;
    
    Impl(net::io_context& io_ctx, std::chrono::milliseconds ms)
    : ctx(io_ctx), timeout(ms) {}
};

// URL 解析辅助函数
static std::tuple<std::string, std::string, unsigned short> parseUrl(const std::string& url) {
    std::string host, port_str, path;
    unsigned short port = 80;
    
    size_t scheme_end = url.find("://");
    std::string scheme;
    if (scheme_end != std::string::npos) {
        scheme = url.substr(0, scheme_end);
        if (scheme == "https") {
            port = 443;
        }
    } else {
        scheme_end = 0;
    }
    
    size_t host_start = scheme_end + 3;
    size_t path_start = url.find('/', host_start);
    
    std::string authority;
    if (path_start != std::string::npos) {
        authority = url.substr(host_start, path_start - host_start);
        path = url.substr(path_start);
    } else {
        authority = url.substr(host_start);
        path = "/";
    }
    
    size_t port_pos = authority.find(':');
    if (port_pos != std::string::npos) {
        host = authority.substr(0, port_pos);
        port_str = authority.substr(port_pos + 1);
        port = static_cast<unsigned short>(std::stoi(port_str));
    } else {
        host = authority;
    }
    
    return {host, path, port};
}

HttpAsyncClient::HttpAsyncClient()
: m_ctx(nullptr), m_own_ctx(std::make_unique<boost::asio::io_context>()) {
    m_ctx = m_own_ctx.get();
}

HttpAsyncClient::HttpAsyncClient(const std::string& url, std::chrono::milliseconds timeout)
: m_url(url), m_timeout(timeout), m_own_ctx(std::make_unique<boost::asio::io_context>()) {
    m_ctx = m_own_ctx.get();
}

HttpAsyncClient::HttpAsyncClient(boost::asio::io_context& ctx, const std::string& url, std::chrono::milliseconds timeout)
: m_url(url), m_timeout(timeout), m_ctx(&ctx), m_own_ctx(nullptr) {
}

HttpAsyncClient::~HttpAsyncClient() = default;

HttpAsyncClient::HttpAsyncClient(HttpAsyncClient&& rhs) noexcept
: m_impl(std::move(rhs.m_impl)),
  m_url(std::move(rhs.m_url)),
  m_timeout(rhs.m_timeout),
  m_default_headers(std::move(rhs.m_default_headers)),
  m_ctx(rhs.m_ctx),
  m_own_ctx(std::move(rhs.m_own_ctx)) {
    // 如果 rhs 使用的是内部 io_context，需要更新指针
    if (m_own_ctx) {
        m_ctx = m_own_ctx.get();
    }
}

HttpAsyncClient& HttpAsyncClient::operator=(HttpAsyncClient&& rhs) noexcept {
    if (this != &rhs) {
        m_impl = std::move(rhs.m_impl);
        m_url = std::move(rhs.m_url);
        m_timeout = rhs.m_timeout;
        m_default_headers = std::move(rhs.m_default_headers);
        m_ctx = rhs.m_ctx;
        m_own_ctx = std::move(rhs.m_own_ctx);
        
        // 如果使用的是内部 io_context，需要更新指针
        if (m_own_ctx) {
            m_ctx = m_own_ctx.get();
        }
    }
    return *this;
}

boost::asio::awaitable<HttpResponseAsync> HttpAsyncClient::request(
    const std::string& method, 
    const std::string& path,
    const std::map<std::string, std::string>& params, 
    const std::map<std::string, std::string>& headers, 
    const char* body,
    size_t body_len, 
    const std::string& content_type) {
    
    HKU_CHECK(!m_url.empty(), "Invalid url: {}", m_url);
    HKU_CHECK(m_ctx != nullptr, "io_context is null");
    
    auto [host, base_path, port] = parseUrl(m_url);
    
    // 构建完整的 URI
    std::ostringstream uri_stream;
    uri_stream << base_path;
    if (!path.empty()) {
        if (base_path.back() != '/' && path.front() != '/') {
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
    
    // 存储解析结果
    tcp::resolver::results_type resolver_results;
    
    HttpResponseAsync response;
    
    try {
        // 创建 socket
        tcp::socket socket{*m_ctx};
        
        // 创建 resolver
        tcp::resolver resolver{*m_ctx};
        
        // DNS 解析（带超时）
#ifdef __APPLE__
        // macOS 下使用原生 getaddrinfo API，避免 async_resolve 卡住的问题
        {
            HKU_INFO("Starting DNS resolve for {}:{}", host, port);
            
            struct ResolveTask {
                std::string host, port_str;
                std::vector<tcp::endpoint> endpoints;
                boost::system::error_code ec;
                bool done = false;
                
                ResolveTask(const std::string& h, const std::string& p)
                    : host(h), port_str(p) {}
                    
                void run() {
                    try {
                        HKU_INFO("Executing native DNS resolve for {}", host);
                        
                        struct addrinfo hints, *res = nullptr;
                        memset(&hints, 0, sizeof(hints));
                        hints.ai_family = AF_UNSPEC;  // 允许 IPv4 或 IPv6
                        hints.ai_socktype = SOCK_STREAM;
                        
                        int ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
                        
                        if (ret != 0) {
                            HKU_ERROR("getaddrinfo failed: {}", gai_strerror(ret));
                            ec = boost::asio::error::host_not_found;
                            return;
                        }
                        
                        // 将结果转换为 boost::asio 的 endpoint
                        std::vector<tcp::endpoint> endpoints;
                        for (struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
                            if (ai->ai_family == AF_INET) {
                                auto* sin = reinterpret_cast<sockaddr_in*>(ai->ai_addr);
                                tcp::endpoint ep(tcp::v4(), ntohs(sin->sin_port));
                                endpoints.push_back(ep);
                            } else if (ai->ai_family == AF_INET6) {
                                auto* sin6 = reinterpret_cast<sockaddr_in6*>(ai->ai_addr);
                                unsigned char* bytes = sin6->sin6_addr.s6_addr;
                                boost::asio::ip::address_v6::bytes_type ep_bytes;
                                std::copy(bytes, bytes + 16, ep_bytes.begin());
                                tcp::endpoint ep(boost::asio::ip::address_v6(ep_bytes), 
                                               ntohs(sin6->sin6_port));
                                endpoints.push_back(ep);
                            }
                        }
                        
                        freeaddrinfo(res);
                        
                        HKU_INFO("Native DNS resolve success, found {} endpoints", endpoints.size());
                        
                    } catch (const std::exception& e) {
                        HKU_ERROR("Native DNS resolve exception: {}", e.what());
                        ec = boost::asio::error::fault;
                    }
                    
                    done = true;
                }
            };
            
            auto task = std::make_shared<ResolveTask>(host, std::to_string(port));
            
            // 在线程中执行同步解析
            std::thread resolve_thread([task]() {
                task->run();
            });
            
            // 等待完成或超时
            auto deadline = std::chrono::steady_clock::now() + m_timeout;
            while (!task->done) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
                
                if (remaining <= 0) {
                    HKU_ERROR("DNS resolve timeout after {}ms", m_timeout.count());
                    resolve_thread.detach();  // 超时后分离线程
                    HKU_THROW("DNS resolve timeout");
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    std::min<long long>(remaining, 10)));
            }
            
            resolve_thread.join();
            
            if (task->ec) {
                HKU_THROW("DNS resolve failed: {}", task->ec.message());
            }
            
            if (task->endpoints.empty()) {
                HKU_THROW("No valid endpoints returned from DNS resolve");
            }
            
            // 使用临时解析器创建 results_type（因为不能直接构造）
            tcp::resolver tmp_resolver(*m_ctx);
            auto result = tmp_resolver.resolve(host, std::to_string(port));
            resolver_results = result;
        }
#else
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
            resolver.async_resolve(op->host, op->port,
                [op](const boost::system::error_code& e, tcp::resolver::results_type eps) {
                    op->ec = e;
                    op->endpoints = std::move(eps);
                    op->done = true;
                });
            
            // 等待操作完成或超时
            auto timer = net::steady_timer{*m_ctx};
            auto deadline = std::chrono::steady_clock::now() + m_timeout;
            
            while (!op->done) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
                
                if (remaining <= 0) {
                    HKU_THROW("DNS resolve timeout");
                }
                
                timer.expires_after(std::chrono::milliseconds(
                    std::min<long long>(remaining, 50)));
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
        
        for (const auto& endpoint : resolver_results) {
            socket.close(connect_ec);
            
            {
                struct ConnectOp {
                    tcp::socket& socket;
                    tcp::endpoint endpoint;
                    boost::system::error_code ec;
                    bool done = false;
                    
                    ConnectOp(tcp::socket& s, const tcp::endpoint& e)
                        : socket(s), endpoint(e) {}
                };
                
                auto op = std::make_shared<ConnectOp>(socket, endpoint);
                
                socket.async_connect(endpoint,
                    [op](const boost::system::error_code& e) {
                        op->ec = e;
                        op->done = true;
                    });
                
                // 轮询等待连接完成或超时
                auto timer = net::steady_timer{*m_ctx};
                auto deadline = std::chrono::steady_clock::now() + m_timeout;
                
                while (!op->done && std::chrono::steady_clock::now() < deadline) {
                    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                        deadline - std::chrono::steady_clock::now()).count();
                    
                    timer.expires_after(std::chrono::milliseconds(
                        std::min<long long>(remaining, 50)));
                    co_await timer.async_wait(net::use_awaitable);
                }
                
                if (!op->done) {
                    continue;  // 超时，尝试下一个端点
                }
                
                if (!op->ec && socket.is_open()) {
                    connected = true;
                    break;
                }
            }
        }
        
        if (!connected) {
            HKU_THROW("Failed to connect to {}:{}", host, port);
        }
        
        // 设置 socket 选项
        socket.set_option(tcp::no_delay(true));
        
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
        req.set(http::field::host, host);
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
                tcp::socket& socket;
                http::request<http::string_body>& req;
                boost::system::error_code ec;
                std::size_t bytes_transferred;
                bool done = false;
                
                WriteOp(tcp::socket& s, http::request<http::string_body>& r) : socket(s), req(r) {}
            };
            
            auto op = std::make_shared<WriteOp>(socket, req);
            
            http::async_write(socket, req,
                [op](const boost::system::error_code& e, std::size_t n) {
                    op->ec = e;
                    op->bytes_transferred = n;
                    op->done = true;
                });
            
            // 轮询等待发送完成或超时
            auto timer = net::steady_timer{*m_ctx};
            auto deadline = std::chrono::steady_clock::now() + m_timeout;
            
            while (!op->done && std::chrono::steady_clock::now() < deadline) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
                
                timer.expires_after(std::chrono::milliseconds(
                    std::min<long long>(remaining, 50)));
                co_await timer.async_wait(net::use_awaitable);
            }
            
            if (!op->done) {
                HKU_THROW("HTTP write timeout");
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
                tcp::socket& socket;
                beast::flat_buffer& buffer;
                http::response<http::string_body>& res;
                boost::system::error_code ec;
                std::size_t bytes_transferred;
                bool done = false;
                
                ReadOp(tcp::socket& s, beast::flat_buffer& b, http::response<http::string_body>& r)
                    : socket(s), buffer(b), res(r) {}
            };
            
            auto op = std::make_shared<ReadOp>(socket, buffer, res);
            
            http::async_read(socket, buffer, res,
                [op](const boost::system::error_code& e, std::size_t n) {
                    op->ec = e;
                    op->bytes_transferred = n;
                    op->done = true;
                });
            
            // 轮询等待读取完成或超时
            auto timer = net::steady_timer{*m_ctx};
            auto deadline = std::chrono::steady_clock::now() + m_timeout;
            
            while (!op->done && std::chrono::steady_clock::now() < deadline) {
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
                
                timer.expires_after(std::chrono::milliseconds(
                    std::min<long long>(remaining, 50)));
                co_await timer.async_wait(net::use_awaitable);
            }
            
            if (!op->done) {
                HKU_THROW("HTTP read timeout");
            }
            
            if (op->ec) {
                HKU_THROW("HTTP read failed: {}", op->ec.message());
            }
        }
        
        // 填充响应对象
        response.m_status = res.result_int();
        response.m_reason = std::string(res.reason());
        response.m_body = std::move(res.body());
        
        for (auto it = res.begin(); it != res.end(); ++it) {
            response.m_headers.emplace(std::string(it->name_string()), 
                                       std::string(it->value()));
        }
        
        // 关闭 socket
        beast::error_code shutdown_ec;
        socket.shutdown(tcp::socket::shutdown_both, shutdown_ec);
        
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
