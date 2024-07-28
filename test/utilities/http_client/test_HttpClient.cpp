/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#include "test_config.h"
#include "hikyuu/utilities/http_client/HttpClient.h"
#include <thread>

using namespace hku;

TEST_CASE("test_HttpResponse") {
    // 未访问 http 情况下， HttpResponse方法调用
    HttpResponse res;
    CHECK_EQ(res.status(), 200);
    CHECK_EQ(res.reason(), "OK");
    CHECK_EQ(res.getHeader("x"), "");
    CHECK_EQ(res.getContentLength(), 0);
    CHECK_UNARY(res.body().empty());
    CHECK_THROWS(res.json());
}

TEST_CASE("test_HttpClient") {
    // 无效 url
    HttpClient invalid_cli;
    CHECK_UNARY(!invalid_cli.valid());

    invalid_cli.setUrl("a");
    CHECK_UNARY(!invalid_cli.valid());

    HttpClient invalid_cli2("b");
    CHECK_UNARY(!invalid_cli2.valid());

    // 超时
    HttpClient cli("http://httpbin.org/");
    CHECK_UNARY(cli.valid());

    cli.setTimeout(3);
    CHECK_THROWS(cli.get("/ip"));

    cli.setTimeout(0);
    auto res = cli.get("/ip");
    HKU_INFO("res: {}", res.body());
    HKU_INFO("res len: {}", res.body().size());
    HKU_INFO("Content-Type: {}", res.getHeader("Content-Type"));
    HKU_INFO("Connection: {}", res.getHeader("Connection"));

    json jres = res.json();
    auto ip = jres["origin"].get<std::string>();
    HKU_INFO("ip: {}", ip);

    // HKU_INFO("wait ...");
    // std::this_thread::sleep_for(std::chrono::seconds(60 * 3));
    // cli.get("/ip");
}