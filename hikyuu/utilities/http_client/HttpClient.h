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
#include "nng_wrap.h"

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

namespace hku {

class HKU_UTILS_API HttpClient {
public:
    HttpClient() = default;
    explicit HttpClient(const std::string& url) : m_url(nng::url(url)) {};
    virtual ~HttpClient();

    bool valid() const noexcept {
        return m_url.valid();
    }

    const std::string& url() const noexcept {
        return m_url.rawUrl();
    }

    void setUrl(const std::string& url) noexcept {
        m_url.setUrl(url);
    }

    void setTimeout(int32_t ms) {
        m_timeout_ms = ms;
    }

    void get(const std::string& path);

private:
    void _connect();

private:
    nng::url m_url;
    nng::http_client m_client;
    nng::aio m_aio;
    nng_http_conn* m_conn{nullptr};
    nng_tls_config* m_tls_cfg{nullptr};
    int32_t m_timeout_ms{0};
};

}  // namespace hku

#endif