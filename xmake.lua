-- config version
set_version("1.0.0", {build="%Y%m%d%H%M"})   --使用 build 参数将导致每次重编译

-- set warning all as error
set_warnings("all", "error")

-- set language: C99, c++ standard
set_languages("cxx17", "c99")

add_rules("mode.debug", "mode.release", "mode.coverage", "mode.profile")

option("mysql")
    set_default(true)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable mysql kdata engine.")
    if is_plat("macosx") then
        if os.exists("/usr/local/opt/mysql-client/lib") then
            add_includedirs("/usr/local/opt/mysql-client/include/mysql")
            add_includedirs("/usr/local/opt/mysql-client/include")
            add_linkdirs("/usr/local/opt/mysql-client/lib")
            add_rpathdirs("/usr/local/opt/mysql-client/lib")
        end
        if os.exists("/usr/local/mysql/lib") then
            add_linkdirs("/usr/local/mysql/lib")
            add_rpathdirs("/usr/local/mysql/lib")
        end
        if not os.exists("/usr/local/include/mysql") then
            if os.exists("/usr/local/mysql/include") then
                os.run("ln -s /usr/local/mysql/include /usr/local/include/mysql")
            else
                print("Not Found MySQL include dir!")
            end
        end
        add_links("mysqlclient")
    elseif is_plat("windows") then
        add_defines("NOMINMAX")
    end        
option_end()

option("sqlite")
    set_default(true)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable sqlite kdata engine.")
option_end()

option("sqlcipher")
    set_default(false)
    set_showmenu(true)
    set_category("yihua")
    set_description("Enalbe sqlchiper driver")
    add_defines("HKU_ENABLE_SQLCIPHER")
option_end()

option("sql_trace")
    set_default(false)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("打印执行的 SQL 语句")
    add_defines("HKU_SQL_TRACE")
option_end()

-- 注意：stacktrace 在 windows 下会严重影响性能
option("stacktrace")
    set_default(false)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable check/assert with stack trace info.")
    add_defines("HKU_ENABLE_STACK_TRACE")
option_end()

option("spend_time")
    set_default(true)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable spend time.")
    add_defines("HKU_CLOSE_SPEND_TIME=0")
option_end()

option("log_level")
    set_default("trace")
    set_values("trace", "debug", "info", "warn", "error", "fatal", "off")
    set_showmenu(true)
    set_category("hikyuu")
    set_description("set log level")
    after_check(function (option)
        local level = get_config("log_level")
        if level == "trace" then
            option:add("defines", "HKU_LOGGER_ACTIVE_LEVEL=0")
        elseif level == "debug" then
            option:add("defines", "HKU_LOGGER_ACTIVE_LEVEL=1")
        elseif level == "info" then
            option:add("defines", "HKU_LOGGER_ACTIVE_LEVEL=2")
        elseif level == "warn" then
            option:add("defines", "HKU_LOGGER_ACTIVE_LEVEL=3")
        elseif level == "error" then
            option:add("defines", "HKU_LOGGER_ACTIVE_LEVEL=4")
        elseif level == "fatal" then
            option:add("defines", "HKU_LOGGER_ACTIVE_LEVEL=5")
        else
            option:add("defines", "HKU_LOGGER_ACTIVE_LEVEL=6")
        end
    end)
option_end()

option("async_log")
    set_default(false)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Use async log.")
    add_defines("HKU_USE_SPDLOG_ASYNC_LOGGER=1")
option_end()

option("leak_check")
    set_default(false)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable leak check for test")
option_end()

if get_config("leak_check") then
    -- 需要 export LD_PRELOAD=libasan.so
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.leak", true)
    -- set_policy("build.sanitizer.memory", true)
    -- set_policy("build.sanitizer.thread", true)
end

if is_plat("windows") then
    if is_mode("release") then
        set_runtimes("MD")
    else
        set_runtimes("MDd")
    end
end

-- is release now
if is_mode("release") then
    if is_plat("windows") then
        -- Unix-like systems hidden symbols will cause the link dynamic libraries to failed!
        set_symbols("hidden")
    end
end

-- for the windows platform (msvc)
if is_plat("windows") then
    -- add some defines only for windows
    add_defines("NOCRYPT", "NOGDI")
    add_cxflags("-EHsc", "/Zc:__cplusplus", "/utf-8")
    add_cxflags("-wd4819") -- template dll export warning
    add_defines("WIN32_LEAN_AND_MEAN")
    if is_mode("debug") then
        add_cxflags("-Gs", "-RTC1", "/bigobj")
    end
end

