/*
 *  Copyright(C) 2021 hikyuu.org
 *
 *  Create on: 2021-05-21
 *     Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/db_connect/DBCondition.h>
#include <hikyuu/utilities/Log.h>
#include <fmt/ranges.h>

using namespace hku;

TEST_CASE("test_DBCondition") {
    DBCondition d = Field("name") == "test";
    CHECK_EQ(d.str(), R"((name="test"))");

    d = Field("name") != "test";
    CHECK_EQ(d.str(), R"((name<>"test"))");

    d = Field("name") >= "test";
    CHECK_EQ(d.str(), R"((name>="test"))");

    d = Field("name") <= "test";
    CHECK_EQ(d.str(), R"((name<="test"))");

    d = Field("name") > "test";
    CHECK_EQ(d.str(), R"((name>"test"))");

    d = Field("name") < "test";
    CHECK_EQ(d.str(), R"((name<"test"))");

    CHECK_EQ(Field("name").like("*test").str(), R"((name like "*test"))");

    d = Field("name") == std::string("test");
    CHECK_EQ(d.str(), R"((name="test"))");

    d = Field("name") != std::string("test");
    CHECK_EQ(d.str(), R"((name<>"test"))");

    d = Field("name") >= std::string("test");
    CHECK_EQ(d.str(), R"((name>="test"))");

    d = Field("name") <= std::string("test");
    CHECK_EQ(d.str(), R"((name<="test"))");

    d = Field("name") > std::string("test");
    CHECK_EQ(d.str(), R"((name>"test"))");

    d = Field("name") < std::string("test");
    CHECK_EQ(d.str(), R"((name<"test"))");

    CHECK_EQ(Field("name").like("*test").str(), R"((name like "*test"))");

    d = Field("name").in(std::vector<std::string>({"1", "2", "3"}));
    CHECK_EQ(d.str(), R"((name in ("1","2","3")))");

    d = Field("name").not_in(std::vector<std::string>({"1", "2", "3"}));
    CHECK_EQ(d.str(), R"((name not in ("1","2","3")))");

    CHECK_THROWS_AS(Field("name").in(std::vector<std::string>()), hku::exception);

    d = Field("id") == 1;
    CHECK_EQ(d.str(), "(id=1)");

    d = Field("id") != 1;
    CHECK_EQ(d.str(), "(id<>1)");

    d = Field("id") <= 1;
    CHECK_EQ(d.str(), "(id<=1)");

    d = Field("id") >= 1;
    CHECK_EQ(d.str(), "(id>=1)");

    d = Field("id") > 1;
    CHECK_EQ(d.str(), "(id>1)");

    d = Field("id") < 1;
    CHECK_EQ(d.str(), "(id<1)");

    CHECK_EQ(Field("id").in(std::vector<int>({1, 2, 3})).str(), "(id in (1,2,3))");
    CHECK_THROWS_AS(Field("id").in(std::vector<int>()), hku::exception);

    CHECK_EQ(Field("id").not_in(std::vector<int>({1, 2, 3})).str(), "(id not in (1,2,3))");
    CHECK_THROWS_AS(Field("id").not_in(std::vector<int>()), hku::exception);

    d = Field("id") == 3.14;
    CHECK_EQ(d.str(), "(id=3.14)");

    d = Field("id") != 3.14;
    CHECK_EQ(d.str(), "(id<>3.14)");

    d = Field("id") >= 3.14;
    CHECK_EQ(d.str(), "(id>=3.14)");

    d = Field("id") <= 3.14;
    CHECK_EQ(d.str(), "(id<=3.14)");

    d = Field("id") > 3.14;
    CHECK_EQ(d.str(), "(id>3.14)");

    d = Field("id") < 3.14;
    CHECK_EQ(d.str(), "(id<3.14)");

    CHECK_EQ(Field("id").in(std::vector<double>({3.14, 3.14, 3.14})).str(),
             "(id in (3.14,3.14,3.14))");

    CHECK_EQ(Field("id").not_in(std::vector<double>({3.14, 3.14, 3.14})).str(),
             "(id not in (3.14,3.14,3.14))");

    CHECK_THROWS_AS(Field("id").in(std::vector<double>()), hku::exception);
    CHECK_THROWS_AS(Field("id").not_in(std::vector<double>()), hku::exception);
}

TEST_CASE("test_DBCondition_sql_escape") {
    /** Normal values: byte-for-byte identical to the old output when no quote is present */
    CHECK_EQ(sqlStringLiteral("test"), R"X("test")X");
    CHECK_EQ(sqlStringLiteral(""), R"X("")X");

    /** Edge cases: bare quote, consecutive quotes, leading/trailing quotes */
    CHECK_EQ(sqlStringLiteral("\""), "\"\"\"\"");  // single quote -> wrapped + doubled = 4 quotes
    CHECK_EQ(sqlStringLiteral("\"\""), "\"\"\"\"\"\"");
    CHECK_EQ(sqlStringLiteral("\"ab\""), "\"\"\"ab\"\"\"");
    CHECK_EQ(sqlStringLiteral("a\"b\"c"), R"X("a""b""c")X");

    /** Single quotes are left as-is (no closing risk inside double-quoted literals) */
    CHECK_EQ(sqlStringLiteral("it's"), R"X("it's")X");

    /** Comparison operators: embedded quotes must not close the literal early */
    DBCondition d = Field("name") == std::string("a\" or 1=1 -- ");
    CHECK_EQ(d.str(), R"X((name="a"" or 1=1 -- "))X");

    d = Field("name") != std::string("a\"b");
    CHECK_EQ(d.str(), R"X((name<>"a""b"))X");

    d = Field("name") > std::string("a\"b");
    CHECK_EQ(d.str(), R"X((name>"a""b"))X");

    d = Field("name") < std::string("a\"b");
    CHECK_EQ(d.str(), R"X((name<"a""b"))X");

    d = Field("name") >= std::string("a\"b");
    CHECK_EQ(d.str(), R"X((name>="a""b"))X");

    d = Field("name") <= std::string("a\"b");
    CHECK_EQ(d.str(), R"X((name<="a""b"))X");

    /** const char* overloads are escaped as well */
    d = Field("name") == "a\"b";
    CHECK_EQ(d.str(), R"X((name="a""b"))X");

    d = Field("name") != "a\"b";
    CHECK_EQ(d.str(), R"X((name<>"a""b"))X");

    d = Field("name") >= "a\"b";
    CHECK_EQ(d.str(), R"X((name>="a""b"))X");

    /** like: quotes in the pattern are escaped, wildcards % and _ are kept */
    CHECK_EQ(Field("name").like("%a\"b%").str(), R"X((name like "%a""b%"))X");
    CHECK_EQ(Field("name").like(std::string("a_'%")).str(), R"X((name like "a_'%"))X");

    /** in / not in: each element is escaped; normal values match the old output */
    d = Field("name").in(std::vector<std::string>({"a\"b", "c"}));
    CHECK_EQ(d.str(), R"X((name in ("a""b","c")))X");

    d = Field("name").not_in(std::vector<std::string>({"a\"b", "c\""}));
    CHECK_EQ(d.str(), "(name not in (\"a\"\"b\",\"c\"\"\"))");

    /** Typical injection payload: quote breakout + always-true condition; after escaping it stays a
     * literal */
    d = Field("code") == std::string(R"X(sh600000" or "1"="1)X");
    CHECK_EQ(d.str(), "(code=\"sh600000\"\" or \"\"1\"\"=\"\"1\")");
}

TEST_CASE("test_DBCondition_sql_identifier") {
    /** Empty names and names containing the null character are rejected */
    CHECK_THROWS_AS(sqlIdentifier(""), hku::exception);
    CHECK_THROWS_AS(sqlIdentifier(std::string("a\0b", 3)), hku::exception);
    CHECK_THROWS_AS(sqlIdentifier(std::string("`a`\0", 4)), hku::exception);

    /** Bare names are wrapped in backticks; embedded backticks are doubled */
    CHECK_EQ(sqlIdentifier("tbl"), R"X(`tbl`)X");
    CHECK_EQ(sqlIdentifier("a`b"), R"X(`a``b`)X");

    /** Idempotent: a validly backtick-quoted name (plain or with doubled inner quotes) is returned
     * unchanged, so callers may pass a pre-quoted table name without double wrapping */
    CHECK_EQ(sqlIdentifier(R"X(`tbl`)X"), R"X(`tbl`)X");
    CHECK_EQ(sqlIdentifier(R"X(`a``b`)X"), R"X(`a``b`)X");

    /** Malformed "quoted" names (an inner backtick that closes the quoting early, or an
     * unterminated trailing quote) are re-escaped as a whole: the payload cannot break out into
     * extra SQL, it just becomes one odd identifier or a plain syntax error */
    CHECK_EQ(sqlIdentifier(R"X(`a`b`)X"), R"X(```a``b```)X");
    CHECK_EQ(sqlIdentifier(R"X(`a``)X"), R"X(```a`````)X");
    CHECK_EQ(sqlIdentifier(R"X(`x` or 1=1 --`)X"), R"X(```x`` or 1=1 --```)X");
    CHECK_EQ(sqlIdentifier(R"X(`x`, `y`)X"), R"X(```x``, ``y```)X");
}