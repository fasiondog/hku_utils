/*
 *  Copyright (c) hikyuu.org
 *
 *  Created on: 2026-10-6
 *      Author: fasiondog
 *
 */

#include <doctest/doctest.h>
#include <set>
#include <hikyuu/utilities/snowflake.h>

using namespace hku;

TEST_CASE("test_snowflake_normal") {
    /** Normal generation: positive unique ids, non-decreasing over time */
    snowflake<1288834974657> gen;  // the Twitter epoch
    gen.init(3, 5);

    std::set<int64_t> ids;
    int64_t prev = 0;
    for (int i = 0; i < 10000; ++i) {
        auto id = gen.nextid();
        CHECK_UNARY(id > 0);
        CHECK_UNARY(id >= prev);  // ids grow monotonically within one instance
        CHECK_UNARY(ids.insert(id).second);
        prev = id;
    }
}

TEST_CASE("test_snowflake_init_invalid") {
    /** Invalid worker/datacenter ids are rejected */
    snowflake<1288834974657> gen;
    CHECK_THROWS(gen.init(32, 0));
    CHECK_THROWS(gen.init(-1, 0));
    CHECK_THROWS(gen.init(0, 32));
    CHECK_THROWS(gen.init(0, -1));
    gen.init(31, 31);  // boundary values are accepted
    CHECK_UNARY(gen.nextid() > 0);
}

TEST_CASE("test_snowflake_clock_before_twepoch") {
    // The system clock is earlier than Twepoch: the delta is negative. The old code left-shifted
    // a negative signed value, which is undefined behavior; the fixed version shifts in unsigned
    // arithmetic (yielding wrapped negative ids) and must not invoke UB
    snowflake<4102444800000> gen;  // 2100-01-01, far in the future

    std::set<int64_t> ids;
    for (int i = 0; i < 1000; ++i) {
        auto id = gen.nextid();
        CHECK_UNARY(ids.insert(id).second);  // ids stay unique even when wrapped
    }
}
