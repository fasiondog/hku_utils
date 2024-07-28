/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#include "hikyuu/utilities/Log.h"
#include "HttpClient.h"

namespace hku {

HttpResponse::HttpResponse() {
    NNG_CHECK(nng_http_res_alloc(&m_res));
}

HttpResponse::~HttpResponse() {
    if (m_res) {
        nng_http_res_free(m_res);
    }
}

HttpResponse::HttpResponse(HttpResponse&& rhs) : m_res(rhs.m_res), m_body(std::move(rhs.m_body)) {
    rhs.m_res = nullptr;
}

HttpResponse& HttpResponse::operator=(HttpResponse&& rhs) {
    if (this != &rhs) {
        if (m_res != nullptr) {
            nng_http_res_free(m_res);
        }
        m_res = rhs.m_res;
        rhs.m_res = nullptr;
        m_body = std::move(rhs.m_body);
    }
    return *this;
}

HttpClient::~HttpClient() {
    reset();
}

void HttpClient::reset() {
    m_client.release();

#if HKU_ENABLE_HTTP_CLIENT_SSL
    m_tls_cfg.release();
#endif

    m_conn.close();
    m_aio.release();
}

void HttpClient::_connect() {
    HKU_CHECK(m_url.valid(), "Invalid url: {}", m_url.raw_url());

    m_client.set_url(m_url);

    if (m_url.is_https()) {
#if HKU_ENABLE_HTTP_CLIENT_SSL
        m_tls_cfg.set_ca_file("test_data/ca-bundle.crt");
        m_client.set_tls(m_tls_cfg.get());
#endif
    }

    m_aio.alloc();
    m_aio.set_timeout(m_timeout_ms);
    m_client.connect(m_aio);

    if (!m_conn.valid()) {
        m_aio.wait().result();
        m_conn = std::move(nng::http_conn((nng_http_conn*)m_aio.get_output(0)));
    }
}

HttpResponse HttpClient::request(const std::string& method, const std::string& path,
                                 const HttpHeaders& headers, const char* body, size_t len,
                                 const std::string& content_type) {
    HKU_CHECK(m_url.valid(), "Invalid url: {}", m_url.raw_url());

    HttpResponse res;
    try {
        nng::http_req req(m_url);
        req.set_method(method)
          .set_uri(path)
          .add_headers(m_default_headers)
          .add_headers(headers)
          .set_data(body, len);
        if (len != 0) {
            req.add_header("Content-Type", content_type);
        }

        _connect();

        // Send the request, and wait for that to finish.
        m_conn.write_req(req, m_aio);
        m_aio.wait().result();
        m_conn.read_res(res.get(), m_aio);
        m_aio.wait().result();

        HKU_IF_RETURN(res.status() != NNG_HTTP_STATUS_OK, res);

        std::string hdr = res.getHeader("Content-Length");
        HKU_WARN_IF_RETURN(hdr.empty(), res, "Missing Content-Length header.");

        size_t len = std::stoi(hdr);
        HKU_IF_RETURN(len == 0, res);

        res._resizeBody(len);

        // Set up a single iov to point to the buffer.
        nng_iov iov;
        iov.iov_len = len;
        iov.iov_buf = res.m_body.data();

        // Following never fails with fewer than 5 elements.
        m_aio.set_iov(1, &iov);

        // Now attempt to receive the data.
        m_conn.read_all(m_aio);

        // Wait for it to complete.
        m_aio.wait().result();

    } catch (const hku::HttpTimeoutException&) {
        throw;
        reset();
    } catch (const std::exception& e) {
        HKU_THROW(e.what());
        reset();
    } catch (...) {
        HKU_THROW_UNKNOWN;
        reset();
    }
    return res;
}

}  // namespace hku