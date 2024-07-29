/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#include "hikyuu/utilities/Log.h"
#include "hikyuu/utilities/os.h"
#include "HttpClient.h"

#if HKU_ENABLE_HTTP_CLIENT_ZIP
#include "gzip/compress.hpp"
#include "gzip/decompress.hpp"
#endif

namespace hku {

HttpResponse::HttpResponse() {
    NNG_CHECK(nng_http_res_alloc(&m_res));
}

HttpResponse::~HttpResponse() {
    if (m_res) {
        nng_http_res_free(m_res);
    }
}

void HttpResponse::reset() {
    if (m_res) {
        nng_http_res_free(m_res);
        NNG_CHECK(nng_http_res_alloc(&m_res));
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
#if HKU_ENABLE_HTTP_CLIENT_SSL
    m_tls_cfg.release();
#endif
}

void HttpClient::reset() {
    m_client.release();
    m_conn.close();
    m_aio.release();
}

void HttpClient::setCaFile(const std::string& filename) {
#if HKU_ENABLE_HTTP_CLIENT_SSL
    HKU_CHECK(!filename.empty(), "Input filename is empty!");
    HKU_IF_RETURN(filename == m_ca_file, void());
    HKU_CHECK(existFile(filename), "Not exist file: {}", filename);
    m_tls_cfg.set_ca_file(filename);
    m_ca_file = filename;
    reset();
#else
    HKU_THROW("Not support https! Please complie with --http_client_ssl!");
#endif
}

void HttpClient::_connect() {
    HKU_CHECK(m_url.valid(), "Invalid url: {}", m_url.raw_url());

    m_client.set_url(m_url);

    if (m_url.is_https()) {
#if HKU_ENABLE_HTTP_CLIENT_SSL
        auto* old_cfg = m_client.get_tls_cfg();
        if (!old_cfg || old_cfg != m_tls_cfg.get()) {
            m_client.set_tls_cfg(m_tls_cfg.get());
        }
#endif
    }

    m_aio.alloc(m_timeout_ms);
    m_client.connect(m_aio);

    if (!m_conn.valid()) {
        NNG_CHECK(m_aio.wait().result());
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
        req.set_method(method).set_uri(path).add_headers(m_default_headers).add_headers(headers);
        if (body != nullptr) {
            HKU_CHECK(len > 0, "Body is not null, but len is zero!");
            req.add_header("Content-Type", content_type);

#if HKU_ENABLE_HTTP_CLIENT_ZIP
            if (req.get_header("Content-Encoding") == "gzip") {
                gzip::Compressor comp(Z_DEFAULT_COMPRESSION);
                std::string output;
                comp.compress(output, body, len);
                req.copy_data(output.data(), output.size());
            } else {
                req.set_data(body, len);
            }
#else
            req.del_header("Content-Encoding").set_data(body, len);
#endif
        }

        int count = 0;
        while (count < 2) {
            count++;
            _connect();

            // Send the request, and wait for that to finish.
            m_conn.write_req(req, m_aio);
            int rv = m_aio.wait().result();
            if (NNG_ECLOSED == rv || NNG_ECONNSHUT == rv || NNG_ECONNREFUSED == rv) {
                // HKU_DEBUG("rv: {}", nng_strerror(rv));
                reset();
                res.reset();
                continue;
            } else if (NNG_ETIMEDOUT == rv) {
                throw HttpTimeoutException();
            } else if (0 != rv) {
                HKU_THROW("[NNG_ERROR] {} ", nng_strerror(rv));
            }

            m_conn.read_res(res.get(), m_aio);
            rv = m_aio.wait().result();
            if (0 == rv) {
                break;
            } else if (NNG_ETIMEDOUT == rv) {
                throw HttpTimeoutException();
            } else if (NNG_ECLOSED == rv || NNG_ECONNSHUT == rv || NNG_ECONNREFUSED == rv) {
                // HKU_DEBUG("rv: {}", nng_strerror(rv));
                reset();
                res.reset();
            } else {
                HKU_THROW("[NNG_ERROR] {} ", nng_strerror(rv));
            }
        }

        HKU_IF_RETURN(res.status() != NNG_HTTP_STATUS_OK, res);

        std::string hdr = res.getHeader("Content-Length");
        HKU_WARN_IF_RETURN(hdr.empty(), res, "Missing Content-Length header.");

        size_t len = std::stoi(hdr);
        HKU_IF_RETURN(len == 0, res);

#if HKU_ENABLE_HTTP_CLIENT_ZIP
        if (res.getHeader("Content-Encoding") == "gzip") {
            nng_iov iov;
            auto data = std::unique_ptr<char[]>(new char[len]);
            iov.iov_len = len;
            iov.iov_buf = data.get();
            m_aio.set_iov(1, &iov);
            m_conn.read_all(m_aio);
            NNG_CHECK(m_aio.wait().result());

            gzip::Decompressor decomp;
            decomp.decompress(res.m_body, data.get(), len);

        } else {
            res._resizeBody(len);
            nng_iov iov;
            iov.iov_len = len;
            iov.iov_buf = res.m_body.data();
            m_aio.set_iov(1, &iov);
            m_conn.read_all(m_aio);
            NNG_CHECK(m_aio.wait().result());
        }
#else
        HKU_WARN_IF(
          res.getHeader("Content-Encoding") == "gzip",
          "Automatic decompression is not supported. You need to decompress it yourself!");
        res._resizeBody(len);
        nng_iov iov;
        iov.iov_len = len;
        iov.iov_buf = res.m_body.data();
        m_aio.set_iov(1, &iov);
        m_conn.read_all(m_aio);
        NNG_CHECK(m_aio.wait().result());

#endif  // #if HKU_ENABLE_HTTP_CLIENT_ZIP

        if (res.getHeader("Connection") == "close") {
            HKU_WARN("Connect closed");
            reset();
        }
    } catch (const std::exception&) {
        throw;
        reset();
    } catch (...) {
        HKU_THROW_UNKNOWN;
        reset();
    }
    return res;
}

}  // namespace hku