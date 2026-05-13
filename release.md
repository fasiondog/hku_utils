# 版本发布说明

## 1.3.10 - 2026年5月12日

### 新功能特性 (Features)

#### 数据库连接 (db_connect)

- **异步数据库支持**: 添加完整的异步数据库连接功能，包括异步SQLite和MySQL支持
  - 添加异步SQLite连接和语句支持
  - 重构AsyncSQLiteStatement线程池管理
  - 将数据库连接基类中的同步方法转换为异步实现
  - 为SQL语句操作添加异步支持 (TableMacro)
- **MySQL增强**:
  - MySQL连接实现替换为Boost.MySQL
  - 添加libmysqlclient支持并优化异步MySQL连接
  - 添加MySQL连接的statement缓存功能
  - 使用Pimpl模式重构MySQL连接实现
  - 使用异步操作重构MySQL连接实现
  - 更新MySQL语句执行逻辑为流式处理
  - 更新MySQL连接配置以支持MySQL 8.0
  - 添加HKU_UTILS_API导出标识符
- **SQLite增强**: 添加数据库检查和备份功能

#### HTTP客户端 (http_client)

- 优化AsioHttpClient的DNS解析逻辑
- 为macOS平台添加DNS解析超时控制
- 添加请求超时保护和提前验证
- 添加URL有效性检查
- 添加对多种数据格式的JSON解析支持

#### 资源池 (ResourceAsioPool)

- 优化资源池等待队列管理和资源归还逻辑

#### 线程库 (thread)

- 更新协程执行函数使用net::error_code替换boost::system::error_code

#### 日志系统 (Log)

- 添加条件异常抛出宏定义

#### 工具库 (util)

- 在内存分配失败检查中添加unlikely属性优化

#### 测试 (test)

- 添加网络库依赖并引入asio命名空间

#### 构建系统

- 修改配置OpenSSL3依赖项
- 禁用boost.math的128位浮点数支持
- 移除boost math的128位浮点数禁用并添加linux平台libquadmath链接

#### CI/CD

- 添加多架构支持并优化缓存策略
- 更新构建脚本并移除AArch64交叉编译工作流

### 问题修复 (Bug Fixes)

#### 数据库连接 (db_connect)

- 修复MySQL时间戳转换中的缓冲区溢出风险
- 修复异步MySQL连接析构时未清理语句缓存的问题
- 修复异步数据库连接中的协程返回问题
- 修复SQLException异常捕获的命名空间问题
- 修复MySQL连接ping方法中的逻辑错误

#### HTTP客户端 (http_client)

- 修复HTTP请求超时时的死锁问题
- 改进异步HTTP请求超时处理机制
- 修复AsioHttpClient中DNS解析器生命周期问题
- future使用超时会造成asio卡死
- 将HTTP请求异常日志级别从ERROR降级为DEBUG

#### 资源池 (ResourceAsioPool)

- 修复资源池析构和资源归还时的竞态条件问题
- 修复协程执行器中的asio命名空间引用错误

#### 日志系统 (Log)

- 修复日志宏中的命名空间引用问题，使用全局命名空间前缀::以避免潜在的命名冲突

### 代码重构 (Refactoring)

#### 数据库连接 (db_connect)

- 重构MySQL连接实现并修复代码格式
- 将异步事务实现移入头文件
- 移除AsyncDBConnectBase中的同步方法声明
- 移除异步操作改用同步实现提升MySQL连接稳定性
- 更新头文件包含路径和命名空间引用

#### HTTP客户端 (http_client)

- 统一网络库接口引入net.h抽象层
- 更新AsioHttpClient执行器类型
- 移除AsioHttpClient中被注释的超时处理代码

#### 资源池 (ResourceAsioPool)

- 重构资源池析构逻辑并优化并发安全机制
- 避免在持有锁时调用unbind方法
- 调整协程执行器获取位置并移除多余代码

#### 线程库 (thread)

- 更新asio库函数调用
- 更新asio命名空间引用
- 优化co_run_ec函数中的asio异步初始化类型定义

## 1.3.9 - 2026年4月18日

去除基于nng的httpclient

