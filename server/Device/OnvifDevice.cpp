#include "OnvifDevice.h"

#include "soapMediaBindingProxy.h"
#include "soapDeviceBindingProxy.h"
#include "soapMedia2BindingProxy.h"
#include "soapPTZBindingProxy.h"
#include "wsseapi.h"
#include "wsaapi.h"
#include "DeviceBinding.nsmap"
#include "Util/util.h"
#include "Http/strCoding.h"
#include "Config.h"

using namespace mediakit;

OnvifDevice::OnvifDevice(const DeviceInfo& info) : RTSPDevice(info) {
}

OnvifDevice::~OnvifDevice() {
    RTSPDevice::media_close();
}

bool OnvifDevice::init() {
    IDevice::init();

    media_service_address_ = "http://" + device_info_.host + "/onvif/media_service";
    //ptz_service_address_ = "http://" + device_info_.host + "/onvif/ptz_service";

    std::string device_service_address_ = "http://" + device_info_.host + "/onvif/device_service";
    DeviceBindingProxy device_proxy(device_service_address_.c_str());
    device_proxy.soap->connect_timeout = 3;
    device_proxy.soap->recv_timeout = 2;
    device_proxy.soap->send_timeout = 2;
    device_proxy.soap->transfer_timeout = 2;
    device_proxy.soap->accept_timeout = 2;
    _tds__GetServices *tds__GetServices = soap_new_req__tds__GetServices(device_proxy.soap, true);
    _tds__GetServicesResponse tds__GetServicesResponse;
    int ret = device_proxy.GetServices(tds__GetServices, tds__GetServicesResponse);
    if(SOAP_OK != ret) {
        ErrorID(this) << "GetServices failed ret=" << ret;
        return false;
    }
    for(int idx=0; idx < tds__GetServicesResponse.Service.size(); idx++) {
        auto service = tds__GetServicesResponse.Service[idx];
        if(service->XAddr.empty() || service->Namespace.empty()) {
            continue;
        }

        if(service->Namespace.find("ptz") != std::string::npos){
            ptz_service_address_ = service->XAddr;
        }
        if(service->Namespace.find("media") != std::string::npos) {
            if(service->Namespace.find("ver20")!= std::string::npos) {
                media2_service_address_ = service->XAddr;
            } else {
                media_service_address_ = service->XAddr;
            }
        }
    }

    InfoID(this) << "media_service_address_=" << media_service_address_
        << " media2: " << media2_service_address_ << " ptz: "<< ptz_service_address_;

    if(media2_service_address_.empty()) {
        MediaBindingProxy media_proxy(media_service_address_.c_str());
        media_proxy.soap->connect_timeout = 3;
        media_proxy.soap->recv_timeout = 2;
        media_proxy.soap->send_timeout = 2;
        media_proxy.soap->transfer_timeout = 2;
        media_proxy.soap->accept_timeout = 2;
        if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(media_proxy.soap,
                                                        nullptr,
                                                        device_info_.user_name.c_str(),
                                                        device_info_.password.c_str())) {
            return false;
        }

        _trt__GetProfiles *trt__GetProfiles_ = soap_new__trt__GetProfiles(media_proxy.soap);
        _trt__GetProfilesResponse trt__GetProfilesResponse_;

        ret = media_proxy.GetProfiles(trt__GetProfiles_, trt__GetProfilesResponse_);
        if(SOAP_OK == ret && !trt__GetProfilesResponse_.Profiles.empty()) {
            InfoID(this) << "GetProfiles success token=" << trt__GetProfilesResponse_.Profiles[0]->token;
            profile_token_ = trt__GetProfilesResponse_.Profiles[0]->token;
        } else {
            ErrorID(this) << "GetProfiles failed ret=" << ret;
        }
        soap_destroy(media_proxy.soap);
        soap_end(media_proxy.soap);

    } else {
        Media2BindingProxy media2_proxy(media2_service_address_.c_str());
        media2_proxy.soap->connect_timeout = 3;
        media2_proxy.soap->recv_timeout = 2;
        media2_proxy.soap->send_timeout = 2;
        media2_proxy.soap->transfer_timeout = 2;
        media2_proxy.soap->accept_timeout = 2;
        if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(media2_proxy.soap,
                                                        nullptr,
                                                        device_info_.user_name.c_str(),
                                                        device_info_.password.c_str())) {
            return false;
        }

        _ns1__GetProfiles *ns1__GetProfiles = soap_new__ns1__GetProfiles(media2_proxy.soap);
        _ns1__GetProfilesResponse ns1__GetProfilesResponse;
        ret = media2_proxy.GetProfiles(ns1__GetProfiles, ns1__GetProfilesResponse);
        if(SOAP_OK == ret && !ns1__GetProfilesResponse.Profiles.empty()) {
            InfoID(this) << "GetProfiles success token=" << ns1__GetProfilesResponse.Profiles[0]->token;
            profile_token_ = ns1__GetProfilesResponse.Profiles[0]->token;
        } else {
            ErrorID(this) << "GetProfiles failed ret=" << ret;
        }

        soap_destroy(media2_proxy.soap);
        soap_end(media2_proxy.soap);
    }
    return SOAP_OK == ret ? true : false;
}

