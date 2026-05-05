/*
 * MySQLStatement.cpp
 *
 *  Copyright (c) 2019, hikyuu.org
 *
 *  Created on: 2019-8-17
 *      Author: fasiondog
 */

#include <vector>
#include <chrono>
#include <boost/mysql/date.hpp>
#include <boost/mysql/datetime.hpp>
#include <boost/mysql/time.hpp>
#include "hikyuu/utilities/Log.h"
#include "MySQLStatement.h"
#include "MySQLConnect.h"

namespace hku {

MySQLStatement::MySQLStatement(DBConnectBase* driver, const std::string& sql_statement)
: SQLStatementBase(driver, sql_statement),
  m_conn(nullptr),
  m_has_result(false),
  m_current_row(0) {
    MySQLConnect* connect = dynamic_cast<MySQLConnect*>(driver);
    HKU_CHECK(connect, "Failed create statement: {}! Failed dynamic_cast<MySQLConnect*>!",
              sql_statement);

    m_conn = connect->getRawConnection();
    _prepare();
}

MySQLStatement::~MySQLStatement() {
    // boost.mysql 的 statement 会自动清理
}

void MySQLStatement::_prepare() {
    try {
        boost::mysql::error_code ec;
        boost::mysql::diagnostics diag;
        m_stmt = m_conn->prepare_statement(m_sql_string, ec, diag);
        
        if (ec) {
            // 如果是连接丢失错误，尝试重连
            // boost.mysql 的错误码需要检查具体值
            if (m_conn) {
                MySQLConnect* connect = dynamic_cast<MySQLConnect*>(m_driver);
                if (connect && connect->ping()) {
                    // 重连成功，重新获取连接并再次准备
                    m_conn = connect->getRawConnection();
                    m_stmt = m_conn->prepare_statement(m_sql_string, ec, diag);
                    
                    if (ec) {
                        HKU_THROW("Failed prepare statement after reconnect: {}! Error: {}", 
                                  m_sql_string, ec.message());
                    }
                    return;
                }
            }
            
            HKU_THROW("Failed prepare statement: {}! Error: {}", m_sql_string, ec.message());
        }
    } catch (const hku::exception&) {
        throw;
    } catch (const std::exception& e) {
        HKU_THROW("Failed prepare statement: {}! {}", m_sql_string, e.what());
    } catch (...) {
        HKU_THROW("Failed prepare statement: {}! Unknown error!", m_sql_string);
    }
}

void MySQLStatement::_reset() {
    m_params.clear();
    m_has_result = false;
    m_current_row = 0;
}

void MySQLStatement::sub_exec() {
    // 不要在这里调用 _reset，因为会清空参数
    // _reset 应该在获取结果后或重新绑定前调用
    
    if (!m_conn) {
        MySQLConnect* connect = dynamic_cast<MySQLConnect*>(m_driver);
        HKU_CHECK(connect, "Invalid driver type!");
        m_conn = connect->getRawConnection();
    }
    
    // 如果语句未准备，先准备
    if (!m_stmt.valid()) {
        _prepare();
    }
    
    boost::mysql::error_code ec;
    boost::mysql::diagnostics diag;
    
    if (m_params.empty()) {
        // 没有参数，直接执行
        m_conn->execute(m_sql_string, m_results, ec, diag);
    } else {
        // 有参数，使用预处理语句
        // 由于 bind 返回的类型依赖于参数数量，我们需要用 auto
        if (m_params.size() == 1) {
            auto bound = m_stmt.bind(m_params[0]);
            m_conn->execute(bound, m_results, ec, diag);
        } else if (m_params.size() == 2) {
            auto bound = m_stmt.bind(m_params[0], m_params[1]);
            m_conn->execute(bound, m_results, ec, diag);
        } else if (m_params.size() == 3) {
            auto bound = m_stmt.bind(m_params[0], m_params[1], m_params[2]);
            m_conn->execute(bound, m_results, ec, diag);
        } else {
            // 更多参数时，使用 field_view 迭代器
            std::vector<boost::mysql::field_view> param_views;
            param_views.reserve(m_params.size());
            for (const auto& f : m_params) {
                param_views.push_back(boost::mysql::field_view(f));
            }
            auto bound = m_stmt.bind(param_views.begin(), param_views.end());
            m_conn->execute(bound, m_results, ec, diag);
        }
    }
    
    if (ec) {
        // 执行失败，尝试重连后再次执行
        MySQLConnect* connect = dynamic_cast<MySQLConnect*>(m_driver);
        if (connect && connect->ping()) {
            // 重连成功，需要重新准备语句
            m_conn = connect->getRawConnection();
            
            // 重新准备语句
            try {
                boost::mysql::diagnostics diag2;
                m_stmt = m_conn->prepare_statement(m_sql_string, ec, diag2);
                if (ec) {
                    SQL_THROW(ec.value(), "Failed to re-prepare statement after reconnect: {}! Error: {}", 
                              m_sql_string, ec.message());
                }
                
                // 重新执行
                if (m_params.empty()) {
                    m_conn->execute(m_sql_string, m_results, ec, diag);
                } else {
                    if (m_params.size() == 1) {
                        auto bound = m_stmt.bind(m_params[0]);
                        m_conn->execute(bound, m_results, ec, diag);
                    } else if (m_params.size() == 2) {
                        auto bound = m_stmt.bind(m_params[0], m_params[1]);
                        m_conn->execute(bound, m_results, ec, diag);
                    } else if (m_params.size() == 3) {
                        auto bound = m_stmt.bind(m_params[0], m_params[1], m_params[2]);
                        m_conn->execute(bound, m_results, ec, diag);
                    } else {
                        std::vector<boost::mysql::field_view> param_views;
                        param_views.reserve(m_params.size());
                        for (const auto& f : m_params) {
                            param_views.push_back(boost::mysql::field_view(f));
                        }
                        auto bound = m_stmt.bind(param_views.begin(), param_views.end());
                        m_conn->execute(bound, m_results, ec, diag);
                    }
                }
            } catch (...) {
                SQL_THROW(-1, "Failed to re-prepare and execute statement after reconnect: {}", m_sql_string);
            }
        }
        
        if (ec) {
            SQL_THROW(ec.value(), "Failed execute statement: {}! Error: {}", 
                      m_sql_string, ec.message());
        }
    }
    
    m_has_result = true;
}

bool MySQLStatement::sub_moveNext() {
    if (!m_has_result) {
        return false;
    }
    
    const auto& rows = m_results.rows();
    // m_current_row 从 0 开始，表示还没有读取任何行
    // 第一次调用时，应该移动到第 1 行（索引 0）
    if (m_current_row >= rows.size()) {
        return false;
    }
    
    m_current_row++;
    return true;
}

void MySQLStatement::sub_bindNull(int idx) {
    HKU_CHECK(idx == static_cast<int>(m_params.size()), 
              "Parameter index must be sequential! Expected index: {}, but got: {}", 
              m_params.size(), idx);
    m_params.push_back(boost::mysql::field());  // 默认构造为 NULL
}

void MySQLStatement::sub_bindInt(int idx, int64_t value) {
    HKU_CHECK(idx == static_cast<int>(m_params.size()), 
              "Parameter index must be sequential! Expected index: {}, but got: {}", 
              m_params.size(), idx);
    m_params.push_back(boost::mysql::field(static_cast<std::int64_t>(value)));
}

void MySQLStatement::sub_bindDouble(int idx, double item) {
    HKU_CHECK(idx == static_cast<int>(m_params.size()), 
              "Parameter index must be sequential! Expected index: {}, but got: {}", 
              m_params.size(), idx);
    m_params.push_back(boost::mysql::field(item));
}

void MySQLStatement::sub_bindDatetime(int idx, const Datetime& item) {
    if (item == Null<Datetime>()) {
        sub_bindNull(idx);
        return;
    }

    HKU_CHECK(idx == static_cast<int>(m_params.size()), 
              "Parameter index must be sequential! Expected index: {}, but got: {}", 
              m_params.size(), idx);
    
    // 将 Datetime 转换为字符串格式
    std::string datetime_str = item.str();
    m_params.push_back(boost::mysql::field(datetime_str));
}

void MySQLStatement::sub_bindText(int idx, const std::string& item) {
    HKU_CHECK(idx == static_cast<int>(m_params.size()), 
              "Parameter index must be sequential! Expected index: {}, but got: {}", 
              m_params.size(), idx);
    m_params.push_back(boost::mysql::field(item));
}

void MySQLStatement::sub_bindText(int idx, const char* item, size_t len) {
    HKU_CHECK(idx == static_cast<int>(m_params.size()), 
              "Parameter index must be sequential! Expected index: {}, but got: {}", 
              m_params.size(), idx);
    std::string str(item, len);
    m_params.push_back(boost::mysql::field(str));
}

void MySQLStatement::sub_bindBlob(int idx, const std::string& item) {
    HKU_CHECK(idx == static_cast<int>(m_params.size()), 
              "Parameter index must be sequential! Expected index: {}, but got: {}", 
              m_params.size(), idx);
    std::vector<unsigned char> blob(item.begin(), item.end());
    m_params.push_back(boost::mysql::field(blob));
}

void MySQLStatement::sub_bindBlob(int idx, const std::vector<char>& item) {
    HKU_CHECK(idx == static_cast<int>(m_params.size()), 
              "Parameter index must be sequential! Expected index: {}, but got: {}", 
              m_params.size(), idx);
    std::vector<unsigned char> blob(item.begin(), item.end());
    m_params.push_back(boost::mysql::field(blob));
}

int MySQLStatement::sub_getNumColumns() const {
    if (!m_has_result) {
        return 0;
    }
    
    const auto& metadata = m_results.meta();
    if (metadata.empty()) {
        return 0;
    }
    
    return static_cast<int>(metadata.size());
}

void MySQLStatement::sub_getColumnAsInt64(int idx, int64_t& item) {
    HKU_CHECK(m_has_result, "No result available!");
    
    const auto& rows = m_results.rows();
    HKU_CHECK(m_current_row > 0 && m_current_row <= rows.size(), 
              "Invalid row index!");
    
    const auto& row = rows[m_current_row - 1];
    HKU_CHECK(idx < static_cast<int>(row.size()), 
              "Column index out of range!");
    
    const auto& value = row[idx];
    if (value.is_null()) {
        item = 0;
        return;
    }
    
    try {
        // 尝试直接转换为 int64
        item = value.as_int64();
    } catch (...) {
        try {
            // 尝试作为 uint64 转换（YEAR 可能以 uint64 返回）
            uint64_t u = value.as_uint64();
            item = static_cast<int64_t>(u);
        } catch (...) {
            try {
                // 最后尝试作为字符串解析
                std::string str = value.as_string();
                item = std::stoll(str);
            } catch (const std::exception& e) {
                HKU_THROW("Failed to convert column {} to int64: {}", idx, e.what());
            }
        }
    }
}

void MySQLStatement::sub_getColumnAsDouble(int idx, double& item) {
    HKU_CHECK(m_has_result, "No result available!");
    
    const auto& rows = m_results.rows();
    HKU_CHECK(m_current_row > 0 && m_current_row <= rows.size(), 
              "Invalid row index!");
    
    const auto& row = rows[m_current_row - 1];
    HKU_CHECK(idx < static_cast<int>(row.size()), 
              "Column index out of range!");
    
    const auto& value = row[idx];
    if (value.is_null()) {
        item = 0.0;
        return;
    }
    
    try {
        // 尝试直接转换为 double
        item = value.as_double();
    } catch (...) {
        try {
            // 如果失败，尝试作为 float 转换
            float f = value.as_float();
            item = static_cast<double>(f);
        } catch (...) {
            try {
                // 最后尝试作为字符串解析（DECIMAL 类型可能以字符串返回）
                std::string str = value.as_string();
                item = std::stod(str);
            } catch (const std::exception& e) {
                HKU_THROW("Failed to convert column {} to double: {}", idx, e.what());
            }
        }
    }
}

void MySQLStatement::sub_getColumnAsDatetime(int idx, Datetime& item) {
    HKU_CHECK(m_has_result, "No result available!");
    
    const auto& rows = m_results.rows();
    HKU_CHECK(m_current_row > 0 && m_current_row <= rows.size(), 
              "Invalid row index!");
    
    const auto& row = rows[m_current_row - 1];
    HKU_CHECK(idx < static_cast<int>(row.size()), 
              "Column index out of range!");
    
    const auto& value = row[idx];
    if (value.is_null()) {
        item = Null<Datetime>();
        return;
    }
    
    try {
        // 尝试直接作为 datetime 读取
        auto dt = value.as_datetime();
        item = Datetime(
            static_cast<long>(dt.year()),
            static_cast<long>(dt.month()),
            static_cast<long>(dt.day()),
            static_cast<long>(dt.hour()),
            static_cast<long>(dt.minute()),
            static_cast<long>(dt.second())
        );
    } catch (...) {
        try {
            // 尝试作为 date 读取（没有时间部分）
            auto d = value.as_date();
            item = Datetime(
                static_cast<long>(d.year()),
                static_cast<long>(d.month()),
                static_cast<long>(d.day())
            );
        } catch (...) {
            // 最后尝试作为字符串解析
            std::string datetime_str = value.as_string();
            item = Datetime(datetime_str);
        }
    }
}

void MySQLStatement::sub_getColumnAsText(int idx, std::string& item) {
    HKU_CHECK(m_has_result, "No result available!");
    
    const auto& rows = m_results.rows();
    HKU_CHECK(m_current_row > 0 && m_current_row <= rows.size(), 
              "Invalid row index!");
    
    const auto& row = rows[m_current_row - 1];
    HKU_CHECK(idx < static_cast<int>(row.size()), 
              "Column index out of range!");
    
    const auto& value = row[idx];
    if (value.is_null()) {
        item.clear();
        return;
    }
    
    try {
        // 尝试直接转换为字符串
        item = value.as_string();
    } catch (...) {
        // 如果失败，尝试其他类型转换
        try {
            // 尝试 date 类型
            auto d = value.as_date();
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", 
                     d.year(), static_cast<int>(d.month()), static_cast<int>(d.day()));
            item = std::string(buffer);
        } catch (...) {
            try {
                // 尝试 datetime 类型
                auto dt = value.as_datetime();
                char buffer[30];
                snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                         dt.year(), static_cast<int>(dt.month()), static_cast<int>(dt.day()),
                         static_cast<int>(dt.hour()), static_cast<int>(dt.minute()), 
                         static_cast<int>(dt.second()));
                item = std::string(buffer);
            } catch (...) {
                try {
                    // 尝试 time 类型（boost.mysql 的 time 是 std::chrono::microseconds）
                    auto t = value.as_time();
                    auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(t).count();
                    int hours = total_seconds / 3600;
                    int minutes = (total_seconds % 3600) / 60;
                    int seconds = total_seconds % 60;
                    char buffer[20];
                    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
                    item = std::string(buffer);
                } catch (...) {
                    HKU_THROW("Failed to convert column {} to string", idx);
                }
            }
        }
    }
}

