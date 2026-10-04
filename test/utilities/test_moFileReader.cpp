/*
 *  Copyright (c) hikyuu.org
 *
 *  Created on: 2026-10-04
 *      Author: fasiondog
 */

#include <doctest/doctest.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "hikyuu/utilities/moFileReader.h"

using namespace moFileLib;

namespace {

// Build a synthetic little endian .mo image in memory, so malformed inputs need no file
class MoBuilder {
public:
    void addPair(const std::string& original, const std::string& translation) {
        m_pairs.emplace_back(original, translation);
    }

    std::string build() const {
        struct Desc {
            int32_t length;
            int32_t offset;
        };
        std::vector<Desc> origDesc, trDesc;
        std::string strings;
        for (const auto& pair : m_pairs) {
            origDesc.push_back(
              {static_cast<int32_t>(pair.first.size()), static_cast<int32_t>(strings.size())});
            strings += pair.first;
            strings += '\0';
        }
        for (const auto& pair : m_pairs) {
            trDesc.push_back(
              {static_cast<int32_t>(pair.second.size()), static_cast<int32_t>(strings.size())});
            strings += pair.second;
            strings += '\0';
        }

        int32_t num = static_cast<int32_t>(m_pairs.size());
        int32_t offOrig = 28;
        int32_t offTr = offOrig + num * 8;
        int32_t stringBase = offTr + num * 8;

        std::string out;
        auto append32 = [&out](int32_t v) {
            for (int i = 0; i < 4; i++) {
                out += static_cast<char>((static_cast<uint32_t>(v) >> (8 * i)) & 0xff);
            }
        };
        append32(static_cast<int32_t>(moFileReader::MagicNumber));
        append32(0);  // file version
        append32(num);
        append32(offOrig);
        append32(offTr);
        append32(0);  // hash table size
        append32(0);  // hash table offset
        for (const auto& d : origDesc) {
            append32(d.length);
            append32(d.offset + stringBase);
        }
        for (const auto& d : trDesc) {
            append32(d.length);
            append32(d.offset + stringBase);
        }
        out += strings;
        return out;
    }

private:
    std::vector<std::pair<std::string, std::string>> m_pairs;
};

}  // namespace

TEST_CASE("test_moFileReader_valid") {
    MoBuilder builder;
    builder.addPair("", "Content-Type: text/plain\n");
    builder.addPair("hello", "bonjour");
    builder.addPair("你好", "こんにちは");

    moFileReader reader;

    /** A valid image loads all pairs, including multibyte UTF-8 content */
    CHECK_EQ(reader.ParseData(builder.build()), moFileReader::EC_SUCCESS);
    CHECK_EQ(reader.GetNumStrings(), 3);
    CHECK_EQ(reader.Lookup("hello"), "bonjour");
    CHECK_EQ(reader.Lookup("你好"), "こんにちは");

    /** An unknown key returns the key itself */
    CHECK_EQ(reader.Lookup("missing"), "missing");
}

TEST_CASE("test_moFileReader_malformed_header") {
    moFileReader reader;

    /** Fewer than the 28 byte header: a partial read sets failbit */
    CHECK_EQ(reader.ParseData("short data"), moFileReader::EC_FILEINVALID);

    /** A well sized header with a wrong magic number */
    std::string badMagic(28, '\0');
    uint32_t wrong = 0x12345678;
    for (int i = 0; i < 4; i++) {
        badMagic[i] = static_cast<char>((wrong >> (8 * i)) & 0xff);
    }
    CHECK_EQ(reader.ParseData(badMagic), moFileReader::EC_MAGICNUMBER_NOMATCH);

    /** A huge string count whose descriptor table cannot fit in the data */
    MoBuilder builder;
    std::string valid = builder.build();  // empty file, only the header
    moFileReader reader2;
    CHECK_EQ(reader2.ParseData(valid), moFileReader::EC_SUCCESS);

    std::string huge(28, '\0');
    auto put32 = [&huge](int pos, int32_t v) {
        for (int i = 0; i < 4; i++) {
            huge[pos + i] = static_cast<char>((static_cast<uint32_t>(v) >> (8 * i)) & 0xff);
        }
    };
    put32(0, static_cast<int32_t>(moFileReader::MagicNumber));
    put32(8, 1000000);
    put32(12, 28);
    put32(16, 28 + 1000000 * 8);
    moFileReader reader3;
    CHECK_EQ(reader3.ParseData(huge), moFileReader::EC_FILEINVALID);
}

TEST_CASE("test_moFileReader_truncated_strings") {
    MoBuilder builder;
    builder.addPair("hello", "bonjour");
    std::string data = builder.build();

    /** Cut the final bytes of the string area: the descriptor points beyond the data instead of
     * silently returning an empty translation (the old code only checked badbit) */
    std::string truncated = data.substr(0, data.size() - 4);
    moFileReader reader;
    CHECK_EQ(reader.ParseData(truncated), moFileReader::EC_FILEINVALID);

    /** A negative length (0xffffffff interpreted as int) is rejected before allocation */
    std::string evil = data;
    evil[28] = static_cast<char>(0xff);
    evil[29] = static_cast<char>(0xff);
    evil[30] = static_cast<char>(0xff);
    evil[31] = static_cast<char>(0xff);
    moFileReader reader2;
    CHECK_EQ(reader2.ParseData(evil), moFileReader::EC_FILEINVALID);
}
