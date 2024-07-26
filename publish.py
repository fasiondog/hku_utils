#!/usr/bin/python
# -*- coding: utf8 -*-
#
# Create on: 2024-07-26
#    Author: fasiondog

try:
    from git import Repo
except:
    print("Please pip install gitpython")
    exit(0)

import os, shutil, subprocess, hashlib

cur_repo = Repo('.')
# if cur_repo.untracked_files or [item.a_path for item in cur_repo.index.diff(None)]:
#     print('尚有未提交的代码，请提交后执行')
#     exit(0)

# 新建标签并推送标签
# new_tag = cur_repo.create_tag(main_version)
# cur_repo.remotes.origin.push(new_tag)

def copy_source(dst_package_path):
    """拷贝源码"""
    if os.path.lexists(dst_package_path):
        shutil.rmtree(dst_package_path)
    shutil.copytree('.', f'{dst_package_path}', ignore=lambda src, names: (".vscode", ".git", ".xmake", "build", "publish.py"))

def get_main_version():
    """获取当前编译版本号"""
    version_file = './xmake.lua'
    with open(version_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    pos = -1
    for i, line in enumerate(lines):
        if line.find("set_version(") >= 0:
            pos = i
            break

    line = lines[pos]
    return line[line.find('"') + 1:line.find('",')]


def make_zip_archive(dst_package_path):
    print("make zip archive ...")
    current_version = get_main_version()
    package_file = 'build/hku_utils-{}'.format(current_version)
    shutil.make_archive(package_file, 'zip', dst_package_path)

    package_file = f'{package_file}.zip'
    with open(package_file, 'rb') as f:
        data = f.read()
    hash = hashlib.sha256(data).hexdigest()
    return (package_file, current_version, hash)

def update_xmake_repo(current_version, hash):
    print("update xmake repo pacakge ...")
    xmake_repo_git = "https://gitee.com/fasiondog/hikyuu_extern_libs.git"
    os.chdir("build")
    if os.path.lexists('hikyuu_extern_libs'):
        shutil.rmtree('hikyuu_extern_libs')
    out = subprocess.run(
        f"git clone {xmake_repo_git}", shell=True
    )
    if out.returncode != 0:
        raise Exception("Failed clone xmake repo!")

    os.chdir("hikyuu_extern_libs")
    lines = ''
    with open('packages/h/hku_utils/xmake.lua', 'r', encoding='utf-8') as f:
        lines = f.readlines()

    first_add_version_pos = -1
    pos = -1
    for i, line in enumerate(lines):
        if first_add_version_pos < 0 and line.find(f'add_versions') >= 0:
            first_add_version_pos = i
        if line.find(f'add_versions("{current_version}"') >= 0:
            pos = i
            break

    if pos >= 0:
        lines[pos] = f'    add_versions("{current_version}", "{hash}")\n'
    elif first_add_version_pos >= 0:
        lines.insert(first_add_version_pos, f'    add_versions("{current_version}", "{hash}")\n')
    else:
        raise Exception("Failed found add_version")

    with open('packages/h/hku_utils/xmake.lua', 'w', encoding='utf-8') as f:
        for line in lines:
            f.write(line)

    # out = subprocess.run("git add packages/h/hku_utils/xmake.lua", shell=True)
    # if out.returncode != 0:
    #     raise Exception("Failed git add!")

    # out = subprocess.run('git commit -m "update hku_utils"', shell=True)
    # if out.returncode != 0:
    #     raise Exception("Failed clone xmake repo!")

    # out = subprocess.run('git push', shell=True)
    # if out.returncode != 0:
    #     raise Exception("Failed clone xmake repo!")
    

dst_package_path = f'build/cpp_package/hku_utils-{get_main_version()}'
copy_source(dst_package_path)
x = make_zip_archive(dst_package_path)
print(x)
# update_xmake_repo(x[1], x[2])