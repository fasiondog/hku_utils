/*
 *  Copyright (c) 2026 hikyuu.org
 *
 *  Created on: 2026-09-20
 *      Author: fasiondog
 *
 */

#include <chrono>
#include <string_view>
#include <thread>
#include <doctest/doctest.h>
#include <hikyuu/utilities/FileLock.h>
#include <hikyuu/utilities/Log.h>
#include <hikyuu/utilities/os.h>
#if !HKU_OS_WINDOWS
#include <unistd.h>
#endif

using namespace hku;

// FileLock.cpp 中的内部实现函数（导出仅为测试，见其定义处注释），此处手工声明
namespace hku {
std::string HKU_UTILS_API normalizeLockKey(std::string_view filename);
}

/**
 * @defgroup test_hikyuu_FileLock test_hikyuu_FileLock
 * @ingroup test_hikyuu_utilities
 */

// 统一使用 test_data/tmp 下的旁路锁文件（test_main 已创建该目录），
// 每个用例使用独立文件名，避免用例间通过进程内互斥注册表相互污染。
static std::string lockPath(const std::string& name) {
    return fmt::format("test_data/tmp/{}.lock", name);
}

TEST_CASE("test_FileLock_constructor") {
    std::string path = lockPath("ctor");
    removeFile(path);
    /** @arg 构造仅记录文件名，不加锁、不创建锁文件 */
    {
        FileLock lock(path);
        CHECK_EQ(lock.filename(), path);
        CHECK_UNARY(!lock.isLocked());
        CHECK_UNARY(!existFile(path));
    }
    /** @arg 空文件名对象构造合法，析构无副作用 */
    {
        FileLock empty("");
        CHECK_UNARY(empty.filename().empty());
        CHECK_UNARY(!empty.isLocked());
    }
}

TEST_CASE("test_FileLock_tryLock") {
    std::string path = lockPath("trylock");
    removeFile(path);
    FileLock lock(path);
    /** @arg 首次加锁成功，锁文件被自动创建 */
    CHECK_UNARY(lock.tryLock());
    CHECK_UNARY(lock.isLocked());
    CHECK_UNARY(existFile(path));
    /** @arg 已持锁时重复 tryLock 幂等返回 true */
    CHECK_UNARY(lock.tryLock());
    CHECK_UNARY(lock.isLocked());
    lock.unlock();
    /** @arg 空文件名对象加锁失败 */
    FileLock empty("");
    CHECK_UNARY(!empty.tryLock());
    CHECK_UNARY(!empty.isLocked());
}

TEST_CASE("test_FileLock_tryLock_in_process_exclusive") {
    std::string path = lockPath("inproc");
    removeFile(path);
    FileLock a(path);
    FileLock b(path);
    /** @arg 同进程内相同路径，第一个持锁后第二个加锁失败 */
    CHECK_UNARY(a.tryLock());
    CHECK_UNARY(!b.tryLock());
    CHECK_UNARY(!b.isLocked());
    /** @arg 第一个释放后，第二个可成功加锁 */
    a.unlock();
    CHECK_UNARY(b.tryLock());
    CHECK_UNARY(b.isLocked());
    b.unlock();
    /** @arg 不同路径互不影响，可同时持锁 */
    FileLock c(path);
    FileLock d(lockPath("inproc_other"));
    CHECK_UNARY(c.tryLock());
    CHECK_UNARY(d.tryLock());
    CHECK_UNARY(c.isLocked());
    CHECK_UNARY(d.isLocked());
    c.unlock();
    d.unlock();
}

TEST_CASE("test_FileLock_tryLock_cross_thread") {
    std::string path = lockPath("thread");
    removeFile(path);
    FileLock main_lock(path);
    CHECK_UNARY(main_lock.tryLock());
    /** @arg 主线程持锁时，子线程 tryLock 返回 false */
    bool child_got = true;
    std::thread t([&path, &child_got]() {
        FileLock child(path);
        child_got = child.tryLock();
        if (child.isLocked()) {
            child.unlock();
        }
    });
    t.join();
    CHECK_UNARY(!child_got);
    /** @arg 主线程释放后，子线程可成功加锁 */
    main_lock.unlock();
    bool child_got2 = false;
    std::thread t2([&path, &child_got2]() {
        FileLock child(path);
        child_got2 = child.tryLock();
        child.unlock();
    });
    t2.join();
    CHECK_UNARY(child_got2);
}