bool OnvifDevice::media_open() {
    if(device_info_.url.empty()) {
        std::string stream_uri;
        bool b_ret = get_stream_uri(stream_uri);
        if(!b_ret) {
            ErrorID(this) << "get_stream_uri failed!";
            return false;
        }
        std::string encode_password = strCoding::UrlEncode(device_info_.password);
        stream_uri.insert(7, device_info_.user_name + ":" + encode_password + "@");
        InfoID(this) << "stream_uri=" << stream_uri;
        
        device_info_.url = stream_uri;
    }

    return RTSPDevice::media_open();
}

bool OnvifDevice::get_position(PTZ& position) {
    if(ptz_service_address_.empty()) {
        return false;
    }

    PTZBindingProxy ptz_proxy(ptz_service_address_.c_str());
    ptz_proxy.soap->connect_timeout = 1;
    ptz_proxy.soap->recv_timeout = 1;
    ptz_proxy.soap->send_timeout = 1;
    ptz_proxy.soap->transfer_timeout = 1;
    ptz_proxy.soap->accept_timeout = 1;
    if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(ptz_proxy.soap,
                                                     nullptr,
                                                     device_info_.user_name.c_str(),
                                                     device_info_.password.c_str())) {
        return false;
    }
    _tptz__GetStatus req;
    req.ProfileToken = profile_token_;
    _tptz__GetStatusResponse resp;
    int ret = ptz_proxy.GetStatus(&req, resp);
    if(SOAP_OK == ret && resp.PTZStatus && resp.PTZStatus->Position) {
        if(resp.PTZStatus->Position->PanTilt->x > 0) {
            position.pan = std::round(resp.PTZStatus->Position->PanTilt->x * 1800);
        } else {
            position.pan = std::round((resp.PTZStatus->Position->PanTilt->x + 2) * 1800);
        }
        position.tilt = std::round(std::abs(resp.PTZStatus->Position->PanTilt->y - 1) * 450);
        if(device_info_.manufacturer_type == ManufacturerType::HIKVISION) {
            position.zoom = std::round((resp.PTZStatus->Position->Zoom->x * (device_info_.ptz.max_zoom - 1)   + 1) * 10);
        } else if (device_info_.manufacturer_type == ManufacturerType::DAHUA) {
            position.zoom = std::round(resp.PTZStatus->Position->Zoom->x * device_info_.ptz.max_zoom * 10);
        }
    } else {
        ErrorID(this) << "GetStatus failed ret=" << ret;
    }

    DebugID(this) << "x=" << resp.PTZStatus->Position->PanTilt->x
                  << ",y=" << resp.PTZStatus->Position->PanTilt->y
                  << ",z=" << resp.PTZStatus->Position->Zoom->x;

    soap_destroy(ptz_proxy.soap);
    soap_end(ptz_proxy.soap);

    return SOAP_OK == ret ? true : false;
}

