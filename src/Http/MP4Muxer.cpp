#include "MP4Muxer.h"

#include "Extension/H264.h"

namespace mediakit{

/////////////////////////////////////////// MP4MuxerInterface /////////////////////////////////////////////

void MP4MuxerInterface::saveSegment(){
    mp4_writer_save_segment(_mov_writter.get());
}

void MP4MuxerInterface::initSegment(){
    mp4_writer_init_segment(_mov_writter.get());
}

bool MP4MuxerInterface::haveVideo() const{
    return _have_video;
}

void MP4MuxerInterface::resetTracks() {
    _have_video = false;
    _mov_writter = nullptr;
    _frameCached.clear();
    _codec_to_trackid.clear();
}

void MP4MuxerInterface::inputFrame(const Frame::Ptr &frame) {
    auto it = _codec_to_trackid.find(frame->getCodecId());
    if(it == _codec_to_trackid.end()){
        return;
    }

    //mp4文件时间戳需要从0开始
    auto &track_info = it->second;
    int64_t dts_out, pts_out;

    switch (frame->getCodecId()) {
        case CodecH264: {
            int type = H264_TYPE(*((uint8_t *)frame->data() + frame->prefixSize()));
            if(type == H264Frame::NAL_SEI){
                break;
            }
        }
        case CodecH265: {
            int type = H265_TYPE(*((uint8_t *)frame->data() + frame->prefixSize()));
            if(type == H265Frame::NAL_SEI_PREFIX) {
                break;
            }
            //这里的代码逻辑是让SPS、PPS、IDR这些时间戳相同的帧打包到一起当做一个帧处理，
            if (!_frameCached.empty() && _frameCached.back()->dts() != frame->dts()) {
                Frame::Ptr back = _frameCached.back();
                //求相对时间戳
                track_info.stamp.revise(back->dts(), back->pts(), dts_out, pts_out);

                if (_frameCached.size() != 1) {
                    //缓存中有多帧，需要按照mp4格式合并一起
                    BufferLikeString merged;
                    merged.reserve(back->size() + 1024);
                    _frameCached.for_each([&](const Frame::Ptr &frame) {
                        uint32_t nalu_size = frame->size() - frame->prefixSize();
                        nalu_size = htonl(nalu_size);
                        merged.append((char *) &nalu_size, 4);
                        merged.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
                    });
                    mp4_writer_write(_mov_writter.get(),
                                       track_info.track_id,
                                       merged.data(),
                                       merged.size(),
                                       pts_out,
                                       dts_out,
                                       back->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0);
                } else {
                    //缓存中只有一帧视频
                    mp4_writer_write_l(_mov_writter.get(),
                                       track_info.track_id,
                                       back->data() + back->prefixSize(),
                                       back->size() - back->prefixSize(),
                                       pts_out,
                                       dts_out,
                                       back->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0,
                                       1/*需要生成头4个字节的MP4格式start code*/);
                }
                _frameCached.clear();
            }
            //缓存帧，时间戳相同的帧合并一起写入mp4
            _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
        }
            break;
        default: {
            track_info.stamp.revise(frame->dts(), frame->pts(), dts_out, pts_out);
            mp4_writer_write(_mov_writter.get(),
                             track_info.track_id,
                             frame->data() + frame->prefixSize(),
                             frame->size() - frame->prefixSize(),
                             pts_out,
                             dts_out,
                             frame->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0);
        }
            break;
    }
}

static uint8_t getObject(CodecId codecId){
    switch (codecId){
        case CodecG711A : return MOV_OBJECT_G711a;
        case CodecG711U : return MOV_OBJECT_G711u;
        case CodecAAC : return MOV_OBJECT_AAC;
        case CodecH264 : return MOV_OBJECT_H264;
        case CodecH265 : return MOV_OBJECT_HEVC;
        default : return 0;
    }
}

void MP4MuxerInterface::addTrack(const Track::Ptr &track) {
    if (!_mov_writter) {
        _mov_writter = createWriter();
    }
    auto mp4_object = getObject(track->getCodecId());
    if (!mp4_object) {
        WarnL << "MP4录制不支持该编码格式:" << track->getCodecName();
        return;
    }

    if (!track->ready()) {
        WarnL << "Track[" << track->getCodecName() << "]未就绪";
        return;
    }

    switch (track->getCodecId()) {
        case CodecH264: {
            auto h264_track = dynamic_pointer_cast<H264Track>(track);
            if (!h264_track) {
                WarnL << "不是H264 Track";
                return;
            }

            struct mpeg4_avc_t avc = {0};
            string sps_pps = string("\x00\x00\x00\x01", 4) + h264_track->getSps() +
                             string("\x00\x00\x00\x01", 4) + h264_track->getPps();
            h264_annexbtomp4(&avc, sps_pps.data(), sps_pps.size(), NULL, 0, NULL, NULL);



            uint8_t extra_data[1024];
            int extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, extra_data, sizeof(extra_data));
            if (extra_data_size == -1) {
                WarnL << "生成H264 extra_data 失败";
                return;
            }

            auto track_id = mp4_writer_add_video(_mov_writter.get(),
                                                 mp4_object,
                                                 h264_track->getVideoWidth(),
                                                 h264_track->getVideoHeight(),
                                                 extra_data,
                                                 extra_data_size);

            if(track_id < 0){
                WarnL << "添加H264 Track失败:" << track_id;
                return;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
            _have_video = true;

            char temp_buffer[32] = {0};
            mpeg4_avc_codecs(&avc, temp_buffer, 32);
            mse_mime_type_.append("video/mp4; codecs=\"").append(temp_buffer).append("\"");
        }
            break;
        case CodecH265: {
            auto h265_track = dynamic_pointer_cast<H265Track>(track);
            if (!h265_track) {
                WarnL << "不是H265 Track";
                return;
            }

            struct mpeg4_hevc_t hevc = {0};
            string vps_sps_pps = string("\x00\x00\x00\x01", 4) + h265_track->getVps() +
                                 string("\x00\x00\x00\x01", 4) + h265_track->getSps() +
                                 string("\x00\x00\x00\x01", 4) + h265_track->getPps();
            h265_annexbtomp4(&hevc, vps_sps_pps.data(), vps_sps_pps.size(), NULL, 0, NULL, NULL);

            uint8_t extra_data[1024];
            int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, extra_data, sizeof(extra_data));
            if (extra_data_size == -1) {
                WarnL << "生成H265 extra_data 失败";
                return;
            }

            auto track_id = mp4_writer_add_video(_mov_writter.get(),
                                                 mp4_object,
                                                 h265_track->getVideoWidth(),
                                                 h265_track->getVideoHeight(),
                                                 extra_data,
                                                 extra_data_size);
            if(track_id < 0){
                WarnL << "添加H265 Track失败:" << track_id;
                return;
            }
            _codec_to_trackid[track->getCodecId()].track_id = track_id;
            _have_video = true;

            char temp_buffer[32] = {0};
            mpeg4_hevc_codecs(&hevc, temp_buffer, 32);
            mse_mime_type_.append("video/mp4; codecs=\"").append(temp_buffer).append("\"");
        }
            break;

        default: WarnL << "MP4录制不支持该编码格式:" << track->getCodecName(); break;
    }
}

/////////////////////////////////////////// MP4MuxerMemory /////////////////////////////////////////////

MP4MuxerMemory::MP4MuxerMemory() {
    _memory_file = std::make_shared<MP4FileMemory>();
}

MP4FileIO::Writer MP4MuxerMemory::createWriter() {
    return _memory_file->createWriter(MOV_FLAG_SEGMENT, true);
}

const string &MP4MuxerMemory::getInitSegment(){
    if (_init_segment.empty()) {
        initSegment();
        saveSegment();
        _init_segment = _memory_file->getAndClearMemory();
    }
    return _init_segment;
}

void MP4MuxerMemory::resetTracks(){
    MP4MuxerInterface::resetTracks();
    _memory_file = std::make_shared<MP4FileMemory>();
    _init_segment.clear();
}

void MP4MuxerMemory::inputFrame(const Frame::Ptr &frame){
    if (_init_segment.empty()) {
        return;
    }

    MP4MuxerInterface::inputFrame(frame);
    saveSegment();
    onSegmentData(_memory_file->getAndClearMemory(), frame->dts(), _key_frame);
    _key_frame = frame->keyFrame();
}


}//namespace mediakit
