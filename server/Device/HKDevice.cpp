#include "HKDevice.h"

#include <thread>           
#include <chrono>            
#include <mutex>              
#include <condition_variable> 

#include "Util/TimeTicker.h"
#include "Util/MD5.h"
#include "Util/logger.h"

#ifdef _X86_64_
    #include "HCNetSDK.h"
#else
    #include "server_HCNetSDK.h"
#endif

#include "Common/config.h"
#include "Extension/H264.h"
#include "Extension/H265.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Config.h"

namespace mediakit {
HKDevice::HKDevice(const DeviceInfo& info): IDevice(info) {
    static onceToken token([](){
        NET_DVR_Init();
        std::string sdk_lib_path = "/workspace/lib/HCNetSDK";
        char cryptoPath[2048] = {0};
        std::snprintf(cryptoPath, 2048, "%s/libcrypto.so", sdk_lib_path.c_str());
        BOOL sdk_ret = NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_LIBEAY_PATH, cryptoPath);
        if(sdk_ret == FALSE) {
            WarnL << "NET_DVR_SetSDKInitCfg NET_SDK_INIT_CFG_LIBEAY_PATH failed!";
            return false;
        }

        char sslPath[2048] = {0};
        std::snprintf(sslPath, 2048, "%s/libssl.so", sdk_lib_path.c_str());
        sdk_ret = NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_SSLEAY_PATH, sslPath); 
        if(sdk_ret == FALSE) {
            WarnL << "NET_DVR_SetSDKInitCfg NET_SDK_INIT_CFG_SSLEAY_PATH failed!";
            return false;
        }
            
        NET_DVR_LOCAL_SDK_PATH struComPath = {0};
        std::strncpy(struComPath.sPath, sdk_lib_path.c_str(), NET_SDK_MAX_FILE_PATH - 1);
        sdk_ret = NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_SDK_PATH, &struComPath);
        if(sdk_ret == FALSE) {
            WarnL << "NET_DVR_SetSDKInitCfg NET_SDK_INIT_CFG_SDK_PATH failed!";
            return false;
        }
        //NET_DVR_SetLogPrint(TRUE);

        if(ConfigInfo.hksdk.enable_port_range) {
            NET_DVR_LOCAL_TCP_PORT_BIND_CFG port_cfg;
            std::memset(&port_cfg, 0, sizeof(port_cfg));
            port_cfg.wLocalBindTcpMinPort = ConfigInfo.hksdk.start_port;
            port_cfg.wLocalBindTcpMaxPort = ConfigInfo.hksdk.end_port;
            NET_DVR_SetSDKLocalCfg(NET_SDK_LOCAL_CFG_TYPE_TCP_PORT_BIND, &port_cfg);
        }
    }, []() {
        NET_DVR_Cleanup();
    });
    InfoL << endl;
}

HKDevice::~HKDevice() {
    logout();
    media_close();
}

bool HKDevice::init() {
    IDevice::init();
    return login();
}

bool HKDevice::login() {
    NET_DVR_USER_LOGIN_INFO struLoginInfo = {0};
    std::memset(&struLoginInfo, 0, sizeof(struLoginInfo));
    NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = {0};
    std::memset(&struDeviceInfoV40, 0, sizeof(struDeviceInfoV40));
    struLoginInfo.bUseAsynLogin = FALSE;
    struLoginInfo.wPort = 8000;
    std::strncpy(struLoginInfo.sDeviceAddress, device_info_.host.c_str(), NET_DVR_DEV_ADDRESS_MAX_LEN - 1);
    std::strncpy(struLoginInfo.sUserName, device_info_.user_name.c_str(), NAME_LEN - 1);
    std::strncpy(struLoginInfo.sPassword, device_info_.password.c_str(), NAME_LEN - 1);

    NET_DVR_SetConnectTime(ConfigInfo.hksdk.timeout, 1);
    sdk_handle_ = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);
    if(sdk_handle_ < 0) {
        ErrorID(this) << "NET_DVR_Login_V40 failed! info.host=" << device_info_.host
                      << ",Error=" << NET_DVR_GetLastError()
                      << ",ErrorMsg=" << NET_DVR_GetErrorMsg();
        return false;
    }

    if(struDeviceInfoV40.struDeviceV30.byStartDChan > 0) {
        device_info_.channel_id = struDeviceInfoV40.struDeviceV30.byStartDChan + device_info_.channel_id - 1;
    }
    
    return true;
}

void HKDevice::logout() {
    if (sdk_handle_ >= 0) {
        NET_DVR_Logout(sdk_handle_);
        sdk_handle_ = -1;
    }
}

