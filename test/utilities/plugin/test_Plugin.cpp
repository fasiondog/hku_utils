/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-03-19
 *      Author: fasiondog
 */

#include "test_config.h"
#include "hikyuu/utilities/os.h"
#include "hikyuu/utilities/plugin/PluginClient.h"
#include "../../plugin/TestPluginInterface.h"
#include <cstdlib>

using namespace hku;

class TestPluginClient : public PluginClient<TestPluginInterface> {
public:
    TestPluginClient(const std::string &path, const std::string &filename)
    : PluginClient<TestPluginInterface>(path, filename) {}

    virtual std::string name() const override {
        HKU_CHECK(m_impl, "Plugin not loaded!");
        return m_impl->name();
    }
};

TEST_CASE("test_plugin") {
    TestPluginClient plugin1(".", "testplugin");
    TestPluginClient plugin = std::move(plugin1);
    CHECK_EQ(plugin.name(), "testplugin");
    CHECK_THROWS(plugin1.name());
    HKU_INFO("{}", plugin.name());
    HKU_INFO("{}", plugin.info());
}
