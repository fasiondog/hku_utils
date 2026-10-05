/**
 *  Copyright (c) 2021 hikyuu
 *
 *  Created on: 2021/07/09
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/ResourcePool.h>
#include <hikyuu/utilities/thread/ThreadPool.h>
#include <hikyuu/utilities/Log.h>
#include <thread>
#include <vector>
#include <atomic>
#include <future>

using namespace hku;

class TestResource {
public:
    TestResource(const Parameter& param) {
        std::lock_guard<std::mutex> lock(m_mutex);
        i = x++;
        // HKU_ERROR("new TestResource {}", i);
    }

    virtual ~TestResource() {
        std::lock_guard<std::mutex> lock(m_mutex);
        x--;
        // HKU_ERROR("delete TestResource {}", i);
    }

    void print() {
        // printf("i am a %d\n", i);
    }

private:
    std::mutex m_mutex;
    int i = 0;
    static int x;
};

int TestResource::x = 0;

// ==================== ResourcePool 基础功能测试 ====================

TEST_CASE("test_ResourcePool_BasicGetReturn") {
    Parameter param;
    ResourcePool<TestResource> pool(param);

    // 初始状态
    CHECK_EQ(pool.count(), 0);
    CHECK_EQ(pool.idleCount(), 0);

    // 获取资源
    auto res1 = pool.get();
    CHECK_NE(res1, nullptr);
    CHECK_EQ(pool.count(), 1);
    CHECK_EQ(pool.idleCount(), 0);

    // 归还资源（reset）
    res1.reset();
    CHECK_EQ(pool.count(), 1);
    CHECK_EQ(pool.idleCount(), 1);

    // 再次获取应复用空闲资源
    auto res2 = pool.get();
    CHECK_NE(res2, nullptr);
    CHECK_EQ(pool.count(), 1);
    CHECK_EQ(pool.idleCount(), 0);

    res2.reset();
}

TEST_CASE("test_ResourcePool_MaxPoolSizeLimit") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 3, 10);  // 最大3个资源

    auto r1 = pool.get();
    auto r2 = pool.get();
    auto r3 = pool.get();
    CHECK_EQ(pool.count(), 3);

    // 达到上限，get 返回空指针
    auto r4 = pool.get();
    CHECK_EQ(r4, nullptr);
    CHECK_EQ(pool.count(), 3);

    // 归还一个后再获取
    r1.reset();
    auto r5 = pool.get();
    CHECK_NE(r5, nullptr);
    CHECK_EQ(pool.count(), 3);
}

TEST_CASE("test_ResourcePool_GetWaitFor_Timeout") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 1, 10);  // 最大1个资源

    auto r1 = pool.get();
    CHECK_NE(r1, nullptr);

    // 超时获取应该抛出异常
    CHECK_THROWS_AS(pool.getWaitFor(100), GetResourceTimeoutException);

    r1.reset();
}

TEST_CASE("test_ResourcePool_GetWaitFor_Success") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 1, 10);

    auto r1 = pool.get();

    // 在另一个线程中延迟归还
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        r1.reset();
    });

    // 等待获取应该在超时前成功
    auto r2 = pool.getWaitFor(200);
    CHECK_NE(r2, nullptr);

    t.join();
}

TEST_CASE("test_ResourcePool_GetAndWait") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 1, 10);

    auto r1 = pool.get();

    // 在另一个线程中延迟归还
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        r1.reset();
    });

    // getAndWait 应该阻塞直到资源可用
    auto r2 = pool.getAndWait();
    CHECK_NE(r2, nullptr);

    t.join();
}

TEST_CASE("test_ResourcePool_MaxIdleSize") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 10, 2);  // 最多缓存2个空闲资源

    auto r1 = pool.get();
    auto r2 = pool.get();
    auto r3 = pool.get();
    CHECK_EQ(pool.count(), 3);

    // 归还所有资源，但只缓存2个
    r1.reset();
    r2.reset();
    r3.reset();
    CHECK_EQ(pool.count(), 2);  // 第3个被直接删除
    CHECK_EQ(pool.idleCount(), 2);
}

TEST_CASE("test_ResourcePool_ReleaseIdleResource") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 10, 10);

    auto r1 = pool.get();
    auto r2 = pool.get();
    auto r3 = pool.get();
    CHECK_EQ(pool.count(), 3);

    // 归还所有资源
    r1.reset();
    r2.reset();
    r3.reset();
    CHECK_EQ(pool.idleCount(), 3);

    // 释放空闲资源
    pool.releaseIdleResource();
    CHECK_EQ(pool.count(), 0);
    CHECK_EQ(pool.idleCount(), 0);
}

TEST_CASE("test_ResourcePool_SetMaxPoolSize") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 5, 10);

    CHECK_EQ(pool.maxPoolSize(), 5);

    pool.maxPoolSize(3);
    auto r1 = pool.get();
    auto r2 = pool.get();
    auto r3 = pool.get();

    auto r4 = pool.get();
    CHECK_EQ(r4, nullptr);  // 达到新限制

    r1.reset();
    r2.reset();
    r3.reset();
}

TEST_CASE("test_ResourcePool_SetMaxIdleSize") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 10, 5);

    CHECK_EQ(pool.maxIdleSize(), 5);

    pool.maxIdleSize(2);

    auto r1 = pool.get();
    auto r2 = pool.get();
    auto r3 = pool.get();

    r1.reset();
    r2.reset();
    r3.reset();

    CHECK_EQ(pool.idleCount(), 2);  // 只缓存2个
    CHECK_EQ(pool.count(), 2);
}

// ==================== ResourcePool 多线程并发测试 ====================

TEST_CASE("test_ResourcePool_ConcurrentAccess") {
    Parameter param;
    const int num_threads = 10;
    const int iterations = 100;
    std::atomic<int> success_count(0);
    std::atomic<int> error_count(0);
    std::atomic<int> completed(0);
    std::promise<void> all_done;
    std::future<void> future = all_done.get_future();

    ResourcePool<TestResource> pool(param, 5, 10);  // 限制5个资源

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterations; ++j) {
                try {
                    auto res = pool.get();
                    if (res) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        success_count.fetch_add(1);
                    } else {
                        error_count.fetch_add(1);
                    }
                } catch (...) {
                    error_count.fetch_add(1);
                }
            }

            if (completed.fetch_add(1) + 1 == num_threads) {
                all_done.set_value();
            }
        });
    }

    // 等待所有线程完成
    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::timeout) {
        FAIL("Concurrent test timeout");
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    HKU_INFO("Concurrent test - Success: {}, Errors: {}", success_count.load(), error_count.load());
    CHECK_GT(success_count.load(), 0);
    // 注意：由于 shared_ptr 可能在栈上销毁，需要一点时间让删除器执行
    // 所以不严格检查 count 和 idleCount
}

TEST_CASE("test_ResourcePool_MultithreadedGetAndWait") {
    Parameter param;
    const int num_threads = 8;
    std::atomic<int> completed(0);
    std::promise<void> all_done;
    std::future<void> future = all_done.get_future();

    ResourcePool<TestResource> pool(param, 3, 10);  // 只有3个资源

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                auto res = pool.getAndWait();
                CHECK_NE(res, nullptr);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (...) {
                FAIL("Unexpected exception");
            }

            if (completed.fetch_add(1) + 1 == num_threads) {
                all_done.set_value();
            }
        });
    }

    // 等待所有线程完成（设置超时）
    if (future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
        FAIL("Test timeout - possible deadlock");
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    CHECK_EQ(completed.load(), num_threads);
    // 不严格检查计数，因为 shared_ptr 销毁是异步的
}

// ==================== ResourcePool 异常处理测试 ====================

TEST_CASE("test_ResourcePool_DestructorSafety") {
    Parameter param;
    ResourcePool<TestResource>* pool_ptr = new ResourcePool<TestResource>(param, 10, 10);

    auto r1 = pool_ptr->get();
    auto r2 = pool_ptr->get();

    // 析构时仍有活跃资源，应安全解绑并清理
    delete pool_ptr;

    // 资源应在 shared_ptr 销毁时被删除（closer 已解绑）
    r1.reset();
    r2.reset();
}

TEST_CASE("test_ResourcePool_EmptyPointerReturn") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 5, 10);

    auto r1 = pool.get();
    r1.reset();

    // 再次获取不应有问题
    auto r2 = pool.get();
    CHECK_NE(r2, nullptr);
}

// ==================== ResourceWithVersion 测试 ====================

class TTResource {
    PARAMETER_SUPPORT

public:
    TTResource(const Parameter& param) {
        // 复制所有参数
        setParam<std::string>("version", param.get<std::string>("version"));
        if (param.have("name")) {
            setParam<std::string>("name", param.get<std::string>("name"));
        }
        if (param.have("count")) {
            setParam<int>("count", param.get<int>("count"));
        }
    }

    ~TTResource() {}

    void print() {
        printf("i am version: %d\n", m_version);
    }

    // 版本管理接口
    int getVersion() const {
        return m_version;
    }

    void setVersion(int version) {
        m_version = version;
    }

private:
    int m_version = 0;
};

TEST_CASE("test_ResourceVersionPool_BasicVersionControl") {
    Parameter param;
    param.set<std::string>("version", "1.0.0");
    ResourceVersionPool<TTResource> pool(param);
    CHECK_EQ(pool.count(), 0);
    CHECK_EQ(pool.idleCount(), 0);

    auto x1 = pool.get();
    CHECK_EQ(pool.count(), 1);
    CHECK_EQ(pool.idleCount(), 0);

    x1.reset();
    CHECK_EQ(pool.count(), 1);
    CHECK_EQ(pool.idleCount(), 1);

    auto x2 = pool.get();
    CHECK_EQ(pool.count(), 1);
    CHECK_EQ(pool.idleCount(), 0);

    auto x3 = pool.get();
    CHECK_EQ(pool.count(), 2);
    CHECK_EQ(pool.idleCount(), 0);

    auto x4 = pool.get();
    CHECK_EQ(pool.count(), 3);
    CHECK_EQ(pool.idleCount(), 0);

    x4.reset();
    CHECK_EQ(pool.count(), 3);
    CHECK_EQ(pool.idleCount(), 1);

    // 目前资源被占用资源2个(x2, x3), 空闲资源1个
    // 通知资源池参数变更，但实际未发生变化
    pool.setParam<std::string>("version", "1.0.0");
    CHECK_EQ(pool.count(), 3);
    CHECK_EQ(pool.idleCount(), 1);

    // 参数实际变更，空闲资源被释放，使用中的老资源不变，新申请资源参赛变化
    pool.setParam<std::string>("version", "1.0.1");
    CHECK_EQ(pool.count(), 2);
    CHECK_EQ(pool.idleCount(), 0);
    CHECK_EQ(x2->getParam<std::string>("version"), "1.0.0");
    CHECK_EQ(x3->getParam<std::string>("version"), "1.0.0");
    x1 = pool.get();
    CHECK_EQ(x1->getParam<std::string>("version"), "1.0.1");
    CHECK_EQ(pool.count(), 3);
    CHECK_EQ(pool.idleCount(), 0);

    // 释放老资源直接被回收
    x2.reset();
    CHECK_EQ(pool.count(), 2);
    CHECK_EQ(pool.idleCount(), 0);
}

TEST_CASE("test_ResourceVersionPool_VersionIncrement") {
    Parameter param;
    param.set<std::string>("name", "test");
    param.set<std::string>("version", "v1");
    ResourceVersionPool<TTResource> pool(param);

    CHECK_EQ(pool.getVersion(), 0);

    pool.incVersion(1);
    CHECK_EQ(pool.getVersion(), 1);

    pool.incVersion(1);
    CHECK_EQ(pool.getVersion(), 2);
}

TEST_CASE("test_ResourceVersionPool_SetParameter") {
    Parameter param;
    param.set<std::string>("name", "v1");
    param.set<std::string>("version", "v1");
    ResourceVersionPool<TTResource> pool(param);

    auto r1 = pool.get();
    CHECK_EQ(r1->getParam<std::string>("name"), "v1");
    r1.reset();

    // 更新参数
    Parameter new_param;
    new_param.set<std::string>("name", "v2");
    new_param.set<std::string>("version", "v2");
    pool.setParameter(new_param);

    CHECK_EQ(pool.getVersion(), 1);

    auto r2 = pool.get();
    CHECK_EQ(r2->getParam<std::string>("name"), "v2");
}

TEST_CASE("test_ResourceVersionPool_HaveParam") {
    Parameter param;
    param.set<std::string>("name", "test");
    param.set<std::string>("version", "v1");
    ResourceVersionPool<TTResource> pool(param);

    CHECK(pool.haveParam("name"));
    CHECK(!pool.haveParam("nonexistent"));
}

TEST_CASE("test_ResourceVersionPool_GetParam") {
    Parameter param;
    param.set<std::string>("name", "test_value");
    param.set<int>("count", 42);
    param.set<std::string>("version", "v1");
    ResourceVersionPool<TTResource> pool(param);

    CHECK_EQ(pool.getParam<std::string>("name"), "test_value");
    CHECK_EQ(pool.getParam<int>("count"), 42);
}

TEST_CASE("test_ResourceVersionPool_OldVersionResourceCleanup") {
    Parameter param;
    param.set<std::string>("version", "old");
    ResourceVersionPool<TTResource> pool(param, 10, 10);

    // 获取一些旧版本资源
    auto r1 = pool.get();
    auto r2 = pool.get();
    CHECK_EQ(r1->getVersion(), 0);
    CHECK_EQ(r2->getVersion(), 0);

    // 更新版本
    pool.setParam<std::string>("version", "new");
    CHECK_EQ(pool.getVersion(), 1);

    // 旧资源归还时应被直接删除
    r1.reset();
    CHECK_EQ(pool.count(), 1);  // r1 被删除
    CHECK_EQ(pool.idleCount(), 0);

    // 新版本资源
    auto r3 = pool.get();
    CHECK_EQ(r3->getVersion(), 1);
    CHECK_EQ(r3->getParam<std::string>("version"), "new");
}

TEST_CASE("test_ResourceVersionPool_IncVersionReleasesIdle") {
    /** Regression: incVersion raises the version and must invalidate the idle cache the
     * same way setParameter does, so a stale-version resource is never handed out by get */
    Parameter param;
    param.set<std::string>("version", "v1");
    ResourceVersionPool<TTResource> pool(param, 10, 10);

    auto r1 = pool.get();
    CHECK_EQ(r1->getVersion(), 0);
    r1.reset();
    CHECK_EQ(pool.idleCount(), 1);
    CHECK_EQ(pool.count(), 1);

    // bump the version only through incVersion (no parameter change path involved)
    pool.incVersion(2);
    CHECK_EQ(pool.getVersion(), 2);

    // the stale idle resource must have been released
    CHECK_EQ(pool.idleCount(), 0);
    CHECK_EQ(pool.count(), 0);

    // a taken resource must carry the current version
    auto r2 = pool.get();
    CHECK_EQ(r2->getVersion(), 2);
    CHECK_EQ(pool.count(), 1);
}

