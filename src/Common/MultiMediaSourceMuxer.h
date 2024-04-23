/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H

#include <mutex>
#include <list>

#include "Common/Stamp.h"
#include "Util/File.h"
#include "Rtsp/RtspMediaSourceMuxer.h"
#include "Rtmp/RtmpMediaSourceMuxer.h"
#include "Http/FMP4MediaSourceMuxer.h"

namespace mediakit{

class MultiMuxerPrivate : public MediaSink,
                                     public std::enable_shared_from_this<MultiMuxerPrivate> {
public:
    friend class MultiMediaSourceMuxer;
    typedef std::shared_ptr<MultiMuxerPrivate> Ptr;
    class Listener{
    public:
        Listener() = default;
        virtual ~Listener() = default;
        virtual void onAllTrackReady() = 0;
    };

    ~MultiMuxerPrivate() override;

private:
    MultiMuxerPrivate(const string &vhost,const string &app, const string &stream,
                            bool enable_rtsp, bool enable_rtmp);
    void resetTracks() override;
    void setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener);
    int totalReaderCount() const;
    void setTimeStamp(uint32_t stamp);
    void setTrackListener(Listener *listener);
    bool isEnabled();
    void onTrackReady(const Track::Ptr & track) override;
    void onTrackFrame(const Frame::Ptr &frame) override;
    void onAllTrackReady() override;

private:
    std::string stream_id_;
    Listener *_track_listener = nullptr;
    RtmpMediaSourceMuxer::Ptr _rtmp;
    RtspMediaSourceMuxer::Ptr _rtsp;
    FMP4MediaSourceMuxer::Ptr _fmp4;
    std::weak_ptr<MediaSourceEvent> _listener;
};

class MultiMediaSourceMuxer : public MediaSourceEventInterceptor,
                                            public MediaSinkInterface,
                                            public MultiMuxerPrivate::Listener,
                                            public std::enable_shared_from_this<MultiMediaSourceMuxer>{
public:
    typedef MultiMuxerPrivate::Listener Listener;
    typedef std::shared_ptr<MultiMediaSourceMuxer> Ptr;

    ~MultiMediaSourceMuxer() override;
    MultiMediaSourceMuxer(const string &vhost,
                                 const string &app,
                                 const string &stream,
                                 bool enable_rtsp = true, bool enable_rtmp = true);
    void setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener);

    void setTrackListener(const std::weak_ptr<MultiMuxerPrivate::Listener> &listener);

    int totalReaderCount() const;

    bool isEnabled();

    void setTimeStamp(uint32_t stamp);


    vector<Track::Ptr> getTracks(MediaSource &sender, bool trackReady = true) const override;
    vector<Track::Ptr> getTracks(bool trackReady = true);
    int totalReaderCount(MediaSource &sender) override;


    /**
    * 添加track，内部会调用Track的clone方法
    * 只会克隆sps pps这些信息 ，而不会克隆Delegate相关关系
    * @param track 添加音频或视频轨道
    */
    void addTrack(const Track::Ptr &track) override;

    void addTrackCompleted() override;

    void resetTracks() override;

    void inputFrame(const Frame::Ptr &frame) override;
    
    bool input_frame(const Frame::Ptr &frame);
    
    void onAllTrackReady() override;

    void trace_fps();
private:
    bool _is_enable = false;
    Ticker _last_check;
    MultiMuxerPrivate::Ptr _muxer;
    std::weak_ptr<MultiMuxerPrivate::Listener> _track_listener;
    std::string stream_id_;

    //frame time stamp
    Stamp _stamp;

    //SEI
    std::string live_sei_frame_;
    std::string analyzer_sei_payload_;

    //trace_fps
    int trace_fps_frame_count_ = 0;
    std::uint64_t trace_fps_start_time_ = 0;
    int trace_fps_print_rate_ = 0;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
