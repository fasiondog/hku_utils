/*
 *  Copyright (c) hikyuu.org
 *
 *  Created on: 2020-8-4
 *      Author: fasiondog
 *
 */

#include <doctest/doctest.h>
#include <hikyuu/utilities/os.h>
#include <hikyuu/utilities/Log.h>
#include <hikyuu/utilities/arithmetic.h>
#if !HKU_OS_WINDOWS
#include <unistd.h>
#endif

using namespace hku;

static void createTestFile(const std::string &filename) {
    FILE *fp = fopen(HKU_PATH(filename).c_str(), "wb");
    int len = 10;
    fwrite(&len, sizeof(int), 1, fp);
    fflush(fp);
    fclose(fp);
}

TEST_CASE("test_getUserDir") {
    auto usr_home = getUserDir();
    CHECK_UNARY_FALSE(usr_home.empty());
    HKU_TRACE(usr_home);
}

TEST_CASE("test_dir_operation") {
    // Removing an empty path should return false instead of throwing
    CHECK_UNARY_FALSE(removeDir(""));

    // 创建并删除空目录
    std::string dirname("tmp");
    CHECK_UNARY_FALSE(existFile(dirname));

    CHECK_UNARY(createDir(dirname));
    CHECK_UNARY(existFile(dirname));

    CHECK_UNARY(removeDir(dirname));
    CHECK_UNARY_FALSE(existFile(dirname));

    // 删除非空目录
    CHECK_UNARY(createDir(dirname));
    CHECK_UNARY(createDir(fmt::format("{}/{}", dirname, dirname)));
    CHECK_UNARY(createDir(fmt::format("{}/{}/{}", dirname, dirname, dirname)));
    CHECK_UNARY(existFile(fmt::format("{}/{}", dirname, dirname)));
    CHECK_UNARY(existFile(fmt::format("{}/{}/{}", dirname, dirname, dirname)));

    std::string filename = fmt::format("{}/{}/temp.txt", dirname, dirname);
    createTestFile(filename);

    CHECK_UNARY(existFile(filename));
    CHECK_UNARY(removeDir(dirname));
    CHECK_UNARY_FALSE(existFile(dirname));
}

TEST_CASE("test_dir_operation_unicode") {
    // 创建并删除空目录
    std::string dirname("中文");
    CHECK_UNARY_FALSE(existFile(dirname));

    CHECK_UNARY(createDir(dirname));
    CHECK_UNARY(existFile(dirname));

    CHECK_UNARY(removeDir(dirname));
    CHECK_UNARY_FALSE(existFile(dirname));

    // 删除非空目录
    CHECK_UNARY(createDir(dirname));
    CHECK_UNARY(createDir(fmt::format("{}/{}", dirname, dirname)));
    CHECK_UNARY(createDir(fmt::format("{}/{}/{}", dirname, dirname, dirname)));
    CHECK_UNARY(existFile(fmt::format("{}/{}", dirname, dirname)));
    CHECK_UNARY(existFile(fmt::format("{}/{}/{}", dirname, dirname, dirname)));

    std::string filename = fmt::format("{}/{}/中文temp.txt", dirname, dirname);
    createTestFile(filename);

    CHECK_UNARY(existFile(filename));
    CHECK_UNARY(removeDir(dirname));
    CHECK_UNARY_FALSE(existFile(dirname));
}

