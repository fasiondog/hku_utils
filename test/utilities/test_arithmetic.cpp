#include "doctest/doctest.h"
#include <hikyuu/utilities/arithmetic.h>
#include <hikyuu/utilities/Log.h>

using namespace hku;

TEST_CASE("test_string_to_upper") {
    std::string x("abcd");
    to_upper(x);
    CHECK(x == "ABCD");

    std::string y("中abcdD");
    to_upper(y);
    CHECK(y == "中ABCDD");
}

TEST_CASE("test_string_to_lower") {
    std::string x("ABcD");
    to_lower(x);
    CHECK(x == "abcd");

    std::string y("中abCdD");
    to_lower(y);
    CHECK(y == "中abcdd");
}

TEST_CASE("test_utf8_gb_convert") {
    /** empty input yields empty output (previously malloc(0) + unchecked iconv) */
    CHECK_EQ(utf8_to_gb(std::string()), std::string());
    CHECK_EQ(gb_to_utf8(std::string()), std::string());

    /** ASCII is identical in utf-8 and gbk, so the round trip must be preserved on any platform */
    const std::string ascii = "hello world 123";
    std::string gbk = utf8_to_gb(ascii);
    CHECK_EQ(gbk, ascii);
    CHECK_EQ(gb_to_utf8(gbk), ascii);

    /** a valid multi-byte utf-8 CJK string ("中文") converts and round-trips only when the
     *  platform iconv ships a GBK table; when it does not, the fixed code now fails cleanly with an
     *  empty result instead of reading past the output buffer, so only assert when conversion works
     */
    const std::string zh = "\xe4\xb8\xad\xe6\x96\x87";
    std::string zgbk = utf8_to_gb(zh);
    if (!zgbk.empty()) {
        CHECK_UNARY(zgbk.size() <= zh.size());
        CHECK_EQ(gb_to_utf8(zgbk), zh);
    }
}

TEST_CASE("test_byteToHexStr") {
    const char *x = "abcd";
    std::string hex = byteToHexStr(x, 4);
    CHECK_EQ(hex, "61626364");

    std::string y(x);
    hex = byteToHexStr(y);
    CHECK_EQ(hex, "61626364");

    CHECK_EQ("", byteToHexStr(""));
}

TEST_CASE("test_byteToHexStrForPrint") {
    const char *x = "abcd";
    std::string hex = byteToHexStrForPrint(x, 4);
    CHECK_EQ(hex, "0x61 0x62 0x63 0x64");

    CHECK_EQ("", byteToHexStrForPrint(""));
}

TEST_CASE("test_split_by_char") {
    std::string x("");
    auto splits = split(x, '.');
    CHECK_EQ(splits.size(), 1);
    CHECK_EQ(splits[0], x);

    x = "100.1.";
    splits = split(x, '.');
    CHECK_EQ(splits.size(), 3);
    CHECK_EQ(splits[0], "100");
    CHECK_EQ(splits[1], "1");

    x = "..";
    splits = split(x, '.');
    CHECK_EQ(splits.size(), 3);
    CHECK_EQ(splits[0], "");
    CHECK_EQ(splits[1], "");
    CHECK_EQ(splits[2], "");
}

TEST_CASE("test_split_by_string") {
    std::string x("");

    // 分割字符串为空
    auto splits = split(x, "");
    CHECK_EQ(splits.size(), 1);
    CHECK_EQ(splits[0], x);

    x = "123";
    splits = split(x, "");
    CHECK_EQ(splits.size(), 1);
    CHECK_EQ(splits[0], x);

    // 分割字符串长度为1
    x = "100.1.";
    splits = split(x, ".");
    CHECK_EQ(splits.size(), 3);
    CHECK_EQ(splits[0], "100");
    CHECK_EQ(splits[1], "1");
    CHECK_EQ(splits[2], "");

    // 分割字符串长度为2
    x = "100.1.234.1.56";
    splits = split(x, ".1");
    CHECK_EQ(splits.size(), 3);
    CHECK_EQ(splits[0], "100");
    CHECK_EQ(splits[1], ".234");
    CHECK_EQ(splits[2], ".56");

    x = "..";
    splits = split(x, ".");
    CHECK_EQ(splits.size(), 3);
    CHECK_EQ(splits[0], "");
    CHECK_EQ(splits[1], "");
    CHECK_EQ(splits[2], "");
}

TEST_CASE("test_utf8_to_lower") {
    CHECK(utf8_to_lower("ABC") == "abc");
    CHECK(utf8_to_lower("AbCd") == "abcd");
    CHECK(utf8_to_lower("中国ABC") == "中国abc");
    CHECK(utf8_to_lower("Hello World!") == "hello world!");
    CHECK(utf8_to_lower("") == "");
    CHECK(utf8_to_lower("123") == "123");
    CHECK(utf8_to_lower("ÉÀÈÂÊÎÔÛÄËÏÖÜÇ") == "éàèâêîôûäëïöüç");
}

