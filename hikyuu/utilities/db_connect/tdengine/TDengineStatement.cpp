/*
 *  Copyright (c) 2025 hikyuu.org
 *
 *  Created on: 2025-06-04
 *      Author: fasiondog
 */

#include "TDengineConnect.h"
#include "TDengineStatement.h"

namespace hku {

TDengineStatement::TDengineStatement(DBConnectBase *driver, const std::string &sql_statement)
: SQLStatementBase(driver, sql_statement) {
    const TDengineConnect *connect = dynamic_cast<TDengineConnect *>(driver);
    HKU_CHECK(connect, "Failed create statement: {}! Failed dynamic_cast<TDengineConnect*>!",
              sql_statement);

    m_taos = connect->getRawTAOS();
    _prepare(driver);

    int param_count = 0;
    int ret = taos_stmt_num_params(m_stmt, &param_count);
    HKU_CHECK(ret == 0, "Failed taos_stmt_num_params! errcode: 0x{:x}, sql: {}", ret,
              sql_statement);
    if (param_count > 0) {
        m_param_bind.resize(param_count);
        memset(m_param_bind.data(), 0, param_count * sizeof(TAOS_MULTI_BIND));
    }
}

TDengineStatement::~TDengineStatement() {
    taos_stmt_close(m_stmt);
}

void TDengineStatement::_prepare(DBConnectBase *driver) {
    m_stmt = taos_stmt_init(m_taos);
    HKU_CHECK(m_stmt, "Failed taos_stmt_init! SQL: {}", m_sql_string);

    int code = taos_stmt_prepare(m_stmt, m_sql_string.c_str(), 0);
    HKU_IF_RETURN(code == 0, void());

    taos_stmt_close(m_stmt);

    // 尝试重连
    TDengineConnect *connect = dynamic_cast<TDengineConnect *>(driver);
    connect->reconnect();

    m_stmt = taos_stmt_init(m_taos);
    HKU_CHECK(m_stmt, "Failed taos_stmt_init! SQL: {}", m_sql_string);

    code = taos_stmt_prepare(m_stmt, m_sql_string.c_str(), 0);
    HKU_IF_RETURN(code == 0, void());

    std::string stmt_errorstr(taos_stmt_errstr(m_stmt));
    taos_stmt_close(m_stmt);
    m_stmt = nullptr;

    HKU_THROW("Failed prepare statement: {}! errcode: 0x{:x}, error msg: {}!", m_sql_string, code,
              stmt_errorstr);
}

}  // namespace hku