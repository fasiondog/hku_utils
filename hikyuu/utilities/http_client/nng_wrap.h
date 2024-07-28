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
#include "hikyuu/utilities/Log.h"

#include "hikyuu/utilities/config.h"
#if HKU_ENABLE_HTTP_CLIENT_SSL
#include <nng/supplemental/tls/tls.h>
#endif

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

    url& operator=(const url&) = delete;
    url& operator=(url&& rhs) noexcept {
        if (this != &rhs) {
            if (m_url != nullptr) {
                nng_url_free(m_url);
            }
            m_url = rhs.m_url;
            rhs.m_url = nullptr;
        }
        return *this;
    }

    ~url() {
        if (m_url) {
            nng_url_free(m_url);
        }
    }

    const std::string& raw_url() const noexcept {
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

    bool is_https() const noexcept {
        return m_url == nullptr ? false : strcmp("https", m_url->u_scheme) == 0;
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
        if (m_aio == nullptr) {
            NNG_CHECK(nng_aio_alloc(&m_aio, NULL, NULL));
        }
    }

    void release() {
        if (m_aio) {
            nng_aio_free(m_aio);
            m_aio = nullptr;
        }
    }

    void wait() {
        nng_aio_wait(m_aio);
    }

    int result() {
        return nng_aio_result(m_aio);
    }

    void* get_output(unsigned index) {
        return nng_aio_get_output(m_aio, index);
    }

    /*
     * 0 - 恢复默认值
     * <0 - 不限制
     */
    void set_timeout(int32_t ms) {
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

    nng_aio* operator->() const noexcept {
        return m_aio;
    }

private:
    nng_aio* m_aio{nullptr};
};

#if HKU_ENABLE_HTTP_CLIENT_SSL
class tls_config final {
public:
    tls_config() = default;

    tls_config(const tls_config& th) : m_cfg(th.m_cfg) {
        if (m_cfg) {
            nng_tls_config_hold(th.m_cfg);
        }
    }

    tls_config(tls_config&& rhs) : m_cfg(rhs.m_cfg) {
        rhs.m_cfg = nullptr;
    }

    ~tls_config() {
        if (m_cfg) {
            nng_tls_config_free(m_cfg);
        }
    }

    tls_config& operator=(const tls_config& th) {
        if (this != &th) {
            m_cfg = th.m_cfg;
            if (m_cfg) {
                nng_tls_config_hold(m_cfg);
            }
        }
        return *this;
    }

    tls_config& operator=(tls_config&& rhs) {
        if (this != &rhs) {
            m_cfg = rhs.m_cfg;
            rhs.m_cfg = nullptr;
        }
        return *this;
    }

    void release() {
        if (m_cfg) {
            nng_tls_config_free(m_cfg);
            m_cfg = nullptr;
        }
    }

    nng_tls_config* get() const noexcept {
        return m_cfg;
    }

    tls_config& set_ca_file(const std::string& filename) {
        NNG_CHECK(alloc());
        NNG_CHECK(nng_tls_config_ca_file(m_cfg, filename.c_str()));
        return *this;
    }

private:
    int alloc() {
        return m_cfg ? 0 : nng_tls_config_alloc(&m_cfg, NNG_TLS_MODE_CLIENT);
    }

private:
    nng_tls_config* m_cfg{nullptr};
};
#endif  // #if HKU_ENABLE_HTTP_CLIENT_SSL

class http_client final {
public:
    http_client() = default;
    ~http_client() {
        if (m_client) {
            nng_http_client_free(m_client);
        }
    }

    void set_url(const nng::url& url) {
#if !HKU_ENABLE_HTTP_CLIENT_SSL
        if (url.is_https()) {
            HKU_THROW("Not support https: ({})! Please compile with --http_client_ssl",
                      url.raw_url());
        }
#endif
        if (!m_client) {
            NNG_CHECK(nng_http_client_alloc(&m_client, url.get()));
        }
    }

    void connect(nng_aio* aio) {
        nng_http_client_connect(m_client, aio);
    }

    void set_tls(nng_tls_config* cfg) {
        NNG_CHECK(nng_http_client_set_tls(m_client, cfg));
    }

    nng_http_client* get() const noexcept {
        return m_client;
    }

    nng_http_client* operator->() const noexcept {
        return m_client;
    }

    explicit operator bool() const noexcept {
        return m_client != nullptr;
    }

    void release() {
        if (m_client) {
            nng_http_client_free(m_client);
            m_client = nullptr;
        }
    }

private:
    nng_http_client* m_client{nullptr};
};

class http_conn final {
public:
    http_conn() = default;
    explicit http_conn(nng_http_conn* conn_) noexcept : m_conn(conn_) {}

    http_conn(const http_conn&) = delete;

    http_conn(http_conn&& rhs) noexcept : m_conn(rhs.m_conn) {
        rhs.m_conn = nullptr;
    }

    http_conn& operator=(const http_conn& rhs) = delete;

    http_conn& operator=(http_conn&& rhs) noexcept {
        if (this != &rhs) {
            if (m_conn != nullptr) {
                nng_http_conn_close(m_conn);
            }
            m_conn = rhs.m_conn;
            rhs.m_conn = nullptr;
        }
        return *this;
    }

    ~http_conn() {
        if (m_conn) {
            nng_http_conn_close(m_conn);
        }
    }

    void close() {
        if (m_conn) {
            nng_http_conn_close(m_conn);
            m_conn = nullptr;
        }
    }

    nng_http_conn* get() const noexcept {
        return m_conn;
    }

    nng_http_conn* operator->() const noexcept {
        return m_conn;
    }

    bool valid() const noexcept {
        return m_conn != nullptr;
    }

    void write_req(nng_http_req* req, nng_aio* aio) {
        nng_http_conn_write_req(m_conn, req, aio);
    }

    void read_res(nng_http_res* res, nng_aio* aio) {
        nng_http_conn_read_res(m_conn, res, aio);
    }

    void read_all(nng_aio* aio) {
        nng_http_conn_read_all(m_conn, aio);
    }

private:
    nng_http_conn* m_conn{nullptr};
};

class req final {};

}  // namespace nng
}  // namespace hku

#endif