if not is_plat("windows") then
    -- disable some compiler errors
    add_cxflags("-Wno-error=deprecated-declarations", "-fno-strict-aliasing")
    add_cxflags("-ftemplate-depth=1023", "-pthread")
    add_shflags("-pthread")
    add_ldflags("-pthread")
end


add_repositories("hikyuu-repo https://github.com/fasiondog/hikyuu_extern_libs.git")
-- add_repositories("hikyuu-repo https://gitee.com/fasiondog/hikyuu_extern_libs.git

add_requires("fmt", {system = false, configs = {header_only = true}})
add_requires("spdlog", {system = false, configs = {header_only = true, fmt_external = true}})
add_requires("boost", {system=false})
add_requires("yas", {system=false})

-- 使用 sqlcipher 时，忽略 sqlite3
if get_config("sqlcipher") then
    if is_plat("iphoneos") then
        add_requires("sqlcipher", {system=false})
    else 
        add_requires("sqlcipher", {system = false, configs = {shared = true, tiny = true, SQLITE_THREADSAFE="2"}})
    end
elseif get_config("sqlite") then
    if is_plat("windows", "android", "cross") then 
        add_requires("sqlite3", {system = false, configs = {shared = true, tiny = true, SQLITE_THREADSAFE="2"}})
    end

    if is_plat("linux") and linuxos.name() == "ubuntu" then
        add_requires("apt::libsqlite3-dev")
        if get_config("mysql") then
            add_requires("apt::libmysqlclient-dev")
        end
    end
end

if get_config("mysql") then 
    if is_plat("windows") then 
        add_requires("mysql-client")
    elseif is_plat("macosx") then 
        add_requires("brew::mysql-client")
    elseif is_plat("linux") and linuxos.name() == "ubuntu" then
        add_requires("apt::libmysqlclient-dev")
    end
end

target("hku_utils")
    set_kind("$(kind)")

    set_configdir("$(projectdir)/hikyuu/utilities")
    add_configfiles("$(projectdir)/version.h.in", {prefix="HKU"})

    add_options("log_level", "spend_time", "sqlcipher", "sqlite", "mysql", "ini_parser", "datetime", "stacktrace",
                "async_log", "leak_check")

    add_packages("fmt", "spdlog", "boost", "yas")

    add_includedirs(".")
    add_includedirs("hikyuu/utilities")

    if get_config("sqlcipher") then
        add_packages("sqlcipher")
    elseif get_config("sqlite") then
        if is_plat("windows", "android", "cross") then
            add_packages("sqlite3")
        elseif is_plat("linux", "cross") then 
            add_links("sqlite3")
        elseif is_plat("macosx", "iphoneos") then 
            add_links("sqlite3")
        end    
        if is_plat("cross") then
            add_syslinks("dl")
        end
    end

    if get_config("mysql") and is_plat("windows") then
        add_packages("mysql-client")
    end
 
    if is_plat("linux", "cross") then 
        add_rpathdirs("$ORIGIN")
    end

    if is_plat("macosx") then
        add_includedirs("/usr/local/include")
    end

    if is_kind("shared") then 
        if is_plat("windows") then
            add_defines("HKU_UTILS_API=__declspec(dllexport)")
        else
            add_defines("HKU_UTILS_API=__attribute__((visibility(\"default\")))")
            add_cxflags("-fPIC", {force=true})
        end
    elseif is_kind("static") and not is_plat("windows") then
        add_cxflags("-fPIC", {force=true})
    end

    if is_plat("macosx", "iphoneos") then
        add_links("iconv")
    end

    -- gcc 4.8.5 必须编译和链接同时加上 -pthread, 否则运行异常
    if is_plat("macosx", "linux", "cross") then
        add_cxflags("-pthread")
        add_syslinks("pthread")
    end

    if is_plat("windows") then
        add_cxflags("-wd4996", "-wd4251")
    end

    add_headerfiles("$(projectdir)/(hikyuu/**.h)", "$(projectdir)/(hikyuu/**.hpp)")

    add_files("hikyuu/utilities/*.cpp")
    add_files("hikyuu/utilities/thread/*.cpp")
    add_files("hikyuu/utilities/db_connect/*.cpp")
    -- add_files("hikyuu/utilities/simd/*.cpp")

    if get_config("sqlite") then
        add_files("hikyuu/utilities/db_connect/sqlite/*.cpp")
    end

    if get_config("mysql") then
        add_files("hikyuu/utilities/db_connect/mysql/*.cpp")
    end

    if get_config("ini_parser") then
        add_files("hikyuu/utilities/ini_parser/*.cpp")
    end

    if get_config("datetime") then
        add_files("hikyuu/utilities/datetime/*.cpp")
    end
target_end()

-- local boost_version = "1.85.0"
-- local hdf5_version = "1.12.2"
-- local fmt_version = "10.2.1"
-- local flatbuffers_version = "24.3.25"
-- local nng_version = "1.8.0"
-- local cpp_httplib_version = "0.14.3"
-- local sqlite_version = "3.46.0+0"
-- local mysql_version = "8.0.31"
-- if is_plat("windows") or (is_plat("linux", "cross") and is_arch("aarch64", "arm64.*")) then 
--     mysql_version = "8.0.21" 
-- end

-- add_repositories("hikyuu-repo https://github.com/fasiondog/hikyuu_extern_libs.git")
-- -- add_repositories("hikyuu-repo https://gitee.com/fasiondog/hikyuu_extern_libs.git")
-- if is_plat("windows") then
--     if get_config("hdf5") then
--         if is_mode("release") then
--             add_requires("hdf5 " .. hdf5_version)
--         else
--             add_requires("hdf5_d " .. hdf5_version)
--         end
--     end
--     if get_config("mysql") then
--         add_requires("mysql " .. mysql_version)
--     end

-- elseif is_plat("linux", "cross") then
--     if get_config("hdf5") then
--         add_requires("hdf5 " .. hdf5_version, { system = false })
--     end
--     if get_config("mysql") then
--         add_requires("mysql " .. mysql_version, { system = false })
--     end
  
-- elseif is_plat("macosx") then
--     if get_config("hdf5") then
--         add_requires("brew::hdf5")
--     end
--     if get_config("mysql") then
--         add_requires("brew::mysql-client")
--     end
-- end

-- add_requires("boost " .. boost_version, {
--   system = false,
--   debug = is_mode("debug"),
--   configs = {
--     shared = is_plat("windows"),
--     multi = true,
--     date_time = true,
--     filesystem = false,
--     serialization = true,
--     system = false,
--     python = false,
--   },
-- })

-- add_requires("spdlog", {system = false, configs = {header_only = true, fmt_external = true}})
-- add_requireconfs("spdlog.fmt", {override = true, version = fmt_version, configs = {header_only = true}})
-- add_requires("sqlite3 " .. sqlite_version, {system = false, configs = {shared = true, cxflags = "-fPIC"}})
-- add_requires("flatbuffers v" .. flatbuffers_version, {system = false})
-- add_requires("nng " .. nng_version, {system = false, configs = {cxflags = "-fPIC"}})
-- add_requires("nlohmann_json", {system = false})
-- add_requires("cpp-httplib " .. cpp_httplib_version, {system = false, configs = {zlib = true, ssl = true}})
-- add_requires("zlib", {system = false})

-- add_defines("SPDLOG_DISABLE_DEFAULT_LOGGER") -- 禁用 spdlog 默认ogger

-- set_objectdir("$(buildir)/$(mode)/$(plat)/$(arch)/.objs")
-- set_targetdir("$(buildir)/$(mode)/$(plat)/$(arch)/lib")

-- -- modifed to use boost static library, except boost.python, serialization
-- if is_plat("windows") and get_config("kind") == "shared" then 
--     add_defines("BOOST_ALL_DYN_LINK") 
-- end

-- -- is release now
-- if is_mode("release") then
--   if is_plat("windows") then
--     -- Unix-like systems hidden symbols will cause the link dynamic libraries to failed!
--     set_symbols("hidden")
--   end
-- end

-- -- for the windows platform (msvc)
-- if is_plat("windows") then
--   -- add some defines only for windows
--   add_defines("NOCRYPT", "NOGDI")
--   add_cxflags("-EHsc", "/Zc:__cplusplus", "/utf-8")
--   add_cxflags("-wd4819") -- template dll export warning
--   add_defines("WIN32_LEAN_AND_MEAN")
--   if is_mode("debug") then
--     add_cxflags("-Gs", "-RTC1", "/bigobj")
--   end
-- end

-- if not is_plat("windows") then
--   -- disable some compiler errors
--   add_cxflags("-Wno-error=deprecated-declarations", "-fno-strict-aliasing")
--   add_cxflags("-ftemplate-depth=1023", "-pthread")
--   add_shflags("-pthread")
--   add_ldflags("-pthread")
-- end


-- includes("./hikyuu_cpp/hikyuu")
-- includes("./hikyuu_pywrap")
-- includes("./hikyuu_cpp/unit_test")
-- includes("./hikyuu_cpp/demo")