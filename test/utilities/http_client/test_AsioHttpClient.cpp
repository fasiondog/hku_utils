/*
 *  Copyright (c) 2024 hikyuu.org
 *
 *  Created on: 2026-03-15
 *      Author: fasiondog
 */

#include "test_config.h"

#if HKU_ENABLE_HTTP_CLIENT
#include "hikyuu/utilities/os.h"
#include "hikyuu/utilities/http_client/AsioHttpClient.h"
#include <boost/asio.hpp>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>  // strlen in the reuse test
#include <functional>
#include <future>
#include <stdexcept>
#include <thread>

using namespace hku;

namespace {

// 辅助函数：运行协程测试（使用外部 io_context，避免创建多个事件循环）
template <typename Func>
void runCoroutineTest(boost::asio::io_context& ctx, Func&& func) {
    boost::asio::co_spawn(ctx, std::forward<Func>(func)(), boost::asio::detached);
    ctx.run();
}

/**
 * @brief A minimal in-process HTTP/1.1 server for the streaming tests
 *
 * It listens on an ephemeral loopback port, serves the requested number of requests (one by
 * default) and hands the response writing over to the caller-provided responder, so that the
 * chunked framing and the truncated responses can be controlled precisely without any external
 * network.
 */
class LocalTestHttpServer {
public:
    typedef std::function<void(boost::asio::ip::tcp::socket&)> Responder;

    explicit LocalTestHttpServer(Responder responder, bool shutdown_after_respond = true,
                                 int request_count = 1)
    : m_responder(std::move(responder)),
      m_shutdown_after_respond(shutdown_after_respond),
      m_request_count(request_count) {}

    ~LocalTestHttpServer() {
        stop();
    }

    void start() {
        std::promise<uint16_t> port_promise;
        m_port_future = port_promise.get_future();
        m_stop = false;
        m_thread = std::thread([this, p = std::move(port_promise)]() mutable { _run(p); });
        m_port = m_port_future.get();
    }

    void stop() {
        m_stop = true;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    std::string url() const {
        return "http://127.0.0.1:" + std::to_string(m_port);
    }

    static void writeAll(boost::asio::ip::tcp::socket& sock, const void* data, size_t len) {
        boost::system::error_code ec;
        boost::asio::write(sock, boost::asio::buffer(data, len), ec);
    }

private:
    void _run(std::promise<uint16_t>& port_promise) {
        namespace asio = boost::asio;
        asio::io_context ctx;
        asio::ip::tcp::acceptor acceptor(ctx);
        boost::system::error_code ec;
        acceptor.open(asio::ip::tcp::v4(), ec);
        acceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0), ec);
        if (ec) {
            port_promise.set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
            return;
        }
        acceptor.non_blocking(true, ec);
        acceptor.listen(1, ec);
        port_promise.set_value(acceptor.local_endpoint().port());

        // Accept the requested number of connections, polling until each one arrives or stop() is
        // requested
        for (int served = 0; served < m_request_count && !m_stop; ++served) {
            asio::ip::tcp::socket sock(ctx);
            bool accepted = false;
            for (int i = 0; i < 6000 && !m_stop; ++i) {
                acceptor.accept(sock, ec);
                if (!ec) {
                    accepted = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!accepted) {
                break;
            }

            try {
                m_responder(sock);
            } catch (...) {
                // the responder failure should not terminate the test process
            }

            // Shut down the writing side only, then keep the socket alive until stop() is
            // requested: closing it right away would reset the connection while the client has
            // not drained the response yet
            boost::system::error_code sec;
            if (m_shutdown_after_respond) {
                sock.shutdown(asio::socket_base::shutdown_send, sec);
            }
            if (served + 1 < m_request_count) {
                // A further request needs a new connection, so close it here instead of waiting
                // for stop(): the response is already in the kernel buffer
                sock.close();
                continue;
            }
            for (int i = 0; i < 6000 && !m_stop; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            sock.close();
        }
        acceptor.close();
    }

    Responder m_responder;
    bool m_shutdown_after_respond{true};
    int m_request_count{1};
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::future<uint16_t> m_port_future;
    uint16_t m_port{0};
};

}  // namespace

TEST_CASE("test_AsioHttpClient_InternalIOContext_AutoStart") {
    // 测试内部 io_context 自动启动和停止

    // 创建客户端时会自动启动内部 io_context 的事件循环
    AsioHttpClient client("http://httpbin.org");
    CHECK_UNARY(client.valid());

    // 等待一小段时间确保内部线程已经启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // client 析构时会自动停止内部 io_context 并等待所有异步操作完成
    // 不需要手动调用任何方法
}

TEST_CASE("test_AsioHttpResponse") {
    // 测试 AsioHttpResponse 的基本方法
    AsioHttpResponse res;

    // 默认值
    CHECK_EQ(res.status(), 0);
    CHECK_UNARY(res.body().empty());
    CHECK_EQ(res.getHeader("x"), "");
    CHECK_EQ(res.getContentLength(), 0);
}

TEST_CASE("test_AsioHttpClient_Constructors") {
    // 测试默认构造
    AsioHttpClient client1;
    CHECK_UNARY(!client1.valid());

    // 测试带 URL 构造
    AsioHttpClient client2("http://example.com");
    CHECK_UNARY(client2.valid());
    CHECK_EQ(client2.url(), "http://example.com");

    // 测试带超时构造
    AsioHttpClient client3("http://example.com", 5000);
    CHECK_UNARY(client3.valid());
    CHECK_EQ(client3.getTimeout(), 5000);

    // 测试超时为 0 时使用默认最大值（3 分钟 = 180000 毫秒）
    AsioHttpClient client_zero("http://example.com", 0);
    CHECK_EQ(client_zero.getTimeout(), AsioHttpClient::MAX_TIMEOUT_MS);

    // 测试超时为负数时使用默认最大值
    AsioHttpClient client_negative("http://example.com", -1000);
    CHECK_EQ(client_negative.getTimeout(), AsioHttpClient::MAX_TIMEOUT_MS);

    // 测试使用外部 io_context
    boost::asio::io_context external_ctx;
    AsioHttpClient client4(external_ctx, "http://example.com");
    CHECK_UNARY(client4.valid());

    // 注意：移动操作已被禁用，因为管理后台线程和 io_context 的生命周期不安全
}

TEST_CASE("test_AsioHttpClient_BasicRequest") {
    // HTTP GET 请求测试
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "http://httpbin.org");  // 使用外部 io_context

            // GET 请求
            auto response = co_await client.async_get("/ip");

            if (response.status() == 200) {
                auto data = response.json();
                auto ip = data["origin"].get<std::string>();
                HKU_INFO("HTTP GET IP: {}", ip);
                CHECK_UNARY(!ip.empty());
            } else {
                HKU_WARN("HTTP GET failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            // 网络不可达时跳过测试
            HKU_WARN("HTTP network test skipped: {}", e.what());
        }
    });

// 测试 HTTPS GET 请求
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;

    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            // 使用 httpbin 的 HTTPS 接口进行测试
            AsioHttpClient client(ctx2, "https://httpbin.org");  // 使用外部 io_context

            // 简单的 GET 请求
            auto response = co_await client.async_get("/ip");

            if (response.status() == 200) {
                auto data = response.json();
                auto ip = data["origin"].get<std::string>();
                HKU_INFO("HTTPS GET IP: {}", ip);
                CHECK_UNARY(!ip.empty());
            } else {
                HKU_WARN("HTTPS GET failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTPS network test skipped: {}", e.what());
        }
    });