bool HKDevice::media_open() {
    IDevice::media_open();
    
    NET_DVR_COMPRESSIONCFG_V30 struMediaInfo = {0};
    std::memset(&struMediaInfo, 0, sizeof(struMediaInfo));    
    DWORD dwReturned = 0; 
    BOOL sdk_ret = NET_DVR_GetDVRConfig(sdk_handle_, NET_DVR_GET_COMPRESSCFG_V30, device_info_.channel_id,
                                        &struMediaInfo, sizeof(struMediaInfo), &dwReturned);
    if(sdk_ret == FALSE) {
        ErrorID(this) << "NET_DVR_GetDVRConfig"
               << ",NET_DVR_GetLastError=" << NET_DVR_GetLastError()
               << ",NET_DVR_GetErrorMsg=" << NET_DVR_GetErrorMsg()
               << ",dwReturned=" << dwReturned;
        return false;
    }
    if(struMediaInfo.struNormHighRecordPara.byVideoEncType == 1) {
        codec_id_ = CodecH264;
        muxer_->addTrack(std::make_shared<H264Track>());
        stream_info_.codec_ = "H264";
    } else if(struMediaInfo.struNormHighRecordPara.byVideoEncType == 10) {
        codec_id_ = CodecH265;
        muxer_->addTrack(std::make_shared<H265Track>());
        stream_info_.codec_ = "H265";
    } else {
        ErrorID(this) << "codec_id not support, byVideoEncType=" 
                      << struMediaInfo.struNormHighRecordPara.byVideoEncType;
        return false;
    }
    muxer_->addTrackCompleted();

    NET_DVR_PREVIEWINFO struPlayInfo = {0};
    std::memset(&struPlayInfo, 0, sizeof(struPlayInfo));
    struPlayInfo.lChannel = device_info_.channel_id;
    struPlayInfo.dwStreamType = 0;      //码流类型，0-主码流，1-子码流，2-码流3，3-码流4 等以此类推
    struPlayInfo.dwLinkMode = 0;        //0：TCP方式,1：UDP方式,2：多播方式,3-RTP方式，4-RTP/RTSP,5-RSTP/HTTP
    struPlayInfo.hPlayWnd = 0;          //播放窗口的句柄,为NULL表示不播放图象
    struPlayInfo.byProtoType = 0;       //应用层取流协议，0-私有协议，1-RTSP协议
    struPlayInfo.dwDisplayBufNum = 0;   //播放库播放缓冲区最大缓冲帧数，范围1-50，置0时默认为1
    struPlayInfo.bBlocked = 0;
    stream_info_.bits_ = struMediaInfo.struNormHighRecordPara.dwVideoBitrate;
    realplay_handle_ = NET_DVR_RealPlay_V40(sdk_handle_, &struPlayInfo, nullptr, nullptr);
    if (realplay_handle_ < 0) {
        return false;
    }

   
    auto call_back = [](LONG lRealHandle, NET_DVR_PACKET_INFO_EX *pkt, void *pUser) {
        if(pkt->dwPacketType != 1 && pkt->dwPacketType != 2 && pkt->dwPacketType != 3) {
            return ;
        }
        HKDevice *self = reinterpret_cast<HKDevice*>(pUser);
        if(self->realplay_handle_ != lRealHandle) {
            return;
        }

        //丢弃初始拉流时OSD错误的视频帧
        if(!self->drop_frame_ && self->drop_frame_count_++ < 50) {
            return ;
        } else {
            self->drop_frame_ = true;
        }

        self->stream_info_.width_ = pkt->wWidth;
        self->stream_info_.height_ = pkt->wHeight;
        self->stream_info_.time_stamps_ = pkt->dwTimeStamp;
        self->stream_info_.frame_rate_   = pkt->dwFrameRate;
        self->total_bytes_ += pkt->dwPacketSize;
        self->speed_ += pkt->dwPacketSize;

        FrameImp::Ptr frame;
        if(self->codec_id_ == CodecH264) {
           frame = std::make_shared<H264Frame>();
        } else {
           frame = std::make_shared<H265Frame>();
        }
        frame->_dts = pkt->dwTimeStamp;
        frame->_pts = frame->_dts;
        frame->_buffer.assign((const char*)pkt->pPacketBuffer, pkt->dwPacketSize);
        frame->_prefix_size = prefixSize(reinterpret_cast<const char*>(pkt->pPacketBuffer), pkt->dwPacketSize);
        
        if(self->device_info_.ptz.enabled && (self->frame_count_++ == ConfigInfo.ptz.frame_interval)) {
            self->frame_count_ = 0;
            PTZ ptz;
            if(self->get_position(ptz)) {
                frame->sei_enabled = true;
                frame->sei_payload.data.ptz_current_pos[0] = ptz.pan;
                frame->sei_payload.data.ptz_current_pos[1] = ptz.tilt;
                frame->sei_payload.data.ptz_current_pos[2] = ptz.zoom;
            }
        }

        if(ConfigInfo.time_stamp.ntp_time_enable) {
            frame->sei_enabled = true;
            frame->sei_payload.data.ntp_time_stamp = toolkit::get_utc_time(pkt->dwYear,
                                                                           pkt->dwMonth,
                                                                           pkt->dwDay,
                                                                           pkt->dwHour,
                                                                           pkt->dwMinute,
                                                                           pkt->dwSecond,
                                                                           pkt->dwMillisecond);
        }

        self->input_frame(frame);
        self->IDevice::keep_alive(); 
    };
    
    sdk_ret = NET_DVR_SetESRealPlayCallBack(realplay_handle_, call_back, this);
    if(sdk_ret == FALSE) {
        ErrorID(this) << "NET_DVR_SetESRealPlayCallBack "
              << ",NET_DVR_GetLastError=" << NET_DVR_GetLastError()
              << ",NET_DVR_GetErrorMsg=" << NET_DVR_GetErrorMsg();
        return false;
    }

    InfoID(this) << "RealPlay success "
                 << ",handle: " << sdk_handle_ 
                 << ",codec:" << stream_info_.codec_;
    return true;
}

