/*
 *  Copyright (c) 2023 hikyuu.org
 *
 *  Created on: 2023-01-10
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/arithmetic.h>
#include <hikyuu/utilities/os.h>
#include <hikyuu/utilities/db_connect/DBConnect.h>

using namespace hku;

struct FaceCodeTable {
    TABLE_BIND5(FaceCodeTable, face_code, cluster, user_id, user_name, feature, extra);

    uint64_t cluster = 0;
    uint64_t user_id = Null<uint64_t>();
    std::string user_name;
    std::vector<float> feature;
    int extra = 0;
};

// 列名含 order / limit 字样，用于回归尾部子句的文本解析
struct OrderTestTable {
    TABLE_BIND3(OrderTestTable, order_test, order_date, amount_limit, memo);

    std::string order_date;
    int amount_limit = 0;
    std::string memo;
};

TEST_CASE("test_SQLResultSet_null_connect") {
    SQLResultSet<FaceCodeTable> results;
    CHECK_UNARY(results.empty());
    CHECK_EQ(results.size(), 0);
    CHECK_THROWS_AS(results.at(0), std::out_of_range);
    CHECK_THROWS_AS(results.at(1000), std::out_of_range);
    auto x = results[0];
    CHECK_UNARY(!x.valid());

    for (const auto& result : results) {
        CHECK_EQ(result.id(), 0);
    }
}

TEST_CASE("test_SQLResultSet") {
    std::string dbname = "sql_result_set.db";
    copyFile("test_data/backup_test.db", dbname);

    Parameter param;
    param.set<std::string>("db", dbname);
    auto con = std::make_shared<SQLiteConnect>(param);

    // 查询全部数据集合
    auto results = con->query<FaceCodeTable>();
    CHECK_EQ(results.size(), 323);
    CHECK_EQ(results.getPageCount(), 7);

    auto x = results[0];
    CHECK_EQ(x.rowid(), 37);

    x = results[1000];
    CHECK_EQ(x.id(), 0);

    CHECK_THROWS_AS(results.at(1000), std::out_of_range);

    size_t id = 37;
    for (size_t i = 0, len = results.size(); i < len; i++) {
        CHECK_EQ(results[i].id(), id);
        id++;
        if (id == 324) {
            id = 1429;
        }
    }

    id = 37;
    for (size_t i = 0, len = results.size(); i < len; i++) {
        CHECK_EQ(results.at(i).id(), id);
        id++;
        if (id == 324) {
            id = 1429;
        }
    }

    id = 37;
    for (auto iter = results.cbegin(); iter != results.cend(); ++iter) {
        // HKU_INFO("id: {}", iter->id());
        if (!iter->valid()) {
            break;
        }
        CHECK_EQ(iter->id(), id);
        id++;
        if (id == 324) {
            id = 1429;
        }
    }

    id = 37;
    for (const auto& x : results) {
        // HKU_INFO("id: {}", x.id());
        CHECK_EQ(x.id(), id);
        id++;
        if (id == 324) {
            id = 1429;
        }
    }

    // 测试查询条件
    results = con->query<FaceCodeTable>((Field("id") == 37) | (Field("id") == 1447));
    CHECK_EQ(results.size(), 2);
    CHECK_EQ(results[0].id(), 37);
    CHECK_EQ(results[1].id(), 1447);
    CHECK_THROWS_AS(results.at(2), std::out_of_range);
}

TEST_CASE("test_SQLResultSet_iterator_copy") {
    std::string dbname = "sql_result_set_iter_copy.db";
    copyFile("test_data/backup_test.db", dbname);

    Parameter param;
    param.set<std::string>("db", dbname);
    auto con = std::make_shared<SQLiteConnect>(param);

    auto results = con->query<FaceCodeTable>();

    auto iter = results.begin();
    CHECK_EQ(iter->id(), 37);
    ++iter;
    ++iter;
    CHECK_EQ(iter->id(), 39);

    /** A copy of a mid-iteration iterator dereferences to the row of its index (the row value
     *  used to be lost in the copy, yielding a default constructed invalid row) */
    auto copied(iter);
    CHECK_EQ(copied->id(), iter->id());
    CHECK_EQ(copied->id(), 39);

    /** Copy assignment carries the row value as well */
    auto assigned = results.begin();
    assigned = iter;
    CHECK_EQ(assigned->id(), 39);

    /** The copied iterator keeps advancing independently of the original */
    ++copied;
    CHECK_EQ(copied->id(), 40);
    CHECK_EQ(iter->id(), 39);
}

