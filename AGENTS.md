# AGENTS.md — hku_utils 项目 AI 开发指南

> 本文件为 AI 编码代理（及新加入的开发者）提供在 hku_utils 仓库中工作所需的核心上下文：
> 项目结构、构建/测试命令、代码规范与常见注意事项。**先读本文件，再动手改代码。**

## 1. 项目概览

- **hku_utils** 是一个基于 **C++20** 的**跨平台基础工具库**，为上层项目（如 hikyuu 量化框架）提供通用的底层能力：日志、参数、异常、资源池、线程/协程、数据库连接、HTTP 客户端、插件等。
- 纯 C++ 库，无 Python 绑定层。对外以 **xmake 包**形式发布，下游通过 `add_requires("hku_utils")` 引用（见 `publish.py` 与 `hikyuu_extern_libs` 仓库）。
- 顶层命名空间统一为 `hku`（子命名空间如 `hku::utils`）；导出宏为 `HKU_UTILS_API`。
- 当前版本 `1.5.2`（记录于 `xmake.lua` 的 `set_version`，变更历史见 `release.md`）。
- 目录约定：源码与头文件统一放在 `hikyuu/utilities/`（下游以 `hikyuu/utilities/...` 路径 include），测试放在 `test/`。

## 2. 仓库结构

```
hku_utils/
├── xmake.lua                    # 顶层构建脚本（工程定义、options、依赖）
├── copy_dependents.lua          # 拷贝第三方依赖头文件/库的任务
├── publish.py                   # 打包发布：生成 zip 并更新 hikyuu_extern_libs 仓库
├── config.h.in / version.h.in   # 配置模板，构建时生成到 hikyuu/utilities/
├── .clang-format                # C++ 格式化规则（Google 基础）
├── release.md                   # 版本发布/变更日志
├── docs/                        # 说明文档（HTTP 流式处理等）
├── hikyuu/utilities/            # ★ 库源码（唯一业务命名空间 hku）
│   ├── Log.h/.cpp               # 日志（基于 spdlog/fmt，HKU_* 宏）
│   ├── Parameter.h/.cpp         # 参数容器（PARAMETER_SUPPORT 宏）
│   ├── Null.h / exception.h     # Null<T>() 与异常体系
│   ├── expected.h               # stdx::expected 封装（基于 tl_expected）
│   ├── os.h/.cpp / runtimeinfo.h
│   ├── arithmetic.h/.cpp        # 数值/字符串工具
│   ├── base64 / md5 / snowflake / string_view / any_to_string
│   ├── Resource*Pool.h          # 资源池（Asio/Tls/Hybrid + *VersionPool）
│   ├── TimerManager.h / SpendTimer.h/.cpp / FilterNode.h / DllLoader.h / FileLock.h
│   ├── datetime/                # Datetime / TimeDelta
│   ├── db_connect/              # 数据库连接层（同步 + 异步）
│   │   ├── DBConnectBase.h / SQLStatementBase.h / SQLResultSet.h
│   │   ├── AsyncDBConnectBase.h / AsyncSQLStatementBase.h / AsyncSQLResultSet.h
│   │   ├── DBUpgrade / DBCondition / AutoTransAction / AsyncTransAction / TableMacro
│   │   └── sqlite/ mysql/ duckdb/   # 各驱动实现
│   ├── http_client/             # AsioHttpClient（Boost.Beast，支持协程/流式）、url
│   ├── ini_parser/              # IniParser
│   ├── node/                    # NodeClient / NodeServer（基于 nng 的 reqrep）
│   ├── plugin/                  # 插件体系（纯 header）：Base/Client/Loader/Manager
│   └── thread/                  # 线程池/协程：GlobalSteal/MQSteal/Steal/ThreadPool、
│                                #   WorkStealQueue/MQStealQueue/ThreadSafeQueue、algorithm
└── test/                        # doctest 单元测试（与源码结构并行）
    ├── xmake.lua                # 定义 target: unit-test / testplugin
    ├── test_main.cpp            # doctest 入口
    ├── test_config.h            # 测试开关宏（ENABLE_MYSQL_TEST 等）
    ├── plugin/                  # 测试用插件（target testplugin）
    └── utilities/               # 各模块测试，目录与 hikyuu/utilities 对应
```

## 3. 构建系统（xmake）