void MySQLStatement::sub_getColumnAsBlob(int idx, std::string& item) {
    HKU_CHECK(m_has_result, "No result available!");
    
    const auto& rows = m_results.rows();
    HKU_CHECK(m_current_row > 0 && m_current_row <= rows.size(), 
              "Invalid row index!");
    
    const auto& row = rows[m_current_row - 1];
    HKU_CHECK(idx < static_cast<int>(row.size()), 
              "Column index out of range!");
    
    const auto& value = row[idx];
    if (value.is_null()) {
        item.clear();
        return;
    }
    
    try {
        const auto& blob = value.as_blob();
        item.assign(blob.begin(), blob.end());
    } catch (...) {
        HKU_THROW("Failed to convert column {} to blob", idx);
    }
}

void MySQLStatement::sub_getColumnAsBlob(int idx, std::vector<char>& item) {
    HKU_CHECK(m_has_result, "No result available!");
    
    const auto& rows = m_results.rows();
    HKU_CHECK(m_current_row > 0 && m_current_row <= rows.size(), 
              "Invalid row index!");
    
    const auto& row = rows[m_current_row - 1];
    HKU_CHECK(idx < static_cast<int>(row.size()), 
              "Column index out of range!");
    
    const auto& value = row[idx];
    if (value.is_null()) {
        item.clear();
        return;
    }
    
    try {
        const auto& blob = value.as_blob();
        item.assign(blob.begin(), blob.end());
    } catch (...) {
        HKU_THROW("Failed to convert column {} to blob", idx);
    }
}

} // namespace hku