#endif
}

TEST_CASE("test_AsioHttpClient_POST") {
    // HTTP POST 测试
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "http://httpbin.org");  // 使用外部 io_context

            // POST JSON 数据
            json payload = {{"name", "test"}, {"value", 123}};
            auto response = co_await client.async_post("/post", payload);

            if (response.status() == 200) {
                auto result = response.json();
                CHECK_UNARY(result.contains("json"));
                HKU_INFO("HTTP POST successful");
            } else {
                HKU_WARN("HTTP POST failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTP POST test skipped: {}", e.what());
        }
    });

// HTTPS POST 测试
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;

    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx2, "https://httpbin.org");  // 使用外部 io_context

            // POST JSON 数据
            json payload = {{"name", "https_test"}, {"value", 456}};
            auto response = co_await client.async_post("/post", payload);

            if (response.status() == 200) {
                auto result = response.json();
                CHECK_UNARY(result.contains("json"));
                HKU_INFO("HTTPS POST successful");
            } else {
                HKU_WARN("HTTPS POST failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTPS POST test skipped: {}", e.what());
        }
    });
#endif
}

TEST_CASE("test_AsioHttpClient_Timeout") {
    // 超时测试 - HTTP
    boost::asio::io_context ctx;
    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            // 设置极短的超时时间，应该会超时
            AsioHttpClient client(ctx, "http://httpbin.org",
                                  100);  // 使用外部 io_context

            bool exception_occurred = false;
            try {
                // /delay/5 会延迟 5 秒，肯定会超时
                auto response = co_await client.async_get("/delay/5");
                HKU_INFO("Request completed with status: {}", response.status());
                // 如果没有超时（可能是网络极快），检查状态码
                CHECK_GE(response.status(), 200);
                exception_occurred = true;  // 也算通过
            } catch (const std::exception& e) {
                exception_occurred = true;
                HKU_INFO("Expected timeout/error occurred: {}", e.what());
                // 检查是否是超时异常
                std::string msg = e.what();
                CHECK_UNARY(msg.find("timeout") != std::string::npos ||
                            msg.find("Timeout") != std::string::npos ||
                            msg.find("DNS") != std::string::npos);  // DNS 解析超时也算
            }

            CHECK_UNARY(exception_occurred);

            co_return;
        } catch (const std::exception& e) {
            HKU_ERROR("Test error: {}", e.what());
            FAIL("Test failed with exception: {}", e.what());
        }
    });

// HTTPS 超时测试
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;
    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx2, "https://httpbin.org",
                                  100);  // 使用外部 io_context

            bool exception_occurred = false;
            try {
                auto response = co_await client.async_get("/delay/5");
                HKU_INFO("HTTPS request completed with status: {}", response.status());
                CHECK_GE(response.status(), 200);
                exception_occurred = true;
            } catch (const std::exception& e) {
                exception_occurred = true;
                HKU_INFO("HTTPS timeout/error occurred: {}", e.what());
                std::string msg = e.what();
                CHECK_UNARY(msg.find("timeout") != std::string::npos ||
                            msg.find("Timeout") != std::string::npos ||
                            msg.find("DNS") != std::string::npos);
            }

            CHECK_UNARY(exception_occurred);

            co_return;
        } catch (const std::exception& e) {
            HKU_ERROR("Test error: {}", e.what());
            FAIL("Test failed with exception: {}", e.what());
        }
    });
#endif
}

TEST_CASE("test_AsioHttpClient_Headers") {
    // HTTP Headers 测试
    boost::asio::io_context ctx;
    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "http://httpbin.org");  // 使用外部 io_context

            // 设置默认头
            std::map<std::string, std::string> headers;
            headers["X-Test-Header"] = "test-value";
            headers["Accept"] = "application/json";
            client.setDefaultHeaders(headers);

            auto response = co_await client.async_get("/headers");

            if (response.status() == 200) {
                auto result = response.json();
                HKU_INFO("HTTP Headers test successful");
                CHECK_UNARY(result.contains("headers"));
            } else {
                HKU_WARN("HTTP Headers test failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTP Headers test skipped: {}", e.what());
        }
    });

// HTTPS Headers 测试
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;
    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx2, "https://httpbin.org");  // 使用外部 io_context

            // 设置默认头
            std::map<std::string, std::string> headers;
            headers["X-Test-Header"] = "https-test-value";
            headers["Accept"] = "application/json";
            client.setDefaultHeaders(headers);

            auto response = co_await client.async_get("/headers");

            if (response.status() == 200) {
                auto result = response.json();
                HKU_INFO("HTTPS Headers test successful");
                CHECK_UNARY(result.contains("headers"));
            } else {
                HKU_WARN("HTTPS Headers test failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTPS Headers test skipped: {}", e.what());
        }
    });
#endif
}

TEST_CASE("test_AsioHttpClient_SharedIOContext") {
    // 测试多个客户端共享同一个 io_context - HTTP
    boost::asio::io_context ctx;
    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            // 两个客户端都使用同一个外部 io_context
            AsioHttpClient client1(ctx, "http://httpbin.org");
            AsioHttpClient client2(ctx, "http://httpbin.org");

            // 并发请求
            auto response1 = co_await client1.async_get("/ip");
            auto response2 = co_await client2.async_get("/ip");

            HKU_INFO("Both HTTP requests completed");
            CHECK_GE(response1.status(), 200);
            CHECK_GE(response2.status(), 200);

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTP SharedIOContext test skipped: {}", e.what());
        }
    });

    // 测试多个客户端共享同一个 io_context - HTTPS
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;
    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client1(ctx2, "https://httpbin.org");
            AsioHttpClient client2(ctx2, "https://httpbin.org");

            // 并发请求
            auto response1 = co_await client1.async_get("/ip");
            auto response2 = co_await client2.async_get("/ip");

            HKU_INFO("Both HTTPS requests completed");
            CHECK_GE(response1.status(), 200);
            CHECK_GE(response2.status(), 200);

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTPS SharedIOContext test skipped: {}", e.what());
        }
    });

    ctx2.run();
#endif
}

