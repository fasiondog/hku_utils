# HTTP 流式处理使用示例

## 概述

新增了流式 HTTP 请求功能，支持大响应体的分块处理，避免一次性加载到内存导致内存溢出。

## 核心特性

1. **自动适配**：自动识别 `Content-Length` 和 `Transfer-Encoding: chunked` 两种模式
2. **内存友好**：数据分块处理（默认 8KB），不会一次性加载整个响应
3. **回调驱动**：通过回调函数实时处理接收到的数据块
4. **完整统计**：提供已读取字节数等统计信息

## API 接口

### 1. 基础流式请求方法

```cpp
net::awaitable<AsioHttpStreamResponse> requestStream(
    const std::string& method, 
    const std::string& path,
    const HttpParams& params, 
    const HttpHeaders& headers,
    const char* body, size_t body_len, 
    const std::string& content_type,
    const HttpChunkCallback& chunk_callback);
```

### 2. 便捷的 GET/POST 流式方法

```cpp
// GET 流式请求
net::awaitable<AsioHttpStreamResponse> getStream(
    const std::string& path,
    const HttpParams& params,
    const HttpHeaders& headers,
    const HttpChunkCallback& chunk_callback);

// POST 流式请求
net::awaitable<AsioHttpStreamResponse> postStream(
    const std::string& path,
    const HttpParams& params,
    const HttpHeaders& headers,
    const char* body, size_t body_len,
    const std::string& content_type,
    const HttpChunkCallback& chunk_callback);
```

## 使用示例

### 示例 1：下载大文件

```cpp
#include "hikyuu/utilities/http_client/AsioHttpClient.h"
#include <boost/asio.hpp>
#include <fstream>

using namespace hku;

boost::asio::awaitable<void> downloadLargeFile() {
    try {
        AsioHttpClient client("http://example.com");
        
        // 打开文件用于写入
        std::ofstream outputFile("download.bin", std::ios::binary);
        
        // 流式下载文件
        auto response = co_await client.getStream(
            "/largefile.zip",
            {},  // params
            {},  // headers
            [&outputFile](const char* data, size_t size) {
                // 每接收到一块数据就立即写入文件
                outputFile.write(data, static_cast<std::streamsize>(size));
                HKU_INFO("Downloaded {} bytes", size);
            }
        );
        
        HKU_INFO("Download completed: {} bytes, status: {}", 
                 response.totalBytesRead(), response.status());
        
    } catch (const std::exception& e) {
        HKU_ERROR("Download failed: {}", e.what());
    }
}

int main() {
    boost::asio::io_context ctx;
    boost::asio::co_spawn(ctx, downloadLargeFile(), boost::asio::detached);
    ctx.run();
    return 0;
}
```

### 示例 2：处理流式 JSON 数据

```cpp
boost::asio::awaitable<void> processStreamingJson() {
    try {
        AsioHttpClient client("http://api.example.com");
        
        std::string json_buffer;
        
        auto response = co_await client.postStream(
            "/streaming-api",
            {},
            {},
            R"({"query": "real-time data"})",
            26,
            "application/json",
            [&json_buffer](const char* data, size_t size) {
                // 累积 JSON 数据
                json_buffer.append(data, size);
                
                // 尝试解析完整的 JSON 对象
                // 注意：实际应用中可能需要更复杂的逻辑来处理分块的 JSON
                HKU_INFO("Received {} bytes of JSON data", size);
            }
        );
        
        if (response.status() == 200) {
            auto result = json::parse(json_buffer);
            HKU_INFO("JSON processing completed");
        }
        
    } catch (const std::exception& e) {
        HKU_ERROR("Stream processing failed: {}", e.what());
    }
}
```

### 示例 3：实时日志上报

```cpp
boost::asio::awaitable<void> uploadLogs() {
    try {
        AsioHttpClient client("http://log-server.com");
        
        std::string log_data = generateLargeLogData();
        size_t total_sent = 0;
        
        auto response = co_await client.postStream(
            "/upload",
            {},
            {{"X-Log-Type", "application"}},
            log_data.data(),
            log_data.size(),
            "text/plain",
            [&total_sent](const char* data, size_t size) {
                total_sent += size;
                HKU_INFO("Server response: {} bytes received", size);
            }
        );
        
        HKU_INFO("Log upload completed, server responded with {} bytes", 
                 response.totalBytesRead());
        
    } catch (const std::exception& e) {
        HKU_ERROR("Log upload failed: {}", e.what());
    }
}
```

### 示例 4：处理 SSE (Server-Sent Events)

```cpp
boost::asio::awaitable<void> subscribeToSSE() {
    try {
        AsioHttpClient client("http://sse-server.com");
        
        auto response = co_await client.getStream(
            "/events",
            {},
            {{"Accept", "text/event-stream"}},
            [](const char* data, size_t size) {
                // 实时处理 SSE 事件
                std::string event(data, size);
                HKU_INFO("SSE Event: {}", event);
                
                // 可以在这里解析 SSE 格式：data: xxx\n\n
            }
        );
        
        HKU_INFO("SSE subscription completed");
        
    } catch (const std::exception& e) {
        HKU_ERROR("SSE failed: {}", e.what());
    }
}
```

## 性能优势

### 传统方式（一次性加载）
```cpp
// ❌ 对于大文件会占用大量内存
auto response = co_await client.get("/large-file");
std::string body = response.body();  // 全部加载到内存
// 内存占用：文件大小（可能几百 MB 甚至几 GB）
```

### 流式方式（分块处理）
```cpp
// ✅ 固定内存占用（默认 8KB 缓冲区）
co_await client.getStream("/large-file", {}, {}, 
    [](const char* data, size_t size) {
        // 立即处理数据块，不需要存储整个文件
        processChunk(data, size);
    });
// 内存占用：~8KB（恒定）
```

## 注意事项

1. **回调函数应该是非阻塞的**：回调函数中不应该执行耗时操作，否则会阻塞网络接收
2. **异常处理**：在回调中抛出的异常会被捕获并终止请求
3. **线程安全**：回调函数可能在网络线程中被调用，注意共享数据的线程安全
4. **超时控制**：流式请求同样受配置的超时时间限制
5. **连接关闭**：请求完成后会自动关闭连接，不支持长连接的多次读写

## 适用场景

✅ **适合使用流式处理的场景：**
- 大文件下载（视频、音频、压缩包等）
- 实时数据流（日志、监控数据等）
- Server-Sent Events (SSE)
- 大数据量的 API 响应
- 内存受限环境

❌ **不适合的场景：**
- 需要随机访问响应数据
- 需要完整数据才能处理的场景（如 JSON 解析）
- 小响应体（< 1MB），传统方式即可

## 技术细节

### 缓冲区大小
默认使用 8KB (8192 字节) 的缓冲区，这个大小在性能和内存占用之间取得了平衡。

### 自动识别传输模式
- **Content-Length**: 已知大小的响应
- **Transfer-Encoding: chunked**: 分块传输编码

Boost.Beast 的 `buffer_body` 会自动处理这两种模式。

### 错误处理
- 网络错误会抛出异常
- 超时错误会抛出 `HttpTimeoutException`
- 回调中的异常会传播到协程
