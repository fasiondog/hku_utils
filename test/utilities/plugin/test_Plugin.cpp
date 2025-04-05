/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-03-19
 *      Author: fasiondog
 */

#include "test_config.h"
#include "hikyuu/utilities/os.h"
#include "hikyuu/utilities/plugin/PluginLoader.h"
#include "../../plugin/TestPluginInterface.h"
#include <cstdlib>

using namespace hku;

template <typename InterfaceT>
class PluginClient : public InterfaceT {
public:
    PluginClient() = delete;
    PluginClient(const std::string &path, const std::string &filename) {
        m_loader = std::make_unique<PluginLoader>(path);
        m_loader->load(filename);
        m_impl = m_loader->instance<InterfaceT>();
    }
    virtual ~PluginClient() = default;

    PluginClient(const PluginClient &) = delete;
    PluginClient &operator=(const PluginClient &) = delete;

    PluginClient(PluginClient &&rhs) : m_impl(rhs.m_impl), m_loader(std::move(rhs.m_loader)) {
        rhs.m_impl = nullptr;
    }

    PluginClient &operator=(PluginClient &&rhs) {
        if (this != &rhs) {
            m_loader = std::move(rhs.m_loader);
            m_impl = rhs.m_impl;
            rhs.m_impl = nullptr;
        }
    }

    InterfaceT *getPlugin() const {
        return m_impl;
    }

protected:
    InterfaceT *m_impl;

protected:
    std::unique_ptr<PluginLoader> m_loader;
};

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
}
