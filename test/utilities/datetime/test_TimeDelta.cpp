/*
 * test_iniparser.cpp
 *
 *  Created on: 2019-12-13
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/datetime/TimeDelta.h>
#include <hikyuu/utilities/datetime/Datetime.h>
#include <hikyuu/utilities/exception.h>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace hku;

namespace bt = boost::posix_time;
namespace bd = boost::gregorian;

/**
 * @defgroup test_hikyuu_TimeDelta test_hikyuu_TimeDelta
 * @ingroup test_hikyuu_datetime_suite
 * @{
 */

/** @par 检测点 */
TEST_CASE("test_TimeDelta") {
    /** @arg days  超出限定值 */
    CHECK_THROWS(TimeDelta(99999999LL + 1));
    CHECK_THROWS(TimeDelta(-99999999LL - 1));

#if !HKU_DISABLE_ASSERT
    /** @arg hours 超出限定值 */
    CHECK_THROWS(TimeDelta(0, 100001));
    CHECK_THROWS(TimeDelta(0, -100001));

    /** @arg minutes 超出限定值 */
    CHECK_THROWS(TimeDelta(0, 0, 100001));
    CHECK_THROWS(TimeDelta(0, 0, -100001));

    /** @arg seconds 超出限定值 */
    CHECK_THROWS(TimeDelta(0, 0, 0, 8640000));
    CHECK_THROWS(TimeDelta(0, 0, 0, -8640000));

    /** @arg milliseconds 超出限定值 */
    CHECK_THROWS(TimeDelta(0, 0, 0, 0, 8640000000000));
    CHECK_THROWS(TimeDelta(0, 0, 0, 0, -8640000000000));

    /** @arg microseconds 超出限定值 */
    CHECK_THROWS(TimeDelta(0, 0, 0, 0, 0, 8640000000000));
    CHECK_THROWS(TimeDelta(0, 0, 0, 0, 0, -8640000000000));
#endif

    /** @arg microseconds总值超出限定值 */
    CHECK_THROWS(TimeDelta(99999999LL, 23, 59, 60, 999, 999));

    /** @arg 正常初始化，时分秒毫秒微秒都在各自的进制范围内 */
    TimeDelta td(7, 10, 20, 3, 5, 7);
    CHECK(td.days() == 7);
    CHECK(td.hours() == 10);
    CHECK(td.minutes() == 20);
    CHECK(td.seconds() == 3);
    CHECK(td.milliseconds() == 5);
    CHECK(td.microseconds() == 7);
    CHECK(td.isNegative() == false);

    /** @arg 正常初始化，时大于23 */
    td = TimeDelta(0, 24);
    CHECK(td.days() == 1);
    CHECK(td.hours() == 0);
    CHECK(td.minutes() == 0);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == 86400000000LL);
    CHECK(td.isNegative() == false);

    /** @arg 正常初始化，分大于59 */
    td = TimeDelta(0, 0, 60);
    CHECK(td.days() == 0);
    CHECK(td.hours() == 1);
    CHECK(td.minutes() == 0);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == 3600000000LL);
    CHECK(td.isNegative() == false);

    /** @arg 正常初始化，秒大于59 */
    td = TimeDelta(0, 0, 0, 60);
    CHECK(td.days() == 0);
    CHECK(td.hours() == 0);
    CHECK(td.minutes() == 1);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == 60000000LL);
    CHECK(td.isNegative() == false);

    /** @arg 负时长初始化, 天数为 -1 */
    td = TimeDelta(-1);
    CHECK(td.isNegative());
    CHECK(td.days() == -1);
    CHECK(td.hours() == 0);
    CHECK(td.minutes() == 0);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == -86400000000LL);

    /** @arg 负时长初始化, hours = -1 */
    td = TimeDelta(0, -1);
    CHECK(td.isNegative());
    CHECK(td.days() == -1);
    CHECK(td.hours() == 23);
    CHECK(td.minutes() == 0);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == -60 * 60 * 1000000LL);

    /** @arg 负时长初始化, minutes = -1 */
    td = TimeDelta(0, 0, -1);
    CHECK(td.isNegative());
    CHECK(td.days() == -1);
    CHECK(td.hours() == 23);
    CHECK(td.minutes() == 59);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == -60000000LL);

    /** @arg 负时长初始化, seconds = -1 */
    td = TimeDelta(0, 0, 0, -1);
    CHECK(td.isNegative());
    CHECK(td.days() == -1);
    CHECK(td.hours() == 23);
    CHECK(td.minutes() == 59);
    CHECK(td.seconds() == 59);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == -1000000LL);

    /** @arg 负时长初始化, milliseconds = -1 */
    td = TimeDelta(0, 0, 0, 0, -1);
    CHECK(td.isNegative());
    CHECK(td.days() == -1);
    CHECK(td.hours() == 23);
    CHECK(td.minutes() == 59);
    CHECK(td.seconds() == 59);
    CHECK(td.milliseconds() == 999);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == -1000LL);

    /** @arg 负时长初始化, microseconds = -1 */
    td = TimeDelta(0, 0, 0, 0, 0, -1);
    CHECK(td.isNegative());
    CHECK(td.days() == -1);
    CHECK(td.hours() == 23);
    CHECK(td.minutes() == 59);
    CHECK(td.seconds() == 59);
    CHECK(td.milliseconds() == 999);
    CHECK(td.microseconds() == 999);
    CHECK(td.ticks() == -1LL);

    /** @arg 负时长初始化, microseconds = -999 */
    td = TimeDelta(0, 0, 0, 0, 0, -999);
    CHECK(td.isNegative());
    CHECK(td.days() == -1);
    CHECK(td.hours() == 23);
    CHECK(td.minutes() == 59);
    CHECK(td.seconds() == 59);
    CHECK(td.milliseconds() == 999);
    CHECK(td.microseconds() == 1);
    CHECK(td.ticks() == -999LL);

    /** @arg 负时长初始化，所有参数均为负数 */
    td = TimeDelta(-1, -2, -13, -11, -12, -15);
    CHECK(td.isNegative());
    CHECK(td.days() == -2);
    CHECK(td.hours() == 21);
    CHECK(td.minutes() == 46);
    CHECK(td.seconds() == 48);
    CHECK(td.milliseconds() == 987);
    CHECK(td.microseconds() == 985);
    CHECK(td.ticks() == -94391012015LL);

    /** @arg 正时长初始化，参数正负混合 */
    td = TimeDelta(2, -23, -10, 4, 80, -917);
    CHECK(td.days() == 1);
    CHECK(td.hours() == 0);
    CHECK(td.minutes() == 50);
    CHECK(td.seconds() == 4);
    CHECK(td.milliseconds() == 79);
    CHECK(td.microseconds() == 83);
    CHECK(td.ticks() == 89404079083LL);
}

