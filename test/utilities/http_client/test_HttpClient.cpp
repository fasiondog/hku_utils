/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2024-07-26
 *      Author: fasiondog
 */

#include "test_config.h"
#include "hikyuu/utilities/http_client/HttpClient.h"

using namespace hku;

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

    cli.setTimeout(NNG_DURATION_INFINITE);
    cli.get("/ip");
}