bool OnvifDevice::goto_position(const PTZ& position) {
    if(ptz_service_address_.empty()) {
        return false;
    }

    PTZBindingProxy ptz_proxy(ptz_service_address_.c_str());
    ptz_proxy.soap->connect_timeout = 1;
    ptz_proxy.soap->recv_timeout = 1;
    ptz_proxy.soap->send_timeout = 1;
    ptz_proxy.soap->transfer_timeout = 1;
    ptz_proxy.soap->accept_timeout = 1;
    if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(ptz_proxy.soap,
                                                    nullptr,
                                                    device_info_.user_name.c_str(),
                                                    device_info_.password.c_str())) {
        return false;
    }

    _tptz__AbsoluteMove req;

    req.ProfileToken = profile_token_;

    req.Position = soap_new_tt__PTZVector(ptz_proxy.soap, -1);
    req.Position->PanTilt = soap_new_tt__Vector2D(ptz_proxy.soap, -1);
    req.Position->Zoom = soap_new_tt__Vector1D(ptz_proxy.soap, -1);

    if(position.pan >= 1800) {
        req.Position->PanTilt->x = position.pan / 1800.0 - 2;
    } else {
        req.Position->PanTilt->x = position.pan / 1800.0;
    }

    if(position.tilt > 450) {
        req.Position->PanTilt->y = 1 + position.tilt / 450.0;
    } else {
        req.Position->PanTilt->y = 1 - position.tilt / 450.0;
    }

    if(device_info_.manufacturer_type == ManufacturerType::HIKVISION) {
        req.Position->Zoom->x = (position.zoom / 10.0 - 1.0) / ((device_info_.ptz.max_zoom - 1)*1.0);
    } else if (device_info_.manufacturer_type == ManufacturerType::DAHUA) {
        req.Position->Zoom->x = position.zoom / (device_info_.ptz.max_zoom * 10.0);
    }

    InfoID(this) << "x=" << req.Position->PanTilt->x << ",y=" << req.Position->PanTilt->y << ",z=" << req.Position->Zoom->x;

    _tptz__AbsoluteMoveResponse resp;

    bool ret = ptz_proxy.AbsoluteMove(&req, resp);
    if(ret != SOAP_OK) {
        ErrorL << "AbsoluteMove soap error" << *soap_faultcode(ptz_proxy.soap)<<" msg:"<< *soap_faultstring(ptz_proxy.soap);
    }

    soap_destroy(ptz_proxy.soap);
    soap_end(ptz_proxy.soap);

    return true;
}


bool OnvifDevice::get_stream_uri(std::string &stream_uri) {
    int ret = SOAP_OK;
    if(media2_service_address_.empty()) {
        MediaBindingProxy media_proxy(media_service_address_.c_str());
        media_proxy.soap->connect_timeout = 3;
        media_proxy.soap->recv_timeout = 2;
        media_proxy.soap->send_timeout = 2;
        media_proxy.soap->transfer_timeout = 2;
        media_proxy.soap->accept_timeout = 2;
        if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(media_proxy.soap,
                                                        nullptr,
                                                        device_info_.user_name.c_str(),
                                                        device_info_.password.c_str())) {
            return false;
        }

        tt__Transport *tt__Transport_ = soap_new_req_tt__Transport(media_proxy.soap,
                                                                tt__TransportProtocol__RTSP);
        tt__StreamSetup *tt__StreamSetup_ = soap_new_req_tt__StreamSetup(media_proxy.soap,
                                                                        tt__StreamType__RTP_Unicast,
                                                                        tt__Transport_);
        _trt__GetStreamUri *_trt__GetStreamUri_ = soap_new_req__trt__GetStreamUri(media_proxy.soap,
                                                                                tt__StreamSetup_,
                                                                                profile_token_);
        _trt__GetStreamUriResponse _trt__GetStreamUriResponse_;
        ret = media_proxy.GetStreamUri(_trt__GetStreamUri_, _trt__GetStreamUriResponse_);
        if(SOAP_OK == ret) {
            stream_uri = _trt__GetStreamUriResponse_.MediaUri->Uri;
        } else {
            ErrorID(this) << "GetStreamUri failed ret=" << ret;
        }
        soap_destroy(media_proxy.soap);
        soap_end(media_proxy.soap);
    } else {
        Media2BindingProxy media_proxy2(media2_service_address_.c_str());
        media_proxy2.soap->connect_timeout = 3;
        media_proxy2.soap->recv_timeout = 2;
        media_proxy2.soap->send_timeout = 2;
        media_proxy2.soap->transfer_timeout = 2;
        media_proxy2.soap->accept_timeout = 2;

        if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(media_proxy2.soap,
                                                        nullptr,
                                                        device_info_.user_name.c_str(),
                                                        device_info_.password.c_str())) {
            return false;
        }

        _ns1__GetStreamUri* ns1__GetStreamUri = soap_new_req__ns1__GetStreamUri(media_proxy2.soap,
                                                                                "RtspUnicast",
                                                                                profile_token_);
        _ns1__GetStreamUriResponse ns1__GetStreamUriResponse;
        ret = media_proxy2.GetStreamUri(ns1__GetStreamUri, ns1__GetStreamUriResponse);
        if(SOAP_OK == ret) {
            stream_uri = ns1__GetStreamUriResponse.Uri;
        } else {
            ErrorID(this) << "GetStreamUri failed ret=" << ret;
        }
        soap_destroy(media_proxy2.soap);
        soap_end(media_proxy2.soap);
    }
    return SOAP_OK == ret ? true : false;
}

