/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtmpProtocol.h"
#include "Rtmp/utils.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"
using namespace toolkit;

#define C1_DIGEST_SIZE 32
#define C1_KEY_SIZE 128
#define C1_SCHEMA_SIZE 764
#define C1_HANDSHARK_SIZE (RANDOM_LEN + 8)
#define C1_FPKEY_SIZE 30
#define S1_FMS_KEY_SIZE 36
#define S2_FMS_KEY_SIZE 68
#define C1_OFFSET_SIZE 4

namespace mediakit {

RtmpProtocol::RtmpProtocol() {
    _next_step_func = [this](const char *data, uint64_t len) {
        return handle_C0C1(data, len);
    };
}

RtmpProtocol::~RtmpProtocol() {
    reset();
}

void RtmpProtocol::reset() {
    ////////////ChunkSize////////////
    _chunk_size_in = DEFAULT_CHUNK_LEN;
    _chunk_size_out = DEFAULT_CHUNK_LEN;
    ////////////Acknowledgement////////////
    _bytes_sent = 0;
    _bytes_sent_last = 0;
    _windows_size = 0;
    ///////////PeerBandwidth///////////
    _bandwidth = 2500000;
    _band_limit_type = 2;
    ////////////Chunk////////////
    _map_chunk_data.clear();
    _now_stream_index = 0;
    _now_chunk_id = 0;
    //////////Invoke Request//////////
    _send_req_id = 0;
    //////////Rtmp parser//////////
    HttpRequestSplitter::reset();
    _stream_index = STREAM_CONTROL;
    _next_step_func = [this](const char *data, uint64_t len) {
        return handle_C0C1(data, len);
    };
}

void RtmpProtocol::sendAcknowledgement(uint32_t size) {
    size = htonl(size);
    std::string acknowledgement((char *) &size, 4);
    sendRequest(MSG_ACK, acknowledgement);
}

void RtmpProtocol::sendAcknowledgementSize(uint32_t size) {
    size = htonl(size);
    std::string set_windowSize((char *) &size, 4);
    sendRequest(MSG_WIN_SIZE, set_windowSize);
}

void RtmpProtocol::sendPeerBandwidth(uint32_t size) {
    size = htonl(size);
    std::string set_peerBandwidth((char *) &size, 4);
    set_peerBandwidth.push_back((char) 0x02);
    sendRequest(MSG_SET_PEER_BW, set_peerBandwidth);
}

void RtmpProtocol::sendChunkSize(uint32_t size) {
    uint32_t len = htonl(size);
    std::string set_chunk((char *) &len, 4);
    sendRequest(MSG_SET_CHUNK, set_chunk);
    _chunk_size_out = size;
}

void RtmpProtocol::sendPingRequest(uint32_t stamp) {
    sendUserControl(CONTROL_PING_REQUEST, stamp);
}

void RtmpProtocol::sendPingResponse(uint32_t time_stamp) {
    sendUserControl(CONTROL_PING_RESPONSE, time_stamp);
}

void RtmpProtocol::sendSetBufferLength(uint32_t stream_index, uint32_t len) {
    std::string control;
    stream_index = htonl(stream_index);
    control.append((char *) &stream_index, 4);

    len = htonl(len);
    control.append((char *) &len, 4);
    sendUserControl(CONTROL_SETBUFFER, control);
}

void RtmpProtocol::sendUserControl(uint16_t event_type, uint32_t event_data) {
    std::string control;
    event_type = htons(event_type);
    control.append((char *) &event_type, 2);

    event_data = htonl(event_data);
    control.append((char *) &event_data, 4);
    sendRequest(MSG_USER_CONTROL, control);
}

void RtmpProtocol::sendUserControl(uint16_t event_type, const string &event_data) {
    std::string control;
    event_type = htons(event_type);
    control.append((char *) &event_type, 2);
    control.append(event_data);
    sendRequest(MSG_USER_CONTROL, control);
}

void RtmpProtocol::sendResponse(int type, const string &str) {
    if(!_data_started && (type == MSG_DATA)){
        _data_started =  true;
    }
    sendRtmp(type, _now_stream_index, str, 0, _data_started ? CHUNK_CLIENT_REQUEST_AFTER : CHUNK_CLIENT_REQUEST_BEFORE);
}

void RtmpProtocol::sendInvoke(const string &cmd, const AMFValue &val) {
    AMFEncoder enc;
    enc << cmd << ++_send_req_id << val;
    sendRequest(MSG_CMD, enc.data());
}

void RtmpProtocol::sendRequest(int cmd, const string& str) {
    sendRtmp(cmd, _stream_index, str, 0, CHUNK_SERVER_REQUEST);
}

class BufferPartial : public Buffer {
public:
    BufferPartial(const Buffer::Ptr &buffer, uint32_t offset, uint32_t size){
        _buffer = buffer;
        _data = buffer->data() + offset;
        _size = size;
    }

