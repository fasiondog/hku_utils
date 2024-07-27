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
struct HttpTimeoutException : hku::exception {
    HttpTimeoutException() : hku::exception("Http timeout!") {}
    virtual ~HttpTimeoutException() noexcept = default;
};
}  // namespace hku

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
    explicit url(const std::string& url_) noexcept : m_rawurl(url_) {
        nng_url_parse(&m_url, m_rawurl.c_str());
    }

    url(const url&) = delete;
    url(url&& rhs) noexcept : m_rawurl(std::move(rhs.m_rawurl)), m_url(rhs.m_url) {
        rhs.m_url = nullptr;
    }

    ~url() {
        if (m_url) {
            nng_url_free(m_url);
        }
    }

    void setUrl(const std::string& url_) noexcept {
        m_rawurl = url_;
        if (m_url) {
            nng_url_free(m_url);
            m_url = nullptr;
        }

        nng_url_parse(&m_url, m_rawurl.c_str());
    }

    const std::string& rawUrl() const noexcept {
        return m_rawurl;
    }

    nng_url* get() const noexcept {
        return m_url;
    }

    nng_url* operator->() const noexcept {
        return m_url;
    }

    bool valid() const noexcept {
        return m_url != nullptr;
    }

private:
    std::string m_rawurl;
    nng_url* m_url{nullptr};
};

class aio final {
public:
    aio() = default;
    aio(const aio&) = delete;
    ~aio() {
        if (m_aio) {
            nng_aio_free(m_aio);
        }
    }

    void alloc() {
        if (!m_aio) {
            NNG_CHECK(nng_aio_alloc(&m_aio, NULL, NULL));
        }
    }

    void wait() {
        nng_aio_wait(m_aio);
    }

    void result() {
        int rv = nng_aio_result(m_aio);
        HKU_IF_RETURN(rv == 0, void());

        if (rv == NNG_ETIMEDOUT) {
            throw HttpTimeoutException();
        } else {
            HKU_THROW("[NNG_ERROR] {} ", nng_strerror(rv));
        }
    }

    /*
     * 0 - 恢复默认值
     * <0 - 不限制
     */
    void setTimeout(int32_t ms) {
        // #define NNG_DURATION_INFINITE (-1)
        // #define NNG_DURATION_DEFAULT (-2)
        // #define NNG_DURATION_ZERO (0)
        if (ms > 0) {
            nng_aio_set_timeout(m_aio, ms);
        } else if (ms == 0) {
            nng_aio_set_timeout(m_aio, NNG_DURATION_DEFAULT);
        } else {
            nng_aio_set_timeout(m_aio, NNG_DURATION_INFINITE);
        }
    }

    nng_aio* get() const noexcept {
        return m_aio;
    }

private:
    nng_aio* m_aio{nullptr};
};

class http_client final {
public:
    http_client() = default;
    ~http_client() {
        if (m_client) {
            nng_http_client_free(m_client);
        }
    }

    void setUrl(const nng::url& url) {
        if (!m_client) {
            NNG_CHECK(nng_http_client_alloc(&m_client, url.get()));
        }
    }

    void connect(nng_aio* aio) {
        nng_http_client_connect(m_client, aio);
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