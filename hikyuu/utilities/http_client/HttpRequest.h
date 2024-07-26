/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#pragma once
#ifndef HKU_UTILS_HTTP_REQUEST_H_
#define HKU_UTILS_HTTP_REQUEST_H_

#include "hikyuu/utilities/config.h"
#include <string>
#include <nng/nng.h>
#include <nng/supplemental/http/http.h>

namespace hku {

class HKU_UTILS_API HttpRequest final {
public:
    HttpRequest();
    ~HttpRequest();

private:
    nng_http_req* m_req{nullptr};
};

}  // namespace hku

#endif