﻿/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SERVER_SESSION_H_
#define SERVER_SESSION_H_

#include <memory>
#include <sstream>
#include "Socket.h"
#include "Util/logger.h"
#include "Util/mini.h"
#include "Thread/ThreadPool.h"
using namespace std;

namespace toolkit {

class TcpServer;

//TCP服务器连接对象，一个tcp连接对应一个TcpSession对象
class TcpSession : public std::enable_shared_from_this<TcpSession>, public SocketHelper{
public:
    typedef std::shared_ptr<TcpSession> Ptr;

    TcpSession(const Socket::Ptr &sock);
    ~TcpSession() override;

    /**
     * 接收数据入口
     * @param buf 数据，可以重复使用内存区
     */
    virtual void onRecv(const Buffer::Ptr &buf) = 0;

    /**
     * 收到eof或其他导致脱离TcpServer事件的回调
     * 收到该事件时，该对象一般将立即被销毁
     * @param err 原因
     */
    virtual void onError(const SockException &err) = 0;

    /**
     * 每隔一段时间触发，用来做超时管理
     */
    virtual void onManager() = 0;

    /**
     * 在创建TcpSession后，TcpServer会把自身的配置参数通过该函数传递给TcpSession
     * @param server 服务器对象
     */
    virtual void attachServer(const TcpServer &server) {};

    /**
     * 作为该TcpSession的唯一标识符
     * @return 唯一标识符
     */
    string getIdentifier() const override;

    /**
     * 线程安全的脱离TcpServer并触发onError事件
     * @param ex 触发onError事件的原因
     */
    void safeShutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown"));
};

} /* namespace toolkit */
#endif /* SERVER_SESSION_H_ */
