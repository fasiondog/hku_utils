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

namespace hku {
namespace nng {

class url final {
public:
    url() = default;
    explicit url(const std::string& url_) {
        nng_url_parse(&m_url, url_.c_str());
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
        nng_url_parse(&m_url, url_.c_str());
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

}  // namespace nng
}  // namespace hku

#endif