## 1.3.8 - 2026年4月12日

* feat(ResourceAsioPool): 添加模板参数支持可配置互斥锁类型，以便使用io_context单/多线程不同场景
* feat(thread): 优化全局任务组实现
* fix(http_client): 添加GCC编译器警告抑制指令避免-Wsubobject-linkage警告

## 1.3.7 - 2026年3月29日

fixed AsioHttpClient Uri 解析错误

## 1.3.6 - 2026年3月29日

1. 改进 ResourceAsioPool，增加最大资源控制更符合协程需求
2. 修改 AsioHttpClient 错误及改进

## 1.3.5 - 2026年3月19日

1. refactor(thread): 优化线程池submit函数参数传递，提高性能避免不必要的拷贝操作
2. feat(thread): 新增co_run函数，用于在指定executor上异步执行函数
3. feat(asio_http_client): 新增 AsioHttpClient，用与协程支持

## 1.3.4 - 2026年3月13日

1. 并行算法添加协程便携封装函数支持，以便封装传统数据库IO等非异步实现
2. 增加 duckdb 支持

## 1.3.3 - 2026年3月11日

1. 修改全局窃取线程池等待逻辑，避免CPU空转导致占用率过高
2. 优化全局并行算法
3. feat(db_connect): 添加批量操作的空容器检查

## 1.3.2 - 2026年2月2日

1. feat(omp): 新增omp_macro.h头文件，提供OpenMP并行计算相关的宏定义
2. feat(thread): 优化全局偷取线程池，增加全局防嵌套并行方法及非阻塞等待
3. feat(LruCache): 添加LRU缓存实现并优化并行算法(强化并发读取性能)

## 1.3.1 - 2026年1月6日

1. feat(datetime): 优化UTC时区偏移计算实现
2. fix(http_client): 修复HttpClient资源管理和异常处理问题

## 1.3.0 - 2025年12月26日

1. feat(utilities): 优化插件管理器线程安全实现，将 PluginManager 中的互斥锁从 std::mutex 升级为 std::shared_mutex，并重构 getPlugin 方法以支持读写分离锁机制，高并发访问性能。同时完善异常处理逻辑，增强插件加载失败时的日志记录。
2. feat(algorithm): 添加 cpu_num 参数以控制并行线程数
3. feat(thread): 添加StealThreadPool, MQStealThreadPool线程池
4. feat(thread): 添加parallel_for_index_single/parallel_for_index_void_single
5. feat(plugin): 增强插件加载异常处理
6. 移除了 `g_unknown_error_msg` 全局变量，并直接在宏定义中使用字符串字面量
7. feat(config): 增加多个编译配置宏定义开关

## 1.2.9 - 2025年10月6日

1. 移除非必要的 utf8_to_utf32 函数
2. Datetime timestamp和timestampUTC 方法返回值改为uint64_t
3. os 添加获取物理内存和空闲内存大小函数
4. fix(datetime): 修复 UTC 偏移计算在非 Windows 平台的问题
5. fix(MySQLStatement): 初始化MYSQL_TIME结构体(消除linux编译告警)
6. feat(arithmetic): 实现浮点数四舍五入、整数判断及分位数计算功能
7. feat(arithmetic):添加 get_quantile 模板函数，支持计算 vector 的指定分位数
8. feat(arithmetic):重载 ostream 输出操作符，支持打印 vector 内容（省略中间元素以提高可读性）
9. feat(arithmetic):新增 isInteger 函数用于判断 double 和 float 是否为整数，考虑了浮点数精度误差
10. feat(PluginLoader): 增强插件加载失败时的错误信息提示

## 1.2.8 - 2025年8月9日

1. 新增 utf8_to_utf32 函数用于转 utf32 字符编码
2. 移除 mo 模块，容易在国际化和本地化时污染依赖项目

## 1.2.7 - 2025年7月20日

1. 规范命名 i8n 为 i18n

## 1.2.6 - 2025年7月4日

1. 优化调整 mo 模块

## 1.2.5 - 2025年7月2日

1. 暂时移除 TDengine（目前原生连接不稳定容易崩溃)
2. Datetime 添加 timestampUTC 和 fromTimestampUTC 方法