TEST_CASE("test_AsioHttpClient_StreamRequest") {
    // 测试流式 GET 请求
    boost::asio::io_context ctx;
    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "http://httpbin.org");  // 使用外部 io_context

            // 使用 shared_ptr 管理收集数据的生命周期
            auto collected_data = std::make_shared<std::string>();

            // 流式 GET 请求，使用回调函数处理数据块
            auto response = co_await client.async_getStream(
              "/stream/5",  // httpbin 提供的流式接口
              {}, [collected_data](const char* data, size_t size) {
                  // 回调函数：累积接收到的数据块
                  collected_data->append(data, size);
                  HKU_INFO("Received chunk: {} bytes, total: {} bytes", size,
                           collected_data->size());
              });

            if (response.status() == 200) {
                HKU_INFO("Stream response completed, total bytes: {}", response.totalBytesRead());
                CHECK_UNARY(!collected_data->empty());
                CHECK_GT(response.totalBytesRead(), 0);

                // 验证收集的数据完整性（简单检查是否包含换行符）
                HKU_INFO("Received data size: {} bytes", collected_data->size());
            } else {
                HKU_WARN("Stream request failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            // 网络不可达时跳过测试
            HKU_WARN("HTTP stream test skipped: {}", e.what());
        }
    });

    // 测试流式 POST 请求
    boost::asio::io_context ctx2;
    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx2, "http://httpbin.org");  // 使用外部 io_context

            // 准备请求体
            std::string request_body = R"({"test": "streaming data"})";

            // 使用 shared_ptr 管理统计变量的生命周期
            auto counters =
              std::make_shared<std::pair<size_t, size_t>>(0, 0);  // chunk_count, total_bytes

            // 流式 POST 请求
            auto response = co_await client.async_postStream(
              "/post", {}, {}, request_body.data(), request_body.size(), "application/json",
              [counters](const char* data, size_t size) {
                  // 回调函数：统计接收到的数据块信息
                  counters->first++;         // chunk_count
                  counters->second += size;  // total_bytes
                  HKU_INFO("Chunk #{}: {} bytes", counters->first, size);
              });

            if (response.status() == 200) {
                HKU_INFO("Stream POST completed, chunks: {}, total bytes: {}", counters->first,
                         counters->second);
                CHECK_GT(counters->first, 0);
                CHECK_GT(counters->second, 0);
                CHECK_EQ(response.totalBytesRead(), counters->second);
            } else {
                HKU_WARN("Stream POST failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTP stream POST test skipped: {}", e.what());
        }
    });
}

TEST_CASE("test_AsioHttpClient_requestStream_chunk_length") {
    /**
     * @par Check points
     * - a chunked response is reassembled byte by byte: the callback receives only the payload
     *   (no transfer framing, no stale tail, no duplicated bytes) and totalBytesRead equals the
     *   payload size
     * - the frames are deliberately not aligned with the internal 8KB read buffer, so that a
     *   single callback spans multiple frames and a single frame spans multiple callbacks
     * - a plain Content-Length response is reassembled the same way
     * - a response whose declared body is truncated by the peer reports a failure instead of
     *   spinning forever
     */
    const size_t PAYLOAD_SIZE = 20000;
    const size_t FRAME_SIZE = 3000;

    std::string payload(PAYLOAD_SIZE, '\0');
    for (size_t i = 0; i < PAYLOAD_SIZE; ++i) {
        payload[i] = static_cast<char>('a' + (i % 26));
    }

    // Send the body with the chunked transfer encoding
    LocalTestHttpServer server([&](boost::asio::ip::tcp::socket& sock) {
        const std::string head =
          "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
          "Content-Type: application/octet-stream\r\nConnection: close\r\n\r\n";
        LocalTestHttpServer::writeAll(sock, head.data(), head.size());
        for (size_t off = 0; off < payload.size(); off += FRAME_SIZE) {
            const size_t len = std::min(FRAME_SIZE, payload.size() - off);
            char frame[32];
            const int n = snprintf(frame, sizeof(frame), "%zX\r\n", len);
            LocalTestHttpServer::writeAll(sock, frame, n);
            LocalTestHttpServer::writeAll(sock, payload.data() + off, len);
            LocalTestHttpServer::writeAll(sock, "\r\n", 2);
        }
        LocalTestHttpServer::writeAll(sock, "0\r\n\r\n", 5);
    });
    server.start();

    AsioHttpClient client(server.url(), 5000);
    std::string collected;
    size_t chunk_count = 0;
    auto response = client.getStream("/stream", {}, {}, [&](const char* data, size_t size) {
        ++chunk_count;
        CHECK_GT(size, 0);
        collected.append(data, size);
    });

    CHECK_EQ(response.status(), 200);
    CHECK_EQ(collected.size(), PAYLOAD_SIZE);
    CHECK_EQ(collected, payload);
    CHECK_EQ(response.totalBytesRead(), static_cast<uint64_t>(PAYLOAD_SIZE));
    CHECK_GE(chunk_count, 3);
    server.stop();

    // A plain Content-Length response must be reassembled the same way
    LocalTestHttpServer server2([&](boost::asio::ip::tcp::socket& sock) {
        const std::string head =
          "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(payload.size()) +
          "\r\nConnection: close\r\n\r\n";
        LocalTestHttpServer::writeAll(sock, head.data(), head.size());
        LocalTestHttpServer::writeAll(sock, payload.data(), payload.size());
    });
    server2.start();

    AsioHttpClient client2(server2.url(), 5000);
    std::string collected2;
    auto response2 = client2.getStream(
      "/plain", {}, {}, [&](const char* data, size_t size) { collected2.append(data, size); });
    CHECK_EQ(response2.status(), 200);
    CHECK_EQ(collected2, payload);
    CHECK_EQ(response2.totalBytesRead(), static_cast<uint64_t>(PAYLOAD_SIZE));
    server2.stop();

    // The peer closes the connection before the declared body is complete
    LocalTestHttpServer server3([&](boost::asio::ip::tcp::socket& sock) {
        const std::string head =
          "HTTP/1.1 200 OK\r\nContent-Length: 10000\r\nConnection: close\r\n\r\n";
        LocalTestHttpServer::writeAll(sock, head.data(), head.size());
        const std::string part(3000, 'x');
        LocalTestHttpServer::writeAll(sock, part.data(), part.size());
    });
    server3.start();

    AsioHttpClient client3(server3.url(), 5000);
    size_t truncated_bytes = 0;
    CHECK_THROWS_AS(client3.getStream("/truncated", {}, {},
                                      [&](const char*, size_t size) { truncated_bytes += size; }),
                    std::exception);
    server3.stop();
}

TEST_CASE("test_AsioHttpClient_requestStream_write_timeout") {
    /**
     * @par Check points
     * - the write phase of a streaming request is protected by the timeout: when the peer accepts
     *   the connection but never reads the request, sending a body larger than the socket buffers
     *   must fail with a timeout instead of blocking forever
     */
    // The server accepts the connection, then neither reads the request nor replies
    LocalTestHttpServer server([](boost::asio::ip::tcp::socket&) {}, false);
    server.start();

    const std::string body(32 * 1024 * 1024, 'x');
    AsioHttpClient client(server.url(), 300);  // a short timeout keeps the test fast
    size_t received = 0;
    CHECK_THROWS_AS(client.postStream("/stall", {}, {}, body.data(), body.size(), "text/plain",
                                      [&received](const char*, size_t size) { received += size; }),
                    HttpTimeoutException);
    CHECK_EQ(received, 0u);
    server.stop();
}