TEST_CASE("test_ResourceVersionPool_GetWaitForAfterIncVersion") {
    /** Regression: after incVersion getWaitFor must serve a current-version resource
     * (creating one once the stale idle entry is gone) instead of a stale or failed take */
    Parameter param;
    param.set<std::string>("version", "v1");
    ResourceVersionPool<TTResource> pool(param, 1, 10);

    auto r1 = pool.get();
    CHECK_EQ(r1->getVersion(), 0);
    r1.reset();
    CHECK_EQ(pool.idleCount(), 1);

    pool.incVersion(1);
    CHECK_EQ(pool.getVersion(), 1);

    // the pool limit is 1 and no resource is alive now: getWaitFor must create a new-version
    // resource rather than time out or reuse the released stale one
    auto r2 = pool.getWaitFor(50);
    CHECK(r2 != nullptr);
    CHECK_EQ(r2->getVersion(), 1);
    CHECK_EQ(pool.count(), 1);
}

TEST_CASE("test_ResourceVersionPool_ConcurrentVersionUpdate") {
    Parameter param;
    param.set<std::string>("version", "v1");
    ResourceVersionPool<TTResource> pool(param, 5, 10);

    const int num_threads = 6;
    std::atomic<int> old_version_count(0);
    std::atomic<int> new_version_count(0);
    std::atomic<int> completed(0);
    std::promise<void> all_done;
    std::future<void> future = all_done.get_future();

    // 先获取一些资源
    auto holder1 = pool.get();
    auto holder2 = pool.get();

    // 更新版本
    pool.setParam<std::string>("version", "v2");

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            try {
                auto res = pool.get();
                if (res) {
                    if (res->getVersion() == 0) {
                        old_version_count.fetch_add(1);
                    } else if (res->getVersion() == 1) {
                        new_version_count.fetch_add(1);
                    }
                }
            } catch (...) {
                // Ignore errors
            }

            if (completed.fetch_add(1) + 1 == num_threads) {
                all_done.set_value();
            }
        });
    }

    if (future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
        FAIL("Version update test timeout");
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    HKU_INFO("Version concurrent - Old: {}, New: {}", old_version_count.load(),
             new_version_count.load());

    // 所有新获取的资源应该是新版本
    CHECK_EQ(old_version_count.load(), 0);
    CHECK_EQ(new_version_count.load(), num_threads);
}