TEST_CASE("test_FileLock_waitLock") {
    std::string path = lockPath("waitlock");
    removeFile(path);
    /** @arg 无人持锁时立即成功 */
    {
        FileLock lock(path);
        CHECK_UNARY(lock.waitLock(3, 10));
        CHECK_UNARY(lock.isLocked());
        lock.unlock();
    }
    /** @arg 已被同进程持锁时，超出尝试次数后返回 false，且不空等最后一轮 */
    {
        FileLock holder(path);
        CHECK_UNARY(holder.tryLock());
        FileLock waiter(path);
        auto start = std::chrono::steady_clock::now();
        CHECK_UNARY(!waiter.waitLock(3, 20));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
        CHECK_UNARY(!waiter.isLocked());
        // 3 次尝试之间只 sleep 2 次（最后一次不空等）
        CHECK_UNARY(elapsed >= 30);
        holder.unlock();
    }
    /** @arg maxAttempts 为 0 时不加锁直接返回 false */
    {
        FileLock lock(lockPath("waitlock_zero"));
        CHECK_UNARY(!lock.waitLock(0, 10));
        CHECK_UNARY(!lock.isLocked());
    }
}

TEST_CASE("test_FileLock_unlock") {
    std::string path = lockPath("unlock");
    removeFile(path);
    /** @arg 持锁时解锁成功并清除持锁状态 */
    FileLock lock(path);
    CHECK_UNARY(lock.tryLock());
    lock.unlock();
    CHECK_UNARY(!lock.isLocked());
    /** @arg 未持锁时解锁为幂等空操作 */
    lock.unlock();
    CHECK_UNARY(!lock.isLocked());
    /** @arg 解锁后可重新加锁 */
    CHECK_UNARY(lock.tryLock());
    CHECK_UNARY(lock.isLocked());
    lock.unlock();
}

TEST_CASE("test_FileLock_destructor_auto_unlock") {
    std::string path = lockPath("dtor");
    removeFile(path);
    /** @arg 离开作用域自动释放锁，且锁文件不被删除 */
    {
        FileLock lock(path);
        CHECK_UNARY(lock.tryLock());
    }
    CHECK_UNARY(existFile(path));
    FileLock reopened(path);
    CHECK_UNARY(reopened.tryLock());
    reopened.unlock();
}

TEST_CASE("test_FileLock_move_constructor") {
    std::string path = lockPath("move_ctor");
    removeFile(path);
    FileLock src(path);
    CHECK_UNARY(src.tryLock());
    /** @arg 移动后源对象持锁状态转移，源文件名清空且不再持锁 */
    FileLock dst(std::move(src));
    CHECK_EQ(dst.filename(), path);
    CHECK_UNARY(dst.isLocked());
    CHECK_UNARY(src.filename().empty());
    CHECK_UNARY(!src.isLocked());
    /** @arg 被移动后的源对象不可再加锁 */
    CHECK_UNARY(!src.tryLock());
    /** @arg 目标对象正常解锁 */
    dst.unlock();
    CHECK_UNARY(!dst.isLocked());
}

TEST_CASE("test_FileLock_move_assignment") {
    std::string a_path = lockPath("move_assign_a");
    std::string b_path = lockPath("move_assign_b");
    removeFile(a_path);
    removeFile(b_path);
    /** @arg 赋值给未持锁对象，直接转移 */
    {
        FileLock src(a_path);
        CHECK_UNARY(src.tryLock());
        FileLock dst(b_path);
        dst = std::move(src);
        CHECK_EQ(dst.filename(), a_path);
        CHECK_UNARY(dst.isLocked());
        CHECK_UNARY(!src.isLocked());
        dst.unlock();
    }
    /** @arg 赋值给已持锁对象，先释放目标旧锁再转移 */
    {
        FileLock src(a_path);
        CHECK_UNARY(src.tryLock());
        FileLock dst(b_path);
        CHECK_UNARY(dst.tryLock());
        dst = std::move(src);
        CHECK_UNARY(dst.isLocked());
        CHECK_EQ(dst.filename(), a_path);
        // 旧 b_path 锁已释放，可被重新获取
        FileLock reborn(b_path);
        CHECK_UNARY(reborn.tryLock());
        reborn.unlock();
        dst.unlock();
    }
}

