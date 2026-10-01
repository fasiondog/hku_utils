/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-03-19
 *      Author: fasiondog
 */

#include "test_config.h"
#include "hikyuu/utilities/os.h"
#include "hikyuu/utilities/plugin/PluginClient.h"
#include "hikyuu/utilities/plugin/PluginLoader.h"
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

TEST_CASE("test_PluginLoader_unload_on_create_failure") {
    /**
     * Regression for the double-unload defect: when createPlugin returns
     * nullptr, load() calls unload() internally; the handle must be reset
     * there, otherwise ~PluginLoader closes the same handle a second time
     * (double dlclose / FreeLibrary, undefined behavior)
     */
    {
        PluginLoader loader(".");
        CHECK_EQ(loader.load("testbadplugin", false), false);
        CHECK_EQ(loader.instance<TestPluginInterface>(), nullptr);
    }

    /** Load a missing plugin: the path is rejected before dlopen, no handle state */
    {
        PluginLoader loader(".");
        CHECK_EQ(loader.load("no_such_plugin", false), false);
        CHECK_EQ(loader.instance<TestPluginInterface>(), nullptr);
    }
}