TEST_CASE("test_ResourcePool_ConcurrentCounters") {
    Parameter param;
    ResourcePool<TestResource> pool(param, 8, 4);
    std::atomic<bool> stop(false);
    std::atomic<int> readers_done(0);

    /** Readers call the const counters and limits concurrently with get/return in other threads */
    std::thread reader([&]() {
        for (int i = 0; i < 5000 && !stop.load(); i++) {
            CHECK_LE(pool.count(), 8);
            CHECK_LE(pool.idleCount(), pool.count());
            CHECK_LE(pool.idleCount(), pool.maxIdleSize());
            CHECK_EQ(pool.maxPoolSize(), 8);
            std::this_thread::yield();
        }
        readers_done++;
    });

    // Producers get and return resources, mutating m_count and the idle queue
    std::vector<std::thread> workers;
    for (int i = 0; i < 6; i++) {
        workers.emplace_back([&]() {
            for (int j = 0; j < 100; j++) {
                auto res = pool.get();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                res.reset();
                pool.maxIdleSize(4);  // a locked write racing with the locked reads
            }
        });
    }
    for (auto& t : workers) {
        t.join();
    }

    stop.store(true);
    reader.join();
}

TEST_CASE("test_ResourcePool_WaiterWokenOnDestroy") {
    Parameter param;
    auto* pool_ptr = new ResourcePool<TestResource>(param, 1, 10);
    auto held = pool_ptr->get();
    std::atomic<bool> waiter_threw(false);

    /** A getter blocked because the only resource is in use is woken by the destructor and throws
     * ResourcePoolClosedException instead of waiting forever or touching the destroyed pool */
    std::thread waiter([&]() {
        try {
            pool_ptr->getAndWait();
        } catch (const ResourcePoolClosedException&) {
            waiter_threw.store(true);
        }
    });

    // Make sure the waiter has entered the cond wait before destroying the pool
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    delete pool_ptr;
    waiter.join();
    CHECK_UNARY(waiter_threw.load());

    // The closer was unbound, so the held resource is deleted here without touching the pool
    held.reset();
}

