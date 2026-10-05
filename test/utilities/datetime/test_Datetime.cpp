/*
 * test_datetime.cpp
 *
 *  Created on: 2012-8-25
 *      Author: fasiondog
 */

#include "doctest/doctest.h"

#include <hikyuu/utilities/datetime/Datetime.h>
#include <hikyuu/utilities/Null.h>
#include <hikyuu/utilities/Log.h>

using namespace hku;

/**
 * @defgroup test_hikyuu_Datetime test_hikyuu_Datetime
 * @ingroup test_hikyuu_datetime_suite
 * @{
 */

/** @par 检测点 */
TEST_CASE("test_Datetime") {
    Datetime null_datetime = Null<Datetime>();

    /** @arg 默认无参构造函数返回Null<Datetime>() */
    Datetime d;
    CHECK(d == Null<Datetime>());
    CHECK(Null<Datetime>() == d);
    d = Datetime(201208062359);
    CHECK(d < Null<Datetime>());

    /** @arg 从Null<unsigned long long>()构造 */
    d = Datetime(Null<unsigned long long>());
    CHECK(d == Null<Datetime>());

    /** @arg 从bd::date 隐式构造 */
    d = Datetime(200101010110);
    d = Datetime(d.date());
    CHECK(Datetime(200101010000) == d);

    /** @arg 测试 unsigned long long 构造 */
    CHECK(Datetime(20010101) == Datetime(2001, 1, 1));
    CHECK(Datetime(20101231) == Datetime(2010, 12, 31));
    CHECK(Datetime(200101010000) == Datetime(2001, 1, 1));

    /** @arg 测试 string 构造 */
    d = Datetime(200101010000L);
    CHECK(Datetime("2001-1-1") == d);
    CHECK(Datetime("2001-01-1") == d);
    CHECK(Datetime("2001-1-01") == d);
    CHECK(Datetime("2001-01-01") == d);
    CHECK(Datetime("2001/1/1") == d);
    CHECK(Datetime("2001/1/01") == d);
    CHECK(Datetime("2001/01/1") == d);
    CHECK(Datetime("2001/01/01") == d);
    CHECK(Datetime("20010101") == d);
    d = Datetime(2001, 1, 2, 3, 4, 5, 0, 0);
    CHECK(Datetime("2001-1-2 3:4:5") == d);
    CHECK(Datetime("2001/1/2 3:4:5") == d);
    CHECK(Datetime("20010102T030405") == d);
    CHECK(Datetime("20010102t030405") == d);
    CHECK(Datetime("20010102 3:4:5") == d);
    CHECK_THROWS(Datetime("2001"));

    /** @arg 非法年份 */
    CHECK_THROWS_AS(Datetime(99999, 1, 1), std::out_of_range);
    CHECK_THROWS_AS(Datetime(000001010000L), std::out_of_range);

    /** @arg 非法月份 */
    CHECK_THROWS_AS(Datetime(2010, 13, 1), std::out_of_range);
    CHECK_THROWS_AS(Datetime(2010, 0, 1), std::out_of_range);

    CHECK_THROWS_AS(Datetime(201013010000L), std::out_of_range);
    CHECK_THROWS_AS(Datetime(201000010000L), std::out_of_range);

    /** @arg 非法日期 */
    CHECK_THROWS_AS(Datetime(2010, 1, 0), std::out_of_range);
    CHECK_THROWS_AS(Datetime(2010, 1, 32), std::out_of_range);

    CHECK_THROWS_AS(Datetime(201001000000L), std::out_of_range);
    CHECK_THROWS_AS(Datetime(201001320000L), std::out_of_range);

    /** @arg 非法小时 */
    CHECK_THROWS_AS(Datetime(201001012400L), std::out_of_range);
    CHECK_THROWS_AS(Datetime(201001012500L), std::out_of_range);

    /** @arg 非法分钟 */
    CHECK_THROWS_AS(Datetime(201001010060L), std::out_of_range);
    CHECK_THROWS_AS(Datetime(201001010061L), std::out_of_range);

    /** @arg 非法参数 */
    CHECK_THROWS_AS(Datetime(2001010203041LL), std::out_of_range);

    /** @arg 属性读取 */
    d = Datetime(2012, 8, 6, 23, 59, 2, 13, 15);
    CHECK(2012 == d.year());
    CHECK(8 == d.month());
    CHECK(6 == d.day());
    CHECK(23 == d.hour());
    CHECK(59 == d.minute());
    CHECK(2 == d.second());
    CHECK(13 == d.millisecond());
    CHECK(15 == d.microsecond());

    /** @arg Null<Datetime> 获取熟悉 */
    d = Datetime();
    CHECK_THROWS_AS(d.year(), std::logic_error);
    CHECK_THROWS_AS(d.month(), std::logic_error);
    CHECK_THROWS_AS(d.day(), std::logic_error);
    CHECK_THROWS_AS(d.hour(), std::logic_error);
    CHECK_THROWS_AS(d.minute(), std::logic_error);
    CHECK_THROWS_AS(d.second(), std::logic_error);
    CHECK_THROWS_AS(d.millisecond(), std::logic_error);
    CHECK_THROWS_AS(d.microsecond(), std::logic_error);

    /** @arg 正常日期转化为unsigned long long */
    d = Datetime(201208062359);
    unsigned long long x = d.number();
    CHECK(x == 201208062359);

    long long y = x;
    Datetime m(y);
    CHECK(m == d);

    /** @arg 兼容oracle datetime表示法 */
    d = Datetime(2021, 4, 25, 23, 16, 27);
    CHECK_EQ(d.hex(), 0x1415041917101bULL);
    CHECK_EQ(d, Datetime::fromHex(d.hex()));
    CHECK_EQ(Datetime(), Datetime::fromHex(Null<uint64_t>()));

    /** @arg Null<Datetime>()转化为number */
    x = Null<unsigned long long>();
    d = Null<Datetime>();
    CHECK(x == d.number());

    /** @arg 测试 str */
    d = Datetime("2001-Jan-01 06:30:00");
    CHECK("2001-01-01 06:30:00" == d.str());
    CHECK("2001-01-01 06:30:00" == std::to_string(d));
    d = Datetime("2001-Jan-01 06:30:11.001");
    CHECK("2001-01-01 06:30:11.001000" == d.str());
    CHECK("2001-01-01 06:30:11.001000" == std::to_string(d));

    /** @arg 测试 dayOfWeek*/
    CHECK(Datetime(201801010000L).dayOfWeek() == 1);
    CHECK(Datetime(201801060000L).dayOfWeek() == 6);
    CHECK(Datetime(201801070000L).dayOfWeek() == 0);

    /** @arg 测试 dayOfYear*/
    CHECK(Datetime(201801010000L).dayOfYear() == 1);
    CHECK(Datetime(201812310000L).dayOfYear() == 365);

    /** @arg 测试 dateOfWeek */
    CHECK(null_datetime.dateOfWeek(-1) == null_datetime);
    CHECK(null_datetime.dateOfWeek(0) == null_datetime);
    CHECK(null_datetime.dateOfWeek(4) == null_datetime);
    CHECK(null_datetime.dateOfWeek(6) == null_datetime);
    CHECK(null_datetime.dateOfWeek(7) == null_datetime);
    CHECK(Datetime(201901010000).dateOfWeek(-1) == Datetime(201812300000));
    CHECK(Datetime(201901010000).dateOfWeek(0) == Datetime(201812300000));
    CHECK(Datetime(201901010000).dateOfWeek(1) == Datetime(201812310000));
    CHECK(Datetime(201901010000).dateOfWeek(2) == Datetime(201901010000));
    CHECK(Datetime(201901010000).dateOfWeek(3) == Datetime(201901020000));
    CHECK(Datetime(201901010000).dateOfWeek(4) == Datetime(201901030000));
    CHECK(Datetime(201901010000).dateOfWeek(5) == Datetime(201901040000));
    CHECK(Datetime(201901010000).dateOfWeek(6) == Datetime(201901050000));
    CHECK(Datetime(201901010000).dateOfWeek(7) == Datetime(201901050000));

    /** @arg 测试 startOfDay */
    CHECK(null_datetime.startOfDay() == null_datetime);
    CHECK(Datetime::min().startOfDay() == Datetime::min());
    CHECK(Datetime::max().startOfDay() == Datetime::max());
    CHECK(Datetime(201812310110).startOfDay() == Datetime(201812310000));

    /** @arg 测试 endOfDay */
    CHECK(null_datetime.endOfDay() == null_datetime);
    CHECK(Datetime::min().endOfDay() == Datetime(1400, 1, 1, 23, 59, 59));
    CHECK(Datetime::max().endOfDay() == Datetime::max());
    CHECK(Datetime(201812310110).endOfDay() == Datetime(2018, 12, 31, 23, 59, 59));

    /** @arg 测试 startOfWeek */
    CHECK(null_datetime.startOfWeek() == null_datetime);
    CHECK(Datetime::min().startOfWeek() == Datetime::min());
    CHECK(Datetime::max().startOfWeek() == Datetime(9999, 12, 27));
    CHECK(Datetime(201812310000).startOfWeek() == Datetime(201812310000));
    CHECK(Datetime(201901050000).startOfWeek() == Datetime(201812310000));
    CHECK(Datetime(201901060000).startOfWeek() == Datetime(201812310000));
    CHECK(Datetime(201901090000).startOfWeek() == Datetime(201901070000));

    /** @arg 测试 endOfWeek */
    CHECK(null_datetime.endOfWeek() == null_datetime);
    CHECK(Datetime::min().endOfWeek() == Datetime(1400, 1, 5));
    CHECK(Datetime::max().endOfWeek() == Datetime::max());
    CHECK(Datetime(201812310000).endOfWeek() == Datetime(201901060000));
    CHECK(Datetime(201901050000).endOfWeek() == Datetime(201901060000));
    CHECK(Datetime(201901060000).endOfWeek() == Datetime(201901060000));
    CHECK(Datetime(201901090000).endOfWeek() == Datetime(201901130000));

    /** @arg 测试 startOfMonth */
    CHECK(null_datetime.startOfMonth() == null_datetime);
    CHECK(Datetime::min().startOfMonth() == Datetime::min());
    CHECK(Datetime::max().startOfMonth() == Datetime(9999, 12, 1));
    CHECK(Datetime(201901050000).startOfMonth() == Datetime(201901010000));
    CHECK(Datetime(201902050000).startOfMonth() == Datetime(201902010000));
    CHECK(Datetime(201902010000).startOfMonth() == Datetime(201902010000));
    CHECK(Datetime(201903030000).startOfMonth() == Datetime(201903010000));
    CHECK(Datetime(201904030000).startOfMonth() == Datetime(201904010000));
    CHECK(Datetime(201912030000).startOfMonth() == Datetime(201912010000));

    /** @arg 测试 endOfMonth*/
    CHECK(null_datetime.endOfMonth() == null_datetime);
    CHECK(Datetime::max().endOfMonth() == Datetime::max());
    CHECK(Datetime::min().endOfMonth() == Datetime(1400, 1, 31));
    CHECK(Datetime(201801010000L).endOfMonth() == Datetime(201801310000L));
    CHECK(Datetime(201802010000L).endOfMonth() == Datetime(201802280000L));
    CHECK(Datetime(201803010000L).endOfMonth() == Datetime(201803310000L));
    CHECK(Datetime(201804010000L).endOfMonth() == Datetime(201804300000L));
    CHECK(Datetime(201805010000L).endOfMonth() == Datetime(201805310000L));
    CHECK(Datetime(201806010000L).endOfMonth() == Datetime(201806300000L));
    CHECK(Datetime(201807010000L).endOfMonth() == Datetime(201807310000L));
    CHECK(Datetime(201808010000L).endOfMonth() == Datetime(201808310000L));
    CHECK(Datetime(201809010000L).endOfMonth() == Datetime(201809300000L));
    CHECK(Datetime(201810010000L).endOfMonth() == Datetime(201810310000L));
    CHECK(Datetime(201811010000L).endOfMonth() == Datetime(201811300000L));
    CHECK(Datetime(201812010000L).endOfMonth() == Datetime(201812310000L));

    /** @arg 测试 startOfQuarter */
    CHECK(null_datetime.startOfQuarter() == null_datetime);
    CHECK(Datetime::min().startOfQuarter() == Datetime::min());
    CHECK(Datetime::max().startOfQuarter() == Datetime(9999, 10, 1));
    CHECK(Datetime(201902050000).startOfQuarter() == Datetime(201901010000));
    CHECK(Datetime(201901010000).startOfQuarter() == Datetime(201901010000));
    CHECK(Datetime(201903310000).startOfQuarter() == Datetime(201901010000));
    CHECK(Datetime(201904010000).startOfQuarter() == Datetime(201904010000));
    CHECK(Datetime(201905010000).startOfQuarter() == Datetime(201904010000));
    CHECK(Datetime(201906300000).startOfQuarter() == Datetime(201904010000));
    CHECK(Datetime(201907010000).startOfQuarter() == Datetime(201907010000));
    CHECK(Datetime(201907060000).startOfQuarter() == Datetime(201907010000));
    CHECK(Datetime(201909300000).startOfQuarter() == Datetime(201907010000));
    CHECK(Datetime(201910010000).startOfQuarter() == Datetime(201910010000));
    CHECK(Datetime(201911010000).startOfQuarter() == Datetime(201910010000));
    CHECK(Datetime(201912310000).startOfQuarter() == Datetime(201910010000));

    /** @arg 测试 endOfQuarter */
    CHECK(null_datetime.endOfQuarter() == null_datetime);
    CHECK(Datetime::min().endOfQuarter() == Datetime(1400, 3, 31));
    CHECK(Datetime::max().endOfQuarter() == Datetime::max());
    CHECK(Datetime(201902050000).endOfQuarter() == Datetime(201903310000));
    CHECK(Datetime(201901010000).endOfQuarter() == Datetime(201903310000));
    CHECK(Datetime(201903310000).endOfQuarter() == Datetime(201903310000));
    CHECK(Datetime(201904010000).endOfQuarter() == Datetime(201906300000));
    CHECK(Datetime(201905010000).endOfQuarter() == Datetime(201906300000));
    CHECK(Datetime(201906300000).endOfQuarter() == Datetime(201906300000));
    CHECK(Datetime(201907010000).endOfQuarter() == Datetime(201909300000));
    CHECK(Datetime(201907060000).endOfQuarter() == Datetime(201909300000));
    CHECK(Datetime(201909300000).endOfQuarter() == Datetime(201909300000));
    CHECK(Datetime(201910010000).endOfQuarter() == Datetime(201912310000));
    CHECK(Datetime(201911010000).endOfQuarter() == Datetime(201912310000));
    CHECK(Datetime(201912310000).endOfQuarter() == Datetime(201912310000));

    /** @arg 测试 startOfHalfyear */
    CHECK(null_datetime.startOfHalfyear() == null_datetime);
    CHECK(Datetime::min().startOfHalfyear() == Datetime::min());
    CHECK(Datetime::max().startOfHalfyear() == Datetime(9999, 7, 1));
    CHECK(Datetime(201812310000).startOfHalfyear() == Datetime(201807010000));
    CHECK(Datetime(201901010000).startOfHalfyear() == Datetime(201901010000));
    CHECK(Datetime(201901020000).startOfHalfyear() == Datetime(201901010000));
    CHECK(Datetime(201906050000).startOfHalfyear() == Datetime(201901010000));
    CHECK(Datetime(201906300000).startOfHalfyear() == Datetime(201901010000));
    CHECK(Datetime(201907010000).startOfHalfyear() == Datetime(201907010000));
    CHECK(Datetime(201907100000).startOfHalfyear() == Datetime(201907010000));
    CHECK(Datetime(201912010000).startOfHalfyear() == Datetime(201907010000));
    CHECK(Datetime(201912310000).startOfHalfyear() == Datetime(201907010000));

    /** @arg 测试 endOfHalfyear */
    CHECK(null_datetime.endOfHalfyear() == null_datetime);
    CHECK(Datetime::min().endOfHalfyear() == Datetime(1400, 6, 30));
    CHECK(Datetime::max().endOfHalfyear() == Datetime::max());
    CHECK(Datetime(201812310000).endOfHalfyear() == Datetime(201812310000));
    CHECK(Datetime(201901010000).endOfHalfyear() == Datetime(201906300000));
    CHECK(Datetime(201901020000).endOfHalfyear() == Datetime(201906300000));
    CHECK(Datetime(201906050000).endOfHalfyear() == Datetime(201906300000));
    CHECK(Datetime(201906300000).endOfHalfyear() == Datetime(201906300000));
    CHECK(Datetime(201907010000).endOfHalfyear() == Datetime(201912310000));
    CHECK(Datetime(201907100000).endOfHalfyear() == Datetime(201912310000));
    CHECK(Datetime(201912010000).endOfHalfyear() == Datetime(201912310000));
    CHECK(Datetime(201912310000).endOfHalfyear() == Datetime(201912310000));

    /** @arg 测试 startOfYear */
    CHECK(null_datetime.startOfYear() == null_datetime);
    CHECK(Datetime::min().startOfYear() == Datetime::min());
    CHECK(Datetime::max().startOfYear() == Datetime(9999, 1, 1));
    CHECK(Datetime(201901050000).startOfYear() == Datetime(201901010000));
    CHECK(Datetime(201911050000).startOfYear() == Datetime(201901010000));

    /** @arg 测试 endOfYear */
    CHECK(null_datetime.endOfYear() == null_datetime);
    CHECK(Datetime::min().endOfYear() == Datetime(1400, 12, 31));
    CHECK(Datetime::max().endOfYear() == Datetime::max());
    CHECK(Datetime(201902050000).endOfYear() == Datetime(201912310000));
    CHECK(Datetime(201912310000).endOfYear() == Datetime(201912310000));

    /** @arg 测试 nextDay */
    CHECK(null_datetime.nextDay() == null_datetime);
    CHECK(Datetime::max().nextDay() == Datetime::max());
    CHECK(Datetime(201802060000L).nextDay() == Datetime(201802070000L));
    CHECK(Datetime(201802280000L).nextDay() == Datetime(201803010000L));
    CHECK(Datetime(201602280000L).nextDay() == Datetime(201602290000L));

    /** @arg 测试 nextWeek */
    CHECK(null_datetime.nextWeek() == null_datetime);
    CHECK(Datetime::max().nextWeek() == Datetime::max());
    CHECK(Datetime(201812300000).nextWeek() == Datetime(201812310000));
    CHECK(Datetime(201812310000).nextWeek() == Datetime(201901070000));
    CHECK(Datetime(201901010000).nextWeek() == Datetime(201901070000));
    CHECK(Datetime(201901020000).nextWeek() == Datetime(201901070000));
    CHECK(Datetime(201901030000).nextWeek() == Datetime(201901070000));
    CHECK(Datetime(201901040000).nextWeek() == Datetime(201901070000));
    CHECK(Datetime(201901050000).nextWeek() == Datetime(201901070000));
    CHECK(Datetime(201901060000).nextWeek() == Datetime(201901070000));
    CHECK(Datetime(201901070000).nextWeek() == Datetime(201901140000));

    /** @arg 测试 nextMonth */
    CHECK(null_datetime.nextMonth() == null_datetime);
    CHECK(Datetime::max().nextMonth() == Datetime::max());
    CHECK(Datetime(201812310000).nextMonth() == Datetime(201901010000));
    CHECK(Datetime(201901010000).nextMonth() == Datetime(201902010000));
    CHECK(Datetime(201901310000).nextMonth() == Datetime(201902010000));

    /** @arg 测试 nextQuarter */
    CHECK(null_datetime.nextQuarter() == null_datetime);
    CHECK(Datetime::max().nextQuarter() == Datetime::max());
    CHECK(Datetime(201901010000).nextQuarter() == Datetime(201904010000));
    CHECK(Datetime(201902010000).nextQuarter() == Datetime(201904010000));
    CHECK(Datetime(201903310000).nextQuarter() == Datetime(201904010000));
    CHECK(Datetime(201904010000).nextQuarter() == Datetime(201907010000));
    CHECK(Datetime(201905010000).nextQuarter() == Datetime(201907010000));
    CHECK(Datetime(201906300000).nextQuarter() == Datetime(201907010000));
    CHECK(Datetime(201907010000).nextQuarter() == Datetime(201910010000));
    CHECK(Datetime(201907020000).nextQuarter() == Datetime(201910010000));
    CHECK(Datetime(201908010000).nextQuarter() == Datetime(201910010000));
    CHECK(Datetime(201909300000).nextQuarter() == Datetime(201910010000));
    CHECK(Datetime(201910010000).nextQuarter() == Datetime(202001010000));
    CHECK(Datetime(201911010000).nextQuarter() == Datetime(202001010000));
    CHECK(Datetime(201912310000).nextQuarter() == Datetime(202001010000));

    /** @arg 测试 nextHalfyear */
    CHECK(null_datetime.nextHalfyear() == null_datetime);
    CHECK(Datetime::max().nextHalfyear() == Datetime::max());
    CHECK(Datetime(201812310000).nextHalfyear() == Datetime(201901010000));
    CHECK(Datetime(201901010000).nextHalfyear() == Datetime(201907010000));
    CHECK(Datetime(201905050000).nextHalfyear() == Datetime(201907010000));
    CHECK(Datetime(201906300000).nextHalfyear() == Datetime(201907010000));
    CHECK(Datetime(201907010000).nextHalfyear() == Datetime(202001010000));
    CHECK(Datetime(201910110000).nextHalfyear() == Datetime(202001010000));
    CHECK(Datetime(201912310000).nextHalfyear() == Datetime(202001010000));

    /** @arg 测试 nextYear */
    CHECK(null_datetime.nextYear() == null_datetime);
    CHECK(Datetime::max().nextYear() == Datetime::max());
    CHECK(Datetime(201812310000).nextYear() == Datetime(201901010000));
    CHECK(Datetime(201901010000).nextYear() == Datetime(202001010000));
    CHECK(Datetime(201901020000).nextYear() == Datetime(202001010000));
    CHECK(Datetime(201907310000).nextYear() == Datetime(202001010000));
    CHECK(Datetime(201912310000).nextYear() == Datetime(202001010000));

    /** @arg 测试 preDay */
    CHECK(null_datetime.preDay() == null_datetime);
    CHECK(Datetime::min().preDay() == Datetime::min());
    CHECK(Datetime(201812310000).preDay() == Datetime(201812300000));
    CHECK(Datetime(201901010000).preDay() == Datetime(201812310000));
    CHECK(Datetime(201901020000).preDay() == Datetime(201901010000));

    /** @arg 测试 preWeek */
    CHECK(null_datetime.preWeek() == null_datetime);
    CHECK(Datetime::min().preWeek() == Datetime::min());
    CHECK(Datetime(201812310000).preWeek() == Datetime(201812240000));
    CHECK(Datetime(201901010000).preWeek() == Datetime(201812240000));
    CHECK(Datetime(201901020000).preWeek() == Datetime(201812240000));
    CHECK(Datetime(201901030000).preWeek() == Datetime(201812240000));
    CHECK(Datetime(201901040000).preWeek() == Datetime(201812240000));
    CHECK(Datetime(201901050000).preWeek() == Datetime(201812240000));
    CHECK(Datetime(201901060000).preWeek() == Datetime(201812240000));
    CHECK(Datetime(201901070000).preWeek() == Datetime(201812310000));

    /** @arg 测试 preMonth */
    CHECK(null_datetime.preMonth() == null_datetime);
    CHECK(Datetime::min().preMonth() == Datetime::min());
    CHECK(Datetime(201602290000).preMonth() == Datetime(201601010000));
    CHECK(Datetime(201901010000).preMonth() == Datetime(201812010000));
    CHECK(Datetime(201901020000).preMonth() == Datetime(201812010000));
    CHECK(Datetime(201901310000).preMonth() == Datetime(201812010000));
    CHECK(Datetime(201902010000).preMonth() == Datetime(201901010000));
    CHECK(Datetime(201902280000).preMonth() == Datetime(201901010000));
    CHECK(Datetime(201903010000).preMonth() == Datetime(201902010000));
    CHECK(Datetime(201903310000).preMonth() == Datetime(201902010000));
    CHECK(Datetime(201904010000).preMonth() == Datetime(201903010000));
    CHECK(Datetime(201904300000).preMonth() == Datetime(201903010000));
    CHECK(Datetime(201905300000).preMonth() == Datetime(201904010000));
    CHECK(Datetime(201912310000).preMonth() == Datetime(201911010000));
    CHECK(Datetime(201912300000).preMonth() == Datetime(201911010000));

    /** @arg 测试 preQuarter */
    CHECK(null_datetime.preQuarter() == null_datetime);
    CHECK(Datetime::min().preQuarter() == Datetime::min());
    CHECK(Datetime(201901010000).preQuarter() == Datetime(201810010000));
    CHECK(Datetime(201903310000).preQuarter() == Datetime(201810010000));
    CHECK(Datetime(201904010000).preQuarter() == Datetime(201901010000));
    CHECK(Datetime(201906300000).preQuarter() == Datetime(201901010000));
    CHECK(Datetime(201907010000).preQuarter() == Datetime(201904010000));
    CHECK(Datetime(201909300000).preQuarter() == Datetime(201904010000));
    CHECK(Datetime(201910010000).preQuarter() == Datetime(201907010000));
    CHECK(Datetime(201912310000).preQuarter() == Datetime(201907010000));

    /** @arg 测试 preHalfyear() */
    CHECK(null_datetime.preHalfyear() == null_datetime);
    CHECK(Datetime::min().preHalfyear() == Datetime::min());
    CHECK(Datetime(201901010000).preHalfyear() == Datetime(201807010000));
    CHECK(Datetime(201906300000).preHalfyear() == Datetime(201807010000));
    CHECK(Datetime(201907010000).preHalfyear() == Datetime(201901010000));
    CHECK(Datetime(201912310000).preHalfyear() == Datetime(201901010000));
    CHECK(Datetime(202001010000).preHalfyear() == Datetime(201907010000));

    /** @arg 测试 preYear */
    CHECK(null_datetime.preYear() == null_datetime);
    CHECK(Datetime::min().preYear() == Datetime::min());
    CHECK(Datetime(201901010000).preYear() == Datetime(201801010000));
    CHECK(Datetime(201912310000).preYear() == Datetime(201801010000));
    CHECK(Datetime(201601010000).preYear() == Datetime(201501010000));
    CHECK(Datetime(201612310000).preYear() == Datetime(201501010000));
}

