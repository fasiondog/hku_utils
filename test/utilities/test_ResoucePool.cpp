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

TEST_CASE("test_ResourcePool") {
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

class TTResource : public ResourceWithVersion {
    PARAMETER_SUPPORT

public:
    TTResource(const Parameter& param) {
        setParam<std::string>("version", param.get<std::string>("version"));
    }

    ~TTResource() {}

    void print() {
        printf("i am version: %d\n", m_version);
    }
};

TEST_CASE("test_ResourceVersionPool") {
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
    x1 = pool.getAndWait();
    CHECK_EQ(x1->getParam<std::string>("version"), "1.0.1");
    CHECK_EQ(pool.count(), 3);
    CHECK_EQ(pool.idleCount(), 0);

    // 释放老资源直接被回收
    x2.reset();
    CHECK_EQ(pool.count(), 2);
    CHECK_EQ(pool.idleCount(), 0);
}