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
    cli.setTimeout(1);
    CHECK_THROWS(cli.get("/ip"));

    // 正常访问 http
    cli.setTimeout(-2);
    auto res = cli.get("/ip");
    json jres = res.json();
    auto ip = jres["origin"].get<std::string>();
    HKU_INFO("ip: {}", ip);

#if HKU_ENABLE_HTTP_CLIENT_SSL
    // 访问 https, 无 CA file
    cli.setUrl("https://httpbin.org/");
    CHECK_THROWS(cli.get("/ip"));

    // 正常访问 https
    cli.setCaFile("test_data/ca-bundle.crt");
    res = cli.get("/ip");
    HKU_INFO("{} {}", cli.url(), res.status());
#endif
}