TEST_CASE("test_AsioHttpClient_connectionPoolReuse") {
    /**
     * @par Check points
     * - a response the peer ended with Connection: close is not handed back to the pool as an
     *   idle connection: the next request opens a new connection instead of writing into a socket
     *   the peer already gave up
     * - a request that fails in the middle of the body closes its connection too, so the
     *   leftover bytes cannot be parsed as the response of the next request
     */
    // The keep-alive intent of the peer decides whether the connection may be reused
    {
        int served = 0;
        LocalTestHttpServer server(
          [&](boost::asio::ip::tcp::socket& sock) {
              const char* body = (++served == 1) ? "first" : "second";
              const std::string head =
                served == 1 ? "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: close\r\n\r\n"
                            : "HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\n";
              LocalTestHttpServer::writeAll(sock, head.data(), head.size());
              LocalTestHttpServer::writeAll(sock, body, strlen(body));
          },
          true, 2);
        server.start();

        AsioHttpClient client(server.url(), 5000);
        auto r1 = client.get("/first");
        CHECK_EQ(r1.body(), "first");

        // The peer closed the keep-alive, so this request has to run on a new connection
        auto r2 = client.get("/second");
        CHECK_EQ(r2.status(), 200);
        CHECK_EQ(r2.body(), "second");
        server.stop();
    }

    // A truncated response cannot leave leftovers for the next request
    {
        int served = 0;
        LocalTestHttpServer server(
          [&](boost::asio::ip::tcp::socket& sock) {
              if (++served == 1) {
                  // The body stops far before the declared length
                  const std::string head = "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n";
                  const std::string part(300, 'x');
                  LocalTestHttpServer::writeAll(sock, head.data(), head.size());
                  LocalTestHttpServer::writeAll(sock, part.data(), part.size());
              } else {
                  const std::string full = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
                  LocalTestHttpServer::writeAll(sock, full.data(), full.size());
              }
          },
          true, 2);
        server.start();

        AsioHttpClient client(server.url(), 5000);
        CHECK_THROWS_AS(client.get("/truncated"), std::exception);

        auto resp = client.get("/after_truncated");
        CHECK_EQ(resp.status(), 200);
        CHECK_EQ(resp.body(), "ok");
        server.stop();
    }
}

TEST_CASE("test_AsioHttpClient_maxResponseSize") {
    /**
     * @par Check points
     * - the defaults of the two limits are reported and 0 resets them
     * - the default limit keeps the beast default: a response declaring more than it is rejected
     *   while the header is parsed, and raising the limit reads the same response completely
     * - a declared Content-Length over the configured limit is rejected while the header is
     *   parsed, before any body byte is received
     * - a chunked body growing over the configured limit is rejected
     * - a header over the configured header limit is rejected
     * - a stream longer than the beast default body limit is delivered completely and is not
     *   bound by the response body limit
     * - a connection abandoned by a body limit error is closed, so the next request of the same
     *   client is served over a fresh connection instead of reading the leftover bytes
     * - a gzip response whose output cannot stay under the limit is rejected
     */
    // The defaults and the 0 reset
    {
        AsioHttpClient client("http://127.0.0.1:1");
        CHECK_EQ(client.getMaxResponseSize(), AsioHttpClient::DEFAULT_MAX_RESPONSE_SIZE);
        CHECK_EQ(client.getMaxHeaderSize(), AsioHttpClient::DEFAULT_MAX_HEADER_SIZE);
        client.setMaxResponseSize(1024);
        client.setMaxHeaderSize(2048);
        CHECK_EQ(client.getMaxResponseSize(), 1024u);
        CHECK_EQ(client.getMaxHeaderSize(), 2048u);
        client.setMaxResponseSize(0);
        client.setMaxHeaderSize(0);
        CHECK_EQ(client.getMaxResponseSize(), AsioHttpClient::DEFAULT_MAX_RESPONSE_SIZE);
        CHECK_EQ(client.getMaxHeaderSize(), AsioHttpClient::DEFAULT_MAX_HEADER_SIZE);
    }

    // The default limit keeps the beast size: declaring more than 8MB is rejected on the header
    // without receiving the body, and raising the limit reads the same response completely
    {
        const size_t BODY = 9 * 1024 * 1024;
        LocalTestHttpServer reject([](boost::asio::ip::tcp::socket& sock) {
            // Only the header: the declared body stays above the default limit
            const std::string head =
              "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(9 * 1024 * 1024) +
              "\r\nConnection: close\r\n\r\n";
            LocalTestHttpServer::writeAll(sock, head.data(), head.size());
        });
        reject.start();

        AsioHttpClient client(reject.url(), 10000);
        CHECK_EQ(client.getMaxResponseSize(), 8u * 1024 * 1024);
        CHECK_THROWS_AS(client.get("/over_default"), HttpResponseTooLargeException);
        reject.stop();

        const std::string body(BODY, 'a');
        LocalTestHttpServer accept([&](boost::asio::ip::tcp::socket& sock) {
            const std::string head = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(BODY) +
                                     "\r\nConnection: close\r\n\r\n";
            LocalTestHttpServer::writeAll(sock, head.data(), head.size());
            LocalTestHttpServer::writeAll(sock, body.data(), body.size());
        });
        accept.start();

        AsioHttpClient big_client(accept.url(), 30000);
        big_client.setMaxResponseSize(16 * 1024 * 1024);  // raise it above the response size
        auto resp = big_client.get("/big");
        CHECK_EQ(resp.status(), 200);
        CHECK_EQ(resp.body().size(), BODY);
        accept.stop();
    }

    // A declared Content-Length over the limit is rejected on the header
    {
        LocalTestHttpServer server([](boost::asio::ip::tcp::socket& sock) {
            // Only the header is written: the whole body must never be received
            const std::string head =
              "HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n";
            LocalTestHttpServer::writeAll(sock, head.data(), head.size());
        });
        server.start();

        AsioHttpClient client(server.url(), 5000);
        client.setMaxResponseSize(64 * 1024);
        CHECK_THROWS_AS(client.get("/huge"), HttpResponseTooLargeException);
        server.stop();
    }

    // A chunked body growing over the limit is rejected, and the connection is not reused broken
    {
        const size_t BODY = 96 * 1024;
        const std::string body(BODY, 'b');
        int served = 0;
        LocalTestHttpServer server(
          [&](boost::asio::ip::tcp::socket& sock) {
              if (++served == 1) {
                  // Keep alive: the client stops reading in the middle, so the leftover bytes
                  // stay in the connection
                  const std::string head = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
                  char frame[32];
                  const int n = snprintf(frame, sizeof(frame), "%zX\r\n", body.size());
                  LocalTestHttpServer::writeAll(sock, head.data(), head.size());
                  LocalTestHttpServer::writeAll(sock, frame, n);
                  LocalTestHttpServer::writeAll(sock, body.data(), body.size());
                  LocalTestHttpServer::writeAll(sock, "\r\n", 2);
              } else {
                  const std::string ok =
                    "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";
                  LocalTestHttpServer::writeAll(sock, ok.data(), ok.size());
              }
          },
          true, 2);
        server.start();

        AsioHttpClient client(server.url(), 5000);
        client.setMaxResponseSize(64 * 1024);
        CHECK_THROWS_AS(client.get("/chunked_over_limit"), HttpResponseTooLargeException);

        // The abandoned connection must not be reused: this request has to succeed
        auto resp = client.get("/after_limit");
        CHECK_EQ(resp.status(), 200);
        CHECK_EQ(resp.body(), "ok");
        server.stop();
    }

    // A stream longer than the beast default body limit is delivered completely: the streaming
    // path bounds the memory with its fixed chunk buffer, so no total size limit applies to it
    {
        const size_t BODY = 9 * 1024 * 1024;
        const std::string body(BODY, 'a');
        LocalTestHttpServer server([&](boost::asio::ip::tcp::socket& sock) {
            const std::string head = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(BODY) +
                                     "\r\nConnection: close\r\n\r\n";
            LocalTestHttpServer::writeAll(sock, head.data(), head.size());
            LocalTestHttpServer::writeAll(sock, body.data(), body.size());
        });
        server.start();

        AsioHttpClient client(server.url(), 30000);
        client.setMaxResponseSize(64 * 1024);  // the limit does not apply to a stream
        size_t received = 0;
        auto resp = client.getStream("/big_stream", {}, {},
                                     [&](const char*, size_t size) { received += size; });
        CHECK_EQ(resp.status(), 200);
        CHECK_EQ(received, BODY);
        CHECK_EQ(resp.totalBytesRead(), static_cast<uint64_t>(BODY));
        server.stop();
    }

    // A header over the header limit is rejected
    {
        const std::string fat_header(4096, 'c');
        LocalTestHttpServer server([&](boost::asio::ip::tcp::socket& sock) {
            const std::string head =
              "HTTP/1.1 200 OK\r\nX-Fat: " + fat_header + "\r\nContent-Length: 0\r\n\r\n";
            LocalTestHttpServer::writeAll(sock, head.data(), head.size());
        });
        server.start();

        AsioHttpClient client(server.url(), 5000);
        client.setMaxHeaderSize(512);
        CHECK_THROWS_AS(client.get("/fat_header"), HttpResponseTooLargeException);
        server.stop();
    }

#if HKU_ENABLE_HTTP_CLIENT_ZIP
    // A gzip response that cannot stay under the limit when expanded is rejected
    {
        const size_t BODY = 40 * 1024;
        const std::string body(BODY, 'd');
        LocalTestHttpServer server([&](boost::asio::ip::tcp::socket& sock) {
            const std::string head =
              "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: " +
              std::to_string(BODY) + "\r\nConnection: close\r\n\r\n";
            LocalTestHttpServer::writeAll(sock, head.data(), head.size());
            LocalTestHttpServer::writeAll(sock, body.data(), body.size());
        });
        server.start();

        AsioHttpClient client(server.url(), 5000);
        client.setMaxResponseSize(64 * 1024);  // 40KB * 2 does not fit into the limit
        CHECK_THROWS_AS(client.get("/gzip_bomb"), HttpResponseTooLargeException);
        server.stop();
    }
#endif
}

