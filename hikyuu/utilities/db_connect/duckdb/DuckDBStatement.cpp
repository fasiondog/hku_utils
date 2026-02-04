/*
 * DuckDBStatement.cpp
 *
 *  Copyright (c) 2024, hikyuu.org
 *
 *  Created on: 2024-01-01
 *      Author: fasiondog
 */

#include "DuckDBStatement.h"
#include "DuckDBConnect.h"

namespace hku {

DuckDBStatement::DuckDBStatement(DBConnectBase *driver, const std::string &sql_statement)
: SQLStatementBase(driver, sql_statement),
  m_connection(dynamic_cast<DuckDBConnect *>(driver)->m_connection),
  m_has_result(false),
  m_current_row(0),
  m_row_count(0) {
    memset(&m_result, 0, sizeof(duckdb_result));
    _prepare();
}

DuckDBStatement::~DuckDBStatement() {
    if (m_stmt) {
        duckdb_destroy_prepare(&m_stmt);
    }
    duckdb_destroy_result(&m_result);
}

void DuckDBStatement::_prepare() {
    duckdb_state state = duckdb_prepare(m_connection, m_sql_string.c_str(), &m_stmt);
    if (state != DuckDBSuccess) {
        const char* error_msg = duckdb_prepare_error(m_stmt);
        std::string msg = error_msg ? std::string(error_msg) : "Unknown error";
        SQL_THROW(-1, "Failed prepare sql statement: {}! error msg: {}", m_sql_string, msg);
    }
}

void DuckDBStatement::_reset() {
    duckdb_destroy_result(&m_result);
    memset(&m_result, 0, sizeof(duckdb_result));
    m_has_result = false;
    m_current_row = 0;
    m_row_count = 0;
}

void DuckDBStatement::sub_exec() {
    _reset();
    
    duckdb_state state = duckdb_execute_prepared(m_stmt, &m_result);
    if (state != DuckDBSuccess) {
        const char* error_msg = duckdb_result_error(&m_result);
        std::string msg = error_msg ? std::string(error_msg) : "Unknown error";
        SQL_THROW(-1, "Failed execute sql statement: {}! error msg: {}", m_sql_string, msg);
    }
    
    m_has_result = true;
    m_row_count = duckdb_row_count(&m_result);
    m_current_row = 0;
}

bool DuckDBStatement::sub_moveNext() {
    if (!m_has_result || m_current_row >= m_row_count) {
        return false;
    }
    
    m_current_row++;
    return m_current_row <= m_row_count;
}

int DuckDBStatement::sub_getNumColumns() const {
    if (!m_has_result) {
        return 0;
    }
    return static_cast<int>(duckdb_column_count(const_cast<duckdb_result*>(&m_result)));
}

void DuckDBStatement::sub_bindNull(int idx) {
    duckdb_state state = duckdb_bind_null(m_stmt, static_cast<idx_t>(idx + 1));
    if (state != DuckDBSuccess) {
        SQL_THROW(-1, "Failed to bind NULL at index {}", idx);
    }
}

void DuckDBStatement::sub_bindInt(int idx, int64_t value) {
    duckdb_state state = duckdb_bind_int64(m_stmt, static_cast<idx_t>(idx + 1), value);
    if (state != DuckDBSuccess) {
        SQL_THROW(-1, "Failed to bind int64 at index {}", idx);
    }
}

void DuckDBStatement::sub_bindDatetime(int idx, const Datetime &item) {
    if (item == Null<Datetime>()) {
        sub_bindNull(idx);
    } else {
        sub_bindText(idx, item.str());
    }
}

void DuckDBStatement::sub_bindText(int idx, const std::string &item) {
    duckdb_state state = duckdb_bind_varchar(m_stmt, static_cast<idx_t>(idx + 1), item.c_str());
    if (state != DuckDBSuccess) {
        SQL_THROW(-1, "Failed to bind text at index {}", idx);
    }
}

void DuckDBStatement::sub_bindText(int idx, const char *item, size_t len) {
    duckdb_state state = duckdb_bind_varchar_length(m_stmt, static_cast<idx_t>(idx + 1), item, len);
    if (state != DuckDBSuccess) {
        SQL_THROW(-1, "Failed to bind text at index {}", idx);
    }
}

void DuckDBStatement::sub_bindDouble(int idx, double item) {
    duckdb_state state = duckdb_bind_double(m_stmt, static_cast<idx_t>(idx + 1), item);
    if (state != DuckDBSuccess) {
        SQL_THROW(-1, "Failed to bind double at index {}", idx);
    }
}

void DuckDBStatement::sub_bindBlob(int idx, const std::string &item) {
    duckdb_state state = duckdb_bind_blob(m_stmt, static_cast<idx_t>(idx + 1), 
                                         item.data(), static_cast<idx_t>(item.size()));
    if (state != DuckDBSuccess) {
        SQL_THROW(-1, "Failed to bind blob at index {}", idx);
    }
}

void DuckDBStatement::sub_bindBlob(int idx, const std::vector<char> &item) {
    duckdb_state state = duckdb_bind_blob(m_stmt, static_cast<idx_t>(idx + 1), 
                                         item.data(), static_cast<idx_t>(item.size()));
    if (state != DuckDBSuccess) {
        SQL_THROW(-1, "Failed to bind blob at index {}", idx);
    }
}

void DuckDBStatement::sub_getColumnAsInt64(int idx, int64_t &item) {
    if (!m_has_result || m_current_row == 0 || m_current_row > m_row_count) {
        SQL_THROW(-1, "No valid result or invalid row position");
    }
    
    if (duckdb_value_is_null(&m_result, static_cast<idx_t>(idx), m_current_row - 1)) {
        item = 0;
        return;
    }
    
    item = duckdb_value_int64(&m_result, static_cast<idx_t>(idx), m_current_row - 1);
}

void DuckDBStatement::sub_getColumnAsDouble(int idx, double &item) {
    if (!m_has_result || m_current_row == 0 || m_current_row > m_row_count) {
        SQL_THROW(-1, "No valid result or invalid row position");
    }
    
    if (duckdb_value_is_null(&m_result, static_cast<idx_t>(idx), m_current_row - 1)) {
        item = 0.0;
        return;
    }
    
    item = duckdb_value_double(&m_result, static_cast<idx_t>(idx), m_current_row - 1);
}

void DuckDBStatement::sub_getColumnAsDatetime(int idx, Datetime &item) {
    std::string date_str;
    sub_getColumnAsText(idx, date_str);
    item = date_str.empty() ? Datetime() : Datetime(date_str);
}

void DuckDBStatement::sub_getColumnAsText(int idx, std::string &item) {
    if (!m_has_result || m_current_row == 0 || m_current_row > m_row_count) {
        SQL_THROW(-1, "No valid result or invalid row position");
    }
    
    if (duckdb_value_is_null(&m_result, static_cast<idx_t>(idx), m_current_row - 1)) {
        item = "";
        return;
    }
    
    char* value = duckdb_value_varchar(&m_result, static_cast<idx_t>(idx), m_current_row - 1);
    if (value) {
        item = std::string(value);
        free(value);
    } else {
        item = "";
    }
}

void DuckDBStatement::sub_getColumnAsBlob(int idx, std::string &item) {
    if (!m_has_result || m_current_row == 0 || m_current_row > m_row_count) {
        throw null_blob_exception();
    }
    
    if (duckdb_value_is_null(&m_result, static_cast<idx_t>(idx), m_current_row - 1)) {
        throw null_blob_exception();
    }
    
    duckdb_blob blob = duckdb_value_blob(&m_result, static_cast<idx_t>(idx), m_current_row - 1);
    if (blob.data && blob.size > 0) {
        item = std::string(static_cast<char*>(blob.data), blob.size);
        free(blob.data);
    } else {
        throw null_blob_exception();
    }
}

void DuckDBStatement::sub_getColumnAsBlob(int idx, std::vector<char> &item) {
    std::string blob_str;
    sub_getColumnAsBlob(idx, blob_str);
    item.assign(blob_str.begin(), blob_str.end());
}

uint64_t DuckDBStatement::sub_getLastRowid() {
    // DuckDB C API没有直接获取last insert rowid的函数
    // 可以通过查询系统表或其他方式获取
    return 0;
}

}  // namespace hku