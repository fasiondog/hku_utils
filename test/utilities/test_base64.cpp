/*
 *  Copyright (c) 2019~2021, hikyuu
 *
 *  Created on: 2021/12/16
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <hikyuu/utilities/base64.h>
#include <hikyuu/utilities/Log.h>

using namespace hku;

TEST_CASE("test_base64_encode") {
    // 测试空指针
    CHECK_THROWS(base64_encode(nullptr, 10));

    // 传入错误的长度
    unsigned char buf[10];
    CHECK_EQ(base64_encode(buf, 0), std::string());

    // 正常编码
    std::string src("ABCDEFGHIJKLMNOPQRSTUVWXYZ 泉州 0123456789+/ 泉州 abcdefghijklmnopqrstuvwxyz");
    std::string dst(
      "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVog5rOJ5beeIDAxMjM0NTY3ODkrLyDms4nlt54gYWJjZGVmZ2hpamtsbW5v"
      "cHFyc3R1dnd4eXo=");
    CHECK_EQ(base64_encode(src), dst);
}

TEST_CASE("test_base64_decode") {
    // C++17 string_view 无法正确处理 null_ptr
    // auto x = std::string_view(nullptr);
    // HKU_INFO("{}", x);

    CHECK_THROWS(base64_decode(nullptr, 10));

    // 传入空字符串
    CHECK_EQ(base64_decode(""), std::string());

    CHECK_EQ(base64_decode("+"), std::string());

    auto x = base64_decode("ABCDEFGHIJKLMNOPQRSTUVWXYZ 泉州");
    // HKU_INFO("{}", x);
    HKU_INFO("{}", base64_encode("ABCDEFGHIJKLMNOPQRSTUVWXYZ 泉州"));

    // 正常解码
    std::string src("ABCDEFGHIJKLMNOPQRSTUVWXYZ 泉州 0123456789+/ 泉州 abcdefghijklmnopqrstuvwxyz");
    std::string dst(
      "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVog5rOJ5beeIDAxMjM0NTY3ODkrLyDms4nlt54gYWJjZGVmZ2hpamtsbW5v"
      "cHFyc3R1dnd4eXo=");
    CHECK_EQ(base64_decode(dst), src);
}
