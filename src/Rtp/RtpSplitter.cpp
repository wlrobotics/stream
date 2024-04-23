﻿/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include "RtpSplitter.h"
namespace mediakit{

RtpSplitter::RtpSplitter() {}
RtpSplitter::~RtpSplitter() {}

int64_t RtpSplitter::onRecvHeader(const char *data,uint64_t len){
    //忽略偏移量
    data += _offset;
    len -= _offset;
    onRtpPacket(data, len);
    return 0;
}

const char *RtpSplitter::onSearchPacketTail(const char *data, uint64_t len) {
    if (len < 4) {
        //数据不够
        return nullptr;
    }

    if (data[0] == '$') {
        //可能是4个字节的rtp头
        _offset = 4;
        return onSearchPacketTail_l(data + 2, len - 2);
    }
    //两个字节的rtp头
    _offset = 2;
    return onSearchPacketTail_l(data, len);
}

const char *RtpSplitter::onSearchPacketTail_l(const char *data, uint64_t len) {
    //这是rtp包
    uint16_t length = (((uint8_t *) data)[0] << 8) | ((uint8_t *) data)[1];
    if (len < length + 2) {
        //数据不够
        return nullptr;
    }
    //返回rtp包末尾
    return data + 2 + length;
}

}//namespace mediakit