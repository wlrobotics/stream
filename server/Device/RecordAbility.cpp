#include "Device/RecordAbility.h"

#include <iomanip>

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "Config.h"
#include "Util/logger.h"
#include "Extension/H264.h"
#include "Extension/H265.h"

RecordAbility::RecordAbility(std::string device_id) : device_id_(device_id) {
}

RecordAbility::~RecordAbility() {
}

void RecordAbility::record_frame(const mediakit::Frame::Ptr &frame) {
    std::lock_guard<std::mutex> lck(frame_list_mtx_);
    
    bool reach_memory_limit = (memory_used_ >= ConfigInfo.record.memory_quota);
    
    bool reach_time_limit = true;
    if(frame_list_.empty()) {
        reach_time_limit = false;
    } else {
        auto first_frame = frame_list_.front();
        auto end_frame = frame_list_.back();
        if(end_frame->get_ntp_stamp() - first_frame->get_ntp_stamp() <  time_quota_) {
            reach_time_limit = false;
        }
    }
    
    if(reach_time_limit || reach_memory_limit) {
        auto drop_frame = frame_list_.front();
        memory_used_ -= drop_frame->size();
        frame_list_.pop_front();
    }
    frame_list_.emplace_back(frame);
    memory_used_ += frame->size();
}

bool RecordAbility::download_record(std::uint64_t start_time,
                             std::uint64_t end_time,
                             std::list<mediakit::Frame::Ptr>& result_list) {
    std::lock_guard<std::mutex> lck(frame_list_mtx_);
    
    for(auto &frame : frame_list_) {
        if(frame->get_ntp_stamp() >= start_time) {
            if(frame->get_ntp_stamp() <= end_time) {
                result_list.emplace_back(frame);
            } else {
                break;
            }
        }
    }

    if(result_list.size() < 50) {
        return false;
    }

    return true;
}


void RecordAbility::set_time_quota(const std::uint32_t time_quota) {
    std::lock_guard<std::mutex> lck(frame_list_mtx_);
    if(time_quota < time_quota_) {
        if(frame_list_.empty()) {
            time_quota_ = time_quota;
            return ;
        }
        std::uint64_t over_time = frame_list_.front()->get_ntp_stamp() + time_quota_ - time_quota;
        for(auto it = frame_list_.begin(); it != frame_list_.end();) {
            if((*it)->get_ntp_stamp() > over_time) {
                break;
            }
            memory_used_ -= (*it)->size();
            it = frame_list_.erase(it);
        }
    }
    time_quota_ = time_quota;
}


std::uint64_t RecordAbility::get_record_len() {
    std::lock_guard<std::mutex> lck(frame_list_mtx_);
    return memory_used_;
}

std::uint64_t RecordAbility::get_record_time() {
    std::lock_guard<std::mutex> lck(frame_list_mtx_);
    if(frame_list_.empty()) {
        return 0u;
    }
    return frame_list_.back()->get_ntp_stamp() - frame_list_.front()->get_ntp_stamp();
}