/** @par 检测点 */
TEST_CASE("test_TimeDelta_operator") {
    /** @arg 相加, 正正相加*/
    TimeDelta td = TimeDelta(1, 20, 100, 1, 3, 5) + TimeDelta(30, 3, 2, 4, 5, 6);
    CHECK(td.isNegative() == false);
    CHECK(td.days() == 32);
    CHECK(td.hours() == 0);
    CHECK(td.minutes() == 42);
    CHECK(td.seconds() == 5);
    CHECK(td.milliseconds() == 8);
    CHECK(td.microseconds() == 11);
    CHECK(td.ticks() == 2767325008011LL);

    /** @arg 相加，和为 0 时长 */
    td = TimeDelta(1) + TimeDelta(-1);
    CHECK(td.isNegative() == false);
    CHECK(td.days() == 0);
    CHECK(td.hours() == 0);
    CHECK(td.minutes() == 0);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == 0);

    /** @arg 相加，和为负 */
    td = TimeDelta(-1) + TimeDelta(0, -1);
    CHECK(td.isNegative() == true);
    CHECK(td.days() == -2);
    CHECK(td.hours() == 23);
    CHECK(td.minutes() == 0);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == -90000000000LL);

    /** @arg + 号 */
    CHECK(+TimeDelta(1) == TimeDelta(1));
    CHECK(+TimeDelta(-1) == TimeDelta(-1));

    /** @arg -号 */
    CHECK(-TimeDelta(1) == TimeDelta(-1));
    CHECK(-TimeDelta(-1) == TimeDelta(1));

    /** @arg 相减，结果为0 */
    td = TimeDelta(1, 0, 1) - TimeDelta(0, 24, 0, 60);
    CHECK(td.isNegative() == false);
    CHECK(td.days() == 0);
    CHECK(td.hours() == 0);
    CHECK(td.minutes() == 0);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == 0);

    /** @arg 相减，结果为负 */
    td = TimeDelta(0, 0, 1) - TimeDelta(0, 1);
    CHECK(td.isNegative() == true);
    CHECK(td.days() == -1);
    CHECK(td.hours() == 23);
    CHECK(td.minutes() == 1);
    CHECK(td.seconds() == 0);
    CHECK(td.milliseconds() == 0);
    CHECK(td.microseconds() == 0);
    CHECK(td.ticks() == -3540000000LL);

    /** @arg 求绝对值 */
    td = TimeDelta() - Microseconds(1);
    CHECK(td == Microseconds(-1));
    CHECK(td.abs() == Microseconds(1));

    /** @arg 乘以0 */
    td = TimeDelta(1, 1, 3) * 0;
    CHECK(td == TimeDelta());

    /** @arg 乘以大于0的整数 */
    td = TimeDelta(1, 1, 3) * 2;
    CHECK(td == TimeDelta(2, 2, 6));

    /** @arg 乘以大于0的小数 */
    td = TimeDelta(2, 2, 6) * 0.5;
    CHECK(td == TimeDelta(1, 1, 3));

    /** @arg 乘以小于0的整数 */
    td = TimeDelta(1, 1, 3) * -2;
    CHECK(td == TimeDelta(-2, -2, -6));

    /** @arg 乘以小于0的小数 */
    td = TimeDelta(-2, -2, -6) * 0.5;
    CHECK(td == TimeDelta(-1, -1, -3));

    /** @arg 正 TimeDelat 乘以负数 */
    td = TimeDelta(1) * -2;
    CHECK(td == TimeDelta(-2));

    /** @arg 负 TimeDelat 乘以负数 */
    td = TimeDelta(-1) * -2;
    CHECK(td == TimeDelta(2));

    /** @arg 除以 0 */
    CHECK_THROWS_AS(TimeDelta(1) / 0, hku::exception);

    /** @arg 正常除法 */
    CHECK(TimeDelta(2) / 2 == TimeDelta(1));
    CHECK(TimeDelta(2) / TimeDelta(1) == 2);
    CHECK(Microseconds(1) / 3 == TimeDelta(0));
    CHECK(Microseconds(2) / 3 == Microseconds(1));

    /** @arg 地板除 */
    CHECK_THROWS_AS(TimeDelta(1).floorDiv(0), hku::exception);
    CHECK(TimeDelta(2).floorDiv(2) == TimeDelta(1));
    CHECK(Microseconds(1).floorDiv(3) == TimeDelta(0));
    CHECK(Microseconds(2).floorDiv(3) == TimeDelta(0));

    /** @arg 除以 zero TimeDelta */
    CHECK_THROWS_AS(TimeDelta(1) / TimeDelta(), hku::exception);

    /** @arg 对零时长取余 */
    CHECK_THROWS_AS(TimeDelta(1) % TimeDelta(), hku::exception);

    /** @arg 取余 */
    CHECK(TimeDelta(3) % TimeDelta(2) == TimeDelta(1));

    /** @arg 相等 */
    CHECK(TimeDelta(1, 2, 1, 1, 1, 1) == TimeDelta(1, 2, 1, 1, 1, 1));
    CHECK(TimeDelta(1) == TimeDelta(0, 24));
    CHECK(TimeDelta(-1) == TimeDelta(0, -24));

    /** @arg 不等 */
    CHECK(TimeDelta(1, 2) != TimeDelta(1));

    /** @arg >, <, >=, <= */
    CHECK(TimeDelta(1) > TimeDelta(0, 23, 59));
    CHECK(TimeDelta(1) >= TimeDelta(0, 23));
    CHECK(TimeDelta(0, 0, 0, 0, 0, 1) > TimeDelta());
    CHECK(TimeDelta(0, 0, 0, 0, 0, 1) > TimeDelta(0, 0, 0, 0, 0, -1));
    CHECK(TimeDelta(1) < TimeDelta(2));
    CHECK(TimeDelta(2) <= TimeDelta(2));

    /** @arg total_days */
    CHECK(TimeDelta(1, 12).total_days() == 1.5);

    /** @arg total_hours */
    CHECK(TimeDelta(1, 12, 30).total_hours() == 36.5);

    /** @arg total_minutes */
    CHECK(TimeDelta(1, 12, 30, 30).total_minutes() == (24 + 12) * 60 + 30 + 0.5);

    /** @arg total_seconds */
    CHECK(TimeDelta(0, 0, 0, 1, 500).total_seconds() == 1.5);

    /** @arg total_milliseconds */
    CHECK(TimeDelta(0, 0, 0, 0, 1, 500).total_milliseconds() == 1.5);
}

