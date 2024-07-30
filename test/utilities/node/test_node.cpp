/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-30
 *      Author: fasiondog
 */

#include "test_config.h"
#include <hikyuu/utilities/node/NodeServer.h>
#include <hikyuu/utilities/node/NodeClient.h>

using namespace hku;

TEST_CASE("test_node") {
    std::string server_addr = "inproc://tmp";
    NodeServer server;
    server.setAddr(server_addr);
    server.regHandle("hello", [](json&& req) {
        json res;
        HKU_INFO("Hello world!");
        return res;
    });

    server.start();

    auto t = std::thread([server_addr]() {
        NodeClient cli(server_addr);
        cli.dial();

        json req, res;
        req["cmd"] = "hello";
        cli.post(req, res);
        CHECK_EQ(res["ret"].get<int>(), NodeErrorCode::SUCCESS);
    });
    t.join();
}