bool HKDevice::media_close() {
    if (realplay_handle_ >= 0) {
        InfoID(this) << "NET_DVR_StopRealPlay success";
        NET_DVR_StopRealPlay(realplay_handle_);
        realplay_handle_ = -1;
    }
}

bool HKDevice::get_position(PTZ& position) {
    NET_DVR_PTZPOS ptz_pos;
    std::memset(&ptz_pos, 0, sizeof(NET_DVR_PTZPOS));
    
    DWORD uiReturnLen;
    BOOL ptzret = NET_DVR_GetDVRConfig(sdk_handle_, NET_DVR_GET_PTZPOS, device_info_.channel_id,
                                       &ptz_pos, sizeof(ptz_pos), &uiReturnLen);
    if(ptzret == FALSE) {
        ErrorID(this) << "NET_DVR_GetDVRConfig NET_DVR_GET_PTZPOS failed!"
                      << ",NET_DVR_GetLastError=" << NET_DVR_GetLastError()
                      << ",NET_DVR_GetErrorMsg=" << NET_DVR_GetErrorMsg();
        return false;
    }

    auto HexToDec = [] (WORD wHex) -> WORD {
        return (wHex / 4096) * 1000 + ((wHex % 4096) / 256) * 100 + ((wHex % 256) / 16) * 10 + (wHex % 16);
    };
    position.pan = HexToDec(ptz_pos.wPanPos);
    position.tilt = HexToDec(ptz_pos.wTiltPos);
    position.zoom = HexToDec(ptz_pos.wZoomPos);
    
    return true;
}

bool HKDevice::drag_zoom_in(const Rect& rect) {
    NET_DVR_POINT_FRAME point_frame;
    std::memset(&point_frame, 0, sizeof(point_frame));

    point_frame.xTop = rect.xTop*255/stream_info_.width_;
    point_frame.yTop = rect.yTop*255/stream_info_.height_;
    point_frame.xBottom = rect.xBottom*255/stream_info_.width_;
    point_frame.yBottom = rect.yBottom*255/stream_info_.height_;
    InfoID(this) << "HKDevice drag_zoom_in=" << stream_info_.width_ << " "
          << stream_info_.height_ << " "
          << point_frame.xTop << " " 
          << point_frame.yTop << " " 
          << point_frame.xBottom << " "
          << point_frame.yBottom;
    BOOL sdk_ret = NET_DVR_PTZSelZoomIn_EX(sdk_handle_, 1, &point_frame);
    if(sdk_ret == FALSE) {
        ErrorID(this) << "zoom in"
                      << ",NET_DVR_GetLastError=" << NET_DVR_GetLastError()
                      << ",NET_DVR_GetErrorMsg=" << NET_DVR_GetErrorMsg();
        return false;
    }
    return true;
}
                                
