/*
 *  Copyright (c) 2026 hikyuu.org
 *
 *  Created on: 2026-03-15
 *      Author: fasiondog
 */

#pragma once

#include "hikyuu/utilities/exception.h"

namespace hku {

struct HKU_UTILS_API HttpTimeoutException : hku::exception {
    HttpTimeoutException() : hku::exception("Http timeout!") {}
    explicit HttpTimeoutException(const char* msg) : hku::exception(msg) {}
    explicit HttpTimeoutException(const std::string& msg) : hku::exception(msg) {}

    // The out-of-line destructor is the key function: it pins the vtable and the typeinfo to a
    // single translation unit inside the library, so that the type thrown here matches the type
    // caught by a caller living in another binary (otherwise each TU emits its own private
    // typeinfo and catch (const HttpTimeoutException&) never hits)
    ~HttpTimeoutException() noexcept override;
};

}  // namespace hku