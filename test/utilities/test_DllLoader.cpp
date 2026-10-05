/*
 *  Copyright (c) hikyuu.org
 *
 *  Created on: 2026-10-6
 *      Author: fasiondog
 *
 */

#include <doctest/doctest.h>
#include <cstdlib>
#include <fstream>
#include <hikyuu/utilities/DllLoader.h>

using namespace hku;

TEST_CASE("test_DllLoader_env_search_path") {
// The Unix dynamic library search path environment variables are colon-separated lists; the
// loader must split them by ':' (not ';') so each directory is searched separately
#if !HKU_OS_WINDOWS
    std::string tmpdir("tmp_dll_env_path");
    removeDir(tmpdir);
    CHECK_UNARY(createDir(tmpdir));

#if HKU_OS_OSX
    std::string libfile = fmt::format("{}/libdummy_env_lib.dylib", tmpdir);
    const char* envname = "DYLD_LIBRARY_PATH";
#else
    std::string libfile = fmt::format("{}/libdummy_env_lib.so", tmpdir);
    const char* envname = "LD_LIBRARY_PATH";
#endif
    {
        std::ofstream f(libfile);
    }
    CHECK_UNARY(existFile(libfile));

    // Multiple entries joined by ':': if the whole value were treated as a single path (the
    // previous ';' separator bug), the library would not be found
    std::string envvalue = fmt::format("/no_such_dir_prefix:{}:", tmpdir);
    setenv(envname, envvalue.c_str(), 1);

    {
        DllLoader loader;
        auto found = loader.search("dummy_env_lib");
        CHECK_UNARY_FALSE(found.empty());
        CHECK_EQ(found, libfile);
    }

    unsetenv(envname);
    removeFile(libfile);
    removeDir(tmpdir);
#endif  // #if !HKU_OS_WINDOWS
}

TEST_CASE("test_DllLoader_explicit_search_path") {
    std::string tmpdir("tmp_dll_explicit_path");
    removeDir(tmpdir);
    CHECK_UNARY(createDir(tmpdir));

#if HKU_OS_WINDOWS
    std::string dllname("dummy_explicit_lib.dll");
    std::string libfile = fmt::format("{}/{}", tmpdir, dllname);
#elif HKU_OS_OSX
    std::string dllname("dummy_explicit_lib");
    std::string libfile = fmt::format("{}/libdummy_explicit_lib.dylib", tmpdir);
#else
    std::string dllname("dummy_explicit_lib");
    std::string libfile = fmt::format("{}/libdummy_explicit_lib.so", tmpdir);
#endif
    {
        std::ofstream f(libfile);
    }
    CHECK_UNARY(existFile(libfile));

    /** A non-empty explicit path list is used directly */
    std::vector<std::string> explicit_paths{tmpdir};
    DllLoader loader(explicit_paths);
    auto found = loader.search(dllname);
    CHECK_UNARY_FALSE(found.empty());
    CHECK_EQ(found, libfile);

    /** An empty explicit path list falls back to the default search path */
    std::vector<std::string> empty_paths;
    DllLoader defaultLoader(empty_paths);
    CHECK_UNARY(defaultLoader.search("no_such_lib_for_test_xx").empty());

    removeFile(libfile);
    removeDir(tmpdir);
}
