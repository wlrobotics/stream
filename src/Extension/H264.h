﻿/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_H264_H
#define ZLMEDIAKIT_H264_H

#include "Frame.h"
#include "Track.h"
#include "Util/base64.h"
#include "Util/logger.h"

using namespace toolkit;
#define H264_TYPE(v) ((uint8_t)(v) & 0x1F)

namespace mediakit{

bool getAVCInfo(const string &strSps,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps);
void splitH264(const char *ptr, int len, int prefix, const std::function<void(const char *, int, int)> &cb);
int prefixSize(const char *ptr, int len);
/**
 * 264帧类
 */
class H264Frame : public FrameImp {
public:
    typedef std::shared_ptr<H264Frame> Ptr;

    typedef enum {
        NAL_IDR = 5,
        NAL_SEI = 6,
        NAL_SPS = 7,
        NAL_PPS = 8,
        NAL_AUD = 9,
        NAL_B_P = 1,
    } NalType;

    H264Frame(){
        _codec_id = CodecH264;
    }

    bool keyFrame() const override {
        return H264_TYPE(_buffer[_prefix_size]) == H264Frame::NAL_IDR;
    }

    bool configFrame() const override{
        switch(H264_TYPE(_buffer[_prefix_size]) ){
            case H264Frame::NAL_SPS:
            case H264Frame::NAL_PPS:return true;
            default:return false;
        }
    }
};

/**
 * 防止内存拷贝的H264类
 * 用户可以通过该类型快速把一个指针无拷贝的包装成Frame类
 * 该类型在DevChannel中有使用
 */
class H264FrameNoCacheAble : public FrameFromPtr {
public:
    typedef std::shared_ptr<H264FrameNoCacheAble> Ptr;

    H264FrameNoCacheAble(char *ptr,uint32_t size, uint32_t dts , uint32_t pts ,int prefix_size = 4){
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _pts = pts;
        _prefix_size = prefix_size;
        _codec_id = CodecH264;
    }

    bool keyFrame() const override {
        return H264_TYPE(_ptr[_prefix_size]) == H264Frame::NAL_IDR;
    }

    bool configFrame() const override{
        switch(H264_TYPE(_ptr[_prefix_size])){
            case H264Frame::NAL_SPS:
            case H264Frame::NAL_PPS:return true;
            default:return false;
        }
    }
};

typedef FrameInternal<H264FrameNoCacheAble> H264FrameInternal;


class H264Track : public VideoTrack{
public:
    typedef std::shared_ptr<H264Track> Ptr;

    /**
     * 不指定sps pps构造h264类型的媒体
     * 在随后的inputFrame中获取sps pps
     */
    H264Track(){}
    /**
     * 构造h264类型的媒体
     * @param sps sps帧数据
     * @param pps pps帧数据
     * @param sps_prefix_len 264头长度，可以为3个或4个字节，一般为0x00 00 00 01
     * @param pps_prefix_len 264头长度，可以为3个或4个字节，一般为0x00 00 00 01
     */
    H264Track(const string &sps,const string &pps,int sps_prefix_len = 4,int pps_prefix_len = 4){
        _sps = sps.substr(sps_prefix_len);
        _pps = pps.substr(pps_prefix_len);
        onReady();
    }

    /**
     * 构造h264类型的媒体
     * @param sps sps帧
     * @param pps pps帧
     */
    H264Track(const Frame::Ptr &sps,const Frame::Ptr &pps){
        if(sps->getCodecId() != CodecH264 || pps->getCodecId() != CodecH264 ){
            throw std::invalid_argument("必须输入H264类型的帧");
        }
        _sps = string(sps->data() + sps->prefixSize(),sps->size() - sps->prefixSize());
        _pps = string(pps->data() + pps->prefixSize(),pps->size() - pps->prefixSize());
        onReady();
    }

    /**
     * 返回不带0x00 00 00 01头的sps
     * @return
     */
    const string &getSps() const{
        return _sps;
    }

    /**
     * 返回不带0x00 00 00 01头的pps
     * @return
     */
    const string &getPps() const{
        return _pps;
    }

    CodecId getCodecId() const override {
        return CodecH264;
    }

    int getVideoHeight() const override{
        return _height ;
    }

    int getVideoWidth() const override{
        return _width;
    }

    bool resolution_is_change(std::string& msg) override {
        if(pre_width_ == 0 || pre_height_ == 0) {
            pre_width_ = _width;
            pre_height_ = _height;
            return false;
        }

        bool change = false;
        if(pre_width_ != _width || pre_height_ != _height) {
            std::ostringstream os;
            os << pre_width_ << "x" << pre_height_ 
               << " to "
               << _width << "x" << _height;
            msg = os.str();
            change = true;
        }
        pre_width_ = _width;
        pre_height_ = _height;
        return change;
    }

    bool ready() override {
        return !_sps.empty() && !_pps.empty();
    }

