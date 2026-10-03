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

TEST_CASE("test_DBCondition_params_fragment") {
    /** 字符串值走占位符，展开式与转义路径逐字节一致 */
    DBCondition d = Field("name") == "test";
    CHECK_EQ(d.sql(), R"((name=?1))");
    CHECK_EQ(d.str(), R"((name="test"))");
    CHECK_UNARY(d.hasParams());
    CHECK_UNARY(d.getOrderBy().empty());
    CHECK_EQ(d.getLimit(), -1);
    REQUIRE_EQ(d.params().size(), (size_t)1);
    CHECK_UNARY(std::holds_alternative<std::string>(d.params()[0]));
    CHECK_EQ(std::get<std::string>(d.params()[0]), "test");

    /** 全部比较运算符的 std::string 与 const char* 两种形式 */
    d = Field("name") != std::string("test");
    CHECK_EQ(d.sql(), R"((name<>?1))");
    CHECK_EQ(d.str(), R"((name<>"test"))");

    d = Field("name") > std::string("test");
    CHECK_EQ(d.sql(), R"((name>?1))");
    CHECK_EQ(d.str(), R"((name>"test"))");

    d = Field("name") >= std::string("test");
    CHECK_EQ(d.sql(), R"((name>=?1))");
    CHECK_EQ(d.str(), R"((name>="test"))");

    d = Field("name") < std::string("test");
    CHECK_EQ(d.sql(), R"((name<?1))");
    CHECK_EQ(d.str(), R"((name<"test"))");

    d = Field("name") <= std::string("test");
    CHECK_EQ(d.sql(), R"((name<=?1))");
    CHECK_EQ(d.str(), R"((name<="test"))");

    d = Field("name") == "test";
    CHECK_EQ(d.sql(), R"((name=?1))");
    d = Field("name") != "test";
    CHECK_EQ(d.sql(), R"((name<>?1))");
    d = Field("name") > "test";
    CHECK_EQ(d.sql(), R"((name>?1))");
    d = Field("name") >= "test";
    CHECK_EQ(d.sql(), R"((name>=?1))");
    d = Field("name") < "test";
    CHECK_EQ(d.sql(), R"((name<?1))");
    d = Field("name") <= "test";
    CHECK_EQ(d.sql(), R"((name<=?1))");

    /** like 的模式串同样走占位符，通配符保留 */
    d = Field("name").like("%a_'%");
    CHECK_EQ(d.sql(), R"((name like ?1))");
    CHECK_EQ(d.str(), R"((name like "%a_'%"))");
    d = Field("name").like(std::string("%b%"));
    CHECK_EQ(d.sql(), R"((name like ?1))");

    /** in / not_in 逐元素产参，编号连续 */
    d = Field("name").in(std::vector<std::string>({("a"), ("b"), ("c")}));
    CHECK_EQ(d.sql(), R"((name in (?1,?2,?3)))");
    CHECK_EQ(d.str(), R"((name in ("a","b","c")))");
    CHECK_EQ(d.params().size(), (size_t)3);

    d = Field("name").not_in(std::vector<std::string>({("a"), ("b")}));
    CHECK_EQ(d.sql(), R"((name not in (?1,?2)))");
    CHECK_EQ(d.str(), R"((name not in ("a","b")))");

    /** 指针列表同样携带文本值，不能回退到拼接路径 */
    d = Field("name").in(std::vector<const char *>({("a"), ("b")}));
    CHECK_EQ(d.sql(), R"((name in (?1,?2)))");
    CHECK_EQ(d.str(), R"((name in ("a","b")))");
    CHECK_EQ(d.params().size(), (size_t)2);

    d = Field("name").not_in(std::vector<const char *>({("a"), ("b")}));
    CHECK_EQ(d.sql(), R"((name not in (?1,?2)))");

    /** 有占位符却无值：构造期拒绝，不能把 ?N 当文本交给驱动 */
    CHECK_THROWS_AS(DBCondition::fromFragment("(name=?1)", BoundValues{}), hku::exception);

    /** 数字、浮点、Datetime 保持内联，不产生参数 */
    d = Field("id") == 1;
    CHECK_EQ(d.sql(), "(id=1)");
    CHECK_EQ(d.str(), "(id=1)");
    CHECK_UNARY(!d.hasParams());

    d = Field("id") >= 3.14;
    CHECK_EQ(d.sql(), "(id>=3.14)");
    CHECK_UNARY(!d.hasParams());

    d = Field("id").in(std::vector<int>({1, 2, 3}));
    CHECK_EQ(d.sql(), "(id in (1,2,3))");
    CHECK_UNARY(!d.hasParams());

    /** 手工书写的条件不被解析，原样保留 */
    d = DBCondition("id=1");
    CHECK_EQ(d.sql(), "id=1");
    CHECK_EQ(d.str(), "id=1");
    CHECK_UNARY(!d.hasParams());

    /** 空条件 */
    d = DBCondition();
    CHECK_UNARY(d.sql().empty());
    CHECK_UNARY(d.str().empty());
    CHECK_UNARY(!d.hasParams());

    /** 含引号的值只在参数向量里，展开式仍按 SQL 标准加倍 */
    d = Field("code") == std::string(R"X(sh600000" or "1"="1)X");
    CHECK_EQ(d.sql(), R"((code=?1))");
    CHECK_EQ(d.str(), "(code=\"sh600000\"\" or \"\"1\"\"=\"\"1\")");
    CHECK_EQ(std::get<std::string>(d.params()[0]), R"X(sh600000" or "1"="1)X");
}

TEST_CASE("test_DBCondition_params_merge") {
    /** & 合并：右侧重开编号，向量追加，两形态同步 */
    DBCondition a = Field("name") == "x";
    DBCondition b = Field("code") == "y";
    a & b;
    CHECK_EQ(a.sql(), R"(((name=?1) and (code=?2)))");
    CHECK_EQ(a.str(), R"(((name="x") and (code="y")))");
    REQUIRE_EQ(a.params().size(), (size_t)2);
    CHECK_EQ(std::get<std::string>(a.params()[0]), "x");
    CHECK_EQ(std::get<std::string>(a.params()[1]), "y");

    /** | 合并，且与多参数的 in 片段错位顺延 */
    DBCondition c = Field("tag").in(std::vector<std::string>({("p"), ("q")}));
    DBCondition d = Field("name") == "x";
    d | c;
    CHECK_EQ(d.sql(), R"(((name=?1) or (tag in (?2,?3))))");
    CHECK_EQ(d.str(), R"(((name="x") or (tag in ("p","q"))))");
    REQUIRE_EQ(d.params().size(), (size_t)3);
    CHECK_EQ(std::get<std::string>(d.params()[2]), "q");

    /** 三段链式合并后编号仍连续 */
    DBCondition e = Field("a") == "1";
    e = e & (Field("b") == "2") & (Field("c") == "3");
    CHECK_EQ(e.sql(), R"((((a=?1) and (b=?2)) and (c=?3)))");
    CHECK_EQ(e.params().size(), (size_t)3);

    /** 空侧合并：空的一侧不改变另一侧 */
    DBCondition empty;
    DBCondition f = Field("name") == "x";
    empty & f;
    CHECK_EQ(empty.sql(), R"((name=?1))");
    CHECK_EQ(empty.params().size(), (size_t)1);

    DBCondition g = Field("name") == "x";
    g &DBCondition();
    CHECK_EQ(g.sql(), R"((name=?1))");
    CHECK_EQ(g.params().size(), (size_t)1);

    /** 自合并不重复追加 */
    DBCondition h = Field("name") == "x";
    h & h;
    CHECK_EQ(h.sql(), R"((name=?1))");
    CHECK_EQ(h.params().size(), (size_t)1);

    /** 手工条件与参数条件合并时，两侧文本都保留 */
    DBCondition i = DBCondition("id>0");
    i &(Field("name") == "x");
    CHECK_EQ(i.sql(), R"((id>0 and (name=?1)))");
    CHECK_EQ(i.str(), R"((id>0 and (name="x")))");

    /** 只带尾部子句的一侧参与合并不产生悬空的 "(A and )" */
    DBCondition tail_only;
    tail_only.orderBy("age", DBCondition::ORDER_ASC);
    tail_only &(Field("name") == "x");
    CHECK_EQ(tail_only.sql(), R"((name=?1))");
    CHECK_EQ(tail_only.str(), R"((name="x") order by age ASC)");

    DBCondition j = Field("name") == "x";
    j & tail_only;
    CHECK_EQ(j.sql(), R"(((name=?1) and (name=?2)))");
    CHECK_EQ(j.getOrderBy(), " order by age ASC");
    CHECK_EQ(j.params().size(), (size_t)2);
}

TEST_CASE("test_DBCondition_tail_parts") {
    /** order-by 与 limit 留在尾部，不进入参数化的 where 段 */
    DBCondition d = (Field("name") == "x") + ASC("age");
    CHECK_EQ(d.sql(), R"((name=?1))");
    CHECK_EQ(d.getOrderBy(), " order by age ASC");
    CHECK_EQ(d.getLimit(), -1);
    CHECK_EQ(d.str(), R"((name="x") order by age ASC)");

    d = (Field("name") == "x") + DESC("age");
    CHECK_EQ(d.getOrderBy(), " order by age DESC");

    /** orderBy 方法同样只动尾部 */
    d = Field("name") == "x";
    d.orderBy("report_date", DBCondition::ORDER_ASC);
    CHECK_EQ(d.getOrderBy(), " order by report_date ASC");
    CHECK_EQ(d.str(), R"((name="x") order by report_date ASC)");

    /** limit 0 是合法的行数限制，不当作「没有 limit」 */
    d = (Field("name") == "x") + LIMIT(0);
    CHECK_EQ(d.getLimit(), 0);
    CHECK_EQ(d.str(), R"((name="x") limit 0)");

    d = (Field("name") == "x") + LIMIT(5);
    CHECK_EQ(d.getLimit(), 5);
    CHECK_EQ(d.str(), R"((name="x") limit 5)");

    /** 先 limit 再合并：尾部仍只出现在末尾 */
    DBCondition a = (Field("name") == "x") + ASC("age");
    DBCondition b = Field("code") == "y";
    a & b;
    CHECK_EQ(a.sql(), R"(((name=?1) and (code=?2)))");
    CHECK_EQ(a.getOrderBy(), " order by age ASC");
    CHECK_EQ(a.str(), R"(((name="x") and (code="y")) order by age ASC)");

    /** conditionParts 给出「where 子句 + 值」，与 str() 的位置一致 */
    auto parts = conditionParts((Field("name") == "x") + ASC("age") + LIMIT(3));
    CHECK_EQ(parts.first, R"((name=?1) order by age ASC limit 3)");
    REQUIRE_EQ(parts.second.size(), (size_t)1);
    CHECK_EQ(std::get<std::string>(parts.second[0]), "x");

    /** 无条件时 conditionParts 与 str() 一致 */
    parts = conditionParts(DBCondition());
    CHECK_UNARY(parts.first.empty());
    CHECK_UNARY(parts.second.empty());
}

TEST_CASE("test_DBCondition_renumber") {
    BoundValues one{std::string("x")};
    BoundValues two{std::string("a"), std::string("b")};
    BoundValues none;

    /** 正常：编号改写为匿名 ?，值按出现顺序排列 */
    auto [sql1, o1] = renumberPlaceholders("select * from t where (x=?1) and (y=?2)", two);
    CHECK_EQ(sql1, "select * from t where (x=?) and (y=?)");
    REQUIRE_EQ(o1.size(), (size_t)2);
    CHECK_EQ(std::get<std::string>(o1[0]), "a");
    CHECK_EQ(std::get<std::string>(o1[1]), "b");

    /** 编号乱序书写时按位置重排，驱动侧仍是左到右顺序绑定 */
    auto [sql2, o2] = renumberPlaceholders("where (x=?2) and (y=?1)", two);
    CHECK_EQ(sql2, "where (x=?) and (y=?)");
    CHECK_EQ(std::get<std::string>(o2[0]), "b");
    CHECK_EQ(std::get<std::string>(o2[1]), "a");

    /** 无占位符：原样返回（内联路径零开销） */
    auto [sql3, o3] = renumberPlaceholders("where (x=1)", none);
    CHECK_EQ(sql3, "where (x=1)");
    CHECK_UNARY(o3.empty());

    /** 仅匿名占位符且数量匹配：原样返回 */
    auto [sql4, o4] = renumberPlaceholders("where (x=?)", one);
    CHECK_EQ(sql4, "where (x=?)");
    CHECK_EQ(o4.size(), (size_t)1);

    /** 字面量 / 引号标识符 / 注释中的 ?N 不算占位符 */
    auto [sql5, o5] =
      renumberPlaceholders("where a='?1' and b=\"?1\" and c=`?1` and d=?1 /* ?2 */", one);
    CHECK_EQ(sql5, "where a='?1' and b=\"?1\" and c=`?1` and d=? /* ?2 */");
    CHECK_EQ(std::get<std::string>(o5[0]), "x");

    /** 行注释中的 ? 不算匿名占位符 */
    auto [sql6, o6] = renumberPlaceholders("where a=?1 -- who knows?\nand b=2", one);
    CHECK_EQ(sql6, "where a=? -- who knows?\nand b=2");
    CHECK_EQ(o6.size(), (size_t)1);

    /** 报错：混用、跳号、重复、数量不符 */
    CHECK_THROWS_AS(renumberPlaceholders("where a=? and b=?1", one), hku::exception);
    CHECK_THROWS_AS(renumberPlaceholders("where a=?1 and b=?3", two), hku::exception);
    CHECK_THROWS_AS(renumberPlaceholders("where a=?1 and b=?1", one), hku::exception);
    CHECK_THROWS_AS(renumberPlaceholders("where a=?1", none), hku::exception);
    CHECK_THROWS_AS(renumberPlaceholders("where a=?1 and b=?2", one), hku::exception);
    CHECK_THROWS_AS(renumberPlaceholders("where a=?1 and b=?", one), hku::exception);
    CHECK_THROWS_AS(renumberPlaceholders("where a=1", one), hku::exception);
}

TEST_CASE("test_DBCondition_name_validation") {
    /** 合法名称逐字节不变，含 order / limit 字样的列名正常使用 */
    CHECK_EQ(Field("order_date").name, "order_date");
    CHECK_EQ(Field("amount_limit").name, "amount_limit");
    CHECK_EQ((Field("order_date") == "2024").str(), R"((order_date="2024"))");
    CHECK_EQ((Field("order_date") == "2024").sql(), R"((order_date=?1))");

    /** 非法名称构造期拒绝 */
    CHECK_THROWS_AS(Field(""), hku::exception);
    CHECK_THROWS_AS(Field(std::string("a\0b", 3)), hku::exception);
    CHECK_THROWS_AS(Field("a\"b"), hku::exception);
    CHECK_THROWS_AS(Field("a'b"), hku::exception);
    CHECK_THROWS_AS(Field("a`b"), hku::exception);
    CHECK_THROWS_AS(Field("a?1"), hku::exception);
    CHECK_THROWS_AS(Field("a;b"), hku::exception);
    CHECK_THROWS_AS(Field("a#b"), hku::exception);
    CHECK_THROWS_AS(Field("a--b"), hku::exception);
    CHECK_THROWS_AS(Field("/*x"), hku::exception);
    CHECK_THROWS_AS(Field("*/x"), hku::exception);

    /** order-by 的列名走同一规则，且报错时不改动条件 */
    DBCondition d = Field("id") == 1;
    CHECK_THROWS_AS(d.orderBy("a`b", DBCondition::ORDER_ASC), hku::exception);
    CHECK_UNARY(d.getOrderBy().empty());
    CHECK_THROWS_AS((Field("id") == 1) + ASC("a;b"), hku::exception);
    CHECK_NOTHROW((Field("id") == 1) + ASC("order_date"));
}

TEST_CASE("test_DBCondition_in_params_limit") {
    /** 超过驱动占位符上限时显式报错，而不是产出驱动必然拒绝的语句 */
    std::vector<std::string> over(MAX_SQL_BIND_PARAMS + 1, "x");
    CHECK_THROWS_AS(Field("code").in(over), hku::exception);
    CHECK_THROWS_AS(Field("code").not_in(over), hku::exception);

    /** 边界：正好等于上限时通过 */
    std::vector<std::string> max_at(MAX_SQL_BIND_PARAMS, "x");
    CHECK_NOTHROW(Field("code").in(max_at));
    CHECK_NOTHROW(Field("code").not_in(max_at));
}

TEST_CASE("test_splitWhereParts") {
    /** 无尾部子句：整体作为过滤条件 */
    WhereParts p = splitWhereParts("(a=1)");
    CHECK_EQ(p.where, "(a=1)");
    CHECK_UNARY(p.orderBy.empty());
    CHECK_EQ(p.limit, -1);

    p = splitWhereParts("");
    CHECK_UNARY(p.where.empty());
    CHECK_EQ(p.limit, -1);

    /** 含 order / limit 字样的列名不被误切（回归既有缺陷） */
    p = splitWhereParts(R"((order_date="2024"))");
    CHECK_EQ(p.where, R"((order_date="2024"))");
    CHECK_UNARY(p.orderBy.empty());

    p = splitWhereParts("(amount_limit=5)");
    CHECK_EQ(p.where, "(amount_limit=5)");
    CHECK_UNARY(p.orderBy.empty());

    /** 单独的 order 列名不子句起点 */
    p = splitWhereParts("(order=1)");
    CHECK_EQ(p.where, "(order=1)");
    CHECK_UNARY(p.orderBy.empty());

    /** order by 切出尾部：切点前的空格留在 where，关键字本身归 orderBy */
    p = splitWhereParts("(a=1) order by x ASC");
    CHECK_EQ(p.where, "(a=1) ");
    CHECK_EQ(p.orderBy, "order by x ASC");
    CHECK_EQ(p.limit, -1);

    /** order by + limit（回归 limit 落入 WHERE 的既有缺陷） */
    p = splitWhereParts("(a=1) order by x ASC limit 5");
    CHECK_EQ(p.where, "(a=1) ");
    CHECK_EQ(p.orderBy, "order by x ASC ");
    CHECK_EQ(p.limit, 5);

    /** 仅 limit */
    p = splitWhereParts("(a=1) limit 7");
    CHECK_EQ(p.where, "(a=1) ");
    CHECK_UNARY(p.orderBy.empty());
    CHECK_EQ(p.limit, 7);

    /** 关键字大小写不敏感 */
    p = splitWhereParts("(a=1) ORDER BY x DESC LIMIT 3");
    CHECK_EQ(p.where, "(a=1) ");
    CHECK_EQ(p.orderBy, "ORDER BY x DESC ");
    CHECK_EQ(p.limit, 3);

    /** 字面量中的 order by 不触发切分 */
    p = splitWhereParts("(name='order by x')");
    CHECK_EQ(p.where, "(name='order by x')");
    CHECK_UNARY(p.orderBy.empty());

    /** 子句写法不合 SQL 语法时不强行改写，文本完整保留 */
    p = splitWhereParts("(a=1) limit ?");
    CHECK_EQ(p.where, "(a=1) limit ?");
    CHECK_UNARY(p.orderBy.empty());
    CHECK_EQ(p.limit, -1);

    /** MySQL 的 limit n, m 双数字形式不是纯 limit，整段保留交由驱动报错 */
    p = splitWhereParts("(a=1) limit 5, 10");
    CHECK_EQ(p.where, "(a=1) limit 5, 10");
    CHECK_UNARY(p.orderBy.empty());
    CHECK_EQ(p.limit, -1);

    /** limit 数字后跟分号同样不是纯 limit */
    p = splitWhereParts("(a=1) limit 5;");
    CHECK_EQ(p.where, "(a=1) limit 5;");
    CHECK_EQ(p.limit, -1);

    p = splitWhereParts("(a=1) limit 5 order by x");
    CHECK_EQ(p.where, "(a=1) limit 5 ");
    CHECK_EQ(p.orderBy, "order by x");
    CHECK_EQ(p.limit, -1);
}