TEST_CASE("test_removeDir_symlink") {
    // The external target directory and its file must survive when removing a directory
    // that only contains symlinks/junctions pointing to them
    std::string targetDir("tmp_symlink_target");
    std::string targetFile = fmt::format("{}/target.txt", targetDir);
    removeFile(targetFile);
    removeDir(targetDir);
    CHECK_UNARY(createDir(targetDir));
    createTestFile(targetFile);
    CHECK_UNARY(existFile(targetFile));

    std::string dirname("tmp_symlink");
    removeDir(dirname);
    CHECK_UNARY(createDir(dirname));

    std::string dirLink;
    std::string fileLink;
#if !HKU_OS_WINDOWS
    // A symlink pointing to an external directory. The target is resolved relative to the
    // link location, so absolute paths are used here.
    std::string absTargetDir = getCurrentDir() + "/" + targetDir;
    std::string absTargetFile = getCurrentDir() + "/" + targetFile;
    dirLink = fmt::format("{}/dir_link", dirname);
    CHECK_EQ(symlink(absTargetDir.c_str(), HKU_PATH(dirLink).c_str()), 0);

    // A symlink pointing to an external file
    fileLink = fmt::format("{}/file_link", dirname);
    CHECK_EQ(symlink(absTargetFile.c_str(), HKU_PATH(fileLink).c_str()), 0);
#else
    // A junction pointing to an external directory, created via mklink /J which needs no
    // privileges. A file symlink requires privileges on Windows, so it is not covered here.
    // Both paths are relative to the current working directory.
    dirLink = fmt::format("{}\\dir_junction", dirname);
    int ret = std::system(
      fmt::format("cmd /c mklink /J \"{}\" \"{}\" 1>nul 2>&1", dirLink, targetDir).c_str());
    REQUIRE_EQ(ret, 0);
    CHECK_UNARY(existFile(dirLink));
#endif

    // Removing the directory must delete only the symlinks/junctions themselves
    CHECK_UNARY(removeDir(dirname));
    CHECK_UNARY_FALSE(existFile(dirname));
    CHECK_UNARY_FALSE(existFile(dirLink));
    CHECK_UNARY_FALSE(existFile(fileLink));

    // The symlink targets must survive
    CHECK_UNARY(existFile(targetDir));
    CHECK_UNARY(existFile(targetFile));

    removeFile(targetFile);
    removeDir(targetDir);
}

TEST_CASE("test_removeFile") {
    std::string filename("中文temp.txt");
    removeFile(filename);
    CHECK_UNARY_FALSE(existFile(filename));

    createTestFile(filename);

    CHECK_UNARY(existFile(filename));
    CHECK_UNARY(removeFile(filename));
    CHECK_UNARY_FALSE(existFile(filename));
}

TEST_CASE("test_copyFile") {
    std::string filename("中文temp.txt");
    removeFile(filename);
    CHECK_UNARY_FALSE(existFile(filename));

    createTestFile(filename);
    CHECK_UNARY(existFile(filename));

    copyFile(filename, "中文temp2.txt");
    CHECK_UNARY(existFile("中文temp2.txt"));
    CHECK_UNARY(removeFile("中文temp2.txt"));

    // 85M 左右的文件，在 v831 上拷贝耗时 6s，直接使用 cp 命令耗时 5s
    // {
    //     SPEND_TIME(copyt_85M_file);
    //     copyFile("test_data/large_user.db", "test_data/large_user.db2");
    // }
}

TEST_CASE("test_renameFile") {
    std::string oldname("中文old.txt");
    std::string newname("中文new.txt");

    // 指定的旧名文件不存在
    CHECK_UNARY_FALSE(existFile(oldname));
    CHECK_UNARY_FALSE(renameFile(oldname, newname, false));
    CHECK_UNARY_FALSE(renameFile(oldname, newname, true));
    CHECK_UNARY_FALSE(existFile(newname));
    CHECK_UNARY_FALSE(existFile(newname));

    // 指定的新名未被占用，非覆盖模式
    createTestFile(oldname);
    CHECK_UNARY_FALSE(existFile(newname));
    CHECK_UNARY(renameFile(oldname, newname, false));
    CHECK_UNARY_FALSE(existFile(oldname));
    CHECK_UNARY(existFile(newname));

    // 指定的新名已被占用，非覆盖模式
    createTestFile(oldname);
    CHECK_UNARY(existFile(newname));
    CHECK_UNARY_FALSE(renameFile(oldname, newname, false));
    CHECK_UNARY(existFile(oldname));
    CHECK_UNARY(existFile(newname));

    // 指定的新名未被占用，覆盖模式
    removeFile(newname);
    createTestFile(oldname);
    CHECK_UNARY_FALSE(existFile(newname));
    CHECK_UNARY(renameFile(oldname, newname, true));
    CHECK_UNARY_FALSE(existFile(oldname));
    CHECK_UNARY(existFile(newname));

    // 指定的新名已被占用，覆盖模式
    createTestFile(oldname);
    CHECK_UNARY(existFile(newname));
    CHECK_UNARY(renameFile(oldname, newname, true));
    CHECK_UNARY_FALSE(existFile(oldname));
    CHECK_UNARY(existFile(newname));
    removeFile(newname);
}

TEST_CASE("test_getDiskFreeSpace") {
#if !HKU_OS_WINDOWS
    CHECK_EQ(getDiskFreeSpace(nullptr), Null<uint64_t>());
#endif
    HKU_INFO("disk free space /: {}", getDiskFreeSpace("/"));
    HKU_INFO("disk free space .: {}", getDiskFreeSpace("."));
}
