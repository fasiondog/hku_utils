# HTTP 流式处理快速参考

## 核心概念

**传统方式** ❌:
```cpp
auto response = co_await client.get("/large-file");
std::string body = response.body();  // 全部加载到内存
```

**流式方式** ✅:
```cpp
co_await client.getStream("/large-file", {}, {}, 
    [](const char* data, size_t size) {
        processChunk(data, size);  // 逐块处理
    });
```

## API 速查

### GET 请求
```cpp
// 简单 GET
co_await client.getStream("/api/data", 
    [](const char* data, size_t size) {
        handleData(data, size);
    });

// 带参数 GET
co_await client.getStream("/api/data", 
    {{"key", "value"}},  // params
    {},                  // headers
    chunkCallback);
```

### POST 请求
```cpp
std::string body = R"({"data": "value"})";

co_await client.postStream("/api/submit",
    {},           // params
    {},           // headers
    body.data(),  // request body
    body.size(),
    "application/json",
    chunkCallback);
```

## 回调函数示例

### 写入文件
```cpp
std::ofstream file("output.bin", std::ios::binary);
co_await client.getStream("/file.zip", {}, {},
    [&file](const char* data, size_t size) {
        file.write(data, static_cast<std::streamsize>(size));
    });
```

### 统计数据
```cpp
size_t totalBytes = 0;
size_t chunkCount = 0;

co_await client.getStream("/data", {}, {},
    [&totalBytes, &chunkCount](const char* data, size_t size) {
        totalBytes += size;
        chunkCount++;
    });
```

### 实时处理
```cpp
co_await client.getStream("/stream", {}, {},
    [](const char* data, size_t size) {
        // 立即处理，不存储
        sendToWebSocket(data, size);
    });
```

## 响应信息

```cpp
auto response = co_await client.getStream("/data", {}, {}, callback);

response.status();              // HTTP 状态码
response.reason();              // 状态描述
response.getHeader("X-Custom"); // 自定义头
response.getContentLength();    // Content-Length
response.isChunked();           // 是否分块传输
response.totalBytesRead();      // 已读取字节数
```

## 性能对比

| 场景 | 传统方式 | 流式方式 |
|------|---------|---------|
| 100MB 文件 | ~100MB 内存 | ~8KB 内存 |
| 1GB 文件 | ~1GB 内存 | ~8KB 内存 |
| 小文件 (<1MB) | 稍快 | 稍慢（可忽略） |

## 注意事项

⚠️ **回调函数应该：**
- ✅ 快速返回，避免阻塞
- ✅ 使用值捕获或引用捕获（注意生命周期）
- ❌ 不要抛出异常（会终止请求）

⚠️ **超时控制：**
```cpp
client.setTimeout(std::chrono::seconds(30));  // 设置超时
```

⚠️ **错误处理：**
```cpp
try {
    co_await client.getStream("/data", {}, {}, callback);
} catch (const HttpTimeoutException& e) {
    HKU_ERROR("Timeout: {}", e.what());
} catch (const std::exception& e) {
    HKU_ERROR("Error: {}", e.what());
}
```

## 完整示例

```cpp
#include "hikyuu/utilities/http_client/AsioHttpClient.h"
#include <boost/asio.hpp>
#include <fstream>

using namespace hku;

boost::asio::awaitable<void> downloadFile() {
    try {
        AsioHttpClient client("http://example.com");
        
        std::ofstream file("download.bin", std::ios::binary);
        
        auto response = co_await client.getStream(
            "/largefile.zip",
            {},  // params
            {},  // headers
            [&file](const char* data, size_t size) {
                file.write(data, static_cast<std::streamsize>(size));
                HKU_INFO("Downloaded {} bytes", size);
            }
        );
        
        HKU_INFO("Download completed: {} bytes", response.totalBytesRead());
        
    } catch (const std::exception& e) {
        HKU_ERROR("Download failed: {}", e.what());
    }
}

int main() {
    boost::asio::io_context ctx;
    boost::asio::co_spawn(ctx, downloadFile(), boost::asio::detached);
    ctx.run();
    return 0;
}
```

## 更多信息

详细文档请参考：`streaming_http_example.md`