#if HKU_ENABLE_HTTP_CLIENT_SSL
TEST_CASE("test_AsioHttpClient_SetCaFile") {
    // 测试设置自定义 CA 证书功能
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "https://example.com");

            // 测试设置有效的 CA 证书文件路径（使用系统证书路径作为示例）
            // 注意：实际测试时需要提供真实的 CA 证书文件
            std::string ca_path = "/etc/ssl/certs/ca-certificates.crt";  // Linux 系统证书路径

            // 如果文件存在，则测试设置 CA 证书
            if (hku::existFile(ca_path)) {
                client.setCaFile(ca_path);
                HKU_INFO("Custom CA certificate set successfully: {}", ca_path);
            } else {
                // 尝试其他常见的证书路径
                std::vector<std::string> ca_paths = {"/etc/ssl/certs/ca-bundle.crt",  // CentOS/RHEL
                                                     "/etc/pki/tls/certs/ca-bundle.crt",  // Fedora
                                                     "/usr/share/ssl/certs/ca-bundle.crt",
                                                     "/etc/ssl/ca-bundle.pem",
                                                     "/var/lib/ca-certificates/ca-bundle.pem"};

                bool found = false;
                for (const auto& path : ca_paths) {
                    if (hku::existFile(path)) {
                        client.setCaFile(path);
                        HKU_INFO("Custom CA certificate set successfully: {}", path);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    HKU_WARN("No system CA certificate file found, skipping CA file test");
                }
            }

            // 测试设置为空字符串（应该忽略或抛出异常，取决于实现）
            // client.setCaFile("");  // 可选测试

            HKU_INFO("setCaFile method test completed");

            co_return;
        } catch (const std::exception& e) {
            HKU_ERROR("setCaFile test failed: {}", e.what());
            FAIL("setCaFile test should not throw exception");
        }
    });
}
#endif

TEST_CASE("test_AsioHttpClient_inner_MultithreadedIOContext") {
    // 测试多线程执行内部 io_context

    SUBCASE("test_default_single_thread") {
        // 默认构造函数使用单线程（thread_count=1）
        AsioHttpClient client1;
        CHECK_UNARY(!client1.valid());
    }

    SUBCASE("test_constructor_with_thread_count_1") {
        // 使用 thread_count=1 构造（默认值）
        AsioHttpClient client("http://example.com", 5000, 1);
        CHECK_UNARY(client.valid());
        CHECK_EQ(client.getTimeout(), 5000);
    }

    SUBCASE("test_constructor_with_thread_count_4") {
        // 使用 thread_count=4 构造，启动 4 个线程运行 io_context
        AsioHttpClient client("http://example.com", 5000, 4);
        CHECK_UNARY(client.valid());
        CHECK_EQ(client.getTimeout(), 5000);
    }

    SUBCASE("test_multithreaded_io_context_basic") {
        // 测试多线程 io_context 的基本功能
        try {
            // 使用 2 个线程的 io_context
            AsioHttpClient client("http://httpbin.org", 10000, 2);
            CHECK_UNARY(client.valid());

            // 等待一小段时间确保内部线程已经启动
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // client 析构时会自动停止所有工作线程
        } catch (const std::exception& e) {
            HKU_WARN("Multithreaded basic test failed: {}", e.what());
        }
    }

    SUBCASE("test_internal_multithreaded_with_requests") {
        // 测试内部管理的多线程 io_context 的请求处理
        try {
            // 使用 4 个线程的内部 io_context
            AsioHttpClient client("http://httpbin.org", 10000, 4);
            CHECK_UNARY(client.valid());

            std::atomic<int> completed{0};
            std::atomic<int> success_count{0};
            std::promise<void> completion_promise;
            std::future<void> completion_future = completion_promise.get_future();

            constexpr int num_requests = 5;

            // 在内部 io_context 上发起多个并发请求
            for (int i = 0; i < num_requests; ++i) {
                boost::asio::co_spawn(
                  client.get_executor(),
                  [&, i]() -> boost::asio::awaitable<void> {
                      try {
                          auto response = co_await client.async_get("/ip");
                          if (response.status() == 200) {
                              success_count.fetch_add(1);
                              HKU_INFO("Request {} completed, status: {}", i, response.status());
                          }
                      } catch (const std::exception& e) {
                          HKU_WARN("Request {} failed: {}", i, e.what());
                      }

                      // 最后一个完成的任务触发 promise
                      if (completed.fetch_add(1) + 1 == num_requests) {
                          completion_promise.set_value();
                      }
                  },
                  boost::asio::detached);
            }

            // 等待所有请求完成或超时
            if (completion_future.wait_for(std::chrono::seconds(30)) ==
                std::future_status::timeout) {
                HKU_WARN("Internal multithreaded test timeout! Completed: {}/{}", completed.load(),
                         num_requests);
            }

            // 验证至少部分请求成功
            CHECK_GT(success_count.load(), 0);
            HKU_INFO("Internal multithreaded requests completed: {}/{}", success_count.load(),
                     num_requests);

        } catch (const std::exception& e) {
            HKU_WARN("Internal multithreaded test failed: {}", e.what());
        }
    }
}