/** @par 检测点 */
TEST_CASE("test_TimeDelta_subclass") {
    /** @arg Days */
    CHECK(Days(1) == TimeDelta(1));
    CHECK(TimeDelta(1) == Days(1));

    /** @arg Hours */
    CHECK(Hours(1) == TimeDelta(0, 1));
    CHECK(TimeDelta(0, 1) == Hours(1));

    /** @arg Minutes */
    CHECK(Minutes(1) == TimeDelta(0, 0, 1));
    CHECK(TimeDelta(0, 0, 1) == Minutes(1));

    /** @arg Seconds */
    CHECK(Seconds(1) == TimeDelta(0, 0, 0, 1));
    CHECK(TimeDelta(0, 0, 0, 1) == Seconds(1));

    /** @arg Milliseconds */
    CHECK(Milliseconds(1) == TimeDelta(0, 0, 0, 0, 1));
    CHECK(TimeDelta(0, 0, 0, 0, 1) == Milliseconds(1));

    /** @arg Microseconds */
    CHECK(Microseconds(1) == TimeDelta(0, 0, 0, 0, 0, 1));
    CHECK(TimeDelta(0, 0, 0, 0, 0, 1) == Microseconds(1));
}

/** @par 检测点 */
TEST_CASE("test_TimeDelta_chrono_constructor") {
    using namespace std::chrono;

    /** @arg 从 hours 构造 */
    TimeDelta td1(hours(2));
    CHECK(td1 == Hours(2));
    CHECK(td1.hours() == 2);
    CHECK(td1.days() == 0);

    /** @arg 从 minutes 构造 */
    TimeDelta td2(minutes(30));
    CHECK(td2 == Minutes(30));
    CHECK(td2.minutes() == 30);

    /** @arg 从 seconds 构造 */
    TimeDelta td3(seconds(45));
    CHECK(td3 == Seconds(45));
    CHECK(td3.seconds() == 45);

    /** @arg 从 milliseconds 构造 */
    TimeDelta td4(milliseconds(500));
    CHECK(td4 == Milliseconds(500));
    CHECK(td4.milliseconds() == 500);

    /** @arg 从 microseconds 构造 */
    TimeDelta td5(microseconds(123456));
    CHECK(td5 == Microseconds(123456));
    CHECK(td5.microseconds() == 456);
    CHECK(td5.milliseconds() == 123);

    /** @arg 从组合 duration 构造 */
    auto combined = hours(1) + minutes(30) + seconds(45);
    TimeDelta td6(combined);
    CHECK(td6 == TimeDelta(0, 1, 30, 45));
    CHECK(td6.hours() == 1);
    CHECK(td6.minutes() == 30);
    CHECK(td6.seconds() == 45);

    /** @arg 从负 duration 构造 */
    TimeDelta td7(hours(-2));
    CHECK(td7 == Hours(-2));
    CHECK(td7.isNegative());
    CHECK(td7.days() == -1);
    CHECK(td7.hours() == 22);

    /** @arg 从零 duration 构造 */
    TimeDelta td8(seconds(0));
    CHECK(td8 == TimeDelta());
    CHECK(td8.ticks() == 0);

    /** @arg 超出范围的 duration 应抛出异常 */
    // hours(100000) 约等于 3.6e14 微秒，在范围内
    // 需要更大的值才会超出，例如 days(100000000) 会超出
    CHECK_THROWS_AS(TimeDelta(days(100000000)), std::out_of_range);
    CHECK_THROWS_AS(TimeDelta(days(-100000000)), std::out_of_range);

    /** @arg 精度测试 - nanoseconds 会被截断为微秒 */
    TimeDelta td9(nanoseconds(1500));  // 1500纳秒 = 1.5微秒，截断为1微秒
    CHECK(td9 == Microseconds(1));

    TimeDelta td10(nanoseconds(999));  // 999纳秒 < 1微秒，截断为0
    CHECK(td10 == TimeDelta());

    /** @arg duration() 方法测试 - 默认返回 microseconds */
    TimeDelta td11(hours(2) + minutes(30));
    auto dur_us = td11.duration();
    CHECK(dur_us == std::chrono::microseconds(9000000000LL));

    /** @arg duration() 方法测试 - 指定返回 seconds */
    auto dur_sec = td11.duration<std::chrono::seconds>();
    CHECK(dur_sec == std::chrono::seconds(9000));

    /** @arg duration() 方法测试 - 指定返回 milliseconds */
    auto dur_ms = td11.duration<std::chrono::milliseconds>();
    CHECK(dur_ms == std::chrono::milliseconds(9000000));

    /** @arg duration() 方法测试 - 指定返回 nanoseconds */
    auto dur_ns = td11.duration<std::chrono::nanoseconds>();
    CHECK(dur_ns == std::chrono::nanoseconds(9000000000000LL));
}

