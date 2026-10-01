/*
 *  Copyright(C) 2021 hikyuu.org
 *
 *  Create on: 2026-10-01
 *     Author: fasiondog
 */

#include <doctest/doctest.h>
#include <string>
#include "hikyuu/utilities/http_client/url.h"

using namespace hku;

TEST_CASE("test_url_escape") {
    /** @par 检查点 */
    CHECK_EQ(url_escape(""), std::string(""));

    /** 无需转义的字符保持不变（字母、数字、- _ . ~） */
    CHECK_EQ(url_escape("abcXYZ0123-_.~"), std::string("abcXYZ0123-_.~"));

    /** 空格转义为 %20 */
    CHECK_EQ(url_escape("hello world"), std::string("hello%20world"));

    /** 常见保留字符逐一转义 */
    CHECK_EQ(url_escape("!\"#$%&'()*+,/:;=?@[]"),
             std::string("%21%22%23%24%25%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"));

    /** 混合字符串仅转义特殊字符 */
    CHECK_EQ(url_escape("a=1&b=hello world"), std::string("a%3D1%26b%3Dhello%20world"));

    /** 高位字节（>= 0x80）不被有符号 char 截断，按字节正确转义 */
    CHECK_EQ(url_escape("\x80"), std::string("%80"));
    CHECK_EQ(url_escape("\xFF"), std::string("%FF"));

    /** UTF-8 多字节序列逐字节正确转义（"中" = E4 B8 AD） */
    CHECK_EQ(url_escape("\xE4\xB8\xAD"), std::string("%E4%B8%AD"));

    /** 混合 ASCII 与非 ASCII 字节 */
    CHECK_EQ(url_escape("a\xE4\xB8\xAD"
                        "b"),
             std::string("a%E4%B8%ADb"));
}

TEST_CASE("test_url_unescape") {
    /** @par 检查点 */
    CHECK_EQ(url_unescape(""), std::string(""));

    /** 正常 %XX 解码 */
    CHECK_EQ(url_unescape("hello%20world"), std::string("hello world"));
    CHECK_EQ(url_unescape("a%2Bb%3Dc"), std::string("a+b=c"));

    /** 高位字节 %XX 正确解码为对应字节 */
    CHECK_EQ(url_unescape("%80"), std::string("\x80"));
    CHECK_EQ(url_unescape("%FF"), std::string("\xFF"));

    /** 大小写十六进制混用 */
    CHECK_EQ(url_unescape("%e4%b8%ad"), std::string("\xE4\xB8\xAD"));
    CHECK_EQ(url_unescape("%E4%B8%AD"), std::string("\xE4\xB8\xAD"));

    /** 非法序列原样保留：截断的 % 与非法十六进制字符 */
    CHECK_EQ(url_unescape("%"), std::string("%"));
    CHECK_EQ(url_unescape("%A"), std::string("%A"));
    CHECK_EQ(url_unescape("%ZZ"), std::string("%ZZ"));
    CHECK_EQ(url_unescape("100%"), std::string("100%"));

    /** 连续编码序列 */
    CHECK_EQ(url_unescape("%41%42%43"), std::string("ABC"));
}

TEST_CASE("test_url_roundtrip") {
    /** @par 检查点 */
    /** escape/unescape 往返一致（含非 ASCII 字节） */
    std::string inputs[] = {
      "hello world",
      "a=1&b=2",
      "path/to/resource?query=1",
      "\xE4\xB8\xAD\xE6\x96\x87",
      "\x80\xFF",
      "mix\xE4\xB8\xAD & text",
    };
    for (const auto& src : inputs) {
        std::string escaped = url_escape(src.c_str());
        CHECK_EQ(url_unescape(escaped.c_str()), src);
    }
}
