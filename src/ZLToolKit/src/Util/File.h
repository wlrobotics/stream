﻿/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_UTIL_FILE_H_
#define SRC_UTIL_FILE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <functional>
#include <limits.h>

#include "util.h"

namespace toolkit {

class File {
public:
    //创建路径
    static bool create_path(const char *file, unsigned int mod);
    //新建文件，目录文件夹自动生成
    static FILE *create_file(const char *file, const char *mode);
    //判断是否为目录
    static bool is_dir(const char *path) ;
    //判断是否为常规文件
    static bool is_file(const char *path) ;
    //判断是否是特殊目录（. or ..）
    static bool is_special_dir(const char *path);
    //删除目录或文件
    static void delete_file(const char *path) ;

    /**
     * 加载文件内容至string
     * @param path 加载的文件路径
     * @return 文件内容
     */
    static std::string loadFile(const std::string path);

    /**
     * 保存内容至文件
     * @param data 文件内容
     * @param path 保存的文件路径
     * @return 是否保存成功
     */
    static bool saveFile(const string &data,const char *path);

    /**
     * 获取父文件夹
     * @param path 路径
     * @return 文件夹
     */
    static string parentDir(const string &path);

    /**
     * 替换"../"，获取绝对路径
     * @param path 相对路径，里面可能包含 "../"
     * @param currentPath 当前目录
     * @param canAccessParent 能否访问父目录之外的目录
     * @return 替换"../"之后的路径
     */
    static string absolutePath(const string &path, const string &currentPath,bool canAccessParent = false);

    /**
     * 遍历文件夹下的所有文件
     * @param path 文件夹路径
     * @param cb 回调对象 ，path为绝对路径，isDir为该路径是否为文件夹，返回true代表继续扫描，否则中断
     * @param enterSubdirectory 是否进入子目录扫描
     */
    static void scanDir(const string &path,const function<bool(const string &path,bool isDir)> &cb, bool enterSubdirectory = false);
private:
    File();
    ~File();
};

} /* namespace toolkit */
#endif /* SRC_UTIL_FILE_H_ */
