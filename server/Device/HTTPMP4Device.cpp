#include "HTTPMP4Device.h"

extern "C" {
#include "libavutil/time.h"
#include "libavformat/avformat.h"
}

#include "EventReport.h"
#include "RPC/FlowRPCClient.h"

HTTPMP4Device::HTTPMP4Device(const DeviceInfo& info) : IDevice(info) {
}

HTTPMP4Device::~HTTPMP4Device()  {
    media_close();
}

bool HTTPMP4Device::media_open() {
    IDevice::media_open();
    
    stream_thread_future_ = std::async([this]() {
        AVDictionary* opts = nullptr;
        auto opts_defer = std::shared_ptr<char>(nullptr, [&opts](char *p){
            av_dict_free(&opts);
        });
        av_dict_set(&opts, "rw_timeout", "5000000", 0);

        uint32_t play_index = 0;
        uint32_t url_list_size = device_info_.url_list.size();
        uint32_t time_stamp = 0u;
        do{
            if(!thread_status_){
                break;
            }
            if(url_list_size){
                device_info_.url = device_info_.url_list[play_index++%url_list_size];
            }
            InfoID(this) << "HTTPMP4Device open " << device_info_.url;
            AVFormatContext* format_ctx = avformat_alloc_context();
            auto format_ctx_defer = std::shared_ptr<char>(nullptr, [&format_ctx](char* p) {
                avformat_close_input(&format_ctx);
            });
            format_ctx->interrupt_callback.callback = [](void* ctx) {
                HTTPMP4Device* self = reinterpret_cast<HTTPMP4Device*>(ctx);
                return self->alive() ? 0 : 1;
            };
            format_ctx->interrupt_callback.opaque = this;
            int n_ret = avformat_open_input(&format_ctx, device_info_.url.c_str(), nullptr, &opts);
            if (n_ret < 0) {
                ErrorID(this) << "avformat_open_input failed! " << ffmpeg_error(n_ret);
                break ;
            }

            av_dump_format(format_ctx, 0, format_ctx->url, 0);

            n_ret = avformat_find_stream_info(format_ctx, nullptr);
            if (n_ret < 0) {
                ErrorID(this) << "avformat_find_stream_info failed! " << ffmpeg_error(n_ret);
                break ;
            }

            int video_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            if(video_stream_index < 0) {
                ErrorID(this) << "not find a video stream";
                break ;
            }

            const AVBitStreamFilter *filter = nullptr;
            enum AVCodecID codec_id = format_ctx->streams[video_stream_index]->codecpar->codec_id;
            if(codec_id == AV_CODEC_ID_H264) {
                filter = av_bsf_get_by_name("h264_mp4toannexb");
            } else if(codec_id == AV_CODEC_ID_HEVC) {
                filter = av_bsf_get_by_name("hevc_mp4toannexb");
            } else {
                ErrorID(this) << "codec_id not support, codec_id=" << codec_id;
                break ;
            }

            AVBSFContext *av_bsf_ctx = nullptr;
            auto av_bsf_ctx_defer = std::shared_ptr<char>(nullptr, [&av_bsf_ctx](char* p) {
                av_bsf_free(&av_bsf_ctx);
            });
            n_ret = av_bsf_alloc(filter, &av_bsf_ctx);
            if (n_ret < 0) {
                ErrorID(this) << "av_bsf_alloc failed!";
                break ;
            }

            n_ret = avcodec_parameters_copy(av_bsf_ctx->par_in, format_ctx->streams[video_stream_index]->codecpar);
            if (n_ret < 0) {
                ErrorID(this) << "avcodec_parameters_copy failed!";
                break ;
            }

            n_ret = av_bsf_init(av_bsf_ctx);
            if (n_ret < 0) {
                ErrorID(this) << "av_bsf_init failed!";
                break ;
            }

            int frame_rate = av_q2d(format_ctx->streams[video_stream_index]->avg_frame_rate);
            if(frame_rate < 1 || frame_rate > 125) {
                WarnID(this) << "frame_rate error " << frame_rate << " to 25fps!";
                frame_rate = 25;
            }

            uint32_t frame_interval = 1000.0 / frame_rate ;
            if(std::isnan(frame_interval)|| frame_interval < 8) {
                frame_interval = 40;
            }

            int64_t duration_ = (1000000.0 / (frame_rate * 1.0)) - 500;
            if(duration_ < 4000 || duration_ > 1000000) {
                WarnID(this) << "duration_ too large duration_=" << duration_;
            }

            if(codec_id == AV_CODEC_ID_H264) {
                muxer_->addTrack(std::make_shared<H264Track>());     
            } else {
                muxer_->addTrack(std::make_shared<H265Track>());
            }
            muxer_->addTrackCompleted();

            InfoID(this) << "open success"
                        << ",frame_rate=" << frame_rate 
                        << ",duration_=" << duration_ 
                        << ",frame_interval=" << frame_interval;
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

            bool wait_key_frame = true;

            bool b_ret = false;

            while(thread_status_) {
                auto pkt = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket* ptr) {
                    av_packet_free(&ptr);
                });
                n_ret = av_read_frame(format_ctx, pkt.get());
                if(n_ret == AVERROR_EOF) {
                    if(play_index != device_info_.url_list.size()) {
                        break;
                    }

                    if(--device_info_.mp4_loop_count == 0) {
                        FlowRPCClient::Instance().delete_stream(device_info_.device_id);
                        break;
                    }
                    
                    n_ret = av_seek_frame(format_ctx, video_stream_index, 0, AVSEEK_FLAG_BACKWARD);
                    if(n_ret < 0) {
                        ErrorID(this) << "av_seek_frame failed! " << ffmpeg_error(n_ret);
                        break;
                    }
                    continue;
                } else if(n_ret < 0) {
                    ErrorID(this) << "av_read_frame failed! " << ffmpeg_error(n_ret);
                    break;
                }
                if(pkt->stream_index != video_stream_index) {
                    continue;
                }

                if(wait_key_frame && !(pkt->flags & AV_PKT_FLAG_KEY)) {
                    continue;
                }
                wait_key_frame = false;

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
                
                FrameImp::Ptr frame;
                if(codec_id == AV_CODEC_ID_H264) {
                frame = std::make_shared<H264Frame>();
                } else {
                frame = std::make_shared<H265Frame>();
                }
                frame->_dts = time_stamp;
                frame->_pts = frame->_dts;
                frame->_buffer.assign((const char*)pkt->data, pkt->size);
                frame->_prefix_size = prefixSize(reinterpret_cast<const char*>(pkt->data), pkt->size);

                time_stamp += frame_interval;

                b_ret = input_frame(frame);
                if(!b_ret) {
                    WarnID(this) << "input frame invalid!";
                    break;
                }
                IDevice::keep_alive();
                av_usleep(duration_);
            }
        }while(url_list_size > 1);
        InfoID(this) << "thread exit";
    });

    return true;
}

bool HTTPMP4Device::media_close() {
    thread_status_ = false;
    if(stream_thread_future_.valid()) {
        stream_thread_future_.get();
    }
}
