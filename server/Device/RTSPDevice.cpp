#include "RTSPDevice.h"

#include <memory>

extern "C" {
#include "libavformat/avformat.h"
}

#include "EventReport.h"
#include "Config.h"

RTSPDevice::RTSPDevice(const DeviceInfo& info): IDevice(info) {
}

RTSPDevice::~RTSPDevice() {
    media_close();
}

bool RTSPDevice::media_open() {
    IDevice::media_open();
    
    stream_thread_future_ = std::async([this]() {
        AVDictionary* opts = nullptr;
        auto opts_defer = std::shared_ptr<char>(nullptr, [&opts](char *p){
            av_dict_free(&opts);
        });
        for(auto opt : ConfigInfo.rtsp.ffmpeg_options) {
            av_dict_set(&opts, opt.first.c_str(), opt.second.c_str(), 0);
        }

        AVFormatContext *format_ctx = nullptr;
        auto format_ctx_defer = std::shared_ptr<char>(nullptr, [&format_ctx](char* p) {
            avformat_close_input(&format_ctx);
        });
            
        int n_ret = avformat_open_input(&format_ctx, device_info_.url.c_str(), nullptr, &opts);
        if (n_ret < 0) {
            ErrorID(this) << "avformat_open_input failed! " << ffmpeg_error(n_ret);
            return ;
        }
        
        av_dump_format(format_ctx, 0, format_ctx->url, 0);
    
        n_ret = avformat_find_stream_info(format_ctx, nullptr);
        if (n_ret < 0) {
            ErrorID(this) << "avformat_find_stream_info failed! " << ffmpeg_error(n_ret);
            return ;
        }
        
        int video_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if(video_stream_index < 0) {
            ErrorID(this) << "not find a video stream";
            return ;
        }
    
        AVCodecParameters *codecpar = format_ctx->streams[video_stream_index]->codecpar;
        if(codecpar->codec_id == AV_CODEC_ID_H264) {
            muxer_->addTrack(std::make_shared<H264Track>());
        } else if(codecpar->codec_id == AV_CODEC_ID_HEVC) {
            muxer_->addTrack(std::make_shared<H265Track>());
        } else {
            ErrorID(this) << "codec_id not support, codec_id=" << codecpar->codec_id;
            return ;
        }
        muxer_->addTrackCompleted();

        //ffmpeg rtpdec_h264.c sdp_parse_fmtp_config_h264
        if(codecpar->extradata != nullptr && codecpar->extradata_size > 0) {
            int prefix_size = prefixSize((char *)codecpar->extradata, codecpar->extradata_size);
            if(prefix_size > 0) {
                FrameImp::Ptr sps_pps_frame;
                if(codecpar->codec_id == AV_CODEC_ID_H264) {
                    sps_pps_frame = std::make_shared<H264Frame>();
                } else {
                    sps_pps_frame = std::make_shared<H265Frame>();
                }
                sps_pps_frame->_dts = 0;
                sps_pps_frame->_pts = sps_pps_frame->_dts;
                sps_pps_frame->_buffer.assign((const char*)codecpar->extradata, codecpar->extradata_size);
                sps_pps_frame->_prefix_size = prefix_size;
                input_frame(sps_pps_frame);
            }
        }

        double samplerate = av_q2d(format_ctx->streams[video_stream_index]->time_base) * 1000;

        InfoID(this) << "open success codec_id=" << avcodec_get_name(codecpar->codec_id);
        EventReport::Instance().report(device_info_.device_id, EventReport::open_success);

        stream_info_.codec_ = avcodec_get_name(codecpar->codec_id);
        stream_info_.width_ = codecpar->width;
        stream_info_.height_ = codecpar->height;
        stream_info_.bits_ = codecpar->bit_rate;
        stream_info_.byte_rate_ = speed_.getSpeed();
        stream_info_.frame_rate_ = format_ctx->streams[video_stream_index]->r_frame_rate.num;
        if(!stream_info_.bits_) {
            stream_info_.bits_ = stream_info_.byte_rate_ * 8;
        }

        bool wait_key_frame = true;
        bool b_ret = false;
        unsigned int frame_count = 0;

        while(thread_status_) {
            auto pkt = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket* ptr) {
                av_packet_free(&ptr);
            });
            n_ret = av_read_frame(format_ctx, pkt.get());
            if(n_ret < 0) {
                ErrorID(this) << "av_read_frame failed! msg=" << ffmpeg_error(n_ret);
                EventReport::Instance().report(device_info_.device_id, EventReport::network_offline, ffmpeg_error(n_ret));
                break;
            }
            if(pkt->stream_index != video_stream_index) {
                continue;
            }

            if(wait_key_frame && !(pkt->flags & AV_PKT_FLAG_KEY)) {
                continue;
            }
            wait_key_frame = false;

            total_bytes_ += pkt->size;
            speed_ += pkt->size;
                 
            FrameImp::Ptr frame;
            if(codecpar->codec_id == AV_CODEC_ID_H264) {
               frame = std::make_shared<H264Frame>();
            } else {
               frame = std::make_shared<H265Frame>();
            }
            frame->_dts = pkt->dts * samplerate;
            frame->_pts = frame->_dts;
            frame->_buffer.assign((const char*)pkt->data, pkt->size);
            frame->_prefix_size = prefixSize(reinterpret_cast<const char*>(pkt->data), pkt->size);

            IDevice::keep_alive();
            if(device_info_.ptz.enabled && (frame_count++ >= ConfigInfo.ptz.frame_interval)) {
                frame_count = 0;
                PTZ info;
                if(get_position(info)) {
                    frame->sei_enabled = true;
                    frame->sei_payload.data.ptz_current_pos[0] = info.pan;
                    frame->sei_payload.data.ptz_current_pos[1] = info.tilt;
                    frame->sei_payload.data.ptz_current_pos[2] = info.zoom;
                }
            }
            b_ret = input_frame(frame);
            if(!b_ret) {
                WarnID(this) << "input frame invalid!";
                break;
            }
        }
        
        InfoID(this) << "thread exit";
    });
    
    return true;
}

bool RTSPDevice::media_close() {
    thread_status_ = false;
    if(stream_thread_future_.valid()) {
        stream_thread_future_.get();
    }
}
