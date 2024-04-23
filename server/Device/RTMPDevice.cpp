#include "RTMPDevice.h"

extern "C" {
#include "libavformat/avformat.h"
}

#include "EventReport.h"

RTMPDevice::RTMPDevice(const DeviceInfo& info): IDevice(info) {
}


RTMPDevice::~RTMPDevice() {
    media_close();
}

bool RTMPDevice::media_open() {
    IDevice::media_open();
    
    stream_thread_future_ = std::async([this]() {
        AVDictionary* opts = nullptr;
        auto opts_defer = std::shared_ptr<char>(nullptr, [&opts](char *p){
            av_dict_free(&opts);
        });
        av_dict_set(&opts, "analyzeduration", "1", 0);
        av_dict_set(&opts, "rw_timeout", "5000000", 0);

        AVFormatContext* format_ctx = avformat_alloc_context();
        auto format_ctx_defer = std::shared_ptr<char>(nullptr, [&format_ctx](char* p) {
            avformat_close_input(&format_ctx);
        });
        format_ctx->interrupt_callback.callback = [](void* ctx) {
            RTMPDevice* self = reinterpret_cast<RTMPDevice*>(ctx);
            return self->alive() ? 0 : 1;
        };
        format_ctx->interrupt_callback.opaque = this;
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

        enum AVCodecID codec_id = format_ctx->streams[video_stream_index]->codecpar->codec_id;
        if(codec_id != AV_CODEC_ID_H264) {
            ErrorID(this) << "codec_id not support, codec_id=" << codec_id;
            return ;
        }
               
        AVBSFContext *av_bsf_ctx = nullptr;
        auto av_bsf_ctx_defer = std::shared_ptr<char>(nullptr, [&av_bsf_ctx](char* p) {
            av_bsf_free(&av_bsf_ctx);
        });
        const AVBitStreamFilter *filter = av_bsf_get_by_name("h264_mp4toannexb");
        n_ret = av_bsf_alloc(filter, &av_bsf_ctx);
        if (n_ret < 0) {
            ErrorID(this) << "av_bsf_alloc failed!";
            return ;
        }
        n_ret = avcodec_parameters_copy(av_bsf_ctx->par_in, format_ctx->streams[video_stream_index]->codecpar);
        if (n_ret < 0) {
            ErrorID(this) << "avcodec_parameters_copy failed!";
            return ;
        }
        n_ret = av_bsf_init(av_bsf_ctx);
        if (n_ret < 0) {
            ErrorID(this) << "av_bsf_init failed!";
            return ;
        }

        double samplerate = av_q2d(format_ctx->streams[video_stream_index]->time_base) * 1000;

        muxer_->addTrack(std::make_shared<H264Track>());
        muxer_->addTrackCompleted();

        InfoID(this) << "open success, codec_id=H264";
        EventReport::Instance().report(device_info_.device_id, EventReport::open_success);

        stream_info_.codec_ = avcodec_get_name(codec_id);
        stream_info_.width_ = format_ctx->streams[video_stream_index]->codecpar->width;
        stream_info_.height_ = format_ctx->streams[video_stream_index]->codecpar->height;
        stream_info_.bits_ = format_ctx->streams[video_stream_index]->codecpar->bit_rate;
        stream_info_.byte_rate_ = speed_.getSpeed();
        stream_info_.frame_rate_ = format_ctx->streams[video_stream_index]->r_frame_rate.num;
        if(!stream_info_.bits_) {
            stream_info_.bits_ = stream_info_.byte_rate_ * 8;
        }

        while(thread_status_) {      
            auto pkt = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket* ptr) {
                av_packet_free(&ptr);
            });
            n_ret = av_read_frame(format_ctx, pkt.get());
            if(n_ret < 0) {
                ErrorID(this) << "av_read_frame failed! error msg=" << ffmpeg_error(n_ret);
                break;
            }
            if(pkt->stream_index != video_stream_index) {
                continue;
            }
 
            n_ret = av_bsf_send_packet(av_bsf_ctx, pkt.get());
            if(n_ret != 0) {
                ErrorID(this) << "av_bsf_send_packet failed! " << ffmpeg_error(n_ret);
                break;
            }
            n_ret = av_bsf_receive_packet(av_bsf_ctx, pkt.get());
            if(n_ret != 0) {
                ErrorID(this) << "av_bsf_receive_packet failed! " << ffmpeg_error(n_ret);
                break;
            }

            total_bytes_ += pkt->size;
            speed_ += pkt->size;
            
            H264Frame::Ptr frame = std::make_shared<H264Frame>();
            frame->_dts = pkt->dts * samplerate;
            frame->_pts = frame->_dts;
            frame->_buffer.assign((const char*)pkt->data, pkt->size);
            frame->_prefix_size = prefixSize(reinterpret_cast<const char*>(pkt->data), pkt->size);

            input_frame(frame);
            IDevice::keep_alive();
        }
        
        InfoID(this) << "thread exit";
    });

    return true;
}

bool RTMPDevice::media_close() {
    thread_status_ = false;
    if(stream_thread_future_.valid()) {
        stream_thread_future_.get();
    }
}