- 构建工具：**xmake**（顶层 `set_xmakever("3.0.0")`，CI 用 3.0.8）。C++ 标准 **C++20**（协程需要）。
- 产物输出到 `build/{mode}/{plat}/{arch}/lib`（`set_targetdir`）。
- 第三方依赖通过 xmake 包管理（`add_requires`）拉取：`fmt`、`spdlog`（均 header_only）、`yas`（序列化）、`tl_expected`、`boost >=1.92`、`utf8proc`；按开关追加 `openssl3`、`mysql`、`sqlite3`/`sqlcipher`、`duckdb`、`nlohmann_json`、`nng`、`gzip-hpp`。外部仓库：`hikyuu-repo`（`https://github.com/fasiondog/hikyuu_extern_libs.git`）。
- 关键配置项（`xmake f` 选项）：`mysql`、`disable_libmysqlclient`、`sqlite`、`duckdb`、`sqlcipher`、`sql_trace`、`stacktrace`、`datetime`、`spend_time`、`log_level`、`async_log`、`leak_check`、`ini_parser`、`http_client`、`http_client_ssl`、`http_client_zip`、`node`。
- 构建时由 `add_configfiles` 依据 `config.h.in` / `version.h.in` 生成 `hikyuu/utilities/config.h`、`hikyuu/utilities/version.h`（均 **gitignore**，勿提交）。

### 常用命令

```bash
# 配置（首次或变更依赖/选项后）
xmake f -k shared -y -vD

# 编译库
xmake -b hku_utils

# 编译并运行单元测试（doctest）
xmake -b unit-test
xmake r unit-test

# 调试 / 覆盖率 / 性能剖析
xmake f -m debug -y          # debug
xmake f -m coverage -y       # coverage（生成 lcov/genhtml/gcovr 报告）
xmake f -m profile -y        # profile
```

> 说明：`xmake r unit-test` 运行前会自动把 `test_data/` 拷贝到可执行文件目录（见 `test/xmake.lua` 的 `before_run`）。CI 默认以 `xmake f ... --mysql=n --http_client=n` 配置后构建 `unit-test`。

### IDE / LSP 索引（clangd）

源码中普遍使用 `#include "hikyuu/utilities/xxx.h"`（依赖 `xmake.lua` 的 `add_includedirs(".")`），因此 **clangd 必须拿到编译数据库**，否则会退化为 fallback 参数（编译目录 = 文件自身所在目录），对新建文件报成片的 `Unknown type name 'XXX'` —— 这类报错是**索引问题而非代码问题**，不要靠改代码去「修」。

```bash
# 生成到工程根目录 compile_commands.json（clangd 原生自动发现的 locations）
xmake project -k compile_commands --lsp=clangd
```

- 新增/删除源文件、变更 `xmake f` 选项后需重跑一次；生成物已 gitignore，勿提交。
- 不要用 `.clangd` 的 `-I.` 代替编译数据库：相对路径按编译目录解析，对 fallback 命令会指向源文件所在目录而非工程根，**实测无效**。
- 工程根 `compile_commands.json` 也可由 `clangd.arguments: --compile-commands-dir=<dir>` 指定目录（如仍想放在 `.vscode/` 下）。

## 4. 测试

基于 **doctest**，入口为 `test/test_main.cpp`，测试开关定义在 `test/test_config.h`（如 `ENABLE_MYSQL_TEST`）。

- 测试工程与源码工程**物理隔离、结构并行**：`test/utilities/…` 对 `hikyuu/utilities/…`。
- **一模块一文件**：文件命名 `test_模块名.cpp`，放在与源码对应的子目录中。
- **用例命名**：`TEST_CASE("test_模块名")` 或 `test_类名_方法名`，重名时追加后缀区分。
- **测试点标注**：在每个用例内用 `/** … */` 注释明确标注各测试点（如 `/** 正常添加、读取、修改参数 */`）。
- **边界必须覆盖**：循环边界（0/1/N/N±1）、极值（空串、空范围、`Null<T>()`、越界、零/负值）、分支（if/else、switch、提前 return）、异常路径（非法输入、文件不存在、格式错误）。
- **覆盖率要求**：尽量达到分支覆盖，最低行覆盖；**必须依赖外部环境（网络/数据库/实盘）才能触发的路径可豁免**。用 `xmake f -m coverage -y` 生成报告自查。
- 涉及 `testplugin` 的测试依赖 `test/xmake.lua` 中的 `testplugin` 目标。

## 5. 代码规范

- 提交前用 `clang-format` 格式化改动文件，避免与现有风格偏离。

### 格式化（`.clang-format`，Google 基础）