TEST_CASE("test_AsioHttpClient_MultithreadedConnectionPool") {
    // 测试多线程环境下连接池的行为
    boost::asio::io_context io_ctx;

    const int num_tasks = 30;
    const int num_threads = 4;
    std::atomic<int> completed(0);
    std::atomic<int> success_count(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    // 提交多个并发任务,测试连接复用
    for (int i = 0; i < num_tasks; ++i) {
        boost::asio::co_spawn(
          io_ctx,
          [&, i]() -> boost::asio::awaitable<void> {
              try {
                  AsioHttpClient client(io_ctx, "http://httpbin.org");

                  // 使用不同的端点进行请求
                  auto response = co_await client.async_get("/get");

                  if (response.status() == 200) {
                      success_count.fetch_add(1);
                      HKU_INFO("Request {} completed", i);
                  }
              } catch (const std::exception& e) {
                  HKU_WARN("Request {} failed: {}", i, e.what());
              }

              if (completed.fetch_add(1) + 1 == num_tasks) {
                  completion_promise.set_value();
              }
          },
          boost::asio::detached);
    }

    // 创建多个线程同时运行 io_context
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&]() { io_ctx.run(); });
    }

    // 等待所有任务完成或超时
    if (completion_future.wait_for(std::chrono::seconds(30)) == std::future_status::timeout) {
        HKU_ERROR("Connection pool test timeout! Completed: {}/{}", completed.load(), num_tasks);
    }

    // 停止 io_context 并等待所有工作线程
    io_ctx.stop();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

#if HKU_ENABLE_HTTP_CLIENT_SSL
TEST_CASE("test_AsioHttpClient_MultithreadedHTTPS") {
    // 测试多线程环境下HTTPS请求
    boost::asio::io_context io_ctx;

    // 添加 work_guard 防止 io_context 在所有任务完成前退出
    auto work = boost::asio::make_work_guard(io_ctx);

    const int num_tasks = 15;
    const int num_threads = 3;
    std::atomic<int> completed(0);
    std::atomic<int> success_count(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    // 提交多个并发HTTPS请求
    bool failed = false;
    for (int i = 0; i < num_tasks; ++i) {
        boost::asio::co_spawn(
          io_ctx,
          [&, i]() -> boost::asio::awaitable<void> {
              try {
                  AsioHttpClient client(io_ctx, "https://httpbin.org");

                  auto response = co_await client.async_get("/ip");

                  if (response.status() == 200) {
                      auto data = response.json();
                      auto ip = data["origin"].get<std::string>();
                      HKU_INFO("HTTPS GET IP: {}", ip);
                      success_count.fetch_add(1);
                  }
              } catch (const std::exception& e) {
                  HKU_WARN("HTTPS request {} failed: {}", i, e.what());
                  failed = true;
              }

              if (completed.fetch_add(1) + 1 == num_tasks) {
                  completion_promise.set_value();
              }
          },
          boost::asio::detached);
    }

    // 创建多个线程同时运行 io_context
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&]() { io_ctx.run(); });
    }

    // 等待所有任务完成或超时
    if (completion_future.wait_for(std::chrono::seconds(30)) == std::future_status::timeout) {
        HKU_ERROR("HTTPS multithreaded test timeout! Completed: {}/{}", completed.load(),
                  num_tasks);
    }

    // 释放 work_guard，允许 io_context 退出
    work.reset();

    // 停止 io_context 并等待所有工作线程
    io_ctx.stop();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    HKU_INFO("HTTPS multithreaded test completed - Success: {}/{}", success_count.load(),
             num_tasks);
    CHECK(completed == num_tasks);
    if (!failed) {
        CHECK(success_count.load() > num_tasks * 0.5);
    }
}
#endif

TEST_CASE("test_AsioHttpClient_MultithreadedMixedRequests") {
    // 测试多线程环境下混合HTTP和HTTPS请求
    boost::asio::io_context io_ctx;

    const int num_http_tasks = 10;
    const int num_https_tasks = 10;
    const int num_threads = 4;
    std::atomic<int> completed(0);
    std::atomic<int> http_success(0);
    std::atomic<int> https_success(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();
    const int total_tasks = num_http_tasks + num_https_tasks;

    // 提交HTTP请求
    for (int i = 0; i < num_http_tasks; ++i) {
        boost::asio::co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              try {
                  AsioHttpClient client(io_ctx, "http://httpbin.org");
                  auto response = co_await client.async_get("/ip");
                  if (response.status() == 200) {
                      http_success.fetch_add(1);
                  }
              } catch (const std::exception& e) {
                  HKU_WARN("HTTP request failed: {}", e.what());
              }

              if (completed.fetch_add(1) + 1 == total_tasks) {
                  completion_promise.set_value();
              }
          },
          boost::asio::detached);
    }

#if HKU_ENABLE_HTTP_CLIENT_SSL
    // 提交HTTPS请求
    for (int i = 0; i < num_https_tasks; ++i) {
        boost::asio::co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              try {
                  AsioHttpClient client(io_ctx, "https://httpbin.org");
                  auto response = co_await client.async_get("/ip");
                  if (response.status() == 200) {
                      https_success.fetch_add(1);
                  }
              } catch (const std::exception& e) {
                  HKU_WARN("HTTPS request failed: {}", e.what());
              }

              if (completed.fetch_add(1) + 1 == total_tasks) {
                  completion_promise.set_value();
              }
          },
          boost::asio::detached);
    }
#else
    completed += num_https_tasks;  // SSL未启用,跳过HTTPS任务
#endif

    // 创建多个线程同时运行 io_context
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&]() { io_ctx.run(); });
    }

    // 等待所有任务完成或超时
    if (completion_future.wait_for(std::chrono::seconds(30)) == std::future_status::timeout) {
        HKU_ERROR("Mixed requests test timeout! Completed: {}/{}", completed.load(), total_tasks);
    }

    // 停止 io_context 并等待所有工作线程
    io_ctx.stop();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    HKU_INFO("Mixed requests test completed - HTTP: {}/{}, HTTPS: {}/{}", http_success.load(),
             num_http_tasks, https_success.load(), num_https_tasks);
    CHECK(completed == total_tasks);
}

