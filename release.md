
# 版本发布说明

## 1.5.4

- feat(db_connect): DBCondition 的字符串条件值改走 ? 占位符 + std::variant 绑定参数，由 sqlite/mysql 预处理语句执行，消除静态转义无法覆盖的 MySQL 字符集与 ANSI_QUOTES 差异
- feat(db_connect): 新增每条语句绑定参数上限校验（默认 32766，取 sqlite 与 mysql 允许的较小值），in/not_in 超限时显式报错而非交由驱动拒绝
- feat(db_connect): Field 与 order-by 的列名构造期校验，拒绝引号、反引号、?、;、# 与注释序列等会破坏语句的输入
- fix(db_connect): 修复分页结果集解析 where 尾部子句时按 ORDER 子串匹配导致的误切，列名含 order（如 order_date）时条件被截断、查询语义错误
- fix(db_connect): 修复条件携带 LIMIT 时被留在子查询 WHERE 内造成的非法 SQL，limit 现与分页协同限总行数并在超出后返回空页
- fix(db_connect): SQLResultSet::getPageCount 在总数恰为页大小整数倍时不再多算一个空页，与异步版本保持一致
- fix(db_connect): 按条件删除改走预处理语句与绑定值，不再拼接进直接执行的 SQL
- fix(db_connect): DBCondition 字符串值集中转义，防止 SQL 拼接注入
- fix(http_client): 修复 url_escape 对非 ASCII 字节的转义损坏
- fix(http_client): 流式响应改用实际写入的 body 字节数作为数据块长度，修正 chunked 响应混入分帧字节、多算/少算的问题
- feat(http_client): 流式响应在对方提前关连且 body 未完时抛异常，不再空转
- fix(http_client): 流式请求的发送阶段补上超时定时器，写入阻塞时不再永久挂住
- fix(http_client): 修复超时 timer 处理器在 completion 与超时竞态下访问已销毁引用的未定义行为
- fix(http_client): HttpTimeoutException 补上 key function 与导出宏，修复跨动态库边界 catch 不到该异常的问题
- feat(http_client): 新增响应体与响应头大小上限配置（默认沿用 8MB / 8KB），超限抛 HttpResponseTooLargeException 且不复用该连接
- fix(http_client): 流式下载不再被响应体上限截断，大于默认上限的响应可完整接收
- fix(http_client): gzip 响应的解压输出改按响应体上限约束，不再按 1GB 放行
- fix(http_client): 请求异常或对端结束 keep-alive 时不再把连接原样归还连接池，避免下次请求读到残留字节而串包
- fix(http_client): 支持带方括号的 IPv6 字面量 URL，并修正畸形主机/端口被静默解析成错误值
- fix(http_client): 以 IP 地址访问 HTTPS 时不再下发非法的 SNI，改由证书校验匹配 IP 主题备用名
- perf(http_client): macOS 的 DNS 解析改到独立线程执行，不再阻塞事件循环线程，且超时能真正中断等待
- fix(http_client): Host 头在非默认端口时携带端口，符合 RFC 6874/9110
- fix(plugin): PluginLoader::unload() 释放句柄后置空，修复加载失败路径析构时二次 dlclose 的未定义行为
- fix(utilities): 资源池等待节点改用原子转账结算 ownership，修复等待者超时与资源归还竞态下的悬垂访问、重复释放与资源丢失
- fix(utilities): 资源池析构时可回收尚未被取走的资源，析构不再因槽位无法收回而永久阻塞
- fix(utilities): 资源池创建改为 CAS 预留槽位，max_count 在并发下成为硬上限
- fix(db_connect): libmysqlclient 驱动不再将 MYSQL_DATA_TRUNCATED 视为结果集结束，列值被截断时抛出异常而非静默提前终止
- perf(db_connect): libmysqlclient 驱动读取 text/blob/decimal 列不再逐字符经 ostringstream 拼接，改为按实际长度直接构造
- fix(db_connect): MySQL 三处 resetAutoIncrement 补上缺失的 TABLE 关键字，ALTER 语句不再必然语法错误
- fix(db_connect): tableExist / resetAutoIncrement / remove 中的表名改为标识符转义或参数绑定，消除 SQL 注入面
- fix(db_connect): libmysqlclient 驱动的文本/blob 结果列缓冲改按需扩容，不再按字段声明上限（LONGTEXT 达 4GB）一次性分配
- perf(db_connect): batchSaveOrUpdate 改为两趟就地处理，不再将全部元素深拷贝进临时 vector（大批量下峰值内存翻倍）；保存的元素自此会回写 rowid，与 batchSave 行为对齐
- fix(thread): ThreadSafeQueue 与 MQStealQueue 的 size() 改为加锁读取，消除线程池 join 忙等期间与任务入队/出队的数据竞争
- fix(thread): 五个线程池构造失败时唤醒并 join 已启动的工作线程后再传播异常，消除成员析构与仍在运行的线程竞争访问导致的未定义行为
- fix(parameter): Parameter::get&lt;float&gt; 值域下界改用 lowest()，修复读取负数时被误判越界抛出异常
- fix(os): removeDir 改用 lstat（Windows 按 reparse point 识别），目录内的符号链接/junction 仅删除链接本身，不再递归进入其目标目录删除外部文件；并修复空路径触发异常、stat 返回值未检查问题
- fix(datetime): TimeDelta 字符串构造解析秒小数改用 llround 取整，避免 59.999999 这类值因浮点表示误差被截断少 1 微秒
- fix(thread): StealThreadPool/GlobalStealThreadPool/MQStealThreadPool 的 join 判据引入 in-flight 计数，修复任务已出队但尚未执行完（或仍会递归 submit 子任务）时 join 提前设 m_done 导致后续 submit 抛 logic_error、子任务丢失的问题
- fix(thread): MQStealThreadPool 多生产者并发 submit 时轮询索引 m_current_index 改为 atomic fetch_add，消除普通 int 读改写的数据竞争（TSan 必报 UB）
- fix(utilities): TimerManager 检测线程内 submit 抛异常（如外部线程池已停止）时捕获并移除该定时器，不再让 logic_error 逃逸出线程函数导致 std::terminate；并加顶层 catch 兜底
- fix(db_connect): AsyncAutoTransAction 明确析构自动提交语义，修正误导性文档与测试断言；事务未启动或 io_context 已停止时析构不再派生提交协程
- fix(db_connect): MySQL 预处理语句的自定义 deleter 改为捕获按连接代际共享的存活状态，连接关闭/重连后外部仍持有的语句析构不再通过裸连接指针调用 close_statement，消除对已释放连接的访问
- fix(db_connect): DBUpgrade 的 module_name 改经 sqlStringLiteral 转义后再写入 SQL，修复模块名含双引号时突破字符串字面量导致的注入（同步/异步共 6 处）
- fix(utilities): ResourceTlsPool/ResourceTlsVersionPool 析构改为按环形取模下标释放全部空闲槽位，修复按连续下标释放漏删 wrap-around 槽位导致的资源泄漏；普通 TlsPool 环满删除路径补齐计数递减，跨线程归还"容量不恢复"作为文档化限制在类注释中明确

