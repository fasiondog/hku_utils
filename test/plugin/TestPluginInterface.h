/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-04-06
 *      Author: fasiondog
 */

#pragma once
#include <hikyuu/utilities/plugin/PluginLoader.h>

namespace hku {

class TestPluginInterface : public PluginBase {
public:
    TestPluginInterface() = default;
    virtual ~TestPluginInterface() = default;

    virtual std::string name() const = 0;
};

}  // namespace hku
