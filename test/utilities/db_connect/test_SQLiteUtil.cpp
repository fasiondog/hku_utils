/*
 *  Copyright (c) 2023 hikyuu.org
 *
 *  Created on: 2023-01-04
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include "hikyuu/utilities/os.h"
#include "hikyuu/utilities/arithmetic.h"
#include "hikyuu/utilities/db_connect/DBConnect.h"

using namespace hku;

TEST_CASE("test_SQLiteUtil_onlineBackup") {
    // 源数据连接为空
    std::shared_ptr<SQLiteConnect> conn;
    CHECK_EQ(SQLiteUtil::onlineBackup(conn, "cannot_backup.db"),
             SQLiteUtil::BACKUP_FAILED_INVALID_SRC);

    // 源数据不是有效的 sqlite3 文件
    Parameter param;
    param.set<std::string>("db", "test_data/bad.db");
    conn = std::make_shared<SQLiteConnect>(param);
    CHECK_EQ(SQLiteUtil::onlineBackup(conn, "cannot_backup.db"),
             SQLiteUtil::BACKUP_FAILED_INVALID_SRC);

    // 正常备份文件
    std::string backup_file = "test_data/tmp/backup_test.db.bak";
    param.set<std::string>("db", "test_data/backup_test.db");
    conn = std::make_shared<SQLiteConnect>(param);
    CHECK_EQ(SQLiteUtil::onlineBackup("test_data/backup_test.db", backup_file, -1),
             SQLiteUtil::BACKUP_SUCCESS);
    param.set<std::string>("db", backup_file);
    conn = std::make_shared<SQLiteConnect>(param);
    CHECK(conn->check());
    CHECK(conn->ping());
}

TEST_CASE("test_SQLiteUtil_recoverFromBackup") {
    // 指定的备份文件不存在
    CHECK_EQ(SQLiteUtil::recoverFromBackup("not_exist", "test_data/tmp/recover.db"),
             SQLiteUtil::RECOVER_FAILED_BACKUP_NOT_EXIST);

    // 指定的备份文件不是有效的数据库文件
    CHECK_EQ(SQLiteUtil::recoverFromBackup("test_data/bad.db", "test_data/tmp/recover.db"),
             SQLiteUtil::RECOVER_FAILED_BACKUP_INVALID);

    // 指定的恢复文件名被占用
    createDir("test_data/tmp/recover.db");
    copyFile("test_data/bad.db", "test_data/tmp/recover.db/bad.db");  // linux必须往空目录里放点东西
    CHECK_EQ(SQLiteUtil::recoverFromBackup("test_data/backup_test.db", "test_data/tmp/recover.db"),
             SQLiteUtil::RECOVER_FAILED_INVALID_DST);
    removeDir("test_data/tmp/recover.db");

    createDir("test_data/tmp/recover.db-journal");
    copyFile("test_data/bad.db", "test_data/tmp/recover.db-journal/bad.db");
    CHECK_EQ(
      SQLiteUtil::recoverFromBackup("test_data/backup_test.db", "test_data/tmp/recover.db-journal"),
      SQLiteUtil::RECOVER_FAILED_INVALID_DST);
    removeDir("test_data/tmp/recover.db-journal");

    // 正常恢复, 不保存损坏的文件
    CHECK_EQ(SQLiteUtil::recoverFromBackup("test_data/backup_test.db", "test_data/tmp/recover.db"),
             SQLiteUtil::RECOVER_SUCCESS);

    // 正常恢复, 保存损坏的文件
    CHECK_EQ(
      SQLiteUtil::recoverFromBackup("test_data/backup_test.db", "test_data/tmp/recover.db", true),
      SQLiteUtil::RECOVER_SUCCESS);
    CHECK_UNARY(existFile("test_data/tmp/recover.db.bad"));

    // 正常恢复, 保存损坏的文件，之前已经存在一份损坏的文件
    CHECK_EQ(
      SQLiteUtil::recoverFromBackup("test_data/backup_test.db", "test_data/tmp/recover.db", true),
      SQLiteUtil::RECOVER_SUCCESS);
    CHECK_UNARY(existFile("test_data/tmp/recover.db.bad"));
}

static void createTestFile(const std::string &filename) {
    FILE *fp = fopen(HKU_PATH(filename).c_str(), "wb");
    int len = 10;
    fwrite(&len, sizeof(int), 1, fp);
    fflush(fp);
    fclose(fp);
}

TEST_CASE("test_SQLiteUtil_removeDBFile") {
    // 待删除的数据库文件及其日志文件都不存在
    CHECK_UNARY(SQLiteUtil::removeDBFile("not_exist_file"));

    // 只存在数据库文件，不存在日志文件
    std::string dbname("will_delete_file");
    std::string journal("will_delete_file-journal");
    createTestFile(dbname);
    CHECK_UNARY(existFile(dbname));
    CHECK_UNARY(!existFile(journal));
    CHECK_UNARY(SQLiteUtil::removeDBFile(dbname));
    CHECK_UNARY(!existFile(dbname));
    CHECK_UNARY(!existFile(journal));

    // 不存在数据库文件，只存在日志文件
    createTestFile(journal);
    CHECK_UNARY(!existFile(dbname));
    CHECK_UNARY(existFile(journal));
    CHECK_UNARY(SQLiteUtil::removeDBFile(dbname));
    CHECK_UNARY(!existFile(dbname));
    CHECK_UNARY(!existFile(journal));

    // 数据库文件和日志文件都存在
    createTestFile(dbname);
    createTestFile(journal);
    CHECK_UNARY(existFile(dbname));
    CHECK_UNARY(existFile(journal));
    CHECK_UNARY(SQLiteUtil::removeDBFile(dbname));
    CHECK_UNARY(!existFile(dbname));
    CHECK_UNARY(!existFile(journal));

    // 数据库文件无法删除
    CHECK_UNARY(!existFile(journal));
    CHECK_UNARY(createDir(dbname));
    // linux 空目录可以被removeFile删除，这里需要在目录下再创建一个文件
    createTestFile(fmt::format("{}/ttt", dbname));
    CHECK_UNARY(!SQLiteUtil::removeDBFile(dbname));
    CHECK_UNARY(removeFile(fmt::format("{}/ttt", dbname)));
    CHECK_UNARY(removeDir(dbname));

    // 日志文件无法删除
    CHECK_UNARY(!existFile(dbname));
    CHECK_UNARY(createDir(journal));
    createTestFile(fmt::format("{}/ttt", journal));
    CHECK_UNARY(!SQLiteUtil::removeDBFile(journal));
    CHECK_UNARY(removeFile(fmt::format("{}/ttt", journal)));
    CHECK_UNARY(removeDir(journal));
}
