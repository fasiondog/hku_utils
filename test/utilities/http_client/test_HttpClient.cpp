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

    HttpClient cli2("http://httpbin.org/", 1);
    CHECK_UNARY(cli2.valid());
    CHECK_THROWS(cli2.get("/ip"));

    // 正常访问 http
    cli.setTimeout(1000);
    auto res = cli.get("/ip");
    if (res.status() == 200) {
        json jres = res.json();
        auto ip = jres["origin"].get<std::string>();
        HKU_INFO("ip: {}", ip);
    }

    // for (size_t i = 0; i < 15; i++) {
    //     res = cli.get("/ip");
    //     jres = res.json();
    //     ip = jres["origin"].get<std::string>();
    //     HKU_INFO("ip: {}", ip);
    // }

    // HKU_INFO("wait");
    // std::this_thread::sleep_for(std::chrono::seconds(60));
    // res = cli.get("/ip");
    // jres = res.json();
    // ip = jres["origin"].get<std::string>();
    // HKU_INFO("ip: {}", ip);

#if HKU_ENABLE_HTTP_CLIENT_SSL
    // 访问 https, 无 CA file
    cli.setUrl("https://httpbin.org/");
    CHECK_THROWS(cli.get("/ip"));

    // 正常访问 https
    cli.setCaFile("test_data/ca-bundle.crt");
    res = cli.get("/ip");
    if (res.status() == 200) {
        HKU_INFO("{} {}", cli.url(), res.status());
    }
#endif

    // try {
    //     cli.setUrl("http://webapi-pc.meitu.com");
    //     res = cli.get("/common/ip_location", {{"ip", "112.97.82.226"}}, HttpHeaders());
    //     if (res.status() == 200) {
    //         auto data = res.json();
    //         HKU_INFO("{}", data.dump());
    //     } else {
    //         HKU_INFO("res status: {}, body: {}", res.status(), res.body());
    //     }
    // } catch (const std::exception& e) {
    //     HKU_INFO(e.what());
    // }
}