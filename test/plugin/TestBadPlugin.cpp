/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-10-02
 *      Author: fasiondog
 */

#include <stdexcept>
#include "hikyuu/utilities/plugin/PluginBase.h"

namespace hku {

/**
 * A plugin whose creation always fails: the constructor throws, so the
 * createPlugin factory catches it and returns nullptr, which drives
 * PluginLoader::load() into the unload() failure path
 */
class TestBadPlugin : public PluginBase {
public:
    TestBadPlugin() {
        throw std::runtime_error("TestBadPlugin: simulated creation failure");
    }
    virtual ~TestBadPlugin() = default;

    virtual std::string info() const noexcept override {
        return {};
    }
};

}  // namespace hku

HKU_PLUGIN_DEFINE(hku::TestBadPlugin)
