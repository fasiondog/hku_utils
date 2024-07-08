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

TEST_CASE("test_split") {
#if !HKU_OS_IOS && CPP_STANDARD >= CPP_STANDARD_17
    std::string x("");
    auto splits = split(x, '.');
    CHECK_EQ(splits.size(), 1);
    CHECK_EQ(splits[0], x);

    x = "100.1.";
    splits = split(x, '.');
    CHECK_EQ(splits.size(), 3);
    CHECK_EQ(splits[0], "100");
    CHECK_EQ(splits[1], "1");
    CHECK_EQ(splits[2], "");

    std::string_view y(x);
    auto splits_y = split(y, '.');
    CHECK_EQ(splits_y.size(), 3);
    CHECK_EQ(splits_y[0], "100");
    CHECK_EQ(splits_y[1], "1");
    CHECK_EQ(splits_y[2], "");
#endif
}