bool RecordAbility::capture_picture(const std::uint64_t time_stamp, PictureInfo& pic_info) {
    std::lock_guard<std::mutex> lck(capture_picture_mtx_);

    std::uint64_t tp = time_stamp;
    if(tp == 0) {
        tp = std::time(nullptr) * 1000;
    }

    mediakit::Frame::Ptr target_frame = nullptr;
    {
        std::lock_guard<std::mutex> lck(frame_list_mtx_);
        if(frame_list_.empty()) {
            ErrorL << device_id_ << ",frame_list_.empty";
            return false;
        }

        std::uint64_t record_start_time = frame_list_.front()->get_ntp_stamp();
        std::uint64_t record_end_time = frame_list_.back()->get_ntp_stamp();
        if(tp < record_start_time - 2000 || tp > record_end_time + 2000) {
            ErrorL << device_id_ << ",time_stamp not in record_time, " << tp;
            return false;
        }

        mediakit::Frame::Ptr pre_frame = nullptr;
        mediakit::Frame::Ptr next_frame = nullptr;
        auto it = frame_list_.begin();
        for(;it != frame_list_.end(); it++) {
            mediakit::Frame::Ptr temp_frame = *it;
            bool is_config_frame = temp_frame->configFrame();
            if(!is_config_frame) {
                if(temp_frame->getCodecId() == mediakit::CodecH264) {
                    int type = H264_TYPE(*((uint8_t *)temp_frame->data() + temp_frame->prefixSize()));
                    if(type == H264Frame::NAL_SEI){
                        splitH264(temp_frame->data(), temp_frame->size(), temp_frame->prefixSize(), [&](const char *ptr, int len, int prefix) {
                            int type = H264_TYPE(ptr[prefix]);
                            if(type == H264Frame::NAL_SPS || type == H264Frame::NAL_PPS) {
                                is_config_frame = true;
                            }
                        });
                    }
                } else {
                    int type = H265_TYPE(*((uint8_t *)temp_frame->data() + temp_frame->prefixSize()));
                    if(type == H265Frame::NAL_SEI_PREFIX){
                        splitH264(temp_frame->data(), temp_frame->size(), temp_frame->prefixSize(), [&](const char *ptr, int len, int prefix){
                            int type = H265_TYPE(ptr[prefix]);
                            if(type == H265Frame::NAL_VPS || type == H265Frame::NAL_PPS || type == H265Frame::NAL_PPS) {
                                is_config_frame = true;
                            }
                        });
                    }
                }
            }

            if(is_config_frame) {
                if(tp <= temp_frame->get_ntp_stamp()) {
                    next_frame = temp_frame;
                    break;
                }
            }
        }

        if(it != frame_list_.begin()) {
            it--;
        }

        for(; it != frame_list_.begin(); it--) {
            mediakit::Frame::Ptr temp_frame = *it;
            bool is_config_frame = temp_frame->configFrame();
            if(!is_config_frame) {
                if(temp_frame->getCodecId() == mediakit::CodecH264) {
                    int type = H264_TYPE(*((uint8_t *)temp_frame->data() + temp_frame->prefixSize()));
                    if(type == H264Frame::NAL_SEI){
                        splitH264(temp_frame->data(), temp_frame->size(), temp_frame->prefixSize(), [&](const char *ptr, int len, int prefix) {
                            int type = H264_TYPE(ptr[prefix]);
                            if(type == H264Frame::NAL_SPS || type == H264Frame::NAL_PPS) {
                                is_config_frame = true;
                            }
                        });
                    }
                } else {
                    int type = H265_TYPE(*((uint8_t *)temp_frame->data() + temp_frame->prefixSize()));
                    if(type == H265Frame::NAL_SEI_PREFIX){
                        splitH264(temp_frame->data(), temp_frame->size(), temp_frame->prefixSize(), [&](const char *ptr, int len, int prefix){
                            int type = H265_TYPE(ptr[prefix]);
                            if(type == H265Frame::NAL_VPS || type == H265Frame::NAL_PPS || type == H265Frame::NAL_PPS) {
                                is_config_frame = true;
                            }
                        });
                    }
                }
            }

            if(is_config_frame) {
                pre_frame = temp_frame;
                break;
            }
        }

        if(pre_frame == nullptr && next_frame == nullptr) {
            ErrorL << device_id_ << ",pre_frame == nullptr && next_frame == nullptr";
            return false;
        } else if(pre_frame == nullptr) {
            InfoL << device_id_ << ",first,tp:" << tp << ",next:" << next_frame->get_ntp_stamp();
            target_frame = next_frame;
        } else if (next_frame == nullptr) {
            InfoL << device_id_ << ",last frame pre:" << pre_frame->get_ntp_stamp() << ",tp:" << tp;
            target_frame = pre_frame;
        } else {
            InfoL << device_id_ << ",pre:" << pre_frame->get_ntp_stamp() << ",tp:" << tp << ",next:" << next_frame->get_ntp_stamp();
            if(tp - pre_frame->get_ntp_stamp() > next_frame->get_ntp_stamp() - tp) {
                target_frame = next_frame;
            } else {
                target_frame = pre_frame;
            }
        }
    }

    //TODO: 偶现pkt_video->size很小的情况，暂时不知道原因，先不管；

    auto pkt_video = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket* ptr) {
        av_packet_free(&ptr);
    });

    pkt_video->data = (uint8_t*)target_frame->data();
    pkt_video->size = target_frame->size();

    auto frame = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame* frame) {
        av_frame_free(&frame);
    });

    AVCodec *codec_video = nullptr;
    if(target_frame->getCodecId() == mediakit::CodecH264) {
        codec_video = avcodec_find_decoder(AV_CODEC_ID_H264);
    } else if(target_frame->getCodecId() == mediakit::CodecH265) {
        codec_video = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    }

    if (codec_video == nullptr) {
        ErrorL << device_id_;
        return false;
    }

    auto avctx_video = std::shared_ptr<AVCodecContext>(avcodec_alloc_context3(codec_video), [](AVCodecContext* ctx) {
        avcodec_free_context(&ctx);
    });

    int ret = avcodec_open2(avctx_video.get(), codec_video, nullptr);
    if (ret < 0) {
        ErrorL << device_id_ << "," << ffmpeg_error(ret);
        return false;
    }

    /* 
        只调用一次avcodec_send_packet和avcodec_receive_frame，会遇到avcodec_receive_frame返回AVERROR(EAGAIN)：Resource temporarily unavailable
        解决方法是：循环调用avcodec_send_packet和avcodec_receive_frame，直到avcodec_receive_frame返回success；
        防止死循环，设置重试次数retry_count；
    */
    int retry_count = 0;
    while (true) {
        ret = avcodec_send_packet(avctx_video.get(), pkt_video.get());
        if (ret < 0) {
            ErrorL << device_id_ << "," << ffmpeg_error(ret);
            return false;
        }

        if(retry_count++ > 4) {
            return false;
        }
        ret = avcodec_receive_frame(avctx_video.get(), frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            continue;
        } else if (ret < 0) {
            ErrorL << device_id_ << "," << ffmpeg_error(ret);
            return false;
        } else {
            break;
        }
    }

    auto pkt_pic = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket* ptr) {
        av_packet_free(&ptr);
    });

    AVCodec* codec_pic = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if(codec_pic == nullptr) {
        ErrorL << device_id_;
        return false;
    }

    auto avctx_pic = std::shared_ptr<AVCodecContext>(avcodec_alloc_context3(codec_pic), [](AVCodecContext* ctx) {
        avcodec_free_context(&ctx);
    });

    avctx_pic->codec_id   = AV_CODEC_ID_MJPEG;
    avctx_pic->codec_type = AVMEDIA_TYPE_VIDEO;
    avctx_pic->pix_fmt    = AV_PIX_FMT_YUVJ420P;
    avctx_pic->width      = frame->width;
    avctx_pic->height     = frame->height;
    avctx_pic->time_base.num = 1;
    avctx_pic->time_base.den = 25;

    ret = avcodec_open2(avctx_pic.get(), codec_pic, nullptr);
    if(ret < 0) {
        ErrorL << device_id_ << "," << ffmpeg_error(ret);
        return false;
    }

    ret = avcodec_send_frame(avctx_pic.get(), frame.get());
    if (ret < 0) {
        ErrorL << device_id_ << "," << ffmpeg_error(ret);
        return false;
    }

    ret = avcodec_receive_packet(avctx_pic.get(), pkt_pic.get());
    if (ret < 0) {
        ErrorL << device_id_ << "," << ffmpeg_error(ret);
        return false;
    }

    pic_info.width = frame->width;
    pic_info.height = frame->height;
    pic_info.picture.assign((const char*)pkt_pic->data, pkt_pic->size);
    pic_info.ntp_time_stamp = target_frame->get_ntp_stamp();

    return true;
}