## 1.5.3 - 2026年9月21日

feat: 新增 FileLock 文件锁

## 1.5.2 - 2026年9月19日

fix(xmake): 适配 boost 1.92

## 1.5.1 - 2026年8月20日

fix(xmake): 移除nng依赖配置中的http_client_ssl选项

## 1.5.0 - 2026年8月19日

1. feat(arithmetic): 添加UTF-8字符串大小写转换和比较功能
2. fixed: 修复GCC编译问题
3. fixed(workflows): 更新配置以禁用http_client功能
4. fixed(db_connect): 优化AsyncSQLResultSet和SQLResultSet中的字符串转大写处理

## 1.4.9 - 2026年5月31日

1. fixed(AsyncMySQLConnect): 重置连接上下文和初始化状态以确保资源正确释放
2. fixed(async-transaction): 优化异步事务类，重构创建和回滚逻辑

## 1.4.8 - 2026年5月25日

1. 移除几乎不使用的线程池实现
2. windows下适应 clang-cl 编译

## 1.4.7 - 2026年5月20日

fixed(sqlite): 重构异步SQLite连接初始化逻辑

## 1.4.6 - 2026年5月20日

fixed: MySQL异步连接可能未初始化

## 1.4.5 - 2026年5月20日

fixed(db_connect): 在AsyncMySQLStatement构造函数中添加SQL_CHECK验证，将boost::mysql::tcp_connection的类型转换移到使用位置

