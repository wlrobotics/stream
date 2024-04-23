#include "Decoder.h"
#include "PSDecoder.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Extension/AAC.h"
#include "Extension/G711.h"

#include "mpeg-ts-proto.h"

namespace mediakit {

DecoderImp::Ptr DecoderImp::createDecoder(MediaSinkInterface *sink){
    auto decoder = std::make_shared<PSDecoder>();
    if(!decoder){
        return nullptr;
    }
    return DecoderImp::Ptr(new DecoderImp(decoder, sink));
}

int DecoderImp::input(const uint8_t *data, int bytes){
    return _decoder->input(data, bytes);
}

DecoderImp::DecoderImp(const Decoder::Ptr &decoder, MediaSinkInterface *sink){
    _decoder = decoder;
    _sink = sink;
    _decoder->setOnDecode([this](int stream, int codecid, int flags, int64_t pts, int64_t dts, const void *data, int bytes) {
        onDecode(stream, codecid, flags, pts, dts, data, bytes);
    });
    _decoder->setOnStream([this](int stream, int codecid, const void *extra, int bytes, int finish) {
        onStream(stream, codecid, extra, bytes, finish);
    });
}

#define SWITCH_CASE(codec_id) case codec_id : return #codec_id

static const char *getCodecName(int codec_id) {
    switch (codec_id) {
        SWITCH_CASE(PSI_STREAM_MPEG1);
        SWITCH_CASE(PSI_STREAM_MPEG2);
        SWITCH_CASE(PSI_STREAM_AUDIO_MPEG1);
        SWITCH_CASE(PSI_STREAM_MP3);
        SWITCH_CASE(PSI_STREAM_AAC);
        SWITCH_CASE(PSI_STREAM_MPEG4);
        SWITCH_CASE(PSI_STREAM_MPEG4_AAC_LATM);
        SWITCH_CASE(PSI_STREAM_H264);
        SWITCH_CASE(PSI_STREAM_MPEG4_AAC);
        SWITCH_CASE(PSI_STREAM_H265);
        SWITCH_CASE(PSI_STREAM_AUDIO_AC3);
        SWITCH_CASE(PSI_STREAM_AUDIO_EAC3);
        SWITCH_CASE(PSI_STREAM_AUDIO_DTS);
        SWITCH_CASE(PSI_STREAM_VIDEO_DIRAC);
        SWITCH_CASE(PSI_STREAM_VIDEO_VC1);
        SWITCH_CASE(PSI_STREAM_VIDEO_SVAC);
        SWITCH_CASE(PSI_STREAM_AUDIO_SVAC);
        SWITCH_CASE(PSI_STREAM_AUDIO_G711A);
        SWITCH_CASE(PSI_STREAM_AUDIO_G711U);
        SWITCH_CASE(PSI_STREAM_AUDIO_G722);
        SWITCH_CASE(PSI_STREAM_AUDIO_G723);
        SWITCH_CASE(PSI_STREAM_AUDIO_G729);
        default : return "unknown codec";
    }
}

void DecoderImp::onStream(int stream, int codecid, const void *extra, int bytes, int finish){
    switch (codecid) {
        case PSI_STREAM_H264: {
            InfoL << "got video track: H264";
            auto track = std::make_shared<H264Track>();
            onTrack(track);
            break;
        }

        case PSI_STREAM_H265: {
            InfoL << "got video track: H265";
            auto track = std::make_shared<H265Track>();
            onTrack(track);
            break;
        }
        default:
            break;
    }

    if (finish) {
        _sink->addTrackCompleted();
        InfoL << "add track finished";
    }
}

void DecoderImp::onDecode(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,int bytes) {
    pts /= 90;
    dts /= 90;

    switch (codecid) {
        case PSI_STREAM_H264: {
            auto frame = std::make_shared<H264FrameNoCacheAble>((char *) data, bytes, dts, pts,0);
            _merger.inputFrame(frame,[this](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer) {
                onFrame(std::make_shared<FrameWrapper<H264FrameNoCacheAble>>(buffer, dts, pts, prefixSize(buffer->data(), buffer->size()), 0));
            });
            break;
        }

        case PSI_STREAM_H265: {
            auto frame = std::make_shared<H265FrameNoCacheAble>((char *) data, bytes, dts, pts, 0);
            _merger.inputFrame(frame,[this](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer) {
                onFrame(std::make_shared<FrameWrapper<H265FrameNoCacheAble>>(buffer, dts, pts, prefixSize(buffer->data(), buffer->size()), 0));
            });
            break;
        }
        default:
            break;
    }
}


void DecoderImp::onTrack(const Track::Ptr &track) {
    _sink->addTrack(track);
}

void DecoderImp::onFrame(const Frame::Ptr &frame) {
    _sink->inputFrame(frame);
}

void FrameMerger::inputFrame(const Frame::Ptr &frame,
                                const std::function<void(uint32_t dts,
                                                         uint32_t pts,
                                                         const Buffer::Ptr &buffer)> &cb){
    if (!_frameCached.empty() && _frameCached.back()->dts() != frame->dts()) {
        Frame::Ptr back = _frameCached.back();
        Buffer::Ptr merged_frame = back;
        if(_frameCached.size() != 1){
            BufferLikeString merged;
            merged.reserve(back->size() + 1024);
            _frameCached.for_each([&](const Frame::Ptr &frame){
                merged.append(frame->data(),frame->size());
            });
            merged_frame = std::make_shared<BufferOffset<BufferLikeString> >(std::move(merged));
        }
        cb(back->dts(),back->pts(),merged_frame);
        _frameCached.clear();
    }
    _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
}


}//namespace mediakit
