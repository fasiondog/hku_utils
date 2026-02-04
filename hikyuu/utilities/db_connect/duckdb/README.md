# DuckDB 驱动说明

## 简介

这是 hku_utils 项目的 DuckDB 数据库驱动实现，提供了与 SQLite 类似的接口，便于在项目中使用 DuckDB 数据库。

## 特性

- 完全兼容 hku_utils 的数据库连接接口
- 支持预编译语句和参数绑定
- 支持事务处理
- 提供实用工具函数
- 支持多种配置选项

## 依赖

- DuckDB C++ API
- hku_utils 基础库

## 编译配置

在 xmake.lua 中启用 DuckDB 支持：

```lua
option("duckdb", {description = "Enable duckdb driver.", default = true})
```

## 使用方法

### 1. 基本连接

```cpp
#include "hikyuu/utilities/db_connect/duckdb/DuckDBConnect.h"

using namespace hku;

// 创建连接参数
Parameter param;
param.set<std::string>("db", "mydatabase.db");

// 创建连接
DuckDBConnectPtr db = std::make_shared<DuckDBConnect>(param);

// 检查连接状态
if (db->ping()) {
    std::cout << "Connected successfully!" << std::endl;
}
```

### 2. 支持的配置参数

```cpp
Parameter param;
param.set<std::string>("db", "database.db");                    // 数据库文件名（必需）
param.set<std::string>("access_mode", "READ_WRITE");           // 访问模式：READ_WRITE 或 READ_ONLY
param.set<bool>("allow_unsigned_extensions", false);           // 是否允许未签名扩展
param.set<std::map<std::string, std::string>>("config", {      // 额外配置选项
    {"memory_limit", "1GB"},
    {"threads", "4"}
});
```

### 3. 基本操作

```cpp
// 创建表
db->exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name VARCHAR(100), age INTEGER)");

// 插入数据
db->exec("INSERT INTO users (name, age) VALUES ('Alice', 25)");

// 查询数据
auto count = db->queryNumber<int64_t>("SELECT COUNT(*) FROM users");

// 使用预编译语句
SQLStatementPtr stmt = db->getStatement("SELECT name, age FROM users WHERE age > ?");
stmt->bind(0, 20);
stmt->exec();

while (stmt->moveNext()) {
    std::string name;
    int age;
    stmt->getColumn(0, name);
    stmt->getColumn(1, age);
    std::cout << name << " is " << age << " years old" << std::endl;
}
```

### 4. 事务处理

```cpp
try {
    db->transaction();
    db->exec("INSERT INTO users (name) VALUES ('Bob')");
    db->exec("INSERT INTO users (name) VALUES ('Charlie')");
    db->commit();
} catch (...) {
    db->rollback();
    throw;
}
```

### 5. 实用工具函数

```cpp
#include "hikyuu/utilities/db_connect/duckdb/DuckDBUtil.h"

// 检查数据库文件有效性
bool valid = isValidDuckDBFile("database.db");

// 获取数据库统计信息
std::string stats = getDatabaseStats(db.get());

// 获取表信息
std::string table_info = getTableInfo(db.get(), "users");

// 获取所有表名
std::vector<std::string> tables = getAllTableNames(db.get());

// 备份数据库
bool success = backupDatabase(db.get(), "backup.db");

// 检查数据库完整性
bool integrity = checkDatabaseIntegrity(db.get());
```

## 与 SQLite 的主要区别

1. **API 差异**：DuckDB 使用自己的 C++ API，而非 SQLite 的 C API
2. **数据类型**：某些数据类型的处理方式略有不同
3. **配置选项**：DuckDB 支持更多现代化的配置选项
4. **性能特性**：DuckDB 针对分析型工作负载进行了优化

## 注意事项

1. **线程安全**：DuckDB 连接不是线程安全的，每个线程应使用独立的连接
2. **内存管理**：使用智能指针管理连接和语句对象
3. **错误处理**：大部分操作会在失败时抛出异常
4. **文件锁定**：同一时间只能有一个写连接访问数据库文件

## 性能建议

1. 对于大量插入操作，使用事务批量处理
2. 合理使用预编译语句提高执行效率
3. 根据需要调整内存限制和线程数配置
4. 定期进行数据库维护和优化

## 测试

运行测试用例：

```bash
cd test/utilities/db_connect/duckdb
# 编译并运行测试
```

## 示例代码

查看 `examples/duckdb_example.cpp` 获取完整的使用示例。