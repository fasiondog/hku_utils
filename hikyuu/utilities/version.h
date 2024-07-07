/*
 *  Copyright(C) 2021 hikyuu.org
 *
 *  Create on: 2021-01-08
 *     Author: fasiondog
 */

#pragma once

#include <string>

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

// clang-format off
#define HKU_UTILS_VERSION "1.0.0"
#define HKU_UTILS_VERSION_MAJOR 1
#define HKU_UTILS_VERSION_MINOR 0
#define HKU_UTILS_VERSION_ALTER 0
#define HKU_UTILS_VERSION_BUILD 202407080141
// clang-format on

#if defined(_DEBUG) || defined(DEBUG)
#define HKU_COMPILE_MODE "debug"
#else
#define HKU_COMPILE_MODE "release"
#endif

namespace hku {
namespace utils {

/**
 * 获取主版本号
 */
std::string HKU_UTILS_API getVersion();

/**
 * 获取详细版本号，包含构建时间
 */
std::string HKU_UTILS_API getVersionWithBuild();

}  // namespace utils
}  // namespace hku