TEST_CASE("test_utf8_to_upper") {
    CHECK(utf8_to_upper("abc") == "ABC");
    CHECK(utf8_to_upper("AbCd") == "ABCD");
    CHECK(utf8_to_upper("中国abc") == "中国ABC");
    CHECK(utf8_to_upper("hello world!") == "HELLO WORLD!");
    CHECK(utf8_to_upper("") == "");
    CHECK(utf8_to_upper("123") == "123");
    CHECK(utf8_to_upper("éàèâêîôûäëïöüç") == "ÉÀÈÂÊÎÔÛÄËÏÖÜÇ");
}

TEST_CASE("test_utf8_fold_equal") {
    CHECK(utf8_fold_equal("ABC", "abc") == true);
    CHECK(utf8_fold_equal("AbCd", "aBcD") == true);
    CHECK(utf8_fold_equal("中国ABC", "中国abc") == true);
    CHECK(utf8_fold_equal("Hello World", "hello world") == true);
    CHECK(utf8_fold_equal("", "") == true);
    CHECK(utf8_fold_equal("123", "123") == true);
    CHECK(utf8_fold_equal("ÉÀÈ", "éàè") == true);
    CHECK(utf8_fold_equal("ABC", "ABD") == false);
    CHECK(utf8_fold_equal("abc", "abcd") == false);
    CHECK(utf8_fold_equal("", "a") == false);
}

TEST_CASE("test_utf8_contains") {
    CHECK(utf8_contains("Hello World", "World") == true);
    CHECK(utf8_contains("Hello World", "hello") == false);
    CHECK(utf8_contains("中国ABC", "ABC") == true);
    CHECK(utf8_contains("中国ABC", "abc") == false);
    CHECK(utf8_contains("Hello World", "") == true);
    CHECK(utf8_contains("", "") == true);
    CHECK(utf8_contains("", "a") == false);
    CHECK(utf8_contains("ÉÀÈÂÊ", "ÂÊ") == true);
    CHECK(utf8_contains("abcdef", "bcd") == true);
    CHECK(utf8_contains("abcdef", "xyz") == false);
}
TEST_CASE("test_string_trim") {
    /** @arg 常规两端空格 */
    std::string s1("  abc  ");
    trim(s1);
    CHECK(s1 == "abc");

    /** @arg 尾部 CRLF：不得残留 "\\r"（旧实现三段 erase 相互干扰） */
    std::string s2("abc\r\n");
    trim(s2);
    CHECK(s2 == "abc");

    /** @arg 混合空白（空格、制表符、CR、LF）两端全部移除 */
    std::string s3(" \t\r\nabc \t\r\n");
    trim(s3);
    CHECK(s3 == "abc");

    /** @arg 前导 CRLF 同样被移除（旧实现仅处理前导空格） */
    std::string s4("\r\nabc");
    trim(s4);
    CHECK(s4 == "abc");

    /** @arg 纯空白字符串收敛为空，且无越界 */
    std::string s5(" \r\n\t ");
    trim(s5);
    CHECK(s5.empty());

    /** @arg 空字符串安全 */
    std::string s6;
    trim(s6);
    CHECK(s6.empty());

    /** @arg 无空白时原样保留 */
    std::string s7("abc");
    trim(s7);
    CHECK(s7 == "abc");
}

TEST_CASE("test_roundUp") {
    /** @arg 已是整数步进值必须保持不变（旧实现 10.0 被错误加为 11.0） */
    CHECK_EQ(roundUp(10.0), 10.0);
    CHECK_EQ(roundUp(-10.0), -10.0);
    CHECK_EQ(roundUp(10.0, 1), 10.0);
    CHECK_EQ(roundUp(10.0, -1), 10.0);

    /** @arg 正数向远离零方向取整 */
    CHECK_EQ(roundUp(10.1), 11.0);
    CHECK_EQ(roundUp(10.5), 11.0);

    /** @arg 负数向远离零方向取整 */
    CHECK_EQ(roundUp(-10.1), -11.0);

    /** @arg 保留小数位 */
    CHECK_EQ(roundUp(1.21, 1), 1.3);
    CHECK_EQ(roundUp(1.0, 1), 1.0);

    /** @arg 负 ndigits：向高位取整 */
    CHECK_EQ(roundUp(15.0, -1), 20.0);
    CHECK_EQ(roundUp(10.0, -1), 10.0);
}