/** @par 检测点 */
TEST_CASE("test_TimeDelta_Datetime_operator") {
    /** @arg Datetime + TimeDelta */
    Datetime d = Datetime(2019, 12, 18) + TimeDelta(1);
    CHECK(d == Datetime(2019, 12, 19));

    d = Datetime(2019, 12, 31, 23, 59, 59, 999, 999) + Microseconds(1);
    CHECK(d == Datetime(2020, 1, 1));

    d = Datetime(2019, 12, 18) + TimeDelta(-1);
    CHECK(d == Datetime(2019, 12, 17));

    /** @arg TimeDelta + Datetime*/
    d = TimeDelta(1) + Datetime(2019, 12, 18);
    CHECK(d == Datetime(2019, 12, 19));

    /** @arg Datetime - TimeDelta */
    d = Datetime(2019, 12, 18) - TimeDelta(0, 0, 1);
    CHECK(d == Datetime(2019, 12, 17, 23, 59));

    d = Datetime(2019, 12, 18) - TimeDelta(-2);
    CHECK(d == Datetime(2019, 12, 20));
}

/** @par 检测点 */
TEST_CASE("test_TimeDelta_chrono_duration_operator") {
    TimeDelta td(1, 2, 3, 4, 5, 6);  // 1天2小时3分4秒5毫秒6微秒

    /** @arg 测试从 chrono duration 构造 */
    TimeDelta td_hours(std::chrono::hours(2));
    CHECK(td_hours == Hours(2));

    TimeDelta td_minutes(std::chrono::minutes(30));
    CHECK(td_minutes == Minutes(30));

    TimeDelta td_seconds(std::chrono::seconds(1800));
    CHECK(td_seconds == Seconds(1800));

    TimeDelta td_ms(std::chrono::milliseconds(1500));
    CHECK(td_ms == Milliseconds(1500));

    TimeDelta td_us(std::chrono::microseconds(2500));
    CHECK(td_us == Microseconds(2500));

    /** @arg 测试与 chrono duration 相加 */
    auto td_plus_hours = td + std::chrono::hours(1);
    CHECK(td_plus_hours == td + Hours(1));

    auto td_plus_minutes = td + std::chrono::minutes(30);
    CHECK(td_plus_minutes == td + Minutes(30));

    auto td_plus_seconds = td + std::chrono::seconds(1800);
    CHECK(td_plus_seconds == td + Seconds(1800));

    auto td_plus_ms = td + std::chrono::milliseconds(1500);
    CHECK(td_plus_ms == td + Milliseconds(1500));

    auto td_plus_us = td + std::chrono::microseconds(2500);
    CHECK(td_plus_us == td + Microseconds(2500));

    /** @arg 测试与 chrono duration 相减 */
    auto td_minus_hours = td - std::chrono::hours(1);
    CHECK(td_minus_hours == td - Hours(1));

    auto td_minus_minutes = td - std::chrono::minutes(30);
    CHECK(td_minus_minutes == td - Minutes(30));

    auto td_minus_seconds = td - std::chrono::seconds(1800);
    CHECK(td_minus_seconds == td - Seconds(1800));

    auto td_minus_ms = td - std::chrono::milliseconds(1500);
    CHECK(td_minus_ms == td - Milliseconds(1500));

    auto td_minus_us = td - std::chrono::microseconds(2500);
    CHECK(td_minus_us == td - Microseconds(2500));

    /** @arg 测试反向运算符 chrono duration + TimeDelta */
    auto reverse_plus = std::chrono::hours(2) + td;
    CHECK(reverse_plus == td + Hours(2));

    auto reverse_plus_min = std::chrono::minutes(30) + td;
    CHECK(reverse_plus_min == td + Minutes(30));

    /** @arg 测试反向运算符 chrono duration - TimeDelta */
    auto reverse_minus = std::chrono::hours(5) - td;
    auto expected_minus = TimeDelta::fromTicks(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::hours(5)).count() -
      td.ticks());
    CHECK(reverse_minus == expected_minus);

    /** @arg 测试反向比较运算符 */
    TimeDelta td_1hour_rev(0, 1);  // 用于反向比较测试

    // chrono duration == TimeDelta
    CHECK(std::chrono::hours(1) == td_1hour_rev);
    CHECK(!(std::chrono::hours(2) == td_1hour_rev));

    // chrono duration != TimeDelta
    CHECK(std::chrono::hours(2) != td_1hour_rev);
    CHECK(!(std::chrono::hours(1) != td_1hour_rev));

    // chrono duration < TimeDelta
    CHECK(std::chrono::minutes(30) < td_1hour_rev);
    CHECK(!(std::chrono::hours(2) < td_1hour_rev));

    // chrono duration > TimeDelta
    CHECK(std::chrono::hours(2) > td_1hour_rev);
    CHECK(!(std::chrono::minutes(30) > td_1hour_rev));

    // chrono duration <= TimeDelta
    CHECK(std::chrono::minutes(30) <= td_1hour_rev);
    CHECK(std::chrono::hours(1) <= td_1hour_rev);
    CHECK(!(std::chrono::hours(2) <= td_1hour_rev));

    // chrono duration >= TimeDelta
    CHECK(std::chrono::hours(2) >= td_1hour_rev);
    CHECK(std::chrono::hours(1) >= td_1hour_rev);
    CHECK(!(std::chrono::minutes(30) >= td_1hour_rev));

    /** @arg 测试与 chrono duration 比较运算（TimeDelta 在左边）*/
    TimeDelta td_1hour(0, 1);
    CHECK(td_1hour == std::chrono::hours(1));
    CHECK(td_1hour != std::chrono::hours(2));
    CHECK(td_1hour < std::chrono::hours(2));
    CHECK(td_1hour > std::chrono::minutes(30));
    CHECK(td_1hour >= std::chrono::hours(1));
    CHECK(td_1hour <= std::chrono::hours(1));

    /** @arg 测试边界值 */
    TimeDelta td_zero;
    CHECK(td_zero == std::chrono::seconds(0));
    CHECK(td_zero < std::chrono::seconds(1));
    CHECK(td_zero > std::chrono::seconds(-1));

    /** @arg 测试负值 */
    TimeDelta td_negative(-1);
    CHECK(td_negative < std::chrono::seconds(0));
    CHECK(td_negative > std::chrono::hours(-25));

    /** @arg 测试超出范围的异常 */
    TimeDelta td_max = TimeDelta::max();
    CHECK_THROWS(td_max + std::chrono::hours(1));

    TimeDelta td_min = TimeDelta::min();
    CHECK_THROWS(td_min - std::chrono::hours(1));
}

