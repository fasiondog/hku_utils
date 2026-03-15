/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2026-03-15
 *      Author: fasiondog
 */

#pragma once
#ifndef HKU_UTILS_HTTP_ASYNC_CLIENT_H
#define HKU_UTILS_HTTP_ASYNC_CLIENT_H

#include "hikyuu/utilities/config.h"
#if !HKU_ENABLE_HTTP_CLIENT
#error "Don't enable http client, please config with --http_client=y"
#endif

#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "hikyuu/utilities/Log.h"

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

namespace hku {

using json = nlohmann::json;

class HKU_UTILS_API HttpAsyncClient;

class HKU_UTILS_API HttpResponseAsync final {
    friend class HKU_UTILS_API HttpAsyncClient;

public:
    HttpResponseAsync() = default;
    ~HttpResponseAsync() = default;

    HttpResponseAsync(const HttpResponseAsync&) = delete;
    HttpResponseAsync& operator=(const HttpResponseAsync&) = delete;

    HttpResponseAsync(HttpResponseAsync&& rhs) = default;
    HttpResponseAsync& operator=(HttpResponseAsync&& rhs) = default;

    const std::string& body() const noexcept {
        return m_body;
    }

    hku::json json() const {
        return json::parse(m_body);
    }

    int status() const noexcept {
        return m_status;
    }

    std::string reason() const {
        return m_reason;
    }

    std::string getHeader(const std::string& key) const {
        auto it = m_headers.find(key);
        return it != m_headers.end() ? it->second : std::string();
    }

    size_t getContentLength() const {
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

class HKU_UTILS_API HttpAsyncClient {
public:
    using executor_type = boost::asio::any_io_executor;

    HttpAsyncClient();
    explicit HttpAsyncClient(const std::string& url, std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    explicit HttpAsyncClient(boost::asio::io_context& ctx, const std::string& url, std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    virtual ~HttpAsyncClient();

    HttpAsyncClient(const HttpAsyncClient&) = delete;
    HttpAsyncClient& operator=(const HttpAsyncClient&) = delete;

    HttpAsyncClient(HttpAsyncClient&& rhs) noexcept;
    HttpAsyncClient& operator=(HttpAsyncClient&& rhs) noexcept;

    bool valid() const noexcept {
        return !m_url.empty();
    }

    const std::string& url() const noexcept {
        return m_url;
    }

    void setUrl(const std::string& url) {
        m_url = url;
    }

    void setTimeout(std::chrono::milliseconds ms) {
        m_timeout = ms;
    }

    std::chrono::milliseconds getTimeout() const noexcept {
        return m_timeout;
    }

    void setDefaultHeaders(const std::map<std::string, std::string>& headers) {
        m_default_headers = headers;
    }

    void setDefaultHeaders(std::map<std::string, std::string>&& headers) {
        m_default_headers = std::move(headers);
    }

    // 异步请求方法 - 返回 boost::asio::awaitable
    boost::asio::awaitable<HttpResponseAsync> request(
        const std::string& method, 
        const std::string& path,
        const std::map<std::string, std::string>& params, 
        const std::map<std::string, std::string>& headers, 
        const char* body,
        size_t body_len, 
        const std::string& content_type);

    boost::asio::awaitable<HttpResponseAsync> get(
        const std::string& path, 
        const std::map<std::string, std::string>& headers = {}) {
        co_return co_await request("GET", path, {}, headers, nullptr, 0, "");
    }

    boost::asio::awaitable<HttpResponseAsync> get(
        const std::string& path, 
        const std::map<std::string, std::string>& params,
        const std::map<std::string, std::string>& headers) {
        co_return co_await request("GET", path, params, headers, nullptr, 0, "");
    }

    boost::asio::awaitable<HttpResponseAsync> post(
        const std::string& path, 
        const std::map<std::string, std::string>& params, 
        const std::map<std::string, std::string>& headers,
        const char* body, 
        size_t len, 
        const std::string& content_type) {
        co_return co_await request("POST", path, params, headers, body, len, content_type);
    }

    boost::asio::awaitable<HttpResponseAsync> post(
        const std::string& path, 
        const std::map<std::string, std::string>& headers, 
        const char* body,
        size_t len, 
        const std::string& content_type) {
        co_return co_await request("POST", path, {}, headers, body, len, content_type);
    }

    boost::asio::awaitable<HttpResponseAsync> post(
        const std::string& path, 
        const std::map<std::string, std::string>& params, 
        const std::map<std::string, std::string>& headers,
        const std::string& content, 
        const std::string& content_type = "text/plain") {
        co_return co_await post(path, params, headers, content.data(), content.size(), content_type);
    }

    boost::asio::awaitable<HttpResponseAsync> post(
        const std::string& path, 
        const std::map<std::string, std::string>& headers,
        const std::string& content, 
        const std::string& content_type = "text/plain") {
        co_return co_await post(path, {}, headers, content, content_type);
    }

    boost::asio::awaitable<HttpResponseAsync> post(
        const std::string& path, 
        const std::map<std::string, std::string>& params, 
        const std::map<std::string, std::string>& headers,
        const json& body) {
        co_return co_await post(path, params, headers, body.dump(), "application/json");
    }

    boost::asio::awaitable<HttpResponseAsync> post(
        const std::string& path, 
        const std::map<std::string, std::string>& headers, 
        const json& body) {
        co_return co_await post(path, {}, headers, body);
    }

    boost::asio::awaitable<HttpResponseAsync> post(
        const std::string& path, 
        const json& body) {
        co_return co_await post(path, {}, body);
    }

    executor_type get_executor() const {
        HKU_CHECK(m_ctx != nullptr, "io_context is null");
        return m_ctx->get_executor();
    }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    std::string m_url;
    std::chrono::milliseconds m_timeout{30000};
    std::map<std::string, std::string> m_default_headers;
    boost::asio::io_context* m_ctx;  // 非 owned 指针，可能指向外部或内部的 io_context
    std::unique_ptr<boost::asio::io_context> m_own_ctx;  // 内部创建的 io_context
};

}  // namespace hku

#endif
