﻿/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPSESSION_H
#define ZLMEDIAKIT_RTPSESSION_H

#include "Network/TcpSession.h"
#include "RtpSplitter.h"
#include "RtpProcess.h"
#include "Util/TimeTicker.h"
using namespace toolkit;

namespace mediakit{

class RtpSession : public TcpSession , public RtpSplitter , public MediaSourceEvent{
public:
    static const string kStreamID;
    RtpSession(const Socket::Ptr &sock);
    ~RtpSession() override;
    void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override;
    void onManager() override;
    void attachServer(const TcpServer &server) override;

protected:
    bool close(MediaSource &sender,bool force) override;
    int totalReaderCount(MediaSource &sender) override;
    void onRtpPacket(const char *data, uint64_t len) override;

private:
    Ticker _ticker;
    string _stream_id;
    struct sockaddr addr;
    RtpProcess::Ptr _process;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_RTPSESSION_H