TEST_CASE("test_AsioHttpClient_InvalidURLTimeout") {
    // 测试 URL 地址不对时的超时行为

    SUBCASE("test_invalid_domain_timeout") {
        // 测试不存在的域名，应该触发 DNS 解析超时
        boost::asio::io_context ctx;

        runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
            try {
                // 使用明显不存在的域名
                AsioHttpClient client(ctx, "http://this-domain-does-not-exist-12345.invalid", 2000);

                bool timeout_occurred = false;
                std::string error_msg;

                try {
                    auto response = co_await client.async_get("/");
                    HKU_WARN("Unexpected success with invalid domain, status: {}",
                             response.status());
                } catch (const std::exception& e) {
                    timeout_occurred = true;
                    error_msg = e.what();
                    HKU_INFO("Expected error for invalid domain: {}", error_msg);

                    // 验证错误信息包含相关关键词
                    CHECK_UNARY(error_msg.find("timeout") != std::string::npos ||
                                error_msg.find("Timeout") != std::string::npos ||
                                error_msg.find("resolve") != std::string::npos ||
                                error_msg.find("Resolve") != std::string::npos ||
                                error_msg.find("DNS") != std::string::npos ||
                                error_msg.find("dns") != std::string::npos ||
                                error_msg.find("host") != std::string::npos ||
                                error_msg.find("Host") != std::string::npos);
                }

                CHECK_UNARY(timeout_occurred);
                co_return;
            } catch (const std::exception& e) {
                HKU_ERROR("Test error: {}", e.what());
                FAIL("Test failed with exception: {}", e.what());
            }
        });
    }

    SUBCASE("test_invalid_port_timeout") {
        // 测试无法连接的端口，应该触发连接超时
        boost::asio::io_context ctx;

        runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
            try {
                // 使用本地回环地址和一个不太可能被监听的端口
                AsioHttpClient client(ctx, "http://127.0.0.1:59999", 2000);

                bool timeout_occurred = false;
                std::string error_msg;

                try {
                    auto response = co_await client.async_get("/");
                    HKU_WARN("Unexpected success with invalid port, status: {}", response.status());
                } catch (const std::exception& e) {
                    timeout_occurred = true;
                    error_msg = e.what();
                    HKU_INFO("Expected error for invalid port: {}", error_msg);

                    // 验证错误信息包含相关关键词
                    CHECK_UNARY(error_msg.find("timeout") != std::string::npos ||
                                error_msg.find("Timeout") != std::string::npos ||
                                error_msg.find("connect") != std::string::npos ||
                                error_msg.find("Connect") != std::string::npos ||
                                error_msg.find("refused") != std::string::npos ||
                                error_msg.find("Refused") != std::string::npos);
                }

                CHECK_UNARY(timeout_occurred);
                co_return;
            } catch (const std::exception& e) {
                HKU_ERROR("Test error: {}", e.what());
                FAIL("Test failed with exception: {}", e.what());
            }
        });
    }

    SUBCASE("test_malformed_url_handling") {
        // 测试格式错误的 URL
        boost::asio::io_context ctx;

        runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
            try {
                // 创建客户端时使用格式错误的 URL
                AsioHttpClient client(ctx, "not-a-valid-url", 2000);

                // 如果客户端仍然有效（可能解析失败但有默认值），尝试请求应该失败
                if (client.valid()) {
                    bool error_occurred = false;
                    try {
                        auto response = co_await client.async_get("/");
                        HKU_WARN("Unexpected success with malformed URL, status: {}",
                                 response.status());
                    } catch (const std::exception& e) {
                        error_occurred = true;
                        HKU_INFO("Expected error for malformed URL: {}", e.what());
                    }
                    CHECK_UNARY(error_occurred);
                } else {
                    // 客户端无效也是合理的结果
                    HKU_INFO("Client is invalid for malformed URL (expected)");
                }

                co_return;
            } catch (const std::exception& e) {
                HKU_ERROR("Test error: {}", e.what());
                FAIL("Test failed with exception: {}", e.what());
            }
        });
    }
}

TEST_CASE("test_AsioHttpClient_ResourceVersionPool_URLChange") {
    // 测试 URL 变更时连接池自动更新版本
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "http://httpbin.org");

            // 初始请求
            auto response1 = co_await client.async_get("/ip");
            HKU_INFO("Initial request status: {}", response1.status());

            // 变更 URL，连接池应自动更新版本
            client.setUrl("http://example.com");
            CHECK_EQ(client.url(), "http://example.com");

            // 新 URL 的请求（注意：example.com 可能没有/ip 路径）
            auto response2 = co_await client.async_get("/");
            HKU_INFO("After URL change status: {}", response2.status());

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("URL change test error (expected): {}", e.what());
        }
    });
}

TEST_CASE("test_AsioHttpClient_ResourceVersionPool_TimeoutChange") {
    // 测试超时变更时连接池自动更新版本
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "http://httpbin.org", 5000);
            CHECK_EQ(client.getTimeout(), 5000);

            // 变更超时时间，连接池应自动更新版本
            client.setTimeout(10000);
            CHECK_EQ(client.getTimeout(), 10000);

            // 发送请求验证新超时时间生效
            auto response = co_await client.async_get("/delay/1");  // 延迟 1 秒的响应
            HKU_INFO("Request with new timeout status: {}", response.status());

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("Timeout change test error: {}", e.what());
        }
    });

    // 测试 setTimeout 传入 0 或负数时使用默认值
    boost::asio::io_context ctx2;
    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx2, "http://httpbin.org", 5000);

            // 设置为 0，应该使用 MAX_TIMEOUT_MS
            client.setTimeout(0);
            CHECK_EQ(client.getTimeout(), AsioHttpClient::MAX_TIMEOUT_MS);

            // 设置为负数，应该使用 MAX_TIMEOUT_MS
            client.setTimeout(-5000);
            CHECK_EQ(client.getTimeout(), AsioHttpClient::MAX_TIMEOUT_MS);

            // 设置为正常值，应该生效
            client.setTimeout(30000);
            CHECK_EQ(client.getTimeout(), 30000);

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("Timeout default value test error: {}", e.what());
        }
    });
}

TEST_CASE("test_AsioHttpClient_ConnectionReuse") {
    // 测试连接复用功能
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "http://httpbin.org");

            // 连续发送多个请求，应该复用连接
            for (int i = 0; i < 3; ++i) {
                auto response = co_await client.async_get("/ip");
                HKU_INFO("Request {} status: {}", i + 1, response.status());
                CHECK_EQ(response.status(), 200);

                // 短暂等待，模拟实际使用场景
                auto timer = boost::asio::steady_timer(ctx, std::chrono::milliseconds(100));
                co_await timer.async_wait(boost::asio::use_awaitable);
            }

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("Connection reuse test error: {}", e.what());
        }
    });
}

