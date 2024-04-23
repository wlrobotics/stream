/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "GB28181Process.h"
#include "Util/File.h"
#include "Extension/CommonRtp.h"
#include "Extension/H264Rtp.h"
#include "Config.h"

namespace mediakit{

GB28181Process::GB28181Process(const MediaInfo &media_info, MediaSinkInterface *interface) {
    assert(interface);
    _media_info = media_info;
    _interface = interface;
}

GB28181Process::~GB28181Process() {}

bool GB28181Process::inputRtp(bool, const char *data, int data_len) {
    return handleOneRtp(0, TrackVideo, 90000, (unsigned char *) data, data_len);
}

void GB28181Process::onRtpSorted(const RtpPacket::Ptr &rtp, int) {
    if (!_rtp_decoder) {
        switch (rtp->PT) {
            case 98: {
                //H264负载
                _rtp_decoder = std::make_shared<H264RtpDecoder>();
                _interface->addTrack(std::make_shared<H264Track>());
                break;
            }
            default: {
                if (rtp->PT != 33 && rtp->PT != 96) {
                    WarnL << "rtp payload type未识别(" << (int) rtp->PT << "),已按ts或ps负载处理";
                }
                _rtp_decoder = std::make_shared<CommonRtpDecoder>(CodecInvalid, 256 * 1024);

                //设置dump目录
                if (!ConfigInfo.rtp.dumpdir.empty()) {
                    auto save_path = File::absolutePath(_media_info._streamid + ".mp2", ConfigInfo.rtp.dumpdir);
                    _save_file_ps.reset(File::create_file(save_path.data(), "wb"), [](FILE *fp) {
                        if (fp) {
                            fclose(fp);
                        }
                    });
                }
                break;
            }
       }

        //设置frame回调
        _rtp_decoder->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([this](const Frame::Ptr &frame) {
            onRtpDecode(frame);
        }));
    }

    _rtp_decoder->inputRtp(rtp, false);
}

const char *GB28181Process::onSearchPacketTail(const char *packet,uint64_t bytes) {
    try {
        auto ret = _decoder->input((uint8_t *) packet, bytes);
        if (ret >= 0) {
            return packet + ret;
        }
        return packet + bytes;
    } catch (std::exception &ex) {
        DebugL << "demux ps exception: bytes=" << bytes
               << ",exception=" << ex.what()
               << ",hex=" << hexdump((uint8_t *) packet, MIN(bytes, 64))
               << ",stream_id=" << _media_info._streamid;
        return packet + bytes;
    }
}

void GB28181Process::onRtpDecode(const Frame::Ptr &frame) {
    if (frame->getCodecId() == CodecH264) {
        _interface->inputFrame(frame);
        return;
    }

    if (_save_file_ps) {
        fwrite(frame->data(), frame->size(), 1, _save_file_ps.get());
    }

    if (!_decoder) {
        InfoL << _media_info._streamid << " judged to be PS";
        _decoder = DecoderImp::createDecoder(_interface);
    }

    if (_decoder) {
        HttpRequestSplitter::input(frame->data(), frame->size());
    }
}


int GB28181Process::get_rtp_loss_rate() {
    if (_rtp_decoder == nullptr) {
        return 0;
    }
    auto rtp_decoder = dynamic_pointer_cast<CommonRtpDecoder>(_rtp_decoder);
    if (rtp_decoder == nullptr) {
        return 0;
    }
    return rtp_decoder->get_rtp_loss_rate();
}

}//namespace mediakit