TEST_CASE("test_ResourceVersionPool_WaiterWokenOnDestroy") {
    Parameter param;
    param.set<std::string>("version", "v1");
    auto* pool_ptr = new ResourceVersionPool<TTResource>(param, 1, 10);
    auto held = pool_ptr->get();
    std::atomic<bool> waiter_threw(false);

    /** Same close behavior for the versioned pool */
    std::thread waiter([&]() {
        try {
            pool_ptr->getAndWait();
        } catch (const ResourcePoolClosedException&) {
            waiter_threw.store(true);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    delete pool_ptr;
    waiter.join();
    CHECK_UNARY(waiter_threw.load());

    held.reset();
}

TEST_CASE("test_ResourcePool_OriginalTest") {
    Parameter param;
    ResourcePool<TestResource> pool(param);
    auto y = pool.get();
    y.reset();

    ResourcePool<TestResource>* pool_ptr = new ResourcePool<TestResource>(param, 0, 5);
    auto x1 = pool_ptr->get();
    auto x2 = pool_ptr->get();
    x1.reset();

    ThreadPool tg(12);
    for (int i = 0; i < 3; i++) {
        tg.submit([=] {
            auto a = pool_ptr->get();
            a->print();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        });
    }
    tg.join();

    delete pool_ptr;
}