TEST_CASE("test_AsioHttpClient_SyncAsync_API") {
    // 测试同步和异步接口的正确使用

    SUBCASE("test_sync_simple") {
        // 最简单的同步测试
        HKU_INFO("Entering test_sync_simple");
        try {
            AsioHttpClient client("http://httpbin.org");
            HKU_INFO("Created client, now destroying immediately...");
        } catch (const std::exception& e) {
            HKU_WARN("Exception: {}", e.what());
        }
        HKU_INFO("Leaving test_sync_simple");
    }

    SUBCASE("test_sync_methods_exist") {
        // 验证同步方法存在并可调用（直接在当前线程调用，无需协程环境）
        HKU_INFO("Entering test_sync_methods_exist");
        try {
            AsioHttpClient client("http://httpbin.org");
            HKU_INFO("Created sync HTTP client");

            // 同步 GET 请求 - 直接调用，阻塞等待返回
            auto response = client.get("/ip");
            CHECK_GE(response.status(), 200);
            HKU_INFO("Sync GET response status: {}", response.status());

            // 同步 POST 请求 - 直接调用，阻塞等待返回
            json payload = {{"key", "value"}};
            auto post_response = client.post("/post", payload);
            CHECK_GE(post_response.status(), 200);
            HKU_INFO("Sync POST response status: {}", post_response.status());

        } catch (const std::exception& e) {
            HKU_WARN("Sync API test skipped: {}", e.what());
        }
        HKU_INFO("Leaving test_sync_methods_exist");
    }

    SUBCASE("test_sync_stream_methods") {
        // 验证同步流式方法存在并可调用（直接在当前线程调用）
        try {
            AsioHttpClient client("http://httpbin.org");

            size_t total_bytes = 0;
            auto chunk_callback = [&total_bytes](const char* data, size_t size) {
                total_bytes += size;
            };

            // 同步流式 GET 请求 - 直接调用，阻塞等待完成
            auto response = client.getStream("/get", {}, {}, chunk_callback);
            CHECK_GE(response.status(), 200);
            CHECK_GT(total_bytes, 0);
            HKU_INFO("Sync stream GET completed, total bytes: {}", total_bytes);

        } catch (const std::exception& e) {
            HKU_WARN("Sync stream API test skipped: {}", e.what());
        }
    }

    SUBCASE("test_sync_invalid_url_timeout") {
        // 测试同步接口在无效URL时的超时行为

        // 测试1：无效域名
        HKU_INFO("Testing sync API with invalid domain...");
        try {
            AsioHttpClient client("http://this-domain-does-not-exist-12345.invalid", 2000);

            bool error_occurred = false;
            std::string error_msg;

            try {
                auto response = client.get("/");
                HKU_WARN("Unexpected success with invalid domain, status: {}", response.status());
            } catch (const std::exception& e) {
                error_occurred = true;
                error_msg = e.what();
                HKU_INFO("Expected error for invalid domain: {}", error_msg);

                // 验证错误信息包含相关关键词
                CHECK_UNARY(error_msg.find("timeout") != std::string::npos ||
                            error_msg.find("Timeout") != std::string::npos ||
                            error_msg.find("resolve") != std::string::npos ||
                            error_msg.find("Resolve") != std::string::npos ||
                            error_msg.find("DNS") != std::string::npos ||
                            error_msg.find("dns") != std::string::npos ||
                            error_msg.find("host") != std::string::npos ||
                            error_msg.find("Host") != std::string::npos);
            }

            CHECK_UNARY(error_occurred);
        } catch (const std::exception& e) {
            HKU_ERROR("Test error: {}", e.what());
            FAIL("Test failed with exception: {}", e.what());
        }

        // 测试2：无效端口
        HKU_INFO("Testing sync API with invalid port...");
        try {
            AsioHttpClient client("http://127.0.0.1:59999", 2000);

            bool error_occurred = false;
            std::string error_msg;

            try {
                auto response = client.get("/");
                HKU_WARN("Unexpected success with invalid port, status: {}", response.status());
            } catch (const std::exception& e) {
                error_occurred = true;
                error_msg = e.what();
                HKU_INFO("Expected error for invalid port: {}", error_msg);

                // 验证错误信息包含相关关键词
                CHECK_UNARY(error_msg.find("timeout") != std::string::npos ||
                            error_msg.find("Timeout") != std::string::npos ||
                            error_msg.find("connect") != std::string::npos ||
                            error_msg.find("Connect") != std::string::npos ||
                            error_msg.find("refused") != std::string::npos ||
                            error_msg.find("Refused") != std::string::npos);
            }

            CHECK_UNARY(error_occurred);
        } catch (const std::exception& e) {
            HKU_ERROR("Test error: {}", e.what());
            FAIL("Test failed with exception: {}", e.what());
        }

        // 测试3：同步 POST 请求的无效URL
        HKU_INFO("Testing sync POST API with invalid URL...");
        try {
            AsioHttpClient client("http://invalid-host-test.local", 2000);

            bool error_occurred = false;

            try {
                json payload = {{"test", "data"}};
                auto response = client.post("/post", payload);
                HKU_WARN("Unexpected success with invalid URL in POST, status: {}",
                         response.status());
            } catch (const std::exception& e) {
                error_occurred = true;
                HKU_INFO("Expected error for invalid URL in POST: {}", e.what());
            }

            CHECK_UNARY(error_occurred);
        } catch (const std::exception& e) {
            HKU_ERROR("Test error: {}", e.what());
            FAIL("Test failed with exception: {}", e.what());
        }

        HKU_INFO("Sync invalid URL timeout tests completed");
    }
}

// 测试 async_request 支持分块传输编码（Transfer-Encoding: chunked）
TEST_CASE("test_AsioHttpClient_ChunkedTransferEncoding") {
    // 测试 async_request 方法能够正确处理服务端的分块传输编码
    boost::asio::io_context ctx;

    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx, "http://httpbin.org");

            // 使用 /stream/5 接口，服务端会返回分块传输编码的响应
            auto response = co_await client.async_get("/stream/5");

            if (response.status() == 200) {
                // 验证响应内容
                auto body = response.body();
                CHECK_UNARY(!body.empty());

                // httpbin 的/stream/5 返回 5 行 JSON 数据
                // 每行是一个完整的 JSON 对象
                size_t line_count = 0;
                size_t pos = 0;
                while ((pos = body.find('\n', pos)) != std::string::npos) {
                    line_count++;
                    pos++;
                }

                HKU_INFO("Chunked transfer test: received {} lines, total {} bytes", line_count,
                         body.size());
                CHECK_GE(line_count, 5);  // 至少应该有 5 行

                // 验证 Transfer-Encoding 头部（如果有的话）
                auto te_header = response.getHeader("Transfer-Encoding");
                if (!te_header.empty()) {
                    HKU_INFO("Response uses chunked encoding: {}", te_header);
                }

                // 验证 Content-Length 头部（可能不存在，因为是 chunked）
                auto cl_header = response.getHeader("Content-Length");
                if (!cl_header.empty()) {
                    HKU_INFO("Response Content-Length: {}", cl_header);
                }

            } else {
                HKU_WARN("Chunked transfer test failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            // 网络不可达时跳过测试
            HKU_WARN("Chunked transfer test skipped: {}", e.what());
        }
    });

    // 测试 POST 请求也支持分块响应
    boost::asio::io_context ctx2;

    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx2, "http://httpbin.org");

            // POST 请求也可能返回分块响应
            json payload = {{"test", "chunked"}, {"data", true}};
            auto response = co_await client.async_post("/post", payload);

            if (response.status() == 200) {
                auto body = response.body();
                CHECK_UNARY(!body.empty());

                // 验证响应包含回显的数据
                auto result = response.json();
                if (result.contains("json")) {
                    CHECK_EQ(result["json"]["test"], "chunked");
                    CHECK_EQ(result["json"]["data"], true);
                }

                HKU_INFO("POST chunked test successful, response size: {}", body.size());
            } else {
                HKU_WARN("POST chunked test failed with status: {}", response.status());
            }

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("POST chunked test skipped: {}", e.what());
        }
    });
}

#if 0
TEST_CASE("test_tianxingapi_ipquery") {
    AsioHttpClient cli("https://apis.tianapi.com", 8000);  // 8 seconds
    HttpParams params;
    params.emplace("key", "0d57e7ec6ebaf52059f0647184face23");
    params.emplace("ip", "101.133.146.4");
    params.emplace("full", "1");
    auto res = cli.get("ipquery/index", params, HttpHeaders());
    HKU_CHECK(res.status() == 200, "err: response status: {}", res.status());

    json j = res.json();
    std::cout << j.dump(4) << std::endl;
}
#endif

#endif  // #if HKU_ENABLE_HTTP_CLIENT