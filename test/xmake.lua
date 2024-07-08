add_requires("doctest")
add_requires("fmt", {system = false, configs = {head_only=false, pic=true}})
add_requires("yas", {system = false})

target("unit-test")
    set_kind("binary")
    set_default(false)

    set_languages("cxx11")
    add_packages("doctest", "spdlog")

    add_options("spend_time")

    add_deps("hku_utils")
    add_packages("fmt", "yas", "boost")
    if get_config("sqlite") then 
        add_packages("sqlite3")
    end

    add_includedirs("..")

    if is_plat("macosx", "linux", "cross") then
        add_cxflags("-fPIC")
        add_syslinks("pthread")
    end

    if is_plat("windows") then
        add_cxflags("-wd4996")
    end

    add_files("*.cpp")
    add_files("utilities/*.cpp")

    if get_config("ini_parser") then
        add_files("utilities/ini_parser/*.cpp")
    end

    if get_config("datetime") then
        add_defines("HKU_SUPPORT_DATETIME")
        add_files("utilities/datetime/*.cpp")
    end

    if get_config("sqlite") then
        add_files("utilities/db_connect/**.cpp")
        if is_plat("cross") then
            add_links("sqlite3")
        end
    end

    before_run(function (target)
        -- 拷贝测试文件
        print("copying test_data ...")
        os.rm("$(buildir)/$(mode)/$(plat)/$(arch)/lib/test_data")
        os.cp("$(projectdir)/test_data", "$(buildir)/$(mode)/$(plat)/$(arch)/lib/")

        if is_mode("coverage") and (not is_plat("windows")) and not (linuxos.name() == "ubuntu" and linuxos.version():lt("20.0"))  then
            if not os.isfile("cover-init.info") then
                -- 初始化并创建基准数据文件
                os.run("lcov -c -i -d ./ -o cover-init.info") 
            end
        end
    end)

    after_run(function (target)
        if is_mode("coverage") and not is_plat("windows") and not (linuxos.name() == "ubuntu" and linuxos.version():lt("20.0")) then 
            -- 收集测试文件运行后产生的覆盖率文件
            os.exec("lcov --rc lcov_branch_coverage=1 -c -d ./ -o cover.info")

            -- 合并基准数据和执行测试文件后生成的覆盖率数据
            os.exec("lcov --rc lcov_branch_coverage=1 -a cover-init.info -a cover.info -o cover-total.info")
            
            -- 删除统计信息中如下的代码或文件，支持正则
            os.run("lcov --rc lcov_branch_coverage=1 --remove cover-total.info '*/usr/include/*' \
                    '*/usr/lib/*' '*/usr/lib64/*' '*/usr/local/include/*' '*/usr/local/lib/*' '*/usr/local/lib64/*' \
                    '*/test/*' '*/.xmake*' '*/boost/*' \
                    '*/hikyuu/utilities/SpendTimer.*' \
                    '*/hikyuu/utilities/arithmetic.*' \
                    '*/hikyuu/utilities/Log.cpp' \
                    '*/hikyuu/utilities/exception.h' \
                    '*/hikyuu/utilities/db_connect/*' \
                    -o cover-final.info")
            
            -- #如果是git目录，可以获取此次版本的commitID，如果不是，忽略此步
            -- # commitId=$(git log | head -n1 | awk '{print $2}')
            -- # 这里可以带上项目名称和提交ID，如果没有，忽略此步
            -- #genhtml -o cover_report --legend --title "${project_name} commit SHA1:${commitId}" --prefix=${curr_path} final.info
            -- # -o 生成的html及相关文件的目录名称，--legend 简单的统计信息说明
            -- # --title 项目名称，--prefix 将要生成的html文件的路径 
            os.exec("genhtml --rc lcov_branch_coverage=1 -o cover_report --legend --title 'HKU_utils'  --prefix=" .. os.projectdir() .. " cover-final.info")  

            -- 生成 sonar 可读取报告
            os.run("gcovr -r . -e test -e hikyuu/utilities/arithmetic.cpp \
                    -e hikyuu/utilities/SpendTimer.cpp \
                    -e hikyuu/utilities/Log.cpp \
                    -e hikyuu/utilities/exception.h \
                    -e hikyuu/utilities/db_connect \
                    --xml -o coverage.xml")
        end
    end)
target_end()