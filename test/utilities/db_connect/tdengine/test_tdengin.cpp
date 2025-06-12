/*
 * test_mysql.cpp
 *
 *  Created on: 2020-9-26
 *      Author: fasiondog
 */

#include "doctest/doctest.h"
#include <string>
#include <vector>
#include <hikyuu/utilities/ResourcePool.h>
#include <hikyuu/utilities/Null.h>
#include <hikyuu/utilities/os.h>
#include <hikyuu/utilities/db_connect/DBConnect.h>
#include <hikyuu/utilities/db_connect/tdengine/TDEngineDll.h>

using namespace hku;

#if 0

TEST_CASE("test_tdengin") {
    Parameter param;
    param.set<std::string>("host", "192.168.5.4");
    param.set<int>("port", 6030);
    param.set<std::string>("usr", "root");
    param.set<std::string>("pwd", "taosdata");
    param.set<std::string>("db", "hku_data");
    auto con = std::make_shared<TDengineConnect>(param);
    CHECK(con->ping());

    struct Day {
        TABLE_BIND6(Day, sh_min_000001, open, high, low, close, volume, amount)
        double open;
        double high;
        double low;
        double close;
        double volume;
        double amount;
    } day_data;

    con->load(day_data, Field("id") == (Datetime(1990, 12, 20) - Hours(8)).timestamp());
    HKU_INFO("id: {}, date: {}, open: {}, high: {}, low: {}, close: {}, volume: {}, amount: {}",
             day_data.id(), Datetime::fromTimestamp(day_data.id()) + Hours(8), day_data.open,
             day_data.high, day_data.low, day_data.close, day_data.volume, day_data.amount);

    std::vector<Day> days;
    con->batchLoad(days, DBCondition("1=1") + LIMIT(10));
    HKU_INFO("days size: {}", days.size());
    for (size_t i = 0; i < 10; i++) {
        HKU_INFO("id: {}, date: {}, open: {}, high: {}, low: {}, close: {}, volume: {}, amount: {}",
                 days[i].id(), Datetime::fromTimestamp(days[i].id()) + Hours(8), days[i].open,
                 days[i].high, days[i].low, days[i].close, days[i].volume, days[i].amount);
    }

    std::string info = TDEngineDll::taos_get_client_info();
    HKU_INFO(info);
}

#endif