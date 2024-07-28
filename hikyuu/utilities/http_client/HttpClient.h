/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#pragma once
#ifndef HKU_UTILS_HTTP_CLIENT_H
#define HKU_UTILS_HTTP_CLIENT_H

#include "hikyuu/utilities/config.h"
#if !HKU_ENABLE_HTTP_CLIENT
#error "Don't enable http client, please config with --http_client=y"
#endif

#include <string>
#include <nlohmann/json.hpp>
#include "nng_wrap.h"

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

using json = nlohmann::json;

namespace hku {

class HKU_UTILS_API HttpResponse;

class HKU_UTILS_API HttpClient {
public:
    HttpClient() = default;
    explicit HttpClient(const std::string& url) : m_url(nng::url(url)) {};
    virtual ~HttpClient();

    bool valid() const noexcept {
        return m_url.valid();
    }

    const std::string& url() const noexcept {
        return m_url.raw_url();
    }

    void setUrl(const std::string& url) noexcept {
        m_url = std::move(nng::url(url));
    }

    void setTimeout(int32_t ms) {
        m_timeout_ms = ms;
        reset();
    }

    void reset();

    HttpResponse get(const std::string& path);

private:
    void _connect();

private:
    nng::url m_url;
    nng::http_client m_client;
    nng::aio m_aio;
    nng::http_conn m_conn;
#if HKU_ENABLE_HTTP_CLIENT_SSL
    nng::tls_config m_tls_cfg;
#endif
    int32_t m_timeout_ms{0};
};

class HKU_UTILS_API HttpResponse final {
    friend class HKU_UTILS_API HttpClient;

public:
    HttpResponse();
    ~HttpResponse();

    HttpResponse(const HttpResponse&) = delete;
    HttpResponse& operator=(const HttpResponse&) = delete;

    HttpResponse(HttpResponse&& rhs);
    HttpResponse& operator=(HttpResponse&& rhs);

    const std::string& body() const noexcept {
        return m_body;
    }

    json json() const {
        return json::parse(m_body);
    }

    int status() const noexcept {
        return nng_http_res_get_status(m_res);
    }

    std::string reason() noexcept {
        return nng_http_res_get_reason(m_res);
    }

    std::string getHeader(const std::string& key) noexcept {
        const char* hdr = nng_http_res_get_header(m_res, key.c_str());
        return hdr ? std::string(hdr) : std::string();
    }

    size_t getContentLength() noexcept {
        std::string slen = getHeader("Content-Length");
        return slen.empty() ? 0 : std::stoi(slen);
    }

private:
    void _resizeBody(size_t len) {
        m_body.resize(len);
    }

    nng_http_res* get() const noexcept {
        return m_res;
    }

private:
    nng_http_res* m_res{nullptr};
    std::string m_body;
};

}  // namespace hku

#endif