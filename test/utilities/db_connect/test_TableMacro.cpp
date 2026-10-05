/*
 * test_TableMacro.cpp
 *
 *  Copyright (c) 2023, hikyuu.org
 *
 *  Created on: 2026-10-06
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <algorithm>
#include <string>
#include <hikyuu/utilities/db_connect/TableMacro.h>

using namespace hku;

// ============================================================================
// COL_TABLE_BIND0..20 与 TABLE_BIND0..20 全实例化编译回归
//
// COL_TABLE_BIND6..20 曾存在两类缺陷：getInsertSQL 的占位符固定 6 个 ?（实际需
// id+字段数），以及 12..20 移动赋值多写 ')' 导致实例化即编译失败。此处的实例化
// 使每级宏都被真实展开编译，SQL 断言再校验占位符与列数的一致性。
// ============================================================================

struct ColBind0 {
    COL_TABLE_BIND0(ColBind0, col_bind0);
};

// 字段统一用 std::string，逐级手写展开
struct ColBind1 {
    COL_TABLE_BIND1(ColBind1, col_bind1, f1);
    std::string f1;
};
struct ColBind2 {
    COL_TABLE_BIND2(ColBind2, col_bind2, f1, f2);
    std::string f1, f2;
};
struct ColBind3 {
    COL_TABLE_BIND3(ColBind3, col_bind3, f1, f2, f3);
    std::string f1, f2, f3;
};
struct ColBind4 {
    COL_TABLE_BIND4(ColBind4, col_bind4, f1, f2, f3, f4);
    std::string f1, f2, f3, f4;
};
struct ColBind5 {
    COL_TABLE_BIND5(ColBind5, col_bind5, f1, f2, f3, f4, f5);
    std::string f1, f2, f3, f4, f5;
};
struct ColBind6 {
    COL_TABLE_BIND6(ColBind6, col_bind6, f1, f2, f3, f4, f5, f6);
    std::string f1, f2, f3, f4, f5, f6;
};
struct ColBind7 {
    COL_TABLE_BIND7(ColBind7, col_bind7, f1, f2, f3, f4, f5, f6, f7);
    std::string f1, f2, f3, f4, f5, f6, f7;
};
struct ColBind8 {
    COL_TABLE_BIND8(ColBind8, col_bind8, f1, f2, f3, f4, f5, f6, f7, f8);
    std::string f1, f2, f3, f4, f5, f6, f7, f8;
};
struct ColBind9 {
    COL_TABLE_BIND9(ColBind9, col_bind9, f1, f2, f3, f4, f5, f6, f7, f8, f9);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9;
};
struct ColBind10 {
    COL_TABLE_BIND10(ColBind10, col_bind10, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
};
struct ColBind11 {
    COL_TABLE_BIND11(ColBind11, col_bind11, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11;
};
struct ColBind12 {
    COL_TABLE_BIND12(ColBind12, col_bind12, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12;
};
struct ColBind13 {
    COL_TABLE_BIND13(ColBind13, col_bind13, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13;
};
struct ColBind14 {
    COL_TABLE_BIND14(ColBind14, col_bind14, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                     f14);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14;
};
struct ColBind15 {
    COL_TABLE_BIND15(ColBind15, col_bind15, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                     f14, f15);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15;
};
struct ColBind16 {
    COL_TABLE_BIND16(ColBind16, col_bind16, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                     f14, f15, f16);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16;
};
struct ColBind17 {
    COL_TABLE_BIND17(ColBind17, col_bind17, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                     f14, f15, f16, f17);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17;
};
struct ColBind18 {
    COL_TABLE_BIND18(ColBind18, col_bind18, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                     f14, f15, f16, f17, f18);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18;
};
struct ColBind19 {
    COL_TABLE_BIND19(ColBind19, col_bind19, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                     f14, f15, f16, f17, f18, f19);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18,
      f19;
};
struct ColBind20 {
    COL_TABLE_BIND20(ColBind20, col_bind20, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                     f14, f15, f16, f17, f18, f19, f20);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18,
      f19, f20;
};

struct TableBind0 {
    TABLE_BIND0(TableBind0, table_bind0);
};
struct TableBind1 {
    TABLE_BIND1(TableBind1, table_bind1, f1);
    std::string f1;
};
struct TableBind2 {
    TABLE_BIND2(TableBind2, table_bind2, f1, f2);
    std::string f1, f2;
};
struct TableBind3 {
    TABLE_BIND3(TableBind3, table_bind3, f1, f2, f3);
    std::string f1, f2, f3;
};
struct TableBind4 {
    TABLE_BIND4(TableBind4, table_bind4, f1, f2, f3, f4);
    std::string f1, f2, f3, f4;
};
struct TableBind5 {
    TABLE_BIND5(TableBind5, table_bind5, f1, f2, f3, f4, f5);
    std::string f1, f2, f3, f4, f5;
};
struct TableBind6 {
    TABLE_BIND6(TableBind6, table_bind6, f1, f2, f3, f4, f5, f6);
    std::string f1, f2, f3, f4, f5, f6;
};
struct TableBind7 {
    TABLE_BIND7(TableBind7, table_bind7, f1, f2, f3, f4, f5, f6, f7);
    std::string f1, f2, f3, f4, f5, f6, f7;
};
struct TableBind8 {
    TABLE_BIND8(TableBind8, table_bind8, f1, f2, f3, f4, f5, f6, f7, f8);
    std::string f1, f2, f3, f4, f5, f6, f7, f8;
};
struct TableBind9 {
    TABLE_BIND9(TableBind9, table_bind9, f1, f2, f3, f4, f5, f6, f7, f8, f9);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9;
};
struct TableBind10 {
    TABLE_BIND10(TableBind10, table_bind10, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
};
struct TableBind11 {
    TABLE_BIND11(TableBind11, table_bind11, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11;
};
struct TableBind12 {
    TABLE_BIND12(TableBind12, table_bind12, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12;
};
struct TableBind13 {
    TABLE_BIND13(TableBind13, table_bind13, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13;
};
struct TableBind14 {
    TABLE_BIND14(TableBind14, table_bind14, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                 f14);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14;
};
struct TableBind15 {
    TABLE_BIND15(TableBind15, table_bind15, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                 f14, f15);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15;
};
struct TableBind16 {
    TABLE_BIND16(TableBind16, table_bind16, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                 f14, f15, f16);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16;
};
struct TableBind17 {
    TABLE_BIND17(TableBind17, table_bind17, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                 f14, f15, f16, f17);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17;
};
struct TableBind18 {
    TABLE_BIND18(TableBind18, table_bind18, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                 f14, f15, f16, f17, f18);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18;
};
struct TableBind19 {
    TABLE_BIND19(TableBind19, table_bind19, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                 f14, f15, f16, f17, f18, f19);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18,
      f19;
};
struct TableBind20 {
    TABLE_BIND20(TableBind20, table_bind20, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13,
                 f14, f15, f16, f17, f18, f19, f20);
    std::string f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18,
      f19, f20;
};

// ---------------------------------------------------------------------------
// SQL 一致性断言：insert 占位符数 + 1（where id=?）== select 列数 == update 占位符数
// ---------------------------------------------------------------------------

static size_t countChar(const std::string &sql, char c) {
    return static_cast<size_t>(std::count(sql.begin(), sql.end(), c));
}

// The select column list is quoted identifiers separated by commas, the table name and the
// keywords carry no commas, so the column count is the comma count plus one. The insert SQL of
// the COL series includes the id column while the plain TABLE series excludes it.
template <typename T>
static void checkBindMacroSQL(bool insert_includes_id) {
    std::string insert_sql = T::getInsertSQL();
    std::string update_sql = T::getUpdateSQL();
    std::string select_sql = T::getSelectSQL();

    size_t insert_placeholders = countChar(insert_sql, '?');
    size_t update_placeholders = countChar(update_sql, '?');
    size_t select_columns = countChar(select_sql, ',') + 1;

    if (insert_includes_id) {
        CHECK_EQ(insert_placeholders, select_columns);
    } else {
        CHECK_EQ(insert_placeholders + 1, select_columns);
    }
    CHECK_EQ(update_placeholders, select_columns);
}

TEST_CASE("test_TableMacro_col_bind_sql_consistency") {
    // COL_TABLE_BIND0 has no field, its insert/select/update SQL shape differs from the others
    checkBindMacroSQL<ColBind1>(true);
    checkBindMacroSQL<ColBind2>(true);
    checkBindMacroSQL<ColBind3>(true);
    checkBindMacroSQL<ColBind4>(true);
    checkBindMacroSQL<ColBind5>(true);
    checkBindMacroSQL<ColBind6>(true);
    checkBindMacroSQL<ColBind7>(true);
    checkBindMacroSQL<ColBind8>(true);
    checkBindMacroSQL<ColBind9>(true);
    checkBindMacroSQL<ColBind10>(true);
    checkBindMacroSQL<ColBind11>(true);
    checkBindMacroSQL<ColBind12>(true);
    checkBindMacroSQL<ColBind13>(true);
    checkBindMacroSQL<ColBind14>(true);
    checkBindMacroSQL<ColBind15>(true);
    checkBindMacroSQL<ColBind16>(true);
    checkBindMacroSQL<ColBind17>(true);
    checkBindMacroSQL<ColBind18>(true);
    checkBindMacroSQL<ColBind19>(true);
    checkBindMacroSQL<ColBind20>(true);

    // Regression: the insert SQL of COL_TABLE_BIND6..20 used to carry only 6 placeholders
    CHECK_EQ(countChar(ColBind6::getInsertSQL(), '?'), static_cast<size_t>(7));
    CHECK_EQ(countChar(ColBind12::getInsertSQL(), '?'), static_cast<size_t>(13));
    CHECK_EQ(countChar(ColBind20::getInsertSQL(), '?'), static_cast<size_t>(21));
}

TEST_CASE("test_TableMacro_table_bind_sql_consistency") {
    checkBindMacroSQL<TableBind1>(false);
    checkBindMacroSQL<TableBind2>(false);
    checkBindMacroSQL<TableBind3>(false);
    checkBindMacroSQL<TableBind4>(false);
    checkBindMacroSQL<TableBind5>(false);
    checkBindMacroSQL<TableBind6>(false);
    checkBindMacroSQL<TableBind7>(false);
    checkBindMacroSQL<TableBind8>(false);
    checkBindMacroSQL<TableBind9>(false);
    checkBindMacroSQL<TableBind10>(false);
    checkBindMacroSQL<TableBind11>(false);
    checkBindMacroSQL<TableBind12>(false);
    checkBindMacroSQL<TableBind13>(false);
    checkBindMacroSQL<TableBind14>(false);
    checkBindMacroSQL<TableBind15>(false);
    checkBindMacroSQL<TableBind16>(false);
    checkBindMacroSQL<TableBind17>(false);
    checkBindMacroSQL<TableBind18>(false);
    checkBindMacroSQL<TableBind19>(false);
    checkBindMacroSQL<TableBind20>(false);
}

TEST_CASE("test_TableMacro_move_semantics") {
    // The move constructor and move assignment compiled and work for every level
    ColBind12 src;
    src.id(100);
    src.f1 = "a";
    src.f12 = "z";

    ColBind12 dst1(std::move(src));
    CHECK_EQ(dst1.id(), static_cast<uint64_t>(100));
    CHECK_EQ(dst1.f1, "a");
    CHECK_EQ(dst1.f12, "z");
    CHECK_EQ(src.id(), static_cast<uint64_t>(0));

    ColBind12 dst2;
    dst2 = std::move(dst1);
    CHECK_EQ(dst2.id(), static_cast<uint64_t>(100));
    CHECK_EQ(dst2.f1, "a");
    CHECK_EQ(dst2.f12, "z");
    CHECK_EQ(dst1.id(), static_cast<uint64_t>(0));

    // move assignment to self is a no-op
    ColBind12 &ref = dst2;
    dst2 = std::move(ref);
    CHECK_EQ(dst2.id(), static_cast<uint64_t>(100));
}
