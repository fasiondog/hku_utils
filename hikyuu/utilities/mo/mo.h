/*
 *  Copyright(C) 2021 hikyuu.org
 *
 *  Create on: 2021-05-01
 *     Author: fasiondog
 */

#pragma once

#include "hikyuu/utilities/config.h"
#if !HKU_ENABLE_MO
#error "Don't enable mo, please config with --mo=y"
#endif

#include <unordered_map>
#include "hikyuu/utilities/string_view.h"
#include "moFileReader.h"

#if defined(_MSC_VER)
// moFileReader.hpp 最后打开了4251告警，这里关闭
#pragma warning(disable : 4251)
#endif /* _MSC_VER */

#ifndef HKU_UTILS_API
#define HKU_UTILS_API
#endif

namespace hku {
namespace mo {

/**
 * @brief 初始化多语言支持
 * @param path 翻译文件路径
 */
void HKU_UTILS_API init(const std::string &path = "i8n");

std::string HKU_UTILS_API translate(const std::string &lang, const char *id);

std::string HKU_UTILS_API translate(const std::string &lang, const char *ctx, const char *id);

std::string HKU_UTILS_API getSystemLanguage();

}  // namespace mo
}  // namespace hku