/** @par 检测点 */
TEST_CASE("test_Datetime_to_local_time") {
    /** @arg 测试 to_local_time 默认返回 microseconds */
    Datetime dt(2024, 1, 15, 10, 30, 45, 123, 456);
    auto local_us = dt.to_local_time();
    CHECK(local_us.time_since_epoch().count() == dt.timestamp());

    /** @arg 测试 to_local_time 指定返回 seconds */
    auto local_sec = dt.to_local_time<std::chrono::seconds>();
    CHECK(local_sec.time_since_epoch().count() == dt.timestamp() / 1000000);

    /** @arg 测试 to_local_time 指定返回 milliseconds */
    auto local_ms = dt.to_local_time<std::chrono::milliseconds>();
    CHECK(local_ms.time_since_epoch().count() == dt.timestamp() / 1000);

    /** @arg 测试 to_local_time 指定返回 nanoseconds */
    auto local_ns = dt.to_local_time<std::chrono::nanoseconds>();
    CHECK(local_ns.time_since_epoch().count() == dt.timestamp() * 1000);

    /** @arg 测试 Null Datetime，返回对应的 max */
    Datetime null_dt;
    CHECK_EQ(null_dt.to_local_time(), std::chrono::local_time<std::chrono::microseconds>::max());

    /** @arg 测试 1970-01-01 之前的日期 */
    Datetime old_dt(1969, 12, 31);
    CHECK_EQ(old_dt.to_local_time(),
             std::chrono::local_time<std::chrono::microseconds>(
               std::chrono::local_days{std::chrono::year_month_day{
                 std::chrono::year(1969), std::chrono::month(12), std::chrono::day(31)}}));
}

