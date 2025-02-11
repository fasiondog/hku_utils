# 版本发布说明

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