    ~BufferPartial() override{}

    char *data() const override {
        return _data;
    }

    uint32_t size() const override{
        return _size;
    }

private:
    char *_data;
    uint32_t _size;
    Buffer::Ptr _buffer;
};

void RtmpProtocol::sendRtmp(uint8_t type, uint32_t stream_index, const std::string &buffer, uint32_t stamp, int chunk_id) {
    sendRtmp(type, stream_index, std::make_shared<BufferString>(buffer), stamp, chunk_id);
}

void RtmpProtocol::sendRtmp(uint8_t type, uint32_t stream_index, const Buffer::Ptr &buf, uint32_t stamp, int chunk_id){
    if (chunk_id < 2 || chunk_id > 63) {
        auto strErr = StrPrinter << "不支持发送该类型的块流 ID:" << chunk_id << endl;
        throw std::runtime_error(strErr);
    }
    //是否有扩展时间戳
    bool ext_stamp = stamp >= 0xFFFFFF;

    //rtmp头
    BufferRaw::Ptr buffer_header = obtainBuffer();
    buffer_header->setCapacity(sizeof(RtmpHeader));
    buffer_header->setSize(sizeof(RtmpHeader));
    //对rtmp头赋值，如果使用整形赋值，在arm android上可能由于数据对齐导致总线错误的问题
    RtmpHeader *header = (RtmpHeader *) buffer_header->data();
    header->flags = (chunk_id & 0x3f) | (0 << 6);
    header->type_id = type;
    set_be24(header->time_stamp, ext_stamp ? 0xFFFFFF : stamp);
    set_be24(header->body_size, buf->size());
    set_le32(header->stream_index, stream_index);
    //发送rtmp头
    onSendRawData(std::move(buffer_header));

    //扩展时间戳字段
    BufferRaw::Ptr buffer_ext_stamp;
    if (ext_stamp) {
        //生成扩展时间戳
        buffer_ext_stamp = obtainBuffer();
        buffer_ext_stamp->setCapacity(4);
        buffer_ext_stamp->setSize(4);
        set_be32(buffer_ext_stamp->data(), stamp);
    }

    //生成一个字节的flag，标明是什么chunkId
    BufferRaw::Ptr buffer_flags = obtainBuffer();
    buffer_flags->setCapacity(1);
    buffer_flags->setSize(1);
    buffer_flags->data()[0] = (chunk_id & 0x3f) | (3 << 6);

    size_t offset = 0;
    uint32_t totalSize = sizeof(RtmpHeader);
    while (offset < buf->size()) {
        if (offset) {
            onSendRawData(buffer_flags);
            totalSize += 1;
        }
        if (ext_stamp) {
            //扩展时间戳
            onSendRawData(buffer_ext_stamp);
            totalSize += 4;
        }
        size_t chunk = min(_chunk_size_out, buf->size() - offset);
        onSendRawData(std::make_shared<BufferPartial>(buf, offset, chunk));
        totalSize += chunk;
        offset += chunk;
    }
    _bytes_sent += totalSize;
    if (_windows_size > 0 && _bytes_sent - _bytes_sent_last >= _windows_size) {
        _bytes_sent_last = _bytes_sent;
        sendAcknowledgement(_bytes_sent);
    }
}

void RtmpProtocol::onParseRtmp(const char *data, uint64_t size) {
    input(data, size);
}

const char *RtmpProtocol::onSearchPacketTail(const char *data,uint64_t len){
    //移动拷贝提高性能
    auto next_step_func(std::move(_next_step_func));
    //执行下一步
    auto ret = next_step_func(data, len);
    if (!_next_step_func) {
        //为设置下一步，恢复之
        next_step_func.swap(_next_step_func);
    }
    return ret;
}

////for client////
void RtmpProtocol::startClientSession(const function<void()> &func) {
    //发送 C0C1
    char handshake_head = HANDSHAKE_PLAINTEXT;
    onSendRawData(obtainBuffer(&handshake_head, 1));
    RtmpHandshake c1(0);
    onSendRawData(obtainBuffer((char *) (&c1), sizeof(c1)));
    _next_step_func = [this, func](const char *data, uint64_t len) {
        //等待 S0+S1+S2
        return handle_S0S1S2(data, len, func);
    };
}

const char* RtmpProtocol::handle_S0S1S2(const char *data, uint64_t len, const function<void()> &func) {
    if (len < 1 + 2 * C1_HANDSHARK_SIZE) {
        //数据不够
        return nullptr;
    }
    if (data[0] != HANDSHAKE_PLAINTEXT) {
        throw std::runtime_error("only plaintext[0x03] handshake supported");
    }
    //发送 C2
    const char *pcC2 = data + 1;
    onSendRawData(obtainBuffer(pcC2, C1_HANDSHARK_SIZE));
    //握手结束
    _next_step_func = [this](const char *data, uint64_t len) {
        //握手结束并且开始进入解析命令模式
        return handle_rtmp(data, len);
    };
    func();
    return data + 1 + 2 * C1_HANDSHARK_SIZE;
}

////for server ////
const char * RtmpProtocol::handle_C0C1(const char *data, uint64_t len) {
    if (len < 1 + C1_HANDSHARK_SIZE) {
        //need more data!
        return nullptr;
    }
    if (data[0] != HANDSHAKE_PLAINTEXT) {
        throw std::runtime_error("only plaintext[0x03] handshake supported");
    }
    handle_C1_simple(data);
    return data + 1 + C1_HANDSHARK_SIZE;
}

void RtmpProtocol::handle_C1_simple(const char *data){
    //发送S0
    char handshake_head = HANDSHAKE_PLAINTEXT;
    onSendRawData(obtainBuffer(&handshake_head, 1));
    //发送S1
    RtmpHandshake s1(0);
    onSendRawData(obtainBuffer((char *) &s1, C1_HANDSHARK_SIZE));
    //发送S2
    onSendRawData(obtainBuffer(data + 1, C1_HANDSHARK_SIZE));
    //等待C2
    _next_step_func = [this](const char *data, uint64_t len) {
        //握手结束并且开始进入解析命令模式
        return handle_C2(data, len);
    };
}

const char* RtmpProtocol::handle_C2(const char *data, uint64_t len) {
    if (len < C1_HANDSHARK_SIZE) {
        //need more data!
        return nullptr;
    }
    _next_step_func = [this](const char *data, uint64_t len) {
        return handle_rtmp(data, len);
    };

    //握手结束，进入命令模式
    return handle_rtmp(data + C1_HANDSHARK_SIZE, len - C1_HANDSHARK_SIZE);
}

static const size_t HEADER_LENGTH[] = {12, 8, 4, 1};

const char* RtmpProtocol::handle_rtmp(const char *data, uint64_t len) {
    auto ptr = data;
    while (len) {
        int offset = 0;
        uint8_t flags = ptr[0];
        size_t header_len = HEADER_LENGTH[flags >> 6];
        _now_chunk_id = flags & 0x3f;
        switch (_now_chunk_id) {
            case 0: {
                //0 值表示二字节形式，并且 ID 范围 64 - 319
                //(第二个字节 + 64)。
                if (len < 2) {
                    //need more data
                    return ptr;
                }
                _now_chunk_id = 64 + (uint8_t) (ptr[1]);
                offset = 1;
                break;
            }

            case 1: {
                //1 值表示三字节形式，并且 ID 范围为 64 - 65599
                //((第三个字节) * 256 + 第二个字节 + 64)。
                if (len < 3) {
                    //need more data
                    return ptr;
                }
                _now_chunk_id = 64 + ((uint8_t) (ptr[2]) << 8) + (uint8_t) (ptr[1]);
                offset = 2;
                break;
            }

            //带有 2 值的块流 ID 被保留，用于下层协议控制消息和命令。
            default : break;
        }

        if (len < header_len + offset) {
            //need more data
            return ptr;
        }

        RtmpHeader &header = *((RtmpHeader *) (ptr + offset));
        auto &chunk_data = _map_chunk_data[_now_chunk_id];
        chunk_data.chunk_id = _now_chunk_id;
        switch (header_len) {
            case 12:
                chunk_data.is_abs_stamp = true;
                chunk_data.stream_index = load_le32(header.stream_index);
            case 8:
                chunk_data.body_size = load_be24(header.body_size);
                chunk_data.type_id = header.type_id;
            case 4:
                chunk_data.ts_field = load_be24(header.time_stamp);
        }

        auto time_stamp = chunk_data.ts_field;
        if (chunk_data.ts_field == 0xFFFFFF) {
            if (len < header_len + offset + 4) {
                //need more data
                return ptr;
            }
            time_stamp = load_be32(ptr + offset + header_len);
            offset += 4;
        }

        if (chunk_data.body_size < chunk_data.buffer.size()) {
            throw std::runtime_error("非法的bodySize");
        }

        auto more = min(_chunk_size_in, (size_t)(chunk_data.body_size - chunk_data.buffer.size()));
        if (len < header_len + offset + more) {
            //need more data
            return ptr;
        }
        if (more) {
            chunk_data.buffer.append(ptr + header_len + offset, more);
        }
        ptr += header_len + offset + more;
        len -= header_len + offset + more;
        if (chunk_data.buffer.size() == chunk_data.body_size) {
            //frame is ready
            _now_stream_index = chunk_data.stream_index;
            chunk_data.time_stamp = time_stamp + (chunk_data.is_abs_stamp ? 0 : chunk_data.time_stamp);
            if (chunk_data.body_size) {
                handle_chunk(chunk_data);
            }
            chunk_data.buffer.clear();
            chunk_data.is_abs_stamp = false;
        }
    }
    return ptr;
}

void RtmpProtocol::handle_chunk(RtmpPacket& chunk_data) {
    switch (chunk_data.type_id) {
        case MSG_ACK: {
            if (chunk_data.buffer.size() < 4) {
                throw std::runtime_error("MSG_ACK: Not enough data");
            }
            //auto bytePeerRecv = load_be32(&chunk_data.buffer[0]);
            //TraceL << "MSG_ACK:" << bytePeerRecv;
            break;
        }

        case MSG_SET_CHUNK: {
            if (chunk_data.buffer.size() < 4) {
                throw std::runtime_error("MSG_SET_CHUNK :Not enough data");
            }
            _chunk_size_in = load_be32(&chunk_data.buffer[0]);
            TraceL << "MSG_SET_CHUNK:" << _chunk_size_in;
            break;
        }

        case MSG_USER_CONTROL: {
            //user control message
            if (chunk_data.buffer.size() < 2) {
                throw std::runtime_error("MSG_USER_CONTROL: Not enough data.");
            }
            uint16_t event_type = load_be16(&chunk_data.buffer[0]);
            chunk_data.buffer.erase(0, 2);
            switch (event_type) {
                case CONTROL_PING_REQUEST: {
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_PING_REQUEST: Not enough data.");
                    }
                    uint32_t timeStamp = load_be32(&chunk_data.buffer[0]);
                    //TraceL << "CONTROL_PING_REQUEST:" << time_stamp;
                    sendUserControl(CONTROL_PING_RESPONSE, timeStamp);
                    break;
                }

                case CONTROL_PING_RESPONSE: {
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_PING_RESPONSE: Not enough data.");
                    }
                    //uint32_t time_stamp = load_be32(&chunk_data.buffer[0]);
                    //TraceL << "CONTROL_PING_RESPONSE:" << time_stamp;
                    break;
                }

                case CONTROL_STREAM_BEGIN: {
                    //开始播放
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_STREAM_BEGIN: Not enough data.");
                    }
                    uint32_t stream_index = load_be32(&chunk_data.buffer[0]);
                    onStreamBegin(stream_index);
                    TraceL << "CONTROL_STREAM_BEGIN:" << stream_index;
                    break;
                }

                case CONTROL_STREAM_EOF: {
                    //暂停
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_STREAM_EOF: Not enough data.");
                    }
                    uint32_t stream_index = load_be32(&chunk_data.buffer[0]);
                    onStreamEof(stream_index);
                    TraceL << "CONTROL_STREAM_EOF:" << stream_index;
                    break;
                }

