/*
 *  Copyright (c) 2019~2021, hikyuu
 *
 *  Created on: 2021/12/06
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <hikyuu/utilities/md5.h>
#include <hikyuu/utilities/Log.h>

using namespace hku;

TEST_CASE("test_md5") {
    // 传入的数据指针为 null
    CHECK_THROWS_AS(md5(nullptr, 1), hku::exception);

    // 传入的数据长度为 0
    std::string a("99983");
    CHECK_EQ(md5((const unsigned char*)a.data(), 0),
             std::string("d41d8cd98f00b204e9800998ecf8427e"));

    // 正常计算 md5
    CHECK_EQ(md5((const unsigned char*)a.data(), a.size()),
             std::string("43d62713df20a658fa61ed5fb6c3040d"));
    CHECK_EQ(md5(a), std::string("43d62713df20a658fa61ed5fb6c3040d"));
}