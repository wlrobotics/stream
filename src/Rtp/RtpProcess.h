/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPPROCESS_H
#define ZLMEDIAKIT_RTPPROCESS_H

#include "ProcessInterface.h"
#include "Common/MultiMediaSourceMuxer.h"

namespace mediakit {

class RtpProcess : public SockInfo, 
                          public MediaSinkInterface,
                          public MediaSourceEventInterceptor,
                          public std::enable_shared_from_this<RtpProcess> {
public:
    typedef std::shared_ptr<RtpProcess> Ptr;
    friend class RtpProcessHelper;
    RtpProcess(const string &stream_id);
    ~RtpProcess();

    bool inputRtp(bool is_udp, const Socket::Ptr &sock,
                     const char *data, int len,
                     const struct sockaddr *addr,
                     uint32_t *dts_out = nullptr);

    bool alive();

    void onDetach();

    void setOnDetach(const std::function<void()> &cb);

    /// SockInfo override
    std::string get_local_ip() override;
    uint16_t get_local_port() override;
    std::string get_peer_ip() override;
    uint16_t get_peer_port() override;
    std::string getIdentifier() const override;

    int totalReaderCount();
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener);
    std::vector<Track::Ptr> getTracks();
    uint64_t get_uptime();
    uint64_t get_total_bytes(){return _total_bytes;};
    uint32_t get_byte_rate(){return speed_.getSpeed(false);};
    int get_rtp_loss_rate();

protected:
    void inputFrame(const Frame::Ptr &frame) override;
    void addTrack(const Track::Ptr & track) override;
    void addTrackCompleted() override;
    void resetTracks() override {};

    //// MediaSourceEvent override ////
    MediaOriginType getOriginType(MediaSource &sender) const override;
    std::string getOriginUrl(MediaSource &sender) const override;
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;

private:
    void emitOnPublish();

private:
    uint32_t _dts = 0;
    uint64_t _total_bytes = 0;
    BytesSpeed speed_;
    struct sockaddr *_addr = nullptr;
    Socket::Ptr _sock;
    MediaInfo _media_info;
    Ticker _last_frame_time;
    std::function<void()> _on_detach;
    std::shared_ptr<FILE> _save_file_rtp;
    std::shared_ptr<FILE> _save_file_video;
    ProcessInterface::Ptr _process;
    MultiMediaSourceMuxer::Ptr _muxer;

    unsigned int frame_count_ = 0;
    Stamp _stamp;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_RTPPROCESS_H
