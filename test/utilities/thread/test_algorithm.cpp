/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-01-25
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <hikyuu/utilities/thread/thread.h>

using namespace hku;

/**
 * @defgroup test_hikyuu_algorithm test_hikyuu_algorithm
 * @ingroup test_hikyuu_utilities
 * @{
 */

/** @par 检测点 */
TEST_CASE("test_parallelIndexRange") {
    /** @arg start=0, end=0 */
    std::vector<range_t> ranges = parallelIndexRange(0, 0);
    CHECK_UNARY(ranges.empty());

    /** @arg start=2, end=2 */
    ranges = parallelIndexRange(2, 2);
    CHECK_UNARY(ranges.empty());

    /** @arg start=1, end=0 */
    ranges = parallelIndexRange(1, 0);
    CHECK_UNARY(ranges.empty());

    /** @arg start=0, end=3 */
    ranges = parallelIndexRange(0, 3);
    CHECK_UNARY(ranges.size() > 0);

    std::vector<size_t> expect(3);
    for (size_t i = 0; i < expect.size(); ++i) {
        expect[i] = i;
    }

    std::vector<size_t> result;
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }

    /** @arg start=0, end=100 */
    ranges = parallelIndexRange(0, 100);
    CHECK_UNARY(ranges.size() > 0);

    expect.resize(100);
    for (size_t i = 0; i < expect.size(); ++i) {
        expect[i] = i;
    }

    result.clear();
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }

    /** @arg start=1, end=100 */
    ranges = parallelIndexRange(1, 100);
    CHECK_UNARY(ranges.size() > 0);

    expect.resize(99);
    for (size_t i = 0; i < expect.size(); ++i) {
        expect[i] = i + 1;
    }

    result.clear();
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }

    /** @arg start=99, end=100 */
    ranges = parallelIndexRange(99, 100);
    CHECK_UNARY(ranges.size() > 0);

    expect.resize(1);
    expect[0] = 99;

    result.clear();
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }

    /** @arg start=99, end=100 */
    ranges = parallelIndexRange(17, 50);
    CHECK_UNARY(ranges.size() > 0);

    expect.resize(50 - 17);
    for (size_t i = 0; i < expect.size(); ++i) {
        expect[i] = i + 17;
    }

    result.clear();
    for (const auto& r : ranges) {
        for (size_t i = r.first; i < r.second; ++i) {
            result.push_back(i);
        }
    }

    CHECK_EQ(result.size(), expect.size());
    for (size_t i = 0; i < expect.size(); i++) {
        CHECK_EQ(result[i], expect[i]);
    }
}

/** @} */
