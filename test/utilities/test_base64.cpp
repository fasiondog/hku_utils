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
    // 传入空字符串
    CHECK_EQ(base64_decode(std::string("")), std::string());

    // 传入非法 base64 字符串
    CHECK_THROWS(base64_decode(std::string("+")));
    CHECK_THROWS(base64_decode(std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ 泉州")));

    // 正常解码
    std::string src("ABCDEFGHIJKLMNOPQRSTUVWXYZ 泉州 0123456789+/ 泉州 abcdefghijklmnopqrstuvwxyz");
    std::string dst(
      "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVog5rOJ5beeIDAxMjM0NTY3ODkrLyDms4nlt54gYWJjZGVmZ2hpamtsbW5v"
      "cHFyc3R1dnd4eXo=");
    CHECK_EQ(base64_decode(dst), src);
}

TEST_CASE("test_base64_decode_tail_chunk") {
    /** A trailing chunk of a single character cannot produce any byte and must throw */
    CHECK_THROWS(base64_decode(std::string("A")));
    CHECK_THROWS(base64_decode(std::string("AAAAQ")));

    /** Lengths modulo 4 of 0/2/3 are valid, with and without padding */
    CHECK_EQ(base64_decode(std::string("QQ==")), std::string("A"));
    CHECK_EQ(base64_decode(std::string("QQ")), std::string("A"));
    CHECK_EQ(base64_decode(std::string("QUI=")), std::string("AB"));
    CHECK_EQ(base64_decode(std::string("QUI")), std::string("AB"));

#if __cplusplus >= 201703L
    /** The string_view overload must behave the same (reading at size() would be UB) */
    CHECK_THROWS(base64_decode(std::string_view("AAAAQ")));
    CHECK_EQ(base64_decode(std::string_view("QQ")), std::string("A"));
#endif
}

TEST_CASE("test_base64_encode_pem_mime") {
    std::string src;
    for (int i = 0; i < 1000; ++i) {
        src.push_back(static_cast<char>(i % 251));
    }

    auto checkLines = [](const std::string& encoded, size_t maxWidth) {
        size_t start = 0;
        while (start < encoded.size()) {
            size_t end = encoded.find('\n', start);
            if (end == std::string::npos) {
                end = encoded.size();
            }
            size_t len = end - start;
            CHECK_UNARY(len > 0 && len <= maxWidth);
            start = end + 1;
        }
    };

    /** PEM: 64 characters per line separated by '\n' (the last line may be shorter) */
    std::string pem = base64_encode_pem(src);
    checkLines(pem, 64);
    CHECK_EQ(base64_decode(pem, true), src);

    /** MIME: 76 characters per line separated by '\n' */
    std::string mime = base64_encode_mime(src);
    checkLines(mime, 76);
    CHECK_EQ(base64_decode(mime, true), src);

    /** The original implementation was O(n^2); 1MB must complete within the test timeout */
    std::string large(1024 * 1024, 'x');
    std::string largePem = base64_encode_pem(large);
    CHECK_UNARY_FALSE(largePem.empty());
    CHECK_EQ(base64_decode(largePem, true), large);
}
