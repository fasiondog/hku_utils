-- config version
set_version("1.0.2", {build="%Y%m%d%H%M"})   --使用 build 参数将导致每次重编译

-- set warning all as error
set_warnings("all", "error")

-- set language: C99, c++ standard
-- set_languages("cxx17", "c99")

add_rules("mode.debug", "mode.release", "mode.coverage", "mode.profile")

set_objectdir("$(buildir)/$(mode)/$(plat)/$(arch)/.objs")
set_targetdir("$(buildir)/$(mode)/$(plat)/$(arch)/lib")

option("mysql")
    set_default(false)
    set_showmenu(true)
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

option("sqlite", {description = "Enable sqlite driver.", default = true})
option("sqlcipher", {description = "Enalbe sqlchiper driver.", default = false})
option("sql_trace", {description = "Print the executed SQL statement", default = false})

-- 注意：stacktrace 在 windows 下会严重影响性能
option("stacktrace", {description = "Enable check/assert with stack trace info.", default = false})

option("datetime", {description = "Enable DateTime.", default = true})
option("spend_time", {description = "Enable spent time.", default = true})

option("log_level", {description = "set log level.", default = 2, values = {1, 2, 3, 4, 5, 6}})
option("async_log", {description = "Use async log.", default = false})

option("leak_check", {description = "Enable leak check for test", default = false})
option("ini_parser", {description = "Enable ini parser.", default = true})
option("mo", {description = "International language support", default = false})
option("http_client", {description = "use http client", default = true})
option("http_client_ssl", {description = "enable https support for http client", default = false})
option("http_client_zip", {description = "enable http support gzip", default = false})
option("node", {description = "enable node reqrep server/client", default = true})

if has_config("leak_check") then
    -- 需要 export LD_PRELOAD=libasan.so
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.leak", true)
    -- set_policy("build.sanitizer.memory", true)
    -- set_policy("build.sanitizer.thread", true)
end

-- SPDLOG_ACTIVE_LEVEL 需要单独加
local log_level = get_config("log_level")
if log_level == nil then
    log_level = 2
end
add_defines("SPDLOG_ACTIVE_LEVEL=" .. log_level)

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
    add_cxflags("-wd4068") -- unknown pragma
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

add_requires("fmt", {configs = {header_only = true}})
add_requires("spdlog", {configs = {header_only = true, fmt_external = true}})
add_requires("yas")
add_requires("boost", {
    system = false,
    debug = is_mode("debug"),
    configs = {
      shared = is_plat("windows"),
      runtimes = get_config("runtime"),
      multi = true,
      date_time = has_config("datetime"),
      filesystem = false,
      serialization = false,
      system = false,
      python = false,
    },
  })

-- 使用 sqlcipher 时，忽略 sqlite3
if has_config("sqlcipher") then
    if is_plat("iphoneos") then
        add_requires("sqlcipher", {system=false})
    else 
        add_requires("sqlcipher", {system = false, configs = {shared = true, SQLITE_THREADSAFE="2"}})
    end
elseif has_config("sqlite") then
    if is_plat("windows", "android", "cross") then 
        add_requires("sqlite3", {system = false, configs = {shared = true, SQLITE_THREADSAFE="2"}})
    end

    if is_plat("linux") and linuxos.name() == "ubuntu" then
        add_requires("apt::libsqlite3-dev", {alias = "sqlite3"})
    end
end

if has_config("mysql") then 
    if is_plat("macosx") then 
        add_requires("brew::mysql-client", {alias = "mysql"})
    elseif is_plat("linux") and linuxos.name() == "ubuntu" then
        add_requires("apt::libmysqlclient-dev", {alias = "mysql"})
    else
        add_requires("mysql")
    end
end

if has_config("http_client") or has_config("node") then
    add_requires("nlohmann_json")
    if is_kind("shared") then
        add_requires("nng", {configs = {NNG_ENABLE_TLS = has_config("http_client_ssl"), cxflags = "-fPIC"}})
        add_requireconfs("nng.mbedtls", {configs = {cxflags = "-fPIC"}})
    else
        add_requires("nng", {configs = {NNG_ENABLE_TLS = has_config("http_client_ssl")}})
    end
    if has_config("http_client_zip") then
        add_requires("gzip-hpp")
    end
