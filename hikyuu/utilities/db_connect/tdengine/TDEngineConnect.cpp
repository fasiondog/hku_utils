/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-06-04
 *      Author: fasiondog
 */

#include "TDengineConnect.h"

namespace hku {

TDengineConnect::TDengineConnect(const Parameter& param) : DBConnectBase(param) {
    close();
    connect();
}

TDengineConnect::~TDengineConnect() {
    close();
}

void TDengineConnect::close() {
    if (m_taos) {
        taos_close(m_taos);
        m_taos = nullptr;
    }
}

void TDengineConnect::connect() {
    try {
        std::string host = tryGetParam<std::string>("host", "localhost");
        std::string usr = tryGetParam<std::string>("usr", "root");
        std::string pwd = tryGetParam<std::string>("pwd", "taosdata");
        std::string database = tryGetParam<std::string>("db", "");
        uint16_t port = tryGetParam<int>("port", 6030);

        m_taos = taos_connect(host.c_str(), usr.c_str(), pwd.c_str(), database.c_str(), port);
        HKU_CHECK(m_taos, "Failed to connect to {}:{}, ErrCode: 0x{:x}, ErrMessage: {}.", host,
                  port, taos_errno(nullptr), taos_errstr(nullptr));

    } catch (const hku::exception& e) {
        close();
        HKU_ERROR(e.what());
        HKU_THROW("Failed create TDengineConnect! {}", e.what());

    } catch (const std::exception& e) {
        close();
        HKU_ERROR(e.what());
        HKU_THROW("Failed create TDengineConnect instance! {}", e.what());

    } catch (...) {
        close();
        const char* errmsg = "Failed create TDengineConnect instance! Unknown error";
        HKU_ERROR(errmsg);
        HKU_THROW("{}", errmsg);
    }
}

int64_t TDengineConnect::exec(const std::string& sql_string) {
#if HKU_SQL_TRACE
    HKU_DEBUG(sql_string);
#endif
    TAOS_RES* result = taos_query(m_taos, sql_string.c_str());
    int code = taos_errno(result);
    SQL_CHECK(code == 0, code, "SQL error: {}! ErrCode: 0x{:x}, ErrMessage: {}", sql_string, code,
              taos_errstr(result));

    int rows = taos_affected_rows(result);
    taos_free_result(result);
    return rows;
}

void TDengineConnect::reconnect() noexcept {
    close();
    connect();
}

}  // namespace hku