TEST_CASE("test_SQLResultSet_condition_parts") {
    std::string dbname = "sql_result_set_parts.db";
    copyFile("test_data/backup_test.db", dbname);

    Parameter param;
    param.set<std::string>("db", dbname);
    auto con = std::make_shared<SQLiteConnect>(param);
    con->exec("drop table if exists order_test");
    con->exec(
      "create table order_test (id integer primary key autoincrement, order_date text, "
      "amount_limit int, memo text)");

    for (int i = 1; i <= 6; i++) {
        OrderTestTable t;
        t.order_date = "2024-01-0" + std::to_string(i);
        t.amount_limit = i;
        t.memo = i % 2 == 0 ? "even" : "odd";
        con->save(t);
    }

    /** 列名含 order 的条件不再被误切成 order-by 子句 */
    auto results = con->query<OrderTestTable, 2>(Field("order_date") >= "2024-01-03");
    CHECK_EQ(results.size(), 4);
    CHECK_EQ(results.getPageCount(), 2);
    CHECK_EQ(results[0].order_date, "2024-01-03");
    CHECK_EQ(results[3].order_date, "2024-01-06");

    /** 带 order-by 尾部的条件分页：值为绑定参数，排序子句参与内外两层查询 */
    auto ordered = con->query<OrderTestTable, 2>((Field("memo") == "even") + DESC("order_date"));
    CHECK_EQ(ordered.size(), 3);
    CHECK_EQ(ordered[0].order_date, "2024-01-06");
    CHECK_EQ(ordered[1].order_date, "2024-01-04");
    CHECK_EQ(ordered[2].order_date, "2024-01-02");
    auto page = ordered.getPage(1);
    REQUIRE_EQ(page.size(), (size_t)1);
    CHECK_EQ(page[0].order_date, "2024-01-02");

    /** 条件的 limit 限制总行数，并与分页协同（超出后返回空页） */
    auto capped = con->query<OrderTestTable, 2>((Field("order_date") >= "2024-01-01") + LIMIT(3));
    CHECK_EQ(capped.size(), 3);
    CHECK_EQ(capped.getPage(0).size(), (size_t)2);
    CHECK_EQ(capped.getPage(1).size(), (size_t)1);
    CHECK_EQ(capped.getPage(2).size(), (size_t)0);

    /** 手工字符串路径同样能识别尾部子句，含 order 的列名不被误切 */
    auto byText =
      con->query<OrderTestTable, 2>("(order_date>=\"2024-01-03\") order by order_date DESC ");
    CHECK_EQ(byText.size(), 4);
    CHECK_EQ(byText[0].order_date, "2024-01-06");
    CHECK_EQ(byText[3].order_date, "2024-01-03");

    /** 条件的 limit 对手工字符串路径同样生效 */
    auto cappedText = con->query<OrderTestTable, 2>("(amount_limit>0) limit 2");
    CHECK_EQ(cappedText.size(), 2);
    CHECK_EQ(cappedText.getPage(0).size(), (size_t)2);

    /** 绑定路径与非分页路径结果一致 */
    std::vector<OrderTestTable> rows;
    con->batchLoad(rows, Field("memo") == "odd");
    CHECK_EQ(rows.size(), (size_t)3);

    /** 含引号的载荷作为值绑定，既能原样存取也不能拓宽条件 */
    OrderTestTable evil;
    evil.order_date = "2024-02-01\" or \"1\"=\"1";
    evil.amount_limit = 1;
    evil.memo = "x";
    con->save(evil);

    std::vector<OrderTestTable> found;
    con->batchLoad(found, Field("order_date") == evil.order_date);
    CHECK_EQ(found.size(), (size_t)1);

    // 该值作为单独的等值条件命中一行，而不是作为恒真表达式命中全部
    found.clear();
    con->batchLoad(found, Field("memo") == "x\" or \"1\"=\"1");
    CHECK_EQ(found.size(), (size_t)0);

    /** 按条件删除走预处理语句 + 绑定值 */
    con->remove("order_test", Field("order_date") == evil.order_date);
    found.clear();
    con->batchLoad(found, Field("order_date") >= "2024-02-01");
    CHECK_EQ(found.size(), (size_t)0);
    CHECK_EQ(con->query<OrderTestTable>().size(), 6);
}

TEST_CASE("test_SQLResultSet_default_order") {
    std::string dbname = "sql_result_set_default_order.db";
    copyFile("test_data/backup_test.db", dbname);

    Parameter param;
    param.set<std::string>("db", dbname);
    auto con = std::make_shared<SQLiteConnect>(param);
    con->exec("drop table if exists order_test");
    con->exec(
      "create table order_test (id integer primary key autoincrement, order_date text, "
      "amount_limit int, memo text)");
    // 额外的索引列，令外层查询存在偏离 id 顺序的扫描/索引选择，用于回归默认分页的稳定顺序
    con->exec("create index order_test_amount on order_test (amount_limit)");

    // 插入 5 行，amount_limit 与 id 顺序相反，以暴露任何非 id 的返回顺序
    for (int i = 1; i <= 5; i++) {
        OrderTestTable t;
        t.order_date = "2024-01-0" + std::to_string(i);
        t.amount_limit = 10 - i;
        t.memo = "m";
        con->save(t);
    }

    /** 无 order-by 的分页：外层缺 ORDER BY 时行序由 SQL 语义决定（未定义），修复后强制按 id 升序，
     *  与内层子查询选页顺序一致，索引映射与跨页顺序稳定 */
    auto results = con->query<OrderTestTable, 2>(Field("amount_limit") > 0);
    CHECK_EQ(results.size(), 5);
    CHECK_EQ(results.getPageCount(), 3);

    // 按全局 index 访问应严格 id 升序
    for (size_t i = 0, len = results.size(); i < len; i++) {
        CHECK_EQ(results[i].id(), static_cast<uint64_t>(i + 1));
    }

    // 逐页取回的行序也应严格 id 升序，且与 index 访问一致
    uint64_t expect = 1;
    for (size_t page = 0, n = results.getPageCount(); page < n; page++) {
        auto rows = results.getPage(page);
        for (const auto& r : rows) {
            CHECK_EQ(r.id(), expect);
            expect++;
        }
    }
    CHECK_EQ(expect, 6);
}