| 项 | 约定 |
| --- | --- |
| 缩进 | 4 空格，`UseTab: Never` |
| 列宽 | `ColumnLimit: 100` |
| 大括号 | `BreakBeforeBraces: Attach`（控制语句/函数/类同行） |
| 指针 | `PointerAlignment: Left`（`type* name`） |
| 其他 | `SortIncludes: false`、`NamespaceIndentation: None`、`SpacesBeforeTrailingComments: 2` |
| 注释 | 代码内注释（含 doxygen `/** ... */` 与测试点标注）统一使用**英文**，不包含问题单号（如 ISS-xxx、#NN） |

### 命名规范（C++）

以下规范自 `hikyuu/utilities/` 现有代码提炼，新增/修改代码须遵循：

| 标识符类别              | 规范                                                     | 示例                                                                |
| ----------------------- | -------------------------------------------------------- | ------------------------------------------------------------------- |
| 命名空间                | 全小写                                                   | `namespace hku`、`namespace utils`                                  |
| 类 / 结构体             | `PascalCase`；导出类加 `HKU_UTILS_API` 宏                | `class HKU_UTILS_API DBConnectBase`、`class PluginManager`、`struct ...` |
| 公开成员函数            | `camelCase`，动词起首                                    | `getVersion()`、`pluginPath()`、`getPlugin()`、`ping()`             |
| 受保护/私有成员函数     | `_` 前缀 + `camelCase`                                   | `_prepare()`、`_reset()`、`_clone()`                                |
| 成员变量                | `m_` 前缀 + `camelCase`                                  | `m_mutex`、`m_plugins`、`m_plugin_path`                             |
| 类静态成员变量          | `ms_` 前缀 + `camelCase`                                 | `ms_logger`                                                         |
| 全局/文件作用域 static  | `g_` 前缀 + `camelCase`                                  | `g_log_level`                                                       |
| 类型别名 / 智能指针别名 | 业务名 + `Ptr`（`typedef std::shared_ptr<T> XPtr;`）     | `typedef std::shared_ptr<FilterNode> FilterNodePtr;`、`AsyncDBConnectPtr` |
| 枚举类型 / 枚举值       | 类型 `PascalCase`；值全大写 + 下划线                     | `enum LOG_LEVEL { LOG_TRACE, LOG_DEBUG, ... }`                     |
| 宏 / 编译开关 / 常量    | 全大写 + 下划线                                          | `HKU_UTILS_API`、`HKU_ENABLE_MYSQL`、`PARAMETER_SUPPORT`          |
| 函数参数 / 局部变量     | `camelCase`                                              | `plugin_path`、`baseInfoParam`                                     |
| 头/源文件名             | 类文件 `PascalCase`（**一 class 一文件**）；轻量工具头用小写下划线 | `DBConnectBase.h`、`PluginManager.h`；`os.h`、`arithmetic.h`、`any_to_string.h` |

> 注：每个 `class` 独占一个头/源文件（类名与文件名一致）；扁平 `struct`、POD、枚举、typedef、宏等轻量定义可与小工具函数共存于同一小写头文件（如 `os.h`、`cppdef.h`）。
>
> **条件编译约定**：新增依赖/驱动时，须在 `xmake.lua` 中用 `set_configvar` 定义对应 `HKU_*` 宏，并在 `config.h.in` 中声明，源码中通过 `#if HKU_ENABLE_XXX` 隔离，同时补齐 `test/xmake.lua` 中相应的 `add_files`/`add_packages`。

## 6. 架构与关键组件

库以 `hikyuu/utilities/` 为根，按能力域划分子模块，各模块相对独立、可单独 include：

