-- config version
set_version("1.0.0", {build="%Y%m%d%H%M"})   --使用 build 参数将导致每次重编译

-- set warning all as error
set_warnings("all", "error")

-- set language: C99, c++ standard
-- set_languages("cxx17", "c99")

add_rules("mode.debug", "mode.release", "mode.coverage", "mode.profile")

option("mysql")
    set_default(false)
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
    set_category("hikyuu")
    set_description("Enalbe sqlchiper driver")
option_end()

option("sql_trace")
    set_default(false)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("打印执行的 SQL 语句")
option_end()

-- 注意：stacktrace 在 windows 下会严重影响性能
option("stacktrace")
    set_default(false)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable check/assert with stack trace info.")
    add_defines("HKU_ENABLE_STACK_TRACE")
option_end()

option("datetime")
    set_default(true)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable DateTime.")
option_end()

option("spend_time")
    set_default(true)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable spend time.")
    add_defines("HKU_CLOSE_SPEND_TIME=0")
option_end()

option("log_name")
    set_default("hikyuu")
    set_showmenu(true)
    set_category("hikyuu")
    set_description("set default log name")
    after_check(function (option)
        local name = get_config("log_name")
        option:add("defines", 'HKU_DEFAULT_LOG_NAME="' .. name .. '"')
    end)
option_end()

option("log_level")
    set_default("info")
    set_values("trace", "debug", "info", "warn", "error", "fatal", "off")
    set_showmenu(true)
    set_category("hikyuu")
    set_description("set log level")
    after_check(function (option)
        local level = get_config("log_level")
        if level == "trace" then
            option:add("defines", "HKU_LOG_ACTIVE_LEVEL=0")
        elseif level == "debug" then
            option:add("defines", "HKU_LOG_ACTIVE_LEVEL=1")
        elseif level == "info" then
            option:add("defines", "HKU_LOG_ACTIVE_LEVEL=2")
        elseif level == "warn" then
            option:add("defines", "HKU_LOG_ACTIVE_LEVEL=3")
        elseif level == "error" then
            option:add("defines", "HKU_LOG_ACTIVE_LEVEL=4")
        elseif level == "fatal" then
            option:add("defines", "HKU_LOG_ACTIVE_LEVEL=5")
        else
            option:add("defines", "HKU_LOG_ACTIVE_LEVEL=6")
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

option("ini_parser")
    set_default(true)
    set_showmenu(true)
    set_category("hikyuu")
    set_description("Enable ini parser")
option_end()

if get_config("leak_check") then
    -- 需要 export LD_PRELOAD=libasan.so
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.leak", true)
    -- set_policy("build.sanitizer.memory", true)
    -- set_policy("build.sanitizer.thread", true)
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

add_requires("fmt", {system=false})
add_requires("spdlog", {system = false, configs = {header_only = true, fmt_external = true}})
add_requires("yas", {system=false})
add_requires("boost", {
    system = false,
    debug = is_mode("debug"),
    configs = {
      shared = is_plat("windows"),
      runtimes = get_config("runtime"),
      multi = true,
      date_time = get_config("datetime"),
      filesystem = false,
      serialization = false,
      system = false,
      python = false,
    },
  })

-- 使用 sqlcipher 时，忽略 sqlite3
if get_config("sqlcipher") then
    if is_plat("iphoneos") then
        add_requires("sqlcipher", {system=false})
    else 
        add_requires("sqlcipher", {system = false, configs = {shared = true, SQLITE_THREADSAFE="2"}})
    end
elseif get_config("sqlite") then
    if is_plat("windows", "android", "cross") then 
        add_requires("sqlite3", {system = false, configs = {shared = true, SQLITE_THREADSAFE="2"}})
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
        add_requires("mysql")
    elseif is_plat("macosx") then 
        add_requires("brew::mysql-client")
    elseif is_plat("linux") and linuxos.name() == "ubuntu" then
        add_requires("apt::libmysqlclient-dev")
    end
end

set_objectdir("$(buildir)/$(mode)/$(plat)/$(arch)/.objs")
set_targetdir("$(buildir)/$(mode)/$(plat)/$(arch)/lib")

target("hku_utils")
    set_kind("$(kind)")

    set_configdir("$(projectdir)/hikyuu/utilities")
    add_configfiles("$(projectdir)/version.h.in")
    add_configfiles("$(projectdir)/config.h.in")

    set_configvar("HKU_ENABLE_MYSQL", get_config("mysql") and 1 or 0)
    set_configvar("HKU_ENABLE_SQLITE", (get_config("sqlite") or get_config("sqlcipher")) and 1 or 0)
    set_configvar("HKU_ENABLE_SQLCIPHER", get_config("sqlcipher") and 1 or 0)
    set_configvar("HKU_SQL_TRACE", get_config("sql_trace") and 1 or 0)
    set_configvar("HKU_SUPPORT_DATETIME", get_config("datetime") and 1 or 0)

    add_options("log_name", "log_level", "spend_time", "ini_parser", 
                "stacktrace", "async_log", "leak_check")

    add_packages("fmt", "spdlog", "boost", "yas")

    add_includedirs(".")

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

    if get_config("mysql") then
        add_packages("mysql")
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

    before_build(function(target)
        local function has_flag(flag)
            for _, name in ipairs(target:get("cxflags")) do
                if name == flag then
                    return true
                end
            end
            return false
        end

        -- 如果没有指定c++标准，设为最低要求 c++11
        if not has_flag("-std") then 
            if is_plat("windows") then
                target:set("languages", "cxx17")
            else
                target:set("languages", "cxx11")
            end
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
        task.run("copy_dependents", {}, target, destpath, true)
    end)
target_end()

includes("copy_dependents.lua")
includes("./test")