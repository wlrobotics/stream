#include "Record/RecordUploader.h"

#include <ctime>

#include "Config.h"

#include "mov-writer.h"
#include "mov-buffer.h"
#include "mov-format.h"
#include "mpeg4-avc.h"
#include "mpeg4-hevc.h"
#include "Common/Stamp.h"
#include "Extension/SPSParser.h"

namespace mediakit {

RecordUploader& RecordUploader::Instance() {
    static std::shared_ptr<RecordUploader> s_instance(new RecordUploader());
    static RecordUploader &s_insteanc_ref = *s_instance;
    return s_insteanc_ref;
}

bool RecordUploader::init() {
    if(ConfigInfo.record.storage_type == "s3") {
        for(int i = 0; ; i++) {
            if(ConfigInfo.record.s3[i].endpoint.empty()) {
                break;
            }
            S3Client::S3Info s3_info;
            s3_info.endpoint = ConfigInfo.record.s3[i].endpoint;
            s3_info.outter_endpoint = ConfigInfo.record.s3[i].outter_endpoint;
            s3_info.ak = ConfigInfo.record.s3[i].accesskey;
            s3_info.sk = ConfigInfo.record.s3[i].secretkey;
            s3_info.bucket = ConfigInfo.record.s3[i].bucket;
            s3_info.object_tags = ConfigInfo.record.s3[i].object_tags;
            s3_info.use_virtual_address = ConfigInfo.record.s3[i].use_host_style_addr;
            S3Client::Ptr s3_client = std::make_shared<S3Client>();
            if(s3_client->init(s3_info)) {
                s3_clients_.push_back(s3_client);
            }
        }
    }

    return true;
}

Status RecordUploader::upload(const std::list<mediakit::Frame::Ptr>& frame_list,
                                       const std::string& bucket,
                                       const std::string& key,
                                      std::string& url) {
    bool ret = false;

    std::string codec = "h264";
    if(frame_list.back()->getCodecId() == CodecH265) {
        codec = "h265";
    }

    if (ConfigInfo.record.storage_type == "local") {
        std::string mp4_file = ConfigInfo.record.local_path + ConfigInfo.record.s3[0].bucket + "/" + key;

        auto mp4_buffer = std::make_shared<std::fstream>(mp4_file, std::ios::out|std::ios::binary);
        if(!mp4_buffer->is_open()) {
            return ERROR_OPEN_FAILED;
        }

        ret = mux_frames_to_mp4(frame_list, mp4_buffer);
        if(!ret) {
            return ERROR_MUX_FAILED;
        }
        ret = generate_metadata_file(key, codec);
        if(!ret) {
            return ERROR_OPEN_FAILED;
        }
        url = "http://" + ConfigInfo.record.s3[0].outter_endpoint + "/" + ConfigInfo.record.s3[0].bucket + "/" + key;
    } else if(ConfigInfo.record.storage_type == "s3") {
        auto mp4_buffer = std::make_shared<std::stringstream>();
        
        ret = mux_frames_to_mp4(frame_list, mp4_buffer);
        if(!ret) {
            return ERROR_MUX_FAILED;
        }

        std::srand(std::time(nullptr));
        ret = s3_clients_[(std::rand() % s3_clients_.size())]->put_object(bucket, key, mp4_buffer, codec, url);
        if(!ret) {
            return ERROR_UPLOAD_FAILED;
        }
    } else {
        return ERROR_UNKNOWN;
    }

    return SUCCESS;
}

bool RecordUploader::mux_frames_to_mp4(const std::list<mediakit::Frame::Ptr>& frame_list,
                                       std::shared_ptr<std::iostream> mp4_buffer) {
    struct mov_buffer_t buffer_io = {
        [](void* param, void* data, uint64_t bytes) {
            return 0;
        },
        [](void* param, const void* data, uint64_t bytes) {
            ((std::iostream*)param)->write((char*)data, bytes);
            return 0;
        },
        [](void* param, uint64_t offset) {
            ((std::iostream*)param)->seekp(offset);
            return 0;
        },
        [](void* param) -> std::uint64_t {
            return ((std::iostream*)param)->tellp();
        }
    };

    auto mov_ctx = std::shared_ptr<struct mov_writer_t>(
        mov_writer_create(&buffer_io, mp4_buffer.get(), MOV_FLAG_SEGMENT),
        [](struct mov_writer_t* mov) { mov_writer_destroy(mov);}
    );

    int vcl = 0;
    int update = 0;
    int track = -1;
    std::shared_ptr<uint8_t[]> frame_buffer(new uint8_t[FRAME_BUFFER_SIZE]);
    std::shared_ptr<uint8_t[]> extra_data(new uint8_t[EX_DATA_SIZE]);
    Stamp stamp;

    if(frame_list.back()->getCodecId() == CodecH264) {
        auto avc_ctx = std::make_shared<struct mpeg4_avc_t>();
        std::memset(avc_ctx.get(), 0x00, sizeof(avc_ctx.get()));

        for(const mediakit::Frame::Ptr& frame : frame_list) {
            int64_t temp_dts = 0;
            int64_t temp_pts = 0;
            stamp.revise(frame->dts(), frame->pts(), temp_dts, temp_pts);
            
            std::string sei_data;
            if (ConfigInfo.record.enabled_sei_data && frame->sei_enabled) {
                SEIParser::generate_sei_frame(sei_data, frame->sei_payload, true, false);
                std::memcpy(frame_buffer.get(), sei_data.c_str(), sei_data.size());
            }
        
            int mp4_frame_size = h264_annexbtomp4(avc_ctx.get(), frame->data(), frame->size(),
                                                  frame_buffer.get() + sei_data.size(),
                                                  FRAME_BUFFER_SIZE - sei_data.size(), &vcl, &update) + sei_data.size();
            if (track < 0) {
                if (avc_ctx->nb_sps < 1 || avc_ctx->nb_pps < 1) {
                    continue;
                }

                int video_width = 1920;
                int video_height = 1080;

                T_GetBitContext tGetBitBuf;
                memset(&tGetBitBuf, 0, sizeof(tGetBitBuf));

                T_SPS tH264SpsInfo;
                std::memset(&tH264SpsInfo, 0x00, sizeof(tH264SpsInfo));

                tGetBitBuf.pu8Buf = (uint8_t*)(avc_ctx->sps[0u].data) + 1;
                tGetBitBuf.iBufSize = avc_ctx->sps[0u].bytes - 1;
                int n_ret = h264DecSeqParameterSet((void*)(&tGetBitBuf), &tH264SpsInfo);
                if(n_ret != 0) {
                    WarnL << "parse sps resolution failed!";
                } else {
                    h264GetWidthHeight(&tH264SpsInfo, &video_width, &video_height);
                }

                int extra_data_size = mpeg4_avc_decoder_configuration_record_save(avc_ctx.get(), extra_data.get(), EX_DATA_SIZE);
                if (extra_data_size <= 0) {
                    continue;
                }

                track = mov_writer_add_video(mov_ctx.get(), MOV_OBJECT_H264, video_width, video_height, extra_data.get(), extra_data_size);
            }
            mov_writer_write_l(mov_ctx.get(), track, frame_buffer.get(), mp4_frame_size, temp_pts, temp_dts,
                                1 == vcl ? MOV_AV_FLAG_KEYFREAME : 0, 0);
        }
    } else if (frame_list.back()->getCodecId() == CodecH265) {
        auto hevc_ctx = std::make_shared<struct mpeg4_hevc_t>();
        std::memset(hevc_ctx.get(), 0x00, sizeof(hevc_ctx.get()));

        for(const mediakit::Frame::Ptr& frame : frame_list) {
            int64_t temp_dts = 0;
            int64_t temp_pts = 0;
            stamp.revise(frame->dts(), frame->pts(), temp_dts, temp_pts);

            std::string sei_data;
            if (ConfigInfo.record.enabled_sei_data && frame->sei_enabled) {
                SEIParser::generate_sei_frame(sei_data, frame->sei_payload, false, false);
                std::memcpy(frame_buffer.get(), sei_data.c_str(), sei_data.size());
            }

            int mp4_frame_size = h265_annexbtomp4(hevc_ctx.get(), frame->data(), frame->size(),
                                                 frame_buffer.get() + sei_data.size(),
                                                 FRAME_BUFFER_SIZE - sei_data.size(), &vcl, &update) + sei_data.size();
            if (track < 0) {
                if(hevc_ctx->numOfArrays < 3) {
                    continue;
                }

                int video_width = 1920;
                int video_height = 1080;

                for (int i = 0; i < hevc_ctx->numOfArrays; i++) {
                    if (hevc_ctx->nalu[i].type == 33) {
                        T_GetBitContext tGetBitBuf;
                        std::memset(&tGetBitBuf, 0x00, sizeof(tGetBitBuf));

                        T_HEVCSPS tH265SpsInfo;
                        std::memset(&tH265SpsInfo, 0x00, sizeof(tH265SpsInfo));
                        
                        tGetBitBuf.pu8Buf = (uint8_t*)(hevc_ctx->nalu[i].data) + 2;
                        tGetBitBuf.iBufSize = hevc_ctx->nalu[i].bytes - 2;
                        int n_ret = h265DecSeqParameterSet((void*)(&tGetBitBuf), &tH265SpsInfo);
                        if(n_ret != 0) {
                            WarnL << "parse sps resolution failed!";
                        } else {
                            h265GetWidthHeight(&tH265SpsInfo, &video_width, &video_height);
                        }
                        break;
                    }
                }

                int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(hevc_ctx.get(), 
                                                                                   extra_data.get(), EX_DATA_SIZE);
                if (extra_data_size <= 0) {
                    continue;
                }

                track = mov_writer_add_video(mov_ctx.get(), MOV_OBJECT_HEVC, video_width, video_height, extra_data.get(), extra_data_size);
            }
            mov_writer_write_l(mov_ctx.get(), track, frame_buffer.get(), mp4_frame_size, temp_pts, temp_dts,
                                1 == vcl ? MOV_AV_FLAG_KEYFREAME : 0, 0);
        }
    }

    return true;
}


bool RecordUploader::generate_metadata_file(const std::string& key, const std::string& codec) {
    std::string metadata_file_name = ConfigInfo.record.local_path + ".sys/buckets/" + ConfigInfo.record.s3[0].bucket + "/" + key + "_fs.json";  
   
    char metadata_json[256] = {0};
    int json_len = std::snprintf(metadata_json,
                                256,
                                R"({"version":"1.0.0","checksum":{"Algorithm":"","Blocksize":0,"Hashes":null},"meta":{"X-Amz-Meta-Codec":"%s", "X-Amz-Tagging":"%s,"content-type":"video/mp4","etag":"ad71fd1e980ace1999087303190af6da"}})",
                                codec.c_str(),
                                ConfigInfo.record.s3[0].object_tags.c_str());

    DebugL << metadata_file_name << "," << metadata_json << "," << json_len;

    std::fstream metadata_file(metadata_file_name, std::ios::out);
    if(!metadata_file.is_open()) {
        return false;
    }

    metadata_file.write(metadata_json, json_len);

    return true;
}
                       
}
