/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#include "hikyuu/utilities/Log.h"
#include "HttpClient.h"

namespace hku {

HttpClient::~HttpClient() {
    if (m_tls_cfg) {
        nng_tls_config_free(m_tls_cfg);
    }
}

void HttpClient::_connect() {
    HKU_CHECK(m_url.valid(), "Invalid url: {}", m_url.raw_url());

    m_client.set_url(m_url);

    if (m_url.is_https()) {
        int rv = nng_tls_config_alloc(&m_tls_cfg, NNG_TLS_MODE_CLIENT);
        NNG_CHECK(rv);

        rv = nng_tls_config_ca_file(m_tls_cfg, "test_data/ca-bundle.crt");
        NNG_CHECK(rv);

        m_client.set_tls(m_tls_cfg);
    }

    m_aio.alloc();
    m_aio.set_timeout(m_timeout_ms);
    m_client.connect(m_aio.get());
}

HttpResponse HttpClient::get(const std::string& path) {
    HttpResponse res;
    _connect();

    nng_http_req* req;
    int rv = nng_http_req_alloc(&req, m_url.get());
    NNG_CHECK(rv);

    // HKU_INFO("http version: {}", nng_http_req_get_version(req));
    // nng_http_req_set_header(req, "Connection", "keep-alive");
    // HKU_INFO("Connection: {}", nng_http_req_get_header(req, "Connection"));

    rv = nng_http_req_set_method(req, "GET");
    NNG_CHECK(rv);

    rv = nng_http_req_set_uri(req, path.c_str());
    NNG_CHECK(rv);

    m_aio.wait();
    m_aio.result();

    if (!m_conn.valid()) {
        m_conn = std::move(nng::http_conn((nng_http_conn*)m_aio.get_output(0)));
    }

    // Send the request, and wait for that to finish.
    m_conn.write_req(req, m_aio.get());

    m_aio.wait();
    m_aio.result();

    m_conn.read_res(res.get(), m_aio.get());

    m_aio.wait();
    m_aio.result();

    nng_http_req_free(req);

    if (res.status() != NNG_HTTP_STATUS_OK) {
        HKU_INFO("HTTP Server Responded: {} {}", res.status(), res.reason());
        return res;
    }

    std::string hdr = res.getHeader("Content-Length");
    if (hdr.empty()) {
        HKU_INFO("Missing Content-Length header.");
        return res;
    }

    size_t len = std::stoi(hdr);
    if (len == 0) {
        return res;
    }

    res._resizeBody(len);

    // Set up a single iov to point to the buffer.
    nng_iov iov;
    iov.iov_len = len;
    iov.iov_buf = res.m_body.data();

    // Following never fails with fewer than 5 elements.
    nng_aio_set_iov(m_aio.get(), 1, &iov);

    // Now attempt to receive the data.
    m_conn.read_all(m_aio.get());

    // Wait for it to complete.
    m_aio.wait();
    m_aio.result();

    return res;
}

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

}  // namespace hku