bool HKDevice::drag_zoom_out(const Rect& rect) {
    NET_DVR_POINT_FRAME point_frame;
    std::memset(&point_frame, 0, sizeof(point_frame));

    if(rect.xBottom - rect.xTop < 2) {
        ErrorID(this) << "drag_zoom_out xtop - xbottom < 2" << rect.xBottom << "  "<< rect.xTop;
        return true;
    }

    point_frame.xTop = rect.xBottom*255/stream_info_.width_;
    point_frame.yTop = rect.yBottom*255/stream_info_.height_;
    point_frame.xBottom = rect.xTop*255/stream_info_.width_;
    point_frame.yBottom = rect.yTop*255/stream_info_.height_;
    
    InfoID(this) << "HKDevice drag_zoom_out=" << stream_info_.width_ << " " << stream_info_.height_
        		 << point_frame.xTop << " " << point_frame.yTop << " " << point_frame.xBottom << " "<<point_frame.yBottom;
    BOOL sdk_ret = NET_DVR_PTZSelZoomIn_EX(sdk_handle_, 1, &point_frame);
    if(sdk_ret == FALSE) {
        ErrorID(this) << "zoom in"
               << ",NET_DVR_GetLastError=" << NET_DVR_GetLastError()
               << ",NET_DVR_GetErrorMsg=" << NET_DVR_GetErrorMsg();
        return false;
    }
    return true;
}

bool HKDevice::goto_position(const PTZ& position) {
    NET_DVR_PTZPOS ptz_pos;
    std::memset(&ptz_pos, 0, sizeof(ptz_pos));

    auto DecToHex = [](int num) -> WORD{
        return (num / 1000) * 4096 + ((num % 1000) / 100) * 256 + ((num % 100) / 10) * 16 + (num % 10);
    };
    ptz_pos.wPanPos = DecToHex(position.pan);
    ptz_pos.wTiltPos = DecToHex(position.tilt);
    ptz_pos.wZoomPos = DecToHex(position.zoom);
    ptz_pos.wAction = 1;
    InfoID(this) << "goto ptz1 "
           << "pan: " << position.pan
           << "tilt: " << position.tilt << " zoom: "<< position.tilt << "hex: " << ptz_pos.wPanPos
           << "  " << ptz_pos.wTiltPos << "  "<< ptz_pos.wZoomPos;


    BOOL sdk_ret = NET_DVR_SetDVRConfig(sdk_handle_, NET_DVR_SET_PTZPOS, 1, &ptz_pos, sizeof(NET_DVR_PTZPOS));
    if(sdk_ret == FALSE) {
        ErrorID(this) << "set pre pos"          
               << ",NET_DVR_GetLastError=" << NET_DVR_GetLastError()
               << ",NET_DVR_GetErrorMsg=" << NET_DVR_GetErrorMsg();
        return false;
    }
    return true;
}
                                 
// bool HKDevice::capture_picture(std::string& picture, std::uint64_t& ntp_time_stamp) {
//     NET_DVR_JPEGPARA jpeg_param;
//     std::memset(&jpeg_param, 0, sizeof(jpeg_param));
//     jpeg_param.wPicSize = 0xff;
//     jpeg_param.wPicQuality = 0;

//     std::uint32_t real_size = 0;

//     std::uint32_t pic_size = 4194304;
//     char *pic_buf = new char[pic_size];
//     auto pic_buf_defer = std::shared_ptr<char>(nullptr, [&pic_buf](char* p) {
//         delete[] pic_buf;
//     });

//     BOOL sdk_ret = NET_DVR_CaptureJPEGPicture_NEW(sdk_handle_,
//                                                   1,
//                                                   &jpeg_param,
//                                                   pic_buf, pic_size,
//                                                   &real_size);
//     if(sdk_ret == FALSE) {
//         ErrorID(this) << "NET_DVR_CaptureJPEGPicture_NEW"
//                << ",NET_DVR_GetLastError=" << NET_DVR_GetLastError()
//                << ",NET_DVR_GetErrorMsg=" << NET_DVR_GetErrorMsg()
//                << ",real_size=" << real_size;
//         return false;
//     }
//     picture.assign(pic_buf, real_size);

//     return true;
// }

