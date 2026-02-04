/*
 * DuckDBUtil.h
 *
 *  Copyright (c) 2024, hikyuu.org
 *
 *  Created on: 2024-01-01
 *      Author: fasiondog
 */
#pragma once
#ifndef HIKYUU_DB_CONNECT_DUCKDB_DUCKDBUTIL_H
#define HIKYUU_DB_CONNECT_DUCKDB_DUCKDBUTIL_H

#include <string>
#include <vector>
#include "DuckDBConnect.h"

namespace hku {

/**
 * @brief 检查DuckDB文件是否有效
 * @param filename 数据库文件路径
 * @return true 有效
 * @return false 无效
 */
bool HKU_UTILS_API isValidDuckDBFile(const std::string& filename) noexcept;

/**
 * @brief 获取数据库统计信息
 * @param connect 数据库连接
 * @return 包含统计信息的字符串
 */
std::string HKU_UTILS_API getDatabaseStats(DuckDBConnect* connect);

/**
 * @brief 获取表信息
 * @param connect 数据库连接
 * @param tablename 表名
 * @return 包含表信息的字符串
 */
std::string HKU_UTILS_API getTableInfo(DuckDBConnect* connect, const std::string& tablename);

/**
 * @brief 获取所有表名列表
 * @param connect 数据库连接
 * @return 表名列表
 */
std::vector<std::string> HKU_UTILS_API getAllTableNames(DuckDBConnect* connect);

/**
 * @brief 备份数据库
 * @param connect 数据库连接
 * @param backup_filename 备份文件名
 * @return true 成功
 * @return false 失败
 */
bool HKU_UTILS_API backupDatabase(DuckDBConnect* connect, const std::string& backup_filename) noexcept;

/**
 * @brief 检查数据库完整性
 * @param connect 数据库连接
 * @return true 完整
 * @return false 不完整
 */
bool HKU_UTILS_API checkDatabaseIntegrity(DuckDBConnect* connect) noexcept;

}  // namespace hku

#endif /* HIKYUU_DB_CONNECT_DUCKDB_DUCKDBUTIL_H */