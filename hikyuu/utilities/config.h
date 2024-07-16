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

// clang-format on

#endif /* HKU_UTILS_CONFIG_H_*/