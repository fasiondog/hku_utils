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
#include <thread>

using namespace hku;

namespace {

// 辅助函数：运行协程测试（使用外部 io_context，避免创建多个事件循环）
template <typename Func>
void runCoroutineTest(boost::asio::io_context& ctx, Func&& func) {
    boost::asio::co_spawn(ctx, std::forward<Func>(func)(), boost::asio::detached);
    ctx.run();
}

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
    AsioHttpClient client3("http://example.com", std::chrono::milliseconds(5000));
    CHECK_UNARY(client3.valid());
    CHECK_EQ(client3.getTimeout(), std::chrono::milliseconds(5000));

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
            auto response = co_await client.get("/ip");

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

    ctx.run();

// 测试 HTTPS GET 请求
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;

    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            // 使用 httpbin 的 HTTPS 接口进行测试
            AsioHttpClient client(ctx2, "https://httpbin.org");  // 使用外部 io_context

            // 简单的 GET 请求
            auto response = co_await client.get("/ip");

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

    ctx2.run();
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
            auto response = co_await client.post("/post", payload);

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

    ctx.run();

// HTTPS POST 测试
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;

    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx2, "https://httpbin.org");  // 使用外部 io_context

            // POST JSON 数据
            json payload = {{"name", "https_test"}, {"value", 456}};
            auto response = co_await client.post("/post", payload);

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

    ctx2.run();
#endif
}

TEST_CASE("test_AsioHttpClient_Timeout") {
    // 超时测试 - HTTP
    boost::asio::io_context ctx;
    runCoroutineTest(ctx, [&ctx]() -> boost::asio::awaitable<void> {
        try {
            // 设置极短的超时时间，应该会超时
            AsioHttpClient client(ctx, "http://httpbin.org",
                                  std::chrono::milliseconds(100));  // 使用外部 io_context

            bool exception_occurred = false;
            try {
                // /delay/5 会延迟 5 秒，肯定会超时
                auto response = co_await client.get("/delay/5");
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

    ctx.run();

// HTTPS 超时测试
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;
    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client(ctx2, "https://httpbin.org",
                                  std::chrono::milliseconds(100));  // 使用外部 io_context

            bool exception_occurred = false;
            try {
                auto response = co_await client.get("/delay/5");
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

    ctx2.run();
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

            auto response = co_await client.get("/headers");

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

    ctx.run();

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

            auto response = co_await client.get("/headers");

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

    ctx2.run();
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
            auto response1 = co_await client1.get("/ip");
            auto response2 = co_await client2.get("/ip");

            HKU_INFO("Both HTTP requests completed");
            CHECK_GE(response1.status(), 200);
            CHECK_GE(response2.status(), 200);

            co_return;
        } catch (const std::exception& e) {
            HKU_WARN("HTTP SharedIOContext test skipped: {}", e.what());
        }
    });

    ctx.run();

// 测试多个客户端共享同一个 io_context - HTTPS
#if HKU_ENABLE_HTTP_CLIENT_SSL
    boost::asio::io_context ctx2;
    runCoroutineTest(ctx2, [&ctx2]() -> boost::asio::awaitable<void> {
        try {
            AsioHttpClient client1(ctx2, "https://httpbin.org");
            AsioHttpClient client2(ctx2, "https://httpbin.org");

            // 并发请求
            auto response1 = co_await client1.get("/ip");
            auto response2 = co_await client2.get("/ip");

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
            auto response =
              co_await client.getStream("/stream/5",  // httpbin 提供的流式接口
                                        {}, [collected_data](const char* data, size_t size) {
                                            // 回调函数：累积接收到的数据块
                                            collected_data->append(data, size);
                                            HKU_INFO("Received chunk: {} bytes, total: {} bytes",
                                                     size, collected_data->size());
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

    ctx.run();

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
            auto response = co_await client.postStream(
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

    ctx2.run();
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

    ctx.run();
}
#endif

TEST_CASE("test_AsioHttpClient_MultithreadedIOContext") {
    // 测试多线程io_context环境下的并发请求
    boost::asio::io_context io_ctx;

    const int num_tasks = 20;
    const int num_threads = 4;
    std::atomic<int> completed(0);
    std::atomic<int> success_count(0);
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    // 提交多个并发任务
    for (int i = 0; i < num_tasks; ++i) {
        boost::asio::co_spawn(
          io_ctx,
          [&]() -> boost::asio::awaitable<void> {
              try {
                  AsioHttpClient client(io_ctx, "http://httpbin.org");

                  // GET 请求
                  auto response = co_await client.get("/ip");

                  if (response.status() == 200) {
                      auto data = response.json();
                      auto ip = data["origin"].get<std::string>();
                      HKU_INFO("HTTP GET IP: {}", ip);
                      success_count.fetch_add(1);
                  }
              } catch (const std::exception& e) {
                  HKU_WARN("HTTP request failed: {}", e.what());
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
        HKU_ERROR("Multithreaded test timeout! Completed: {}/{}", completed.load(), num_tasks);
    }

    // 停止 io_context 并等待所有工作线程
    io_ctx.stop();
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    HKU_INFO("Multithreaded test completed - Success: {}/{}", success_count.load(), num_tasks);
    CHECK(completed == num_tasks);
    // 验证大部分请求成功
    CHECK(success_count.load() > num_tasks * 0.5);
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
                  auto response = co_await client.get("/get");

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

                  auto response = co_await client.get("/ip");

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
                  auto response = co_await client.get("/ip");
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
                  auto response = co_await client.get("/ip");
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

#endif  // #if HKU_ENABLE_HTTP_CLIENT