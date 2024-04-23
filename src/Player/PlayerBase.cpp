/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include "PlayerBase.h"
#include "Rtsp/RtspPlayerImp.h"
#include "Rtmp/RtmpPlayerImp.h"
using namespace toolkit;

namespace mediakit {

PlayerBase::PlayerBase() {
    this->mINI::operator[](kTimeoutMS) = 10000;
    this->mINI::operator[](kMediaTimeoutMS) = 5000;
    this->mINI::operator[](kBeatIntervalMS) = 5000;
    this->mINI::operator[](kMaxAnalysisMS) = 5000;
}

///////////////////////////Demuxer//////////////////////////////
bool Demuxer::isInited(int analysisMs) {
    if(analysisMs && _ticker.createdTime() > analysisMs){
        //analysisMs毫秒后强制初始化完毕
        return true;
    }
    if (_videoTrack && !_videoTrack->ready()) {
        //视频未准备好
        return false;
    }
    if (_audioTrack && !_audioTrack->ready()) {
        //音频未准备好
        return false;
    }
    return true;
}

vector<Track::Ptr> Demuxer::getTracks(bool trackReady) const {
    vector<Track::Ptr> ret;
    if(_videoTrack){
        if(trackReady){
            if(_videoTrack->ready()){
                ret.emplace_back(_videoTrack);
            }
        }else{
            ret.emplace_back(_videoTrack);
        }
    }
    if(_audioTrack){
        if(trackReady){
            if(_audioTrack->ready()){
                ret.emplace_back(_audioTrack);
            }
        }else{
            ret.emplace_back(_audioTrack);
        }
    }
    return ret;
}

float Demuxer::getDuration() const {
    return _fDuration;
}

void Demuxer::onAddTrack(const Track::Ptr &track){
    if(_listener){
        _listener->onAddTrack(track);
    }
}

void Demuxer::setTrackListener(Demuxer::Listener *listener) {
    _listener = listener;
}

} /* namespace mediakit */