## 1.4.3 - 2026年5月19日

修正优化 ResouceTlsPool/ResourceHybirdPool

## 1.4.2 - 2026年5月19日

1. fixed: MySQL异步连接及补充测试
2. fixed: AsyncSQLResultSet及补充测试

## 1.4.1 - 2026年5月17日

1. 优化 ResouceAsioPool/ResouceAsioVersionPool 超时等待及可能的竞态问题
2. ResourceTlsPool/ResourceTlsVersionPool 移除 asyncGet 方法，thread local 资源池通常不使用也不建议 async 方式获取

## 1.4.0 - 2026年05月15日

### 新功能特性 (Features)

#### 资源池管理 (Resource Pool)

- **新增线程局部资源池 (ResourceTlsPool)**:

  - 基于 Ring Buffer 架构实现高性能的线程局部存储资源池
  - 提供 `get()` 同步接口和 `asyncGet()` 异步接口
  - 支持非阻塞模式获取资源
  - 统一使用 `stdx::expected` 进行错误处理
- **新增混合资源池 (ResourceHybridPool)**:

  - 结合 TLS 本地池和全局共享池的优势
  - 优先从本地线程池获取资源，失败时自动降级到全局池
  - 支持运行时动态配置 TLS 池大小
  - 添加 TLS 池禁用功能，灵活适配不同场景
- **新增版本化资源池**:

  - ResourceTlsVersionPool: 线程局部版本资源池，每个线程维护独立版本号
  - ResourceHybridVersionPool: 混合版本资源池，支持全局原子版本号和参数同步
  - 采用鸭子类型设计，资源类只需实现 `getVersion()` 和 `setVersion()` 方法
  - 使用编译期检查 (`static_assert`) 强制要求版本接口，零运行时开销
- **资源池接口统一**:

  - 同步方法统一命名为 `get()`
  - 异步方法统一使用 `asyncGet()` 前缀
  - 所有返回值统一使用 `stdx::expected<ResourcePtr, std::string>`
  - 移除废弃的 `getForce()` 和 `asyncGetForce()` 接口
  - 新增 `createStandalone()` 创建独立资源（不受池大小限制）
- **ResourceAsioPool 优化**:

  - 优化等待队列管理和资源归还逻辑
  - 修复析构和资源归还时的竞态条件问题
  - 避免在持有锁时调用 unbind 方法
  - 异步资源获取改用 expected 类型返回结果

#### 数据库连接 (db_connect)

- **异步数据库支持增强**:

  - 添加完整的异步 SQLite 连接和语句支持
  - 重构 AsyncSQLiteStatement 线程池管理
  - 为 SQL 语句操作添加异步支持 (TableMacro)
  - 将数据库连接基类中的同步方法转换为异步实现
- **MySQL 驱动重构与增强**:

  - MySQL 连接实现替换为 Boost.MySQL
  - 添加 libmysqlclient 支持并优化异步 MySQL 连接
  - 使用 Pimpl 模式重构 MySQL 连接实现，隐藏实现细节
  - 添加 MySQL 连接的 statement 缓存功能，提升性能
  - 更新 MySQL 语句执行逻辑为流式处理
  - 更新 MySQL 连接配置以支持 MySQL 8.0
  - 添加 HKU_UTILS_API 导出标识符

#### HTTP 客户端 (http_client)

- **DNS 解析优化**:

  - 优化 AsioHttpClient 的 DNS 解析逻辑
  - 为 macOS 平台添加 DNS 解析超时控制
  - 修复 DNS 解析器生命周期问题
- **超时保护机制**:

  - 添加请求超时保护和提前验证
  - 改进异步 HTTP 请求超时处理机制
  - 修复 HTTP 请求超时时的死锁问题
  - future 使用超时会造成 asio 卡死的问题已修复
