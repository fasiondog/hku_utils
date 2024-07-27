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
    HttpClient cli("https://httpbin.org");
    cli.get("/ip");
    // cli.get("/ip");
}