end



target("hku_utils")
    set_kind("$(kind)")

    set_configdir("$(projectdir)/hikyuu/utilities")
    add_configfiles("$(projectdir)/version.h.in")
    add_configfiles("$(projectdir)/config.h.in")

    set_configvar("HKU_ENABLE_MYSQL", has_config("mysql") and 1 or 0)
    set_configvar("HKU_ENABLE_SQLITE", (has_config("sqlite") or has_config("sqlcipher")) and 1 or 0)
    set_configvar("HKU_ENABLE_SQLCIPHER", has_config("sqlcipher") and 1 or 0)
    set_configvar("HKU_SQL_TRACE", has_config("sql_trace") and 1 or 0)
    set_configvar("HKU_SUPPORT_DATETIME", has_config("datetime") and 1 or 0)
    set_configvar("HKU_ENABLE_INI_PARSER", has_config("ini_parser") and 1 or 0)
    set_configvar("HKU_ENABLE_STACK_TRACE", has_config("stacktrace") and 1 or 0)
    set_configvar("HKU_CLOSE_SPEND_TIME", has_config("spend_time") and 0 or 1)
    set_configvar("HKU_ENABLE_MO", has_config("mo") and 1 or 0)
    set_configvar("HKU_ENABLE_HTTP_CLIENT", has_config("http_client") and 1 or 0)
    set_configvar("HKU_ENABLE_HTTP_CLIENT_SSL", has_config("http_client_ssl") and 1 or 0)
    set_configvar("HKU_ENABLE_HTTP_CLIENT_ZIP", has_config("http_client_zip") and 1 or 0)
    set_configvar("HKU_ENABLE_NODE", has_config("node") and 1 or 0)
    
    set_configvar("HKU_USE_SPDLOG_ASYNC_LOGGER", has_config("async_log") and 1 or 0)
    set_configvar("HKU_LOG_ACTIVE_LEVEL", get_config("log_level"))

    add_packages("fmt", "spdlog", "boost", "yas")

    add_includedirs(".")

    if has_config("sqlcipher") then
        add_packages("sqlcipher")
    elseif has_config("sqlite") then
        add_packages("sqlite3")
        if is_plat("cross") then
            add_syslinks("dl")
        end
    end

    if has_config("mysql") then
        add_packages("mysql")
    end

    if has_config("http_client") or has_config("node") then
        add_packages("nng", "nlohmann_json")
    end

    if has_config("http_client_zip") then
        add_packages("gzip-hpp")
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

    if has_config("sqlite") then
        add_files("hikyuu/utilities/db_connect/sqlite/*.cpp")
    end

    if has_config("mysql") then
        add_files("hikyuu/utilities/db_connect/mysql/*.cpp")
    end

    if has_config("ini_parser") then
        add_files("hikyuu/utilities/ini_parser/*.cpp")
    end

    if has_config("datetime") then
        add_files("hikyuu/utilities/datetime/*.cpp")
    end

    if has_config("mo") then
        add_files("hikyuu/utilities/mo/*.cpp")
    end

    if has_config("http_client") then
        add_files("hikyuu/utilities/http_client/*.cpp")
    end

    before_build(function(target)
        -- 注：windows 使用 dll 需要 c++17, linux 使用静态库最低需要 C++ 17
        -- 未指定 C++标准时，设置最低要求 c++11
        local x = target:get("languages")
        if x == nil then
            target:set("languages", "cxx17")
        end

        if is_plat("macosx") then
            if not os.exists("/usr/local/include/mysql") then
                if os.exists("/usr/local/mysql/include") then
                    os.run("ln -s /usr/local/mysql/include /usr/local/include/mysql")
                else
                    print("Not Found MySQL include dir!")
                end
            end
        end
    end)

    after_build(function(target)
        local destpath = get_config("buildir") .. "/" .. get_config("mode") .. "/" .. get_config("plat") .. "/" .. get_config("arch")
        print(destpath)
        import("core.project.task")
        task.run("copy_dependents", {}, target, destpath, true)
    end)

    after_install(function(target)
        local destpath = target:installdir()
        import("core.project.task")
        task.run("copy_dependents", {}, target, destpath, false)
    end)
target_end()

includes("copy_dependents.lua")
includes("./test")