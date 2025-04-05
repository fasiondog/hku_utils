/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-03-19
 *      Author: fasiondog
 */

#pragma once

#include <string>
#include "hikyuu/utilities/plugin/PluginBase.h"
#include "TestPluginInterface.h"

namespace hku {

class TestPlugin : public TestPluginInterface {
public:
    TestPlugin() = default;
    virtual ~TestPlugin() = default;

    virtual std::string name() const override;
};

}  // namespace hku