bool OnvifDevice::get_preset(std::vector<PresetInfo>& preset_list) {
    return true;

    PTZBindingProxy ptz_proxy(ptz_service_address_.c_str());
    soap_set_mode(ptz_proxy.soap, SOAP_C_UTFSTRING);
    ptz_proxy.soap->connect_timeout = 1;
    if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(ptz_proxy.soap,
                                                     nullptr,
                                                     device_info_.user_name.c_str(),
                                                     device_info_.password.c_str())) {
        ErrorL << "token digest failed";
        return false;
    }

    _tptz__GetPresets preset;
    _tptz__GetPresetsResponse response;
    preset.ProfileToken = profile_token_;
    int result = ptz_proxy.GetPresets(&preset, response);
    if (SOAP_OK != result) {
        ErrorL << "GetPresets soap error" << *soap_faultcode(ptz_proxy.soap)<<" msg:"<< *soap_faultstring(ptz_proxy.soap);
    } else {
        for (int i = 0;i<response.Preset.size();i++) {
            tt__Vector2D* panTilt = response.Preset[i]->PTZPosition->PanTilt;
            tt__Vector1D* zoom = response.Preset[i]->PTZPosition->Zoom;
            std::string name = *(response.Preset[i]->Name);
            std::string token = *(response.Preset[i]->token);
            PresetInfo preset;
            preset.ptz.pan =  (panTilt->x +1.0) * 10000;
            preset.ptz.tilt = (panTilt->y +1.0) * 10000;
            preset.ptz.zoom = (zoom->x + 1.0) * 10000;
            preset.preset_id = std::atoi(token.c_str());
            if(preset.ptz.pan == 0 && preset.ptz.tilt == 0) {
                continue;
            }

            InfoL << "preset_name: "<< name << " len:"<<name.length()  << " token:" << token << " pos: "<<panTilt->x<<"."<<panTilt->y<<"."<<zoom->x;
            preset.preset_name = name;
            preset_list.push_back(preset);

        }
    }
    soap_destroy(ptz_proxy.soap);
    soap_end(ptz_proxy.soap);
    return result == SOAP_OK ? true : false;
}

bool OnvifDevice::set_preset(mediakit::PresetCmdType preset_cmd, uint32_t index) {
    return true;

    int result = SOAP_ERR;
    PTZBindingProxy ptz_proxy(ptz_service_address_.c_str());
    ptz_proxy.soap->connect_timeout = 1;
    if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(ptz_proxy.soap,
                                                     nullptr,
                                                     device_info_.user_name.c_str(),
                                                     device_info_.password.c_str())) {
        ErrorL << "GetPresets soap error" << *soap_faultcode(ptz_proxy.soap)<<" msg:"<< *soap_faultstring(ptz_proxy.soap);
        return false;
    }
    InfoL << "set preset "<< preset_cmd << " " << index;
    switch(preset_cmd) {
        case mediakit::PRESET_CMD_SET: {
                _tptz__SetPreset preset;
                _tptz__SetPresetResponse response;
                preset.ProfileToken = profile_token_;
                std::string Name = "preset_"+ std::to_string(index);
                preset.PresetName = &Name;
                std::string toke = std::to_string(index);
                preset.PresetToken = &toke;
                result = ptz_proxy.SetPreset(&preset, response);
                if (SOAP_OK != result){
                    ErrorL << "SetPreset soap error" << *soap_faultcode(ptz_proxy.soap)<<" msg:"<< *soap_faultstring(ptz_proxy.soap);
                }
            }
            break;
        case mediakit::PRESET_CMD_GOTO: {
                _tptz__GotoPreset preset;
                _tptz__GotoPresetResponse response;
                preset.ProfileToken = profile_token_;
                preset.PresetToken = std::to_string(index);
                preset.Speed = soap_new_tt__PTZSpeed(ptz_proxy.soap, -1);
                preset.Speed->PanTilt = soap_new_tt__Vector2D(ptz_proxy.soap, -1);
                preset.Speed->PanTilt->space =NULL;
                preset.Speed->Zoom = soap_new_tt__Vector1D(ptz_proxy.soap, -1);
                result = ptz_proxy.GotoPreset(&preset, response);
                if (SOAP_OK != result){
                    ErrorL << "GotoPreset soap error" << *soap_faultcode(ptz_proxy.soap)<<" msg:"<< *soap_faultstring(ptz_proxy.soap);
                }
            }
            break;
        case mediakit::PRESET_CMD_DEL: {
                _tptz__RemovePreset preset;
                _tptz__RemovePresetResponse response;
                preset.ProfileToken = profile_token_;
                preset.PresetToken = std::to_string(index);
                result = ptz_proxy.RemovePreset(&preset, response);
                if (SOAP_OK != result){
                    ErrorL << "RemovePreset soap error" << *soap_faultcode(ptz_proxy.soap)<<" msg:"<< *soap_faultstring(ptz_proxy.soap);
                }
            }
            break;
        default:
            ErrorL << "invalid preset cmd";
    }

    soap_destroy(ptz_proxy.soap);
    soap_end(ptz_proxy.soap);
    return result == SOAP_OK ? true : false;
}

