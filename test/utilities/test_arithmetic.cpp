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