## 1.2.4 - 2025年6月28日

1. xmake最低版本限制 3.0.0
2. 添加 TDengine 支持

## 1.2.3 - 2025年6月1日

1. PluginLoader getFileName 方法由私有改为公有
2. 优化 ThreadPool、MQThreadPool 在 join 时增加互斥保护，以便能跨线程调用 stop 或 join

## 1.2.2 - 2025年5月26日

1. 优化线程池，将使用局部线程变量(依赖全局变量)的线程池和普通线程池区分，防止误用
2. roundEx 从银行家算法改为国内常用的传统四舍五入方法

## 1.2.1 - 2025年5月3日

优化mysql重连，statement准备失败时，返回1（连接丢失）重连

## 1.2.0 - 2025年4月25日

1. fixed HttpClient, 在相应状态不为200时，继续获取相应内容，以便可以接受 restful 详细错误信息
2. 调整 Pluging 支持，改为纯 headonly

## 1.1.9 - 2025年4月7日

1. fixed macosx xcode 升级导致线程池编译错误
2. 新增插件Plugin支持

## 1.1.8 - 2025年3月23日

1. Datetime/TimeDelta 增加 hash 支持
2. getDateRange 在 end 日期为空时，取 Datetime::max
3. fixed xmake.lua 在 mysql 和 sqlite 选项都为 n 时，编译失败

## 1.1.7 - 2025年2月11日

稳定性增强，编译兼容C++20及编译告警与错误消除

## 1.1.6 - 2025年2月5日

fixed parallelIndexRange

## 1.1.5 - 2025年1月30日

fixed MySQL驱动重连优化

## 1.1.4 - 2025年1月26日

1. fixed parallelIndexRange
2. fixed MySQL驱动重连优化

## 1.1.3 - 2025年1月4日

MySQLStatement 重连优化

## 1.1.2 - 2025年1月3日

1. 改进 Null, 以便double/float同样可以使用 Null<>==value方式判断nan值，防止出错
2. clang下编译 Parameter 完善
3. MySQLStatement::_prepare 仍有连接丢失情况，添加日志输出错误码，后续观察

## 1.1.1 - 2024年12月12日

优化 MySQL Statement 准备失败时尝试重连

## 1.1.0 - 2024年11月12日

1. fixed DBUpgrade 判断 sqlite 还是 mysql 示例
2. MySQL Statement 准备失败时尝试重连

## 1.0.9 - 2024年10月20日

1. fixed TABLE_NO_AUTOID_BIND2, TABLE_NO_AUTOID_BIND6, TABLE_NO_AUTOID_BIND12, TABLE_NO_AUTOID_BIND20, TABLE_BIND20
2. 改进 MySQLStatement 支持 SMALLINT, TINYINT

## 1.0.8 - 2024年10月6日

优化 TransAction，中间处理异常时，自动全部回滚

## 1.0.7 - 2024年10月4日

1. 优化 DBConnect, transaction, commit 抛出异常

## 1.0.6 - 2024年9月28日

1. fixed DBUpgrade 创建模块版本表失败

## 1.0.5 - 2024年9月20日

1. fixed MySQLStatement::sub_getColumnAsBlob 未正确获取 blob 长度
2. fixed HttpClient 未正确处理含有多个值的 HttpParams
3. 优化 TimerManager, 可以指定使用外部任务组
4. Datetime 新增支持 "20240822 11:30:06.230" 的字符串方式构造
5. 调整 base64 编解码接口

## 1.0.4 - 2024年8月6日

1. 屏蔽 HttpClient 接收对端 Connect close 时的打印
2. HttpClient创建时增加参数直接指定超时时间
3. NodeServer start 增加参数自行指定最大并发数，默认128

## 1.0.3 - 2024年8月5日

fixed DBUpgrade 自动创建mysql module_version 表失败

## 1.0.2 - 2024年8月2日

1. 增加基于 nng 的简单请求响应服务及客户端（NodeServer, NodeClient）
2. 优化 logger

## 1.0.1 - 2024年7月29日

add http_client

## 1.0.0 - 2024年7月10日

初始版本