                case CONTROL_STREAM_DRY: {
                    //停止播放
                    if (chunk_data.buffer.size() < 4) {
                        throw std::runtime_error("CONTROL_STREAM_DRY: Not enough data.");
                    }
                    uint32_t stream_index = load_be32(&chunk_data.buffer[0]);
                    onStreamDry(stream_index);
                    TraceL << "CONTROL_STREAM_DRY:" << stream_index;
                    break;
                }

                default: /*WarnL << "unhandled user control:" << event_type; */ break;
            }
            break;
        }

        case MSG_WIN_SIZE: {
            _windows_size = load_be32(&chunk_data.buffer[0]);
            TraceL << "MSG_WIN_SIZE:" << _windows_size;
            break;
        }

        case MSG_SET_PEER_BW: {
            _bandwidth = load_be32(&chunk_data.buffer[0]);
            _band_limit_type =  chunk_data.buffer[4];
            TraceL << "MSG_SET_PEER_BW:" << _windows_size;
            break;
        }

        case MSG_AGGREGATE: {
            auto ptr = (uint8_t *) chunk_data.buffer.data();
            auto ptr_tail = ptr + chunk_data.buffer.size();
            while (ptr + 8 + 3 < ptr_tail) {
                auto type = *ptr;
                ptr += 1;
                auto size = load_be24(ptr);
                ptr += 3;
                auto ts = load_be24(ptr);
                ptr += 3;
                ts |= (*ptr << 24);
                ptr += 1;
                ptr += 3;
                //参考FFmpeg多拷贝了4个字节
                size += 4;
                if (ptr + size > ptr_tail) {
                    break;
                }
                RtmpPacket sub_packet;
                sub_packet.buffer.assign((char *)ptr, size);
                sub_packet.type_id = type;
                sub_packet.body_size = size;
                sub_packet.time_stamp = ts;
                sub_packet.stream_index = chunk_data.stream_index;
                sub_packet.chunk_id = chunk_data.chunk_id;
                handle_chunk(sub_packet);
                ptr += size;
            }
            break;
        }

        default: onRtmpChunk(chunk_data); break;
    }
}

BufferRaw::Ptr RtmpProtocol::obtainBuffer() {
    return std::make_shared<BufferRaw>() ;//_bufferPool.obtain();
}

BufferRaw::Ptr RtmpProtocol::obtainBuffer(const void *data, int len) {
    auto buffer = obtainBuffer();
    buffer->assign((const char *) data, len);
    return buffer;
}

} /* namespace mediakit */