    void inputFrame(const Frame::Ptr &frame) override{
        int type = H264_TYPE(*((uint8_t *)frame->data() + frame->prefixSize()));
        if(type != H264Frame::NAL_SEI) {
            VideoTrack::statistics_frame_rate();
        }
        if(type != H264Frame::NAL_B_P && type != H264Frame::NAL_IDR){
            //非I/B/P帧情况下，split一下，防止多个帧粘合在一起
            splitH264(frame->data(), frame->size(), frame->prefixSize(), [&](const char *ptr, int len, int prefix) {
                H264FrameInternal::Ptr sub_frame = std::make_shared<H264FrameInternal>(frame, (char *)ptr, len, prefix);
                int sub_type = H264_TYPE(*((uint8_t *)sub_frame->data() + sub_frame->prefixSize()));
                if(sub_type == H264Frame::NAL_B_P || sub_type == H264Frame::NAL_IDR) {
                    sub_frame->raw_sei_payload_ = frame->raw_sei_payload_;
                }
                inputFrame_l(sub_frame);
            });
        } else{
            inputFrame_l(frame);
        }
    }
private:
    void onReady(){
        getAVCInfo(_sps, _width, _height, fps_);
    }

    Track::Ptr clone() override {
        return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this);
    }

    void inputFrame_l(const Frame::Ptr &frame){
        int type = H264_TYPE(*((uint8_t *)frame->data() + frame->prefixSize()));
        switch (type){
            case H264Frame::NAL_SPS:{
                _sps = string(frame->data() + frame->prefixSize(),frame->size() - frame->prefixSize());
                break;
            }
            case H264Frame::NAL_PPS:{
                _pps = string(frame->data() + frame->prefixSize(),frame->size() - frame->prefixSize());
                break;
            }
            case H264Frame::NAL_IDR:{
                insertConfigFrame(frame);
                VideoTrack::inputFrame(frame);
                break;
            }
            case H264Frame::NAL_AUD:{
                break;
            }
            default: {
                VideoTrack::inputFrame(frame);
                break;
            }
        }
        _last_frame_is_idr = type == H264Frame::NAL_IDR;
        if(_width == 0 && ready()){
            onReady();
        }
    }

    Sdp::Ptr getSdp() override;
private:
    void insertConfigFrame(const Frame::Ptr &frame){
        if(_last_frame_is_idr){
            return;
        }
        
        if(!_sps.empty() && !_pps.empty()){
            getAVCInfo(_sps, _width, _height, fps_);
        }

        if(!_sps.empty()){
            auto spsFrame = std::make_shared<H264Frame>();
            spsFrame->_prefix_size = 4;
            spsFrame->_buffer.assign("\x0\x0\x0\x1",4);
            spsFrame->_buffer.append(_sps);
            spsFrame->_dts = frame->dts();
            VideoTrack::inputFrame(spsFrame);
        }

        if(!_pps.empty()){
            auto ppsFrame = std::make_shared<H264Frame>();
            ppsFrame->_prefix_size = 4;
            ppsFrame->_buffer.assign("\x0\x0\x0\x1",4);
            ppsFrame->_buffer.append(_pps);
            ppsFrame->_dts = frame->dts();
            VideoTrack::inputFrame(ppsFrame);
        }
    }

private:
    string _sps;
    string _pps;
    int _width = 0;
    int _height = 0;
    int pre_width_ = 0;
    int pre_height_ = 0;
    bool _last_frame_is_idr = false;
};

/**
* h264类型sdp
*/
class H264Sdp : public Sdp {
public:
    /**
     *
     * @param sps 264 sps,不带0x00000001头
     * @param pps 264 pps,不带0x00000001头
     * @param payload_type  rtp payload type 默认96
     * @param bitrate 比特率
     */
    H264Sdp(const string &strSPS,
            const string &strPPS,
            int bitrate = 4000,
            int payload_type = 96) : Sdp(90000,payload_type) {
        _printer << "m=video 0 RTP/AVP " << payload_type << "\r\n";
        if (bitrate) {
            _printer << "b=AS:" << bitrate << "\r\n";
        }
        _printer << "a=rtpmap:" << payload_type << " H264/" << 90000 << "\r\n";
        _printer << "a=fmtp:" << payload_type << " packetization-mode=1; profile-level-id=";
        
        char strTemp[1024] = {0};
        
        uint32_t profile_level_id = 0;
        if (strSPS.length() >= 4) { // sanity check
            profile_level_id = (uint8_t(strSPS[1]) << 16) |
                               (uint8_t(strSPS[2]) << 8)  |
                               (uint8_t(strSPS[3])); // profile_idc|constraint_setN_flag|level_idc
        }
        std::memset(strTemp, 0, sizeof(strTemp));
        std::snprintf(strTemp, sizeof(strTemp), "%06X", profile_level_id);
        _printer << strTemp;
        
        _printer << "; sprop-parameter-sets=";
        
        std::memset(strTemp, 0, sizeof(strTemp));
        av_base64_encode(strTemp, sizeof(strTemp), (uint8_t *) strSPS.data(), strSPS.size());
        _printer << strTemp << ",";
        
        std::memset(strTemp, 0, sizeof(strTemp));
        av_base64_encode(strTemp, sizeof(strTemp), (uint8_t *) strPPS.data(), strPPS.size());
        _printer << strTemp << "\r\n";
        
        _printer << "a=control:trackID=" << (int)TrackVideo << "\r\n";

        //为了适配海康播放器；
        _printer << "a=Media_header:MEDIAINFO=494D4B48010300000400000100000000000000000000000000000000000000000000000000000000;\r\n";
        _printer << "a=appversion:1.0\r\n";
    }

    string getSdp() const override {
        return _printer;
    }

    CodecId getCodecId() const override {
        return CodecH264;
    }
private:
    _StrPrinter _printer;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_H264_H