TEST_CASE("test_FileLock_symlink_refused") {
// O_NOFOLLOW：锁路径是符号链接时必须拒绝加锁（返回 ELOOP），而不是锁到链接指向的
// 任意 inode 上导致互斥失效
#if !HKU_OS_WINDOWS
    std::string path = lockPath("symlink");
    std::string target = lockPath("symlink_target");
    removeFile(path);
    removeFile(target);

    /** @arg 锁路径为指向外部文件的符号链接时加锁失败，且目标文件不受影响 */
    {
        FILE* fp = fopen(target.c_str(), "wb");
        REQUIRE(fp != nullptr);
        fclose(fp);
        CHECK_EQ(symlink(target.c_str(), path.c_str()), 0);

        FileLock lock(path);
        CHECK_UNARY(!lock.tryLock());
        CHECK_UNARY(!lock.isLocked());
        CHECK_UNARY(existFile(target));
    }

    /** @arg 移除符号链接后，同路径可正常加锁（锁文件按普通文件创建） */
    CHECK_UNARY(removeFile(path));
    FileLock lock(path);
    CHECK_UNARY(lock.tryLock());
    CHECK_UNARY(lock.isLocked());
    lock.unlock();

    removeFile(path);
    removeFile(target);
#endif
}

TEST_CASE("test_FileLock_path_normalize") {
    std::string base = lockPath("normalize");  // test_data/tmp/normalize.lock
    // 与 base 指向同一文件，但含 "./" 与冗余分隔符，规范化后应映射到同一键
    std::string variant = "./test_data/tmp/./normalize.lock";
    removeFile(base);
    /** @arg 规范化后等价路径映射到同一进程内互斥键，第二个持锁失败 */
    FileLock a(base);
    FileLock b(variant);
    CHECK_UNARY(a.tryLock());
    CHECK_UNARY(!b.tryLock());
    a.unlock();
    CHECK_UNARY(b.tryLock());
    b.unlock();
}

TEST_CASE("test_FileLock_normalizeLockKey_platform") {
    // normalizeLockKey 为纯字符串函数，直接断言键映射，平台分支用
    // #if HKU_OS_WINDOWS 隔离（Windows 分支不依赖真实文件系统）
    /** @arg 平台公共：折叠 "./"、重复与末尾分隔符 */
    CHECK_EQ(normalizeLockKey("./x/y.lock"), "x/y.lock");
    CHECK_EQ(normalizeLockKey("/x//y.lock/"), "/x/y.lock");
    /** @arg 平台公共：单分量绝对路径不重复拼接（回归：旧实现 Windows 下 "/a" 错拼为 "/a/a"） */
    CHECK_EQ(normalizeLockKey("/a"), "/a");
    CHECK_EQ(normalizeLockKey("/a/"), normalizeLockKey("/a"));
    /** @arg 平台公共：根路径与 ".." 分量（".." 不折叠，语义设计如此） */
    CHECK_EQ(normalizeLockKey("/"), "/");
    CHECK_EQ(normalizeLockKey("a/../b.lock"), "a/../b.lock");
    /** @arg 平台公共：空串合法；相对路径不补前置 '/' */
    CHECK_EQ(normalizeLockKey(""), "");
    CHECK_EQ(normalizeLockKey("a/b.lock"), "a/b.lock");

#if HKU_OS_WINDOWS
    /** @arg Windows：'\' 与 '/' 均为分隔符，两种写法映射同键 */
    CHECK_EQ(normalizeLockKey("C:\\a\\b.lock"), normalizeLockKey("C:/a/b.lock"));
    CHECK_EQ(normalizeLockKey("C:\\a\\b.lock"), "C:/a/b.lock");
    /** @arg Windows：驱动器根 "C:" 保留，不折叠为 "/C:"，尾分隔符被去除 */
    CHECK_EQ(normalizeLockKey("C:"), "C:");
    CHECK_EQ(normalizeLockKey("C:/"), "C:");
    CHECK_EQ(normalizeLockKey("C:\\a.lock"), "C:/a.lock");
    /** @arg Windows：UNC 路径与 '/' 写法映射同键 */
    CHECK_EQ(normalizeLockKey("//server/share"), "/server/share");
    CHECK_EQ(normalizeLockKey("\\\\server\\share"), "/server/share");
    /** @arg Windows：相对路径中 '\' 仍视为分隔符，与 '/' 映射同键 */
    CHECK_EQ(normalizeLockKey("dir\\sub\\a.lock"), "dir/sub/a.lock");
    CHECK_EQ(normalizeLockKey("dir\\a.lock"), normalizeLockKey("dir/a.lock"));
#else
    /** @arg POSIX：'\' 是合法文件名字符不参与折叠，与 '/' 语义不同，产生不同键 */
    CHECK_NE(normalizeLockKey("a\\b.lock"), normalizeLockKey("a/b.lock"));
    CHECK_EQ(normalizeLockKey("a\\b.lock"), "a\\b.lock");
    CHECK_EQ(normalizeLockKey("a/b.lock"), "a/b.lock");
#endif
}
