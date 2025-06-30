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
#include <hikyuu/utilities/SpendTimer.h>
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

#if 1
TEST_CASE("test_tdengin") {
    Parameter param;
    param.set<std::string>("host", "192.168.5.4");
    param.set<int>("port", 6030);
    param.set<std::string>("usr", "root");
    param.set<std::string>("pwd", "taosdata");
    // param.set<std::string>("db", "day_data");
    auto con = std::make_shared<TDengineConnect>(param);
    CHECK(con->ping());

    {
        SPEND_TIME(queryInt);
        con->queryInt("select count(date) from min_data.kdata", 0);
    }

    {
        SPEND_TIME(queryInt);
        con->queryInt("select count(date) from day_data.kdata", 0);
    }

    {
        SPEND_TIME(queryInt);
        CHECK(con->tableExist("min_data.sh000001"));
    }

    {
        SPEND_TIME(query_kdata);
        struct KDataView {
            TAOS_BIND6(KDataView, day_data.sh000001, date, open, high, low, close, amount, volume)
            int64_t date;
            double open;
            double high;
            double low;
            double close;
            double amount;
            double volume;
        } krecord;

        con->load(krecord, Field("date") == Datetime(20250625).timestampUTC());
        HKU_INFO("open: {}", krecord.open);

        std::vector<KDataView> ks;
        con->batchLoad(ks, Field("date") >= Datetime(20250615).timestampUTC());
        for (auto &k : ks) {
            HKU_INFO("date: {}, open: {}", k.ts(), k.open);
        }
    }
}

TEST_CASE("test_mysql") {
    Parameter param;
    param.set<std::string>("host", "192.168.5.7");
    param.set<int>("port", 30006);
    param.set<std::string>("usr", "root");
    param.set<std::string>("pwd", "W773brqCWM!17Yt_x06z");
    // param.set<std::string>("db", "hku_data");
    auto con = std::make_shared<MySQLConnect>(param);
    CHECK(con->ping());

    // {
    //     SPEND_TIME(queryInt);
    //     con->queryInt("select count(date) from test.meters", 0);
    // }

    {
        SPEND_TIME(queryInt);
        con->queryInt("select count(date) from sh_day.000001", 0);
    }
}
#endif