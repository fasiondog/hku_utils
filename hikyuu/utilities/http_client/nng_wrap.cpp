/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#include "hikyuu/utilities/Log.h"
#include "nng_wrap.h"

#ifndef NNG_CHECK
#define NNG_CHECK(rv)                                      \
    {                                                      \
        if (rv != 0) {                                     \
            HKU_THROW("[NNG_ERROR] {}", nng_strerror(rv)); \
        }                                                  \
    }
#endif

namespace hku {
namespace nng {}  // namespace nng
}  // namespace hku