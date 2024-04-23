/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stdexcept>
#include "PipeWrap.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Network/sockutil.h"

using namespace std;

#define checkFD(fd) \
    if (fd == -1) { \
        clearFD(); \
        throw runtime_error(StrPrinter << "create windows pipe failed:" << get_uv_errmsg());\
    }

#define closeFD(fd) \
    if (fd != -1) { \
        close(fd);\
        fd = -1;\
    }

namespace toolkit {

PipeWrap::PipeWrap(){
    if (pipe(_pipe_fd) == -1) {
        throw runtime_error(StrPrinter << "create posix pipe failed:" << get_uv_errmsg());
    }
    SockUtil::setNoBlocked(_pipe_fd[0],true);
    SockUtil::setNoBlocked(_pipe_fd[1],false);
    SockUtil::setCloExec(_pipe_fd[0]);
    SockUtil::setCloExec(_pipe_fd[1]);
}

void PipeWrap::clearFD() {
    closeFD(_pipe_fd[0]);
    closeFD(_pipe_fd[1]);
}

PipeWrap::~PipeWrap(){
    clearFD();
}

int PipeWrap::write(const void *buf, int n) {
    int ret;
    do {
        ret = ::write(_pipe_fd[1],buf,n);
    } while (-1 == ret && UV_EINTR == get_uv_error(true));
    return ret;
}

int PipeWrap::read(void *buf, int n) {
    int ret;
    do {
        ret = ::read(_pipe_fd[0], buf, n);
    } while (-1 == ret && UV_EINTR == get_uv_error(true));
    return ret;
}

} /* namespace toolkit*/