/** @par 检测点 */
TEST_CASE("test_Datetime_fromLocalTime") {
    /** @arg 测试从 microseconds local_time 构造 */
    // 注意：local_time 表示的是本地时间（wall-clock time），不是 UTC
    // 1705312245123456 微秒 = 2024-01-15 10:30:45.123456 (本地时间)
    auto local_us = std::chrono::local_time<std::chrono::microseconds>(
      std::chrono::microseconds(1705312245123456));
    Datetime dt_us = Datetime::fromLocalTime(local_us);

    // fromTimestamp 会将 timestamp 解释为 UTC 并转换为本地时间
    // 在 UTC+8 时区，UTC 时间 2024-01-15 10:30:45 会变成本地时间 2024-01-15 18:30:45
    // 但这里我们直接用 fromTimestamp，它不做时区转换
    CHECK(dt_us.year() == 2024);
    CHECK(dt_us.month() == 1);
    CHECK(dt_us.day() == 15);
    // 实际的小时和分钟取决于 fromTimestamp 的实现
    // fromTimestamp 直接基于 1970-01-01 00:00:00 + timestamp，不涉及时区

    /** @arg 测试往返转换一致性 */
    Datetime original(2024, 6, 15, 14, 30, 22, 456, 789);
    auto converted_back = Datetime::fromLocalTime(original.to_local_time());
    CHECK(converted_back == original);

    /** @arg 测试 epoch (1970-01-01 00:00:00) */
    auto local_epoch =
      std::chrono::local_time<std::chrono::microseconds>(std::chrono::microseconds(0));
    Datetime dt_epoch = Datetime::fromLocalTime(local_epoch);
    CHECK(dt_epoch == Datetime(1970, 1, 1, 0, 0, 0, 0, 0));

    /** @arg 测试负值 */
    auto local_negative =
      std::chrono::local_time<std::chrono::microseconds>(std::chrono::microseconds(-1));
    CHECK_EQ(Datetime::fromLocalTime(local_negative), Datetime(1969, 12, 31, 23, 59, 59, 999, 999));
}

