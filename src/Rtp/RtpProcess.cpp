/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#include "RtpProcess.h"

#include "GB28181Process.h"
#include "RtpSplitter.h"
#include "Util/File.h"
#include "Extension/H264.h"
#include "Config.h"

namespace mediakit {

static std::string printAddress(const struct sockaddr *addr) {
    return StrPrinter << SockUtil::inet_ntoa(((struct sockaddr_in *) addr)->sin_addr) 
                      << ":" 
                      << ntohs(((struct sockaddr_in *) addr)->sin_port);
}

RtpProcess::RtpProcess(const string &stream_id) {
    _media_info._schema = "rtp";
    _media_info._vhost = DEFAULT_VHOST;
    _media_info._app = LIVE_APP;
    _media_info._streamid = stream_id;

    {
        FILE *fp = !ConfigInfo.rtp.dumpdir.empty() ? File::create_file(File::absolutePath(_media_info._streamid + ".rtp", ConfigInfo.rtp.dumpdir).data(), "wb") : nullptr;
        if (fp) {
            _save_file_rtp.reset(fp, [](FILE *fp) {
                fclose(fp);
            });
        }
    }

    {
        FILE *fp = !ConfigInfo.rtp.dumpdir.empty() ? File::create_file(File::absolutePath(_media_info._streamid + ".video", ConfigInfo.rtp.dumpdir).data(), "wb") : nullptr;
        if (fp) {
            _save_file_video.reset(fp, [](FILE *fp) {
                fclose(fp);
            });
        }
    }
    
    WarnP(this) << _media_info._streamid;
}

RtpProcess::~RtpProcess() {
    uint64_t duration = (_last_frame_time.createdTime() - _last_frame_time.elapsedTime()) / 1000;
    WarnP(this) << "RTP Pusher Break,"
                << _media_info._streamid
                << "," << duration << "s";
    if (_addr) {
        delete _addr;
        _addr = nullptr;
    }
}

bool RtpProcess::inputRtp(bool is_udp, const Socket::Ptr &sock,
                             const char *data, int len,
                             const struct sockaddr *addr, uint32_t *dts_out) {
    if (!_addr) {
        _addr = new struct sockaddr;
        _sock = sock;
        memcpy(_addr, addr, sizeof(struct sockaddr));
        InfoP(this) << "bind to address:" << printAddress(_addr) << ","<< _media_info._streamid;
        emitOnPublish();
    }

    if (!_muxer) {
        return false;
    }
    
    if (std::memcmp(_addr, addr, sizeof(struct sockaddr)) != 0) {
        DebugP(this) << "address dismatch:" << printAddress(addr) << " != " << printAddress(_addr);
        return false;
    }

    _total_bytes += len;
    speed_ += len;
    if (_save_file_rtp) {
        uint16_t size = len;
        size = htons(size);
        fwrite((uint8_t *) &size, 2, 1, _save_file_rtp.get());
        fwrite((uint8_t *) data, len, 1, _save_file_rtp.get());
    }
    if (!_process) {
        _process = std::make_shared<GB28181Process>(_media_info, this);
    }

    if (!_muxer->isEnabled() && !dts_out && ConfigInfo.rtp.dumpdir.empty()) {
        //无人访问、且不取时间戳、不导出调试文件时，我们可以直接丢弃数据
        _last_frame_time.resetTime();
        return false;
    }

    bool ret = _process ? _process->inputRtp(is_udp, data, len) : false;
    if (dts_out) {
        *dts_out = _dts;
    }
    return ret;
}

void RtpProcess::inputFrame(const Frame::Ptr &frame) {
    _last_frame_time.resetTime();

    _dts = frame->dts();

    _muxer->input_frame(frame);
}

void RtpProcess::addTrack(const Track::Ptr &track) {
    _muxer->addTrack(track);
}

void RtpProcess::addTrackCompleted() {
    _muxer->addTrackCompleted();
}

bool RtpProcess::alive() {
    GET_CONFIG(int, timeoutSec, RtpProxy::kTimeoutSec)
    if (_last_frame_time.elapsedTime() / 1000 < timeoutSec) {
        return true;
    }
    return false;
}

void RtpProcess::onDetach() {
    if (_on_detach) {
        _on_detach();
    }
}

void RtpProcess::setOnDetach(const function<void()> &cb) {
    _on_detach = cb;
}

std::string RtpProcess::get_peer_ip() {
    if (_addr) {
        return SockUtil::inet_ntoa(((struct sockaddr_in *) _addr)->sin_addr);
    }
    return "0.0.0.0";
}

uint16_t RtpProcess::get_peer_port() {
    if (!_addr) {
        return 0;
    }
    return ntohs(((struct sockaddr_in *) _addr)->sin_port);
}

std::string RtpProcess::get_local_ip() {
    if (_sock) {
        return _sock->get_local_ip();
    }
    return "0.0.0.0";
}

uint16_t RtpProcess::get_local_port() {
    if (_sock) {
        return _sock->get_local_port();
    }
    return 0;
}

std::string RtpProcess::getIdentifier() const {
    return _media_info._streamid;
}

int RtpProcess::totalReaderCount() {
    return _muxer ? _muxer->totalReaderCount() : 0;
}

void RtpProcess::setListener(const std::weak_ptr<MediaSourceEvent> &listener) {
    setDelegate(listener);
}

std::vector<Track::Ptr> RtpProcess::getTracks() {
    return _muxer ? _muxer->getTracks() : vector<Track::Ptr>();
}

uint64_t RtpProcess::get_uptime() {
    return _last_frame_time.createdTime();
}

void RtpProcess::emitOnPublish() {
    _muxer = std::make_shared<MultiMediaSourceMuxer>(_media_info._vhost,
                                                     _media_info._app,
                                                     _media_info._streamid);
    _muxer->setMediaListener(shared_from_this());
}

MediaOriginType RtpProcess::getOriginType(MediaSource &sender) const{
    return MediaOriginType::rtp_push;
}

std::string RtpProcess::getOriginUrl(MediaSource &sender) const {
    return _media_info._full_url;
}

std::shared_ptr<SockInfo> RtpProcess::getOriginSock(MediaSource &sender) const{
    return const_cast<RtpProcess *>(this)->shared_from_this();
}

int RtpProcess::get_rtp_loss_rate() {
    if (_process == nullptr) {
        return 0;
    }
    auto process = dynamic_pointer_cast<GB28181Process>(_process);
    if (process == nullptr) {
        return 0;
    }
    return process->get_rtp_loss_rate();
}

}//namespace mediakit
