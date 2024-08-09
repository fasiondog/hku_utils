# 版本发布说明

## 1.0.5 -

1. fixed MySQLStatement::sub_getColumnAsBlob 未正确获取 blob 长度
2. fixed HttpClient 未正确处理含有多个值的 HttpParams

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