/** @par 检测点 */
TEST_CASE("test_Datetime_fromTimePointUTC") {
    /** @arg 测试从 system_clock time_point 构造（微秒精度）*/
    auto tp_us = std::chrono::system_clock::time_point(
      std::chrono::microseconds(1705312245123456));  // 2024-01-15 10:30:45.123456 UTC
    Datetime dt_us = Datetime::fromTimePointUTC(tp_us);

    // fromTimestampUTC 会将 timestamp 解释为 UTC 并转换为本地时间
    // 验证基本属性
    CHECK(!dt_us.isNull());
    CHECK(dt_us.year() >= 2024);

    /** @arg 测试从 seconds 精度的 time_point 构造 */
    auto tp_sec = std::chrono::system_clock::time_point(std::chrono::seconds(1705312245));
    Datetime dt_sec = Datetime::fromTimePointUTC(tp_sec);
    CHECK(!dt_sec.isNull());

    /** @arg 测试从 milliseconds 精度的 time_point 构造 */
    auto tp_ms = std::chrono::system_clock::time_point(std::chrono::milliseconds(1705312245123));
    Datetime dt_ms = Datetime::fromTimePointUTC(tp_ms);
    CHECK(!dt_ms.isNull());

    /** @arg 测试从 nanoseconds 精度的 time_point 构造（截断到微秒）*/
    auto tp_ns = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>(
      std::chrono::nanoseconds(1705312245123456789LL));
    Datetime dt_ns = Datetime::fromTimePointUTC(tp_ns);
    CHECK(!dt_ns.isNull());

    /** @arg 测试 epoch (1970-01-01 00:00:00 UTC) */
    auto tp_epoch = std::chrono::system_clock::time_point(std::chrono::microseconds(0));
    Datetime dt_epoch = Datetime::fromTimePointUTC(tp_epoch);
    // fromTimestampUTC 会将 UTC 时间转换为本地时间
    // 在 UTC+8 时区，epoch 的本地时间是 1970-01-01 08:00:00
    CHECK(!dt_epoch.isNull());
    CHECK(dt_epoch.year() == 1970);
    CHECK(dt_epoch.month() == 1);
    CHECK(dt_epoch.day() == 1);

    /** @arg 测试负值 */
    auto tp_negative = std::chrono::system_clock::time_point(std::chrono::microseconds(-1));
    CHECK_EQ(Datetime::fromTimePointUTC(tp_negative),
             Datetime(1970, 1, 1) - Microseconds(1) + UTCOffset());

    /** @arg 测试与 fromTimestampUTC 的等价性 */
    int64_t timestamp = 1705312245123456;
    auto tp = std::chrono::system_clock::time_point(std::chrono::microseconds(timestamp));
    Datetime dt_from_tp = Datetime::fromTimePointUTC(tp);
    Datetime dt_from_ts = Datetime::fromTimestampUTC(timestamp);
    CHECK(dt_from_tp == dt_from_ts);
}

