/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-03-18
 *      Author: fasiondog
 */

#pragma once
#ifndef HKU_UTILS_PLUGIN_BASE_H_
#define HKU_UTILS_PLUGIN_BASE_H_

#include <string>
#include "hikyuu/utilities/config.h"
#include "hikyuu/utilities/osdef.h"

namespace hku {

class PluginBase {
public:
    PluginBase() = default;
    virtual ~PluginBase() = default;

private:
    std::string m_name;
};

}  // namespace hku

#if HKU_OS_WINDOWS
#define HKU_PLUGIN_DEFINE(plugin)                                      \
    extern "C" __declspec(dllexport) hku::PluginBase* createPlugin() { \
        return new plugin();                                           \
    }
#else
#define HKU_PLUGIN_DEFINE(plugin)                \
    extern "C" hku::PluginBase* createPlugin() { \
        return new plugin();                     \
    }
#endif

#endif /* HKU_UTILS_PLUGIN_BASE_H_ */