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
    HttpClient() = delete;
    explicit HttpClient(const std::string& url);
    virtual ~HttpClient();

    void get(const std::string& path);

private:
    // nng::url m_url;
    nng_url* m_url{nullptr};
    nng_http_client* m_client{nullptr};
    nng_aio* m_aio{nullptr};
    nng_http_conn* m_conn{nullptr};
};

}  // namespace hku

#endif