/** @par 检测点 */
TEST_CASE("test_Datetime_chrono_duration_operator") {
    Datetime dt(2024, 1, 15, 10, 30, 45, 123, 456);

    /** @arg 测试与 chrono seconds 相加 */
    auto dt_plus_sec = dt + std::chrono::seconds(3600);  // 加1小时
    CHECK(dt_plus_sec == dt + Hours(1));

    /** @arg 测试与 chrono minutes 相加 */
    auto dt_plus_min = dt + std::chrono::minutes(30);
    CHECK(dt_plus_min == dt + Minutes(30));

    /** @arg 测试与 chrono hours 相加 */
    auto dt_plus_hour = dt + std::chrono::hours(2);
    CHECK(dt_plus_hour == dt + Hours(2));

    /** @arg 测试与 chrono milliseconds 相加 */
    auto dt_plus_ms = dt + std::chrono::milliseconds(1500);  // 1.5秒
    CHECK(dt_plus_ms == dt + Milliseconds(1500));

    /** @arg 测试与 chrono microseconds 相加 */
    auto dt_plus_us = dt + std::chrono::microseconds(2500);  // 2.5毫秒
    CHECK(dt_plus_us == dt + Microseconds(2500));

    /** @arg 测试与 chrono nanoseconds 相加（精度截断到微秒）*/
    auto dt_plus_ns = dt + std::chrono::nanoseconds(3500);  // 3.5微秒，截断为3微秒
    CHECK(dt_plus_ns == dt + Microseconds(3));

    /** @arg 测试与 chrono seconds 相减 */
    auto dt_minus_sec = dt - std::chrono::seconds(1800);  // 减30分钟
    CHECK(dt_minus_sec == dt - Minutes(30));

    /** @arg 测试与 chrono minutes 相减 */
    auto dt_minus_min = dt - std::chrono::minutes(15);
    CHECK(dt_minus_min == dt - Minutes(15));

    /** @arg 测试与 chrono hours 相减 */
    auto dt_minus_hour = dt - std::chrono::hours(1);
    CHECK(dt_minus_hour == dt - Hours(1));

    /** @arg 测试与 chrono milliseconds 相减 */
    auto dt_minus_ms = dt - std::chrono::milliseconds(500);
    CHECK(dt_minus_ms == dt - Milliseconds(500));

    /** @arg 测试与 chrono microseconds 相减 */
    auto dt_minus_us = dt - std::chrono::microseconds(1000);
    CHECK(dt_minus_us == dt - Microseconds(1000));

    /** @arg 测试与 chrono nanoseconds 相减（精度截断到微秒）*/
    auto dt_minus_ns = dt - std::chrono::nanoseconds(4500);  // 4.5微秒，截断为4微秒
    CHECK(dt_minus_ns == dt - Microseconds(4));

    /** @arg 测试 Null Datetime 运算 */
    Datetime null_dt;
    auto null_plus = null_dt + std::chrono::seconds(100);
    CHECK(null_plus.isNull());

    auto null_minus = null_dt - std::chrono::seconds(100);
    CHECK(null_minus.isNull());

    /** @arg 测试反向运算符 chrono duration + Datetime */
    auto reverse_plus = std::chrono::hours(2) + dt;
    CHECK(reverse_plus == dt + Hours(2));

    auto reverse_plus_min = std::chrono::minutes(30) + dt;
    CHECK(reverse_plus_min == dt + Minutes(30));

    auto reverse_plus_sec = std::chrono::seconds(3600) + dt;
    CHECK(reverse_plus_sec == dt + Seconds(3600));
}