/** @par 检测点 */
TEST_CASE("test_TimeDelta_string_constructor") {
    /** @arg normal round-trip of integer seconds */
    CHECK(TimeDelta("0 days, 00:00:01").ticks() == TimeDelta(0, 0, 0, 1).ticks());
    CHECK(TimeDelta("1 days, 00:00:00").ticks() == TimeDelta(1).ticks());
    CHECK(TimeDelta("7 days, 10:20:03").ticks() == TimeDelta(7, 10, 20, 3).ticks());

    /** @arg sub-second part rounded to nearest microsecond instead of truncated, so
     * "59.999999" yields 59999999us rather than 59999998us */
    CHECK(TimeDelta("0 days, 00:00:59.999999").ticks() == 59999999LL);
    CHECK(TimeDelta("0 days, 00:00:00.000001").ticks() == 1LL);
    CHECK(TimeDelta("0 days, 00:00:00.500000").ticks() == 500000LL);
    CHECK(TimeDelta("0 days, 00:00:59.000000").ticks() == 59000000LL);

    /** @arg negative duration parsed correctly */
    CHECK(TimeDelta("-1 days, 23:00:00").ticks() == TimeDelta(0, -1).ticks());
    CHECK(TimeDelta("-1 days, 23:59:59.999999").ticks() == TimeDelta(0, 0, 0, 0, 0, -1).ticks());

    /** @arg invalid format throws */
    CHECK_THROWS(TimeDelta("abc"));
    CHECK_THROWS(TimeDelta("1:2"));
    CHECK_THROWS(TimeDelta("0 days, 00:00"));
}

/** @} */