#pragma once
#ifndef HKU_UTILS_CONFIG_H_
#define HKU_UTILS_CONFIG_H_

#include "osdef.h"

// clang-format off

#define HKU_ENABLE_MYSQL 0
#if HKU_ENABLE_MYSQL && HKU_OS_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#define HKU_ENABLE_SQLITE 1
#define HKU_ENABLE_SQLCIPHER 0
#define HKU_SQL_TRACE 0

#define HKU_SUPPORT_DATETIME 1

#define HKU_ENABLE_INI_PARSER 1

#define HKU_ENABLE_STACK_TRACE 0

#define HKU_CLOSE_SPEND_TIME 0

#define HKU_DEFAULT_LOG_NAME "hikyuu"
#define HKU_USE_SPDLOG_ASYNC_LOGGER 0
#define HKU_LOG_ACTIVE_LEVEL 0

// clang-format on

#endif /* HKU_UTILS_CONFIG_H_*/