bool OnvifDevice::ptz_control(mediakit::PTZCmdType ptz_cmd, uint8_t speed)
{
    int32_t speed_x = 0;
    int32_t speed_y = 0;
    int32_t speed_z = 0;

    PTZBindingProxy ptz_proxy(ptz_service_address_.c_str());
    ptz_proxy.soap->connect_timeout = 1;
    if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(ptz_proxy.soap,
                                                     nullptr,
                                                     device_info_.user_name.c_str(),
                                                     device_info_.password.c_str())) {
        ErrorL << "token digest failed";
        return false;
    }

    //proto speed  [0,9], onvif [-1,1],0.1测试下来无效,调整速度[-1,-0.2], [0.2,1]
    speed += 2;
    speed = (speed > 10) ? 10 : speed;

    InfoL << "ptz_control " << ptz_cmd << " "<< speed;
    switch (ptz_cmd) {
        case mediakit::PTZ_CMD_RIGHT:
            speed_x = speed;
            break;
        case mediakit::PTZ_CMD_LEFT:
            speed_x = -speed;
            break;
        case mediakit::PTZ_CMD_UP:
            speed_y = speed;
            break;
        case mediakit::PTZ_CMD_DOWN:
            speed_y = -speed;
            break;
        case mediakit::PTZ_CMD_LEFT_UP:
            speed_x = -speed;
            speed_y = speed;
            break;
        case mediakit::PTZ_CMD_LEFT_DOWN:
            speed_x = -speed;
            speed_y = -speed;
            break;
        case mediakit::PTZ_CMD_RIGHT_UP:
            speed_x = speed;
            speed_y = speed;
            break;
        case mediakit::PTZ_CMD_RIGHT_DOWN:
            speed_x = speed;
            speed_y = -speed;
            break;
        case mediakit::PTZ_CMD_ZOOM_IN:
            speed_z = speed;
            break;
        case mediakit::PTZ_CMD_ZOOM_OUT:
            speed_z = -speed;
            break;
        case mediakit::PTZ_CMD_STOP:
        default:{
                struct _tptz__Stop req;
                struct _tptz__StopResponse resp;
                req.ProfileToken = profile_token_;
                auto ret = ptz_proxy.Stop(&req, resp);
                if (ret != SOAP_OK) {
                    ErrorL << "Stop soap error" << *soap_faultcode(ptz_proxy.soap)<<" msg:"<< *soap_faultstring(ptz_proxy.soap);
                }
            }
            soap_destroy(ptz_proxy.soap);
            soap_end(ptz_proxy.soap);
            return true;
    }

    struct _tptz__ContinuousMove continuousMove;
    struct _tptz__ContinuousMoveResponse continuousMoveresponse;

    continuousMove.ProfileToken = profile_token_;
 
    //pantilt speed ｜x,y｜ <= 1,设置0.1,调用成功但没有移动效果
    struct tt__Vector2D panTilt;
    panTilt.x = (float)speed_x / 10;
    panTilt.y = (float) speed_y / 10;
    panTilt.space = NULL;

    struct tt__Vector1D zoom;
    zoom.x = (float)speed_z / 10;
    zoom.space = NULL;

    struct tt__PTZSpeed velocity;
    velocity.Zoom = &zoom;
    velocity.PanTilt = &panTilt;

    continuousMove.Velocity = &velocity;
    InfoL << "ptz_control speed "<< continuousMove.Velocity->PanTilt->x << ","<< continuousMove.Velocity->PanTilt->y << "," << continuousMove.Velocity->Zoom->x;
    auto ret = ptz_proxy.ContinuousMove(&continuousMove, continuousMoveresponse);
    if(ret != SOAP_OK) {
        ErrorL << "ContinuousMove soap error" << *soap_faultcode(ptz_proxy.soap)<<" msg:"<< *soap_faultstring(ptz_proxy.soap);
    }

    soap_destroy(ptz_proxy.soap);
    soap_end(ptz_proxy.soap);

    return SOAP_OK == ret ? true : false;
}