/*
 *  Copyright (c) 2019~2021, hikyuu
 *
 *  Created on: 2021/12/06
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <vector>
#include <hikyuu/utilities/md5.h>
#include <hikyuu/utilities/Log.h>

using namespace hku;

TEST_CASE("test_md5") {
    // 传入的数据指针为 null
    CHECK_THROWS_AS(md5(nullptr, 1), hku::exception);

    // 传入的数据长度为 0
    std::string a("99983");
    CHECK_EQ(md5((const unsigned char*)a.data(), 0),
             std::string("d41d8cd98f00b204e9800998ecf8427e"));

    // 正常计算 md5
    CHECK_EQ(md5((const unsigned char*)a.data(), a.size()),
             std::string("43d62713df20a658fa61ed5fb6c3040d"));
    CHECK_EQ(md5(a), std::string("43d62713df20a658fa61ed5fb6c3040d"));
}
// ============================================================================
// L7/L22 回归：填充边界参考向量、跨位长数字边界的长输入、异常安全
// ============================================================================

TEST_CASE("test_md5_padding_boundaries") {
    // 与参考实现（hashlib）对照的填充边界向量：55/56/57 覆盖"0x80 恰好挤占长度字段需
    // 追加整块"的边界，63/64/65 覆盖整块对齐
    struct Vector {
        size_t len;
        char fill;
        const char* digest;
    };

    static const Vector vectors[] = {
      {0, 'A', "d41d8cd98f00b204e9800998ecf8427e"},
      {55, 'A', "e38a93ffe074a99b3fed47dfbe37db21"},
      {56, 'A', "a2f3e2024931bd470555002aa5ccc010"},
      {57, 'A', "9a7c38569e5a96e3cfbad45fb9ce5209"},
      {63, 'A', "5f1c4bb2970471a5c75b7ba1dc9ee3ed"},
      {64, 'A', "d289a97565bc2d27ac8b8545a5ddba45"},
      {65, 'A', "162b6d6eb17cd9da55f95f8c73a32dda"},
      {119, 'B', "327460bcadf6634f0069b1f0806489a0"},
      {120, 'B', "3bb9f6d07ed99b08f9ce627be07c3a10"},
    };

    for (const auto& v : vectors) {
        std::string data(v.len, v.fill);
        INFO("len: ", v.len);
        CHECK_EQ(md5(data), std::string(v.digest));
    }
}

TEST_CASE("test_md5_large_input_bit_length") {
    // 2^29 字节（512MB）的位长恰为 2^32，跨越 64 位长度字段的第三个 65536 进制位：
    // Windows（LLP64）下 unsigned long 为 32 位时块数与位长都会被截断，摘要必然错误
    const size_t size = 1ull << 29;
    std::vector<unsigned char> data(size, 0);
    CHECK_EQ(md5(data.data(), data.size()), std::string("aa559b4e3523a6c931f08f4df52d58f2"));
}
