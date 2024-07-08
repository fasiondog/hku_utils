/**
 *  Copyright (c) 2021 hikyuu
 *
 *  Created on: 2021/05/18
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <iostream>
#include <hikyuu/utilities/Parameter.h>

using namespace hku;

TEST_CASE("test_Parameter") {
    hku::Parameter param;
    REQUIRE(param.empty());

    /** 正常添加、读取、修改参数 */
    param.set<int>("n", 1);
    param.set<int64_t>("n64", 1024);
    param.set<bool>("bool", true);
    param.set<double>("double", 10);
    param.set<float>("float", 10);
    param.set<std::string>("string", "test");
    CHECK_EQ(param.size(), 6);
    CHECK(param.get<int>("n") == 1);
    CHECK(param.get<int64_t>("n") == 1);
    CHECK(param.get<int>("n64") == 1024);
    CHECK(param.get<int64_t>("n64") == 1024);
    CHECK(param.get<bool>("bool") == true);
    CHECK(param.get<double>("double") == 10.0);
    CHECK(param.get<float>("float") == 10.0);
    CHECK(param.get<double>("float") == 10.0);
    CHECK(param.get<std::string>("string") == "test");
    param.set<int>("n", 10);
    param.set<int64_t>("n64", std::numeric_limits<int64_t>::max());
    param.set<bool>("bool", false);
    param.set<double>("double", 10.01);
    param.set<float>("float", 10.01);
    param.set<std::string>("string", "test2");
    CHECK(param.get<int>("n") == 10);
    CHECK(param.get<int64_t>("n64") == std::numeric_limits<int64_t>::max());
    CHECK(param.get<bool>("bool") == false);
    CHECK(param.get<double>("double") == 10.01);
    CHECK(param.get<float>("float") == 10.01f);
    CHECK(param.get<double>("float") == doctest::Approx(10.01).epsilon(0.001));
    CHECK(param.get<std::string>("string") == "test2");

    /** 将超出int值域范围的数，以 int 方式取出 */
    param.set<int64_t>("n64", std::numeric_limits<int>::max());
    CHECK(param.get<int>("n64") == std::numeric_limits<int>::max());
    param.set<int64_t>("n64", std::numeric_limits<int>::min());
    CHECK(param.get<int>("n64") == std::numeric_limits<int>::min());

    param.set<int64_t>("n64", int64_t(std::numeric_limits<int>::max()) + 1);
    CHECK(param.get<int64_t>("n64") == int64_t(std::numeric_limits<int>::max()) + 1);
    CHECK_THROWS_AS(param.get<int>("n64"), std::out_of_range);

    param.set<int64_t>("n64", int64_t(std::numeric_limits<int>::min()) - 1);
    CHECK(param.get<int64_t>("n64") == int64_t(std::numeric_limits<int>::min()) - 1);
    CHECK_THROWS_AS(param.get<int>("n64"), std::out_of_range);

    /** 将超出 float 值域范围的数 */
    param.set<float>("float", std::numeric_limits<float>::max());
    CHECK(param.get<float>("float") == std::numeric_limits<float>::max());
    param.set<float>("float", std::numeric_limits<float>::min());
    CHECK(param.get<float>("float") == std::numeric_limits<float>::min());

    // 注：float 最大值由于精度问题，简单加1，最大值不会发生变化
    param.set<double>("float", double(std::numeric_limits<float>::max()) + 1e+38f);
    CHECK(param.get<double>("float") == double(std::numeric_limits<float>::max()) + 1e+38f);
    CHECK_THROWS_AS(param.get<float>("float"), std::out_of_range);

    param.set<double>("float", double(std::numeric_limits<float>::min()) - 1);
    CHECK(param.get<double>("float") == double(std::numeric_limits<float>::min()) - 1);
    CHECK_THROWS_AS(param.get<float>("float"), std::out_of_range);

    /** 添加不支持的参数类型 */
    CHECK_THROWS_AS(param.set<size_t>("n", 10), std::logic_error);
    CHECK_THROWS_AS(param.set<float>("n", 10.0), std::logic_error);

    /** 修改参数时，指定的类型和原有类型不符 */
    CHECK_THROWS_AS(param.set<float>("n", 10.0), std::logic_error);
    CHECK_THROWS_AS(param.set<float>("bool", 10.0), std::logic_error);

    /** 测试相等比较 */
    Parameter p1, p2;
    p1.set<std::string>("string", "test");
    p1.set<bool>("bool", true);
    p1.set<double>("double", 0.01);
    p1.set<std::string>("test", "test2");

    p2.set<double>("double", 0.01);
    p2.set<std::string>("test", "test2");
    p2.set<std::string>("string", "test");
    p2.set<bool>("bool", true);

#if defined(HKU_SUPPORT_DATETIME)
    param.set<Datetime>("date", Datetime(202106170000));
    CHECK_EQ(param.get<Datetime>("date"), Datetime(202106170000));

    param.set<TimeDelta>("delta", TimeDelta(1));
    CHECK_EQ(param.get<TimeDelta>("delta"), TimeDelta(1));
#endif

    // 测试 clear
    REQUIRE(p2.size() > 0);
    p2.clear();
    CHECK(p2.empty());
    CHECK_EQ(p2.size(), 0);

    /** 移动赋值 */
    REQUIRE_UNARY(p1.size() > 0 && p2.empty());
    p2 = std::move(p1);
    CHECK(p2.size() > 0);
    CHECK(p1.empty());

    /** 移动构造 */
    Parameter p3(std::move(p2));
    CHECK(p3.size() > 0);
}

// TEST_CASE("test_Parameter_serialize") {
//     Parameter p1;
//     p1.set<int>("i", 10);
//     p1.set<bool>("b", true);
//     p1.set<double>("d", 0.12);
//     p1.set<float>("f", 0.12);
//     p1.set<std::string>("s", "this");

//     constexpr std::size_t flags = yas::mem | yas::binary;
//     auto buf = yas::save<flags>(p1);

//     Parameter p2;
//     yas::load<flags>(buf, p2);
//     CHECK_EQ(p1.get<int>("i"), p2.get<int>("i"));
//     CHECK_EQ(p1.get<bool>("b"), p2.get<bool>("b"));
//     CHECK_EQ(p1.get<double>("d"), p2.get<double>("d"));
//     CHECK_EQ(p1.get<float>("f"), p2.get<float>("f"));
//     CHECK_EQ(p1.get<std::string>("s"), p2.get<std::string>("s"));
// }
