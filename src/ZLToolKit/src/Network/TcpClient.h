/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_TCPCLIENT_H
#define NETWORK_TCPCLIENT_H

#include <mutex>
#include <memory>
#include <functional>
#include "Socket.h"
#include "Util/TimeTicker.h"

using namespace std;

namespace toolkit {

//Tcp客户端，Socket对象默认开始互斥锁
class TcpClient : public std::enable_shared_from_this<TcpClient>, public SocketHelper {
public:
    typedef std::shared_ptr<TcpClient> Ptr;
    TcpClient(const EventPoller::Ptr &poller = nullptr);
    ~TcpClient() override;

    /**
     * 开始连接tcp服务器
     * @param url 服务器ip或域名
     * @param port 服务器端口
     * @param timeout_sec 超时时间,单位秒
     */
    virtual void startConnect(const string &url, uint16_t port, float timeout_sec = 5);

    /**
     * 主动断开连接
     * @param ex 触发onErr事件时的参数
     */
    void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown")) override;

    /**
     * 判断是否与服务器连接中
     */
    virtual bool alive();

    /**
     * 设置网卡适配器,使用该网卡与服务器通信
     * @param local_ip 本地网卡ip
     */
    virtual void setNetAdapter(const string &local_ip);

protected:
    /**
     * 连接服务器结果回调
     * @param ex 成功与否
     */
    virtual void onConnect(const SockException &ex) {}

    /**
     * 收到数据回调
     * @param buf 接收到的数据(该buffer会重复使用)
     */
    virtual void onRecv(const Buffer::Ptr &buf) {}

    /**
     * 数据全部发送完毕后回调
     */
    virtual void onFlush() {}

    /**
     * 被动断开连接回调
     * @param ex 断开原因
     */
    virtual void onErr(const SockException &ex) {}

    /**
     * tcp连接成功后每2秒触发一次该事件
     */
    virtual void onManager() {}

private:
    void onSockConnect(const SockException &ex);

private:
    string _net_adapter = "0.0.0.0";
    std::shared_ptr<Timer> _timer;
};

} /* namespace toolkit */
#endif /* NETWORK_TCPCLIENT_H */