bool HKDevice::get_preset(std::vector<PresetInfo>& preset_list) {
    NET_DVR_PRESET_NAME array_preset[MAX_PRESET_V40];
    std::memset(array_preset, 0, MAX_PRESET_V40*sizeof(NET_DVR_PRESET_NAME));
    DWORD total_returned;
    BOOL bret = NET_DVR_GetDVRConfig(sdk_handle_, NET_DVR_GET_PRESET_NAME, 1, &array_preset, 
                                    MAX_PRESET_V40*sizeof(NET_DVR_PRESET_NAME), &total_returned);
    if(bret == FALSE) {
        ErrorL << "NET_DVR_GetDVRConfig NET_DVR_GET_PRESET_NAME failed!"
                << ",NET_DVR_GetLastError=" << NET_DVR_GetLastError()
                << ",NET_DVR_GetErrorMsg=" << NET_DVR_GetErrorMsg();
        return false;
    }

    uint32_t actual_num = total_returned/sizeof(NET_DVR_PRESET_NAME);
 
    for(int index = 0; index < actual_num; index++) {
        if(array_preset[index].wPanPos == 0 && array_preset[index].wTiltPos == 0) {
            continue;
        }
        PresetInfo preset;
        char str_utf8_name[128] = {0};
        toolkit::gb2312_to_utf8(array_preset[index].byName, std::strlen(array_preset[index].byName), str_utf8_name, 128);
        preset.preset_id = array_preset[index].wPresetNum;
        preset.preset_name = str_utf8_name;
        preset.ptz = {array_preset[index].wPanPos, array_preset[index].wTiltPos, array_preset[index].wZoomPos};
        InfoL << "preset "<< preset.preset_id 
			  << " len:"<< std::strlen(array_preset[index].byName) 
			  << " name:"<< preset.preset_name << " "<< preset.ptz.pan;
        preset_list.emplace_back(preset);
    }
    return true;
}

bool HKDevice::set_preset(mediakit::PresetCmdType preset_cmd, uint32_t index) {
    DWORD dwPTZPresetCmd = PAN_LEFT;
    DWORD dwPresetIndex = index;

    if(dwPresetIndex > MAX_PRESET_V40){
        ErrorID(this) << "set_preset index except "<< index;
        return false;
    }

    switch (preset_cmd) {
    case PRESET_CMD_SET:
        dwPTZPresetCmd = SET_PRESET;
        break;
    case PRESET_CMD_GOTO:
        dwPTZPresetCmd = GOTO_PRESET;
        break;
    case PRESET_CMD_DEL:
        dwPTZPresetCmd = CLE_PRESET;
        break;
    default:
        return false;
    }

    BOOL sdk_ret = NET_DVR_PTZPreset_Other(sdk_handle_, 1, dwPTZPresetCmd, dwPresetIndex);
    if(sdk_ret == FALSE) {
        ErrorL << "set_preset failed"
               << ",Error=" << NET_DVR_GetLastError()
               << ",Msg=" << NET_DVR_GetErrorMsg();
        return false;
    }
    WarnL << "set_preset cmd "<< dwPTZPresetCmd << " " << dwPresetIndex;
    return true;
}

bool HKDevice::ptz_control(mediakit::PTZCmdType ptz_cmd, uint8_t speed) {
    DWORD dwPTZCommand = PAN_LEFT;
    DWORD dwStop = 0;
    speed++;
    DWORD dwSpeed = speed > 7 ? 7: speed;//[1,7]

    switch (ptz_cmd)
    {
        case mediakit::PTZ_CMD_RIGHT:
            dwPTZCommand = PAN_RIGHT;
            break;
        case mediakit::PTZ_CMD_LEFT:
            dwPTZCommand = PAN_LEFT;
            break;
        case mediakit::PTZ_CMD_UP:
            dwPTZCommand = TILT_UP;
            break;
        case mediakit::PTZ_CMD_DOWN:
            dwPTZCommand = TILT_DOWN;
            break;
        case mediakit::PTZ_CMD_LEFT_UP:
            dwPTZCommand = UP_LEFT;
            break;
        case mediakit::PTZ_CMD_LEFT_DOWN:
            dwPTZCommand = DOWN_LEFT;
            break;
        case mediakit::PTZ_CMD_RIGHT_UP:
            dwPTZCommand = UP_RIGHT;
            break;
        case mediakit::PTZ_CMD_RIGHT_DOWN:
            dwPTZCommand = DOWN_RIGHT;
            break;
        case mediakit::PTZ_CMD_ZOOM_IN:
            dwPTZCommand = ZOOM_IN;
            break;
        case mediakit::PTZ_CMD_ZOOM_OUT:
            dwPTZCommand = ZOOM_OUT;
            break;
        case mediakit::PTZ_CMD_STOP:
        default:
            dwStop = 1;
            break;
    }

    BOOL sdk_ret = NET_DVR_PTZControlWithSpeed_Other(sdk_handle_, 1, dwPTZCommand, dwStop, dwSpeed);
    if(sdk_ret == FALSE) {
        ErrorL << "ptz_control failed"
            << " "<< dwPTZCommand << " "<< dwStop
            << ",Error=" << NET_DVR_GetLastError()
            << ",Msg=" << NET_DVR_GetErrorMsg();
        return false;
    }
    WarnL << "ptz_control success" << " "<< dwPTZCommand << " "<< dwStop;
    return true;
}

}
