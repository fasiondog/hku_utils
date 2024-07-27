/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#include "hikyuu/utilities/Log.h"
#include "HttpClient.h"

namespace hku {

HttpClient::HttpClient(const std::string& url) : m_url(nng::url(url)) {
    if (!m_url) {
        HKU_WARN("Invalid url: {}", url);
        return;
    }

    int rv = nng_http_client_alloc(&m_client, m_url.get());
    if (rv != 0) {
        HKU_WARN("[NNG_ERROR] {}", nng_strerror(rv));
        return;
    }

    rv = nng_tls_config_alloc(&m_tls_cfg, NNG_TLS_MODE_CLIENT);
    NNG_CHECK(rv);

    rv = nng_tls_config_ca_file(m_tls_cfg, "test_data/ca-bundle.crt");
    NNG_CHECK(rv);

    nng_http_client_set_tls(m_client, m_tls_cfg);
    NNG_CHECK(rv);

    rv = nng_aio_alloc(&m_aio, NULL, NULL);
    if (rv != 0) {
        nng_http_client_free(m_client);
        HKU_WARN("[NNG_ERROR] {}", nng_strerror(rv));
        return;
    }

    nng_http_client_connect(m_client, m_aio);
    m_valid = true;
}

HttpClient::~HttpClient() {
    if (m_conn) {
        nng_http_conn_close(m_conn);
    }

    if (m_client) {
        nng_http_client_free(m_client);
    }

    if (m_tls_cfg) {
        nng_tls_config_free(m_tls_cfg);
    }
    // nng_aio_free(m_aio);  // nng_http_client_free会释放 aio
}

void HttpClient::get(const std::string& path) {
    nng_http_req* req;
    int rv = nng_http_req_alloc(&req, m_url.get());
    NNG_CHECK(rv);

    rv = nng_http_req_set_method(req, "GET");
    NNG_CHECK(rv);

    rv = nng_http_req_set_uri(req, path.c_str());
    NNG_CHECK(rv);

    nng_aio_wait(m_aio);

    rv = nng_aio_result(m_aio);
    NNG_CHECK(rv);

    if (!m_conn) {
        m_conn = (nng_http_conn*)nng_aio_get_output(m_aio, 0);
    }

    nng_http_res* res{nullptr};
    rv = nng_http_res_alloc(&res);
    nng_http_conn_transact(m_conn, req, res, m_aio);
    nng_aio_wait(m_aio);
    nng_aio_result(m_aio);
    if (nng_http_res_get_status(res) != NNG_HTTP_STATUS_OK) {
        HKU_INFO("HTTP Server Responded: {} {}", nng_http_res_get_status(res),
                 nng_http_res_get_reason(res));
        return;
    }

    void* data;
    size_t len;
    nng_http_res_get_data(res, &data, &len);
    HKU_INFO("data len: {}", len);

    nng_http_req_free(req);
    nng_http_res_free(res);

#if 0
    // Send the request, and wait for that to finish.
    nng_http_conn_write_req(m_conn, req, m_aio);

    HKU_INFO("*******************");

    nng_aio_wait(m_aio);
    rv = nng_aio_result(m_aio);
    HKU_INFO("*******************");

    nng_http_req_free(req);
    HKU_INFO("*******************");
    NNG_CHECK(rv);
    HKU_INFO("*******************");

    // Read a response.
    nng_http_res* res{nullptr};
    rv = nng_http_res_alloc(&res);
    NNG_CHECK(rv);

    nng_http_conn_read_res(m_conn, res, m_aio);
    nng_aio_wait(m_aio);
    rv = nng_aio_result(m_aio);
    NNG_CHECK(rv);

    if (nng_http_res_get_status(res) != NNG_HTTP_STATUS_OK) {
        HKU_INFO("HTTP Server Responded: {} {}", nng_http_res_get_status(res),
                 nng_http_res_get_reason(res));
        return;
    }

    const char* hdr;
    if ((hdr = nng_http_res_get_header(res, "Content-Length")) == NULL) {
        HKU_INFO("Missing Content-Length header.");
        return;
    }

    int len = std::atoi(hdr);
    if (len == 0) {
        return;
    }

    HKU_INFO("data len: {}", len);

    void* data{nullptr};
    nng_iov iov;
    // Allocate a buffer to receive the body data.
    data = malloc(len);

    // Set up a single iov to point to the buffer.
    iov.iov_len = len;
    iov.iov_buf = data;

    // Following never fails with fewer than 5 elements.
    nng_aio_set_iov(m_aio, 1, &iov);

    // Now attempt to receive the data.
    nng_http_conn_read_all(m_conn, m_aio);

    // Wait for it to complete.
    nng_aio_wait(m_aio);

    nng_http_res_free(res);

    if ((rv = nng_aio_result(m_aio)) != 0) {
        free(data);
        HKU_THROW("[NNG_ERROR] {}", nng_strerror(rv));
    }

    free(data);
#endif
}

}  // namespace hku