| 模块 | 关键组件 | 说明 |
| --- | --- | --- |
| 基础设施 | Log / Parameter / Null / exception / expected / os / runtimeinfo | 日志（spdlog）、参数容器、空值、异常与 `stdx::expected` 错误处理 |
| 通用算法与编码 | arithmetic / base64 / md5 / snowflake / string_view / any_to_string | 数值与字符串工具、编解码、雪花 ID、UTF-8 处理 |
| 资源池 | ResourcePool / ResourceAsioPool / ResourceTlsPool / ResourceHybridPool（含 `*VersionPool`） | 统一 `get()` / `asyncGet()` 接口，返回 `stdx::expected<ResourcePtr, std::string>` |
| 并发 | thread/（GlobalSteal/MQSteal/Steal/ThreadPool、WorkSteal/MQSteal/ThreadSafeQueue、algorithm） | 线程池、工作窃取队列、并行算法、协程执行（`co_run`） |
| 日期时间 | datetime/（Datetime、TimeDelta） | 时间戳/UTC 偏移/时区处理 |
| 数据库 | db_connect/（DBConnectBase、SQLStatementBase、SQLResultSet、DBUpgrade、TransAction、TableMacro）+ sqlite/mysql/duckdb | 同步与异步双接口；MySQL 基于 Boost.MySQL（Pimpl + statement 缓存） |
| HTTP | http_client/（AsioHttpClient、url） | 基于 Boost.Beast，支持协程与流式响应（`requestStream` + 回调） |
| 配置解析 | ini_parser/（IniParser） | INI 文件解析 |
| 进程间通信 | node/（NodeClient、NodeServer、NodeMessage、NodeError） | 基于 nng 的 reqrep 服务/客户端 |
| 插件 | plugin/（PluginBase、PluginClient、PluginLoader、PluginManager） | 纯 header；`shared_mutex` 读写分离，动态加载 `.so/.dll/.dylib` |
| 其他 | TimerManager / SpendTimer / FilterNode / DllLoader / FileLock / LruCache / LRUCache11 | 定时器、耗时统计、过滤器、动态库加载、文件锁、缓存 |

## 7. 文档

- `docs/` 存放说明性文档（如 HTTP 流式处理：`STREAMING_HTTP_SUMMARY.md`、`HTTP_STREAMING_QUICK_REFERENCE.md`、`streaming_http_example.md`），新增文档优先使用 Markdown。
- `release.md` 记录版本变更，遵循简洁的变更条目（如 `feat(xxx): 描述`、`fix(xxx): 描述`），**每次改动对外行为时同步追加**。
- `readme.md` 为项目简介，保持简洁。

## 8. AI 开发工作流与注意事项

1. **定位代码**：源码/头文件 → `hikyuu/utilities/`；测试 → `test/utilities/`；构建与依赖 → `xmake.lua`、`test/xmake.lua`。
2. **修改 C++ 后必须重新编译验证**：`xmake -b hku_utils`（及 `xmake -b unit-test`），确认无编译告警/错误。
3. **只增删测试或文档时无需改动库 target**，但仍应确保 `unit-test` 可构建并运行通过。
4. **编译产物不要提交**：`*.so`、`*.dylib`、`*.dll`、`*.a`、`*.lib`、`build/`、生成的 `hikyuu/utilities/config.h` 与 `version.h`、`cover-*`、`coverage.xml` 均在 `.gitignore` 中。
5. **新增依赖**：在 `xmake.lua` 中 `add_requires`（注意平台差异与版本，如 MySQL 在 macosx 为 8.0.40、windows 为 8.0.21），并同步 `test/xmake.lua` 的测试依赖。
6. **条件编译**：新增可选能力时，使用 `set_configvar` + `config.h.in` + `#if HKU_*` 的方式接入，避免破坏默认构建。
7. **测试优先**：改动涉及核心模块时至少运行 `xmake r unit-test`；数据库/网络相关改动若无真实环境，走豁免路径并避免破坏默认（`mysql=n`、`http_client=n`）构建。
8. **CI 会验证**：`.github/workflows/` 下 `macos.yml`、`ubuntu.yml`、`windows.yml`、`windows_arm64.yml` 四套流水线（分支 `main`，matrix: static/shared，部分含 aarch64/x86_64、arm64），PR 合入前需通过构建。
9. **提交信息**：统一使用**英文**，简短风格，遵循 conventional commits（`fix(xxx): ...` / `feat(xxx): ...`），首字母小写、不加句号，不包含问题单号（如 ISS-xxx、#NN 等）。
10. **发布**：执行 `python publish.py` 生成版本 zip 并更新 `hikyuu_extern_libs` 仓库中的包定义；发布前记得更新 `xmake.lua` 的 `set_version` 与 `release.md`。
11. **谨慎处理**：修改 `xmake.lua` 的依赖顺序（如 `openssl3` 必须在 `boost` 之前）或包配置后，须重新 `xmake f` 配置；Windows 使用 clang-cl 编译，注意平台差异宏。

## 9. 快速自查清单（提交前）

- [ ] `clang-format` 已格式化改动文件
- [ ] C++ 改动已编译通过（`xmake -b hku_utils`）
- [ ] 相关单测已运行（`xmake r unit-test`）
- [ ] 新增可选能力已正确接入 `config.h.in` + `#if HKU_*` 条件编译
- [ ] 对外行为变更已记录到 `release.md`
- [ ] 未提交任何编译产物/生成文件/本地数据
