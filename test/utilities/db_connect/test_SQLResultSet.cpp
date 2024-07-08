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
#include <hikyuu/utilities/db_connect/sqlite/SQLiteConnect.h>

using namespace hku;

struct FaceCodeTable {
    TABLE_BIND5(FaceCodeTable, face_code, cluster, user_id, user_name, feature, extra);

    uint64_t cluster = 0;
    uint64_t user_id = Null<uint64_t>();
    std::string user_name;
    std::vector<float> feature;
    int extra = 0;
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