/** @par 检测点 */
TEST_CASE("test_Datetime_related_operator") {
    /** @arg 小于比较 */
    CHECK(Datetime(200101010000) < Null<Datetime>());
    CHECK(Datetime(200101010000) < Datetime::max());
    CHECK(Datetime::min() < Datetime(200101010000));
    CHECK(Datetime(200101010000) < Datetime(200101020000));
}

/** @} */

/** @par 检测点 */
TEST_CASE("test_Datetime_timestamp_signed") {
    /** @arg 1970-01-01 之前的日期返回负值，不再无符号下溢（M22 回归） */
    Datetime dt(1969, 12, 31, 23, 59, 59);
    CHECK_EQ(dt.timestamp(), static_cast<int64_t>(-1000000));
    CHECK(dt.timestamp() < 0);

    Datetime dt2(1900, 1, 1);
    CHECK(dt2.timestamp() < 0);
    CHECK(dt2.timestamp() < dt.timestamp());

    // 1970-01-01 之前的日期经 fromTimestamp 往返还原
    auto roundtrip = Datetime::fromTimestamp(dt2.timestamp());
    CHECK_EQ(roundtrip, dt2);

    /** @arg 1970-01-01 纪元为 0 */
    CHECK_EQ(Datetime(1970, 1, 1).timestamp(), static_cast<int64_t>(0));

    /** @arg 1970 年后的正值保持不变 */
    Datetime dt3(2024, 1, 15, 10, 30, 45, 123, 456);
    CHECK(dt3.timestamp() > 0);
    CHECK_EQ(Datetime::fromTimestamp(dt3.timestamp()), dt3);

    /** @arg Null Datetime 映射 Null<int64_t>（INT64_MAX），不再是无符号最大值语义混淆 */
    Datetime null_dt;
    CHECK_EQ(null_dt.timestamp(), Null<int64_t>());
    CHECK_EQ(null_dt.timestampUTC(), Null<int64_t>());

    /** @arg timestampUTC 的负值与 UTC 偏移扣减语义保持（以正偏移环境下的本机行为为准） */
    CHECK_NOTHROW(Datetime(1960, 6, 1).timestampUTC());

    /** @arg 极端范围不溢出 int64 */
    CHECK_NOTHROW(Datetime::min().timestamp());
    CHECK_NOTHROW(Datetime::max().timestamp());
    CHECK(Datetime::max().timestamp() > 0);
    CHECK(Datetime::min().timestamp() < 0);
}
