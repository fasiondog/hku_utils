/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-03-19
 *      Author: fasiondog
 */

#include "TestPlugin.h"

namespace hku {

std::string TestPlugin::name() const {
    return "testplugin";
}

}  // namespace hku

HKU_PLUGIN_DEFINE(hku::TestPlugin)