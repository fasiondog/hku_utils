/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-30
 *      Author: fasiondog
 */

#include "test_config.h"
#include <atomic>
#include <chrono>
#include <thread>
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

TEST_CASE("test_node_regHandle_concurrent") {
    // Per the usage contract, regHandle must be completed before start(); this case only checks
    // that registering another handle from inside a running handler works (the handler is
    // invoked outside any map lock on the server side)
    std::string server_addr = "inproc://tmp_reg_handle";
    NodeServer server;
    server.setAddr(server_addr);
    server.regHandle("hello", [](json&& req) {
        json res;
        return res;
    });
    server.regHandle("reentrant", [&server](json&& req) {
        server.regHandle("extra", [](json&& req) {
            json res;
            return res;
        });
        json res;
        return res;
    });
    server.start();

    NodeClient cli(server_addr);
    CHECK_UNARY(cli.dial());

    json req, res;
    req["cmd"] = "reentrant";
    CHECK_UNARY(cli.post(req, res));
    CHECK_EQ(res["ret"].get<int>(), NodeErrorCode::SUCCESS);

    // The handle registered by the reentrant handler must be usable
    req["cmd"] = "extra";
    CHECK_UNARY(cli.post(req, res));
    CHECK_EQ(res["ret"].get<int>(), NodeErrorCode::SUCCESS);
}

TEST_CASE("test_node_client_concurrent_close_and_post") {
    // close() must be serialized with post() on the operation mutex; concurrent closing used to
    // run nng_close while _send/_recv were using the socket
    std::string server_addr = "inproc://tmp_close_post";
    NodeServer server;
    server.setAddr(server_addr);
    server.regHandle("hello", [](json&& req) {
        json res;
        return res;
    });
    server.start();

    NodeClient cli(server_addr);
    CHECK_UNARY(cli.dial());

    std::atomic<bool> stop{false};
    auto closer = std::thread([&stop, &cli]() {
        while (!stop.load(std::memory_order_relaxed)) {
            cli.close();
            std::this_thread::yield();
        }
    });

    // Posts may fail while the closer runs (the connection is being closed), they must not crash
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);
    closer.join();

    // Re-dial and post normally afterwards
    CHECK_UNARY(cli.dial());
    json req, res;
    req["cmd"] = "hello";
    CHECK_UNARY(cli.post(req, res));
    CHECK_EQ(res["ret"].get<int>(), NodeErrorCode::SUCCESS);
}