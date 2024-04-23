/*
* Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
*
* This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
*
* Use of this source code is governed by MIT license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/

#include <math.h>
#include <chrono>
#include "Common/config.h"
#include "MultiMediaSourceMuxer.h"
#include "Config.h"
#include "json/json.h"

namespace mediakit {

MultiMuxerPrivate::MultiMuxerPrivate(const string &vhost,
                                          const string &app,
                                          const string &stream,
                                          bool enable_rtsp,
                                          bool enable_rtmp) : MediaSink(stream), stream_id_(stream)  {
    if (enable_rtmp) {
        _rtmp = std::make_shared<RtmpMediaSourceMuxer>(vhost, app, stream, std::make_shared<TitleMeta>(0));
    }
    if (enable_rtsp) {
        _rtsp = std::make_shared<RtspMediaSourceMuxer>(vhost, app, stream, std::make_shared<TitleSdp>());
    }

    _fmp4 = std::make_shared<FMP4MediaSourceMuxer>(vhost, app, stream);
}

MultiMuxerPrivate::~MultiMuxerPrivate() {}

void MultiMuxerPrivate::resetTracks() {
    if (_rtmp) {
        _rtmp->resetTracks();
    }
    if (_rtsp) {
        _rtsp->resetTracks();
    }
    if (_fmp4) {
        _fmp4->resetTracks();
    }
}

void MultiMuxerPrivate::setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener) {
    _listener = listener;
    if (_rtmp) {
        _rtmp->setListener(listener);
    }
    if (_rtsp) {
        _rtsp->setListener(listener);
    }
    if (_fmp4) {
        _fmp4->setListener(listener);
    }
}

int MultiMuxerPrivate::totalReaderCount() const {
    return (_rtsp ? _rtsp->readerCount() : 0) +
           (_rtmp ? _rtmp->readerCount() : 0) +
           (_fmp4 ? _fmp4->readerCount() : 0);
}


void MultiMuxerPrivate::setTimeStamp(uint32_t stamp) {
    if (_rtmp) {
        _rtmp->setTimeStamp(stamp);
    }
    if (_rtsp) {
        _rtsp->setTimeStamp(stamp);
    }
}

void MultiMuxerPrivate::setTrackListener(Listener *listener) {
    _track_listener = listener;
}

void MultiMuxerPrivate::onTrackReady(const Track::Ptr &track) {
    if (_rtmp) {
        _rtmp->addTrack(track);
    }
    if (_rtsp) {
        _rtsp->addTrack(track);
    }
    if (_fmp4) {
        _fmp4->addTrack(track);
    }
}

bool MultiMuxerPrivate::isEnabled(){
    return (_rtmp ? _rtmp->isEnabled() : false) ||
           (_fmp4 ? _fmp4->isEnabled() : false) ||
           (_rtsp ? _rtsp->isEnabled() : false);
}

void MultiMuxerPrivate::onTrackFrame(const Frame::Ptr &frame) {
    if (_rtmp) {
        _rtmp->inputFrame(frame);
    }
    if (_rtsp) {
        _rtsp->inputFrame(frame);
    }
    if (_fmp4) {
        _fmp4->inputFrame(frame);
    }
}

static string getTrackInfoStr(const TrackSource *track_src){
    _StrPrinter codec_info;
    auto tracks = track_src->getTracks(true);
    for (auto &track : tracks) {
        auto codec_type = track->getTrackType();
        codec_info << track->getCodecName();
        switch (codec_type) {
            case TrackAudio : {
                auto audio_track = dynamic_pointer_cast<AudioTrack>(track);
                codec_info << "["
                           << audio_track->getAudioSampleRate() << "/"
                           << audio_track->getAudioChannel() << "/"
                           << audio_track->getAudioSampleBit() << "] ";
                break;
            }
            case TrackVideo : {
                auto video_track = dynamic_pointer_cast<VideoTrack>(track);
                codec_info << "["
                           << video_track->getVideoWidth() << "/"
                           << video_track->getVideoHeight() << "/"
                           << round(video_track->getVideoFps()) << "] ";
                break;
            }
            default:
                break;
        }
    }
    return codec_info;
}

void MultiMuxerPrivate::onAllTrackReady() {
    if (_rtmp) {
        _rtmp->onAllTrackReady();
    }
    if (_rtsp) {
        _rtsp->onAllTrackReady();
    }
    if (_fmp4) {
        _fmp4->onAllTrackReady();
    }
    if (_track_listener) {
        _track_listener->onAllTrackReady();
    }
    InfoL << stream_id_ << ", codec info: " << getTrackInfoStr(this);
}

MultiMediaSourceMuxer::MultiMediaSourceMuxer(const string &vhost,
                                                    const string &app,
                                                    const string &stream,
                                                    bool enable_rtsp, bool enable_rtmp) : stream_id_(stream){
    _muxer.reset(new MultiMuxerPrivate(vhost, app, stream, enable_rtsp, enable_rtmp));
    _muxer->setTrackListener(this);
}

MultiMediaSourceMuxer::~MultiMediaSourceMuxer() {}

void MultiMediaSourceMuxer::setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener) {
    setDelegate(listener);
    _muxer->setMediaListener(shared_from_this());
}

void MultiMediaSourceMuxer::setTrackListener(const std::weak_ptr<MultiMuxerPrivate::Listener> &listener) {
    _track_listener = listener;
}

int MultiMediaSourceMuxer::totalReaderCount() const {
    return _muxer->totalReaderCount();
}

void MultiMediaSourceMuxer::setTimeStamp(uint32_t stamp) {
    _muxer->setTimeStamp(stamp);
}

vector<Track::Ptr> MultiMediaSourceMuxer::getTracks(MediaSource &sender, bool trackReady) const {
    return _muxer->getTracks(trackReady);
}

vector<Track::Ptr> MultiMediaSourceMuxer::getTracks(bool trackReady) {
    return _muxer->getTracks(trackReady);
}

int MultiMediaSourceMuxer::totalReaderCount(MediaSource &sender) {
    auto listener = getDelegate();
    if (!listener) {
        return totalReaderCount();
    }
    return listener->totalReaderCount(sender);
}

void MultiMediaSourceMuxer::addTrack(const Track::Ptr &track) {
    _muxer->addTrack(track);
}

void MultiMediaSourceMuxer::addTrackCompleted() {
    _muxer->addTrackCompleted();
}

void MultiMediaSourceMuxer::onAllTrackReady(){
    _muxer->setMediaListener(shared_from_this());
    auto listener = _track_listener.lock();
    if(listener){
        listener->onAllTrackReady();
    }
}

void MultiMediaSourceMuxer::resetTracks() {
    _muxer->resetTracks();
}

void MultiMediaSourceMuxer::inputFrame(const Frame::Ptr &frame) {
    _muxer->inputFrame(frame);
}

bool MultiMediaSourceMuxer::input_frame(const Frame::Ptr &frame) {    
    _muxer->inputFrame(frame);
    return _muxer->get_inputframe_valid();
}

bool MultiMediaSourceMuxer::isEnabled(){
    if (!_is_enable || _last_check.elapsedTime() > ConfigInfo.preview.stream_none_reader_timeout) {
        //无人观看时，每次检查是否真的无人观看
        //有人观看时，则延迟一定时间检查一遍是否无人观看了(节省性能)
        _is_enable = _muxer->isEnabled();
        if (_is_enable) {
            //无人观看时，不刷新计时器,因为无人观看时每次都会检查一遍，所以刷新计数器无意义且浪费cpu
            _last_check.resetTime();
        }
    }
    return _is_enable;
}

}//namespace mediakit
