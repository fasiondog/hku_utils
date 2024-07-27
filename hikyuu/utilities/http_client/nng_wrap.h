/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#pragma once
#ifndef HKU_UTILS_NNG_WRAP_H
#define HKU_UTILS_NNG_WRAP_H

#include <string>
#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>
#include "hikyuu/utilities/Log.h"

namespace hku {
namespace nng {

#ifndef NNG_CHECK
#define NNG_CHECK(rv)                                       \
    {                                                       \
        if (rv != 0) {                                      \
            HKU_THROW("[NNG_ERROR] {} ", nng_strerror(rv)); \
        }                                                   \
    }
#endif

#ifndef NNG_CHECK_M
#define NNG_CHECK_M(rv, ...)                                                              \
    {                                                                                     \
        if (rv != 0) {                                                                    \
            HKU_THROW("{} | [NNG_ERROR] {}", fmt::format(__VA_ARGS__), nng_strerror(rv)); \
        }                                                                                 \
    }
#endif

class url final {
public:
    url() = default;
    url(const std::string& url_) {
        HKU_WARN_IF(nng_url_parse(&m_url, url_.c_str()) != 0, "Invalid url: {}", url_);
        // NNG_CHECK_M(nng_url_parse(&m_url, url_.c_str()), "Invalid url: {}", url_);
    }

    url(const url&) = delete;
    url(url&& rhs) noexcept : m_url(rhs.m_url) {
        rhs.m_url = nullptr;
    }

    ~url() {
        if (m_url) {
            nng_url_free(m_url);
        }
    }

    void set_url(const std::string& url_) noexcept {
        if (m_url) {
            nng_url_free(m_url);
        }

        int rv = nng_url_parse(&m_url, url_.c_str());
        HKU_WARN_IF(rv != 0, "Invalid url: {}", url_);
    }

    nng_url* get() const noexcept {
        return m_url;
    }

    nng_url* operator->() const noexcept {
        return m_url;
    }

    explicit operator bool() const noexcept {
        return m_url != nullptr;
    }

private:
    nng_url* m_url{nullptr};
};

class http_client final {
public:
    http_client() = delete;
    http_client(const nng::url& url) {
        nng_http_client_alloc(&m_client, url.get());
    }

    ~http_client() {
        if (m_client) {
            nng_http_client_free(m_client);
        }
    }

    nng_http_client* get() const noexcept {
        return m_client;
    }

    explicit operator bool() const noexcept {
        return m_client != nullptr;
    }

private:
    nng_http_client* m_client{nullptr};
};

class req final {};

}  // namespace nng
}  // namespace hku

#endif