- **URL 处理增强**:

  - 添加 URL 有效性检查
  - 添加对多种数据格式的 JSON 解析支持
  - 将 HTTP 请求异常日志级别从 ERROR 降级为 DEBUG

#### 线程库 (thread)

- **协程支持优化**:
  - 更新协程执行函数使用 `net::error_code` 替换 `boost::system::error_code`
  - 优化 `co_run_ec` 函数中的 asio 异步初始化类型定义
  - 修复协程执行器中的 asio 命名空间引用错误
  - 更新 asio 库函数调用和命名空间引用

#### 日志系统 (Log)

- 添加条件异常抛出宏定义
- 修复日志宏中的命名空间引用问题，使用全局命名空间前缀 `::` 以避免潜在的命名冲突

#### 工具库 (util)

- 在内存分配失败检查中添加 `unlikely` 属性优化，提升分支预测性能

#### 构建系统

- **依赖管理优化**:

  - 修改配置 OpenSSL3 依赖项
  - 禁用 boost.math 的 128 位浮点数支持
  - 添加 Linux 平台 libquadmath 链接支持
  - 调整包依赖顺序确保 openssl3 在 boost 之前
- **CI/CD 改进**:

  - 添加多架构支持并优化缓存策略
  - 更新构建脚本并移除 AArch64 交叉编译工作流
  - 添加 Windows 版本定义以支持 Windows 7 及以上系统

#### 其他组件

- **FilterNode**: 添加共享互斥锁支持并改进异常处理
- **expected 类型支持**: 添加 `stdx::expected` 封装层，统一错误处理机制

### 问题修复 (Bug Fixes)

#### 数据库连接 (db_connect)

- 修复 MySQL 时间戳转换中的缓冲区溢出风险
- 修复异步 MySQL 连接析构时未清理语句缓存的问题
- 修复异步数据库连接中的协程返回问题
- 修复 SQLException 异常捕获的命名空间问题
- 修复 MySQL 连接 ping 方法中的逻辑错误

#### HTTP 客户端 (http_client)

- 修复 HTTP 请求超时时的死锁问题
- 修复 AsioHttpClient 中 DNS 解析器生命周期问题
- 修复 future 使用超时造成的 asio 卡死问题

#### 资源池 (ResourceAsioPool)

- 修复资源池析构和资源归还时的竞态条件问题
- 修复协程执行器中的 asio 命名空间引用错误

#### 构建系统

- 修复 Linux 平台下 quadmath 库链接问题
- 修复 BOOST_HAS_FLOAT128 定义导致的编译问题
- 移除 OpenSSL3 的 system=false 和 shared 配置

### 代码重构 (Refactoring)

#### 资源池架构重构

- **移除继承基类模式**:

  - 移除 `ResourceWithVersion` 基类，改用编译期检查
  - 移除 `AsyncResourceWithVersion` 基类，改用 SFINAE 检查
  - 采用鸭子类型原则，降低耦合度，提高灵活性
- **重命名优化**:

  - 将 `ResourceThreadLocalPool` 重命名为 `ResourceTlsPool`，更准确表达含义
- **测试优化**:

  - 重构版本资源池测试代码以避免测试间状态污染

#### 数据库连接重构

- 重构 MySQL 连接实现并修复代码格式
- 将异步事务实现移入头文件
- 移除 AsyncDBConnectBase 中的同步方法声明
- 移除异步操作改用同步实现提升 MySQL 连接稳定性
- 更新头文件包含路径和命名空间引用

#### HTTP 客户端重构

- 统一网络库接口引入 `net.h` 抽象层
- 更新 AsioHttpClient 执行器类型
- 移除 AsioHttpClient 中被注释的超时处理代码
- 移除 nng http 客户端实现

#### 资源池重构

- 重构资源池析构逻辑并优化并发安全机制
- 避免在持有锁时调用 unbind 方法
- 调整协程执行器获取位置并移除多余代码
- 修正注释对齐问题

#### 线程库重构

- 更新 asio 库函数调用
- 更新 asio 命名空间引用
- 优化 co_run_ec 函数中的 asio 异步初始化类型定义

#### 其他重构

- 移除未使用的并发节点映射头文件
- 移除废弃的异常抛出宏定义
- 完善资源池单元测试覆盖并发场景

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
