#include "RPC/RPCServer.h"

#include <chrono>

#include "grpcpp/grpcpp.h"
#include "Device/IDevice.h"
#include "Device/DeviceManager.h"
#include "RPC/FlowRPCClient.h"
#include "Device/OnvifDevice.h"
#include "Record/RecordUploader.h"
#include "Config.h"
#include "Rtp/RtpSelector.h"
#include "Util/util.h"
#include "ServiceManager.h"

using namespace mediakit;

RPCServer& RPCServer::Instance() {
    static std::shared_ptr<RPCServer> s_instance(new RPCServer());
    static RPCServer &s_insteanc_ref = *s_instance;
    return s_insteanc_ref;
}

bool RPCServer::init() {
    std::unique_ptr<grpc::ServerBuilder> builder = std::make_unique<grpc::ServerBuilder>();
    std::string addr_uri = "0.0.0.0:" + std::to_string(ConfigInfo.grpc.port);
    builder->AddListeningPort(addr_uri, grpc::InsecureServerCredentials());
    builder->RegisterService(this);
    server_ = builder->BuildAndStart();
    InfoL << "start grpc server " << addr_uri;
    server_->Wait();
    return true;
}

void RPCServer::set_enabled(bool enable) {
    server_enabled_ = enable;
}

grpc::Status RPCServer::GetPTZInfo(grpc::ServerContext* context,
                                    const vmr_proto::GetPTZInfoRequest* request,
                                      vmr_proto::GetPTZInfoResponse* response) {
    InfoL << "device_id=" << request->device_id();
    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    PTZ position;
    bool b_ret = device->get_position(position);
    if(!b_ret) {
        ErrorL << "get_position failed device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "get_position failed!");
    }
    
    InfoL << "GetPTZInfo success device_id=" << request->device_id()
          << ",pan=" << position.pan
          << ",tilt=" << position.tilt
          << ",zoom=" << position.zoom;

    vmr_proto::PTZ *cur_ptz = response->mutable_current_pos();
    cur_ptz->set_pan(position.pan);
    cur_ptz->set_tilt(position.tilt);
    cur_ptz->set_zoom(position.zoom);
    
    return grpc::Status::OK;
}

grpc::Status RPCServer::DragZoomIn(grpc::ServerContext* context,
                                      const ::vmr_proto::DragZoomRequest* request,
                                      google::protobuf::Empty* response) {
    InfoL << "device_id=" << request->device_id() 
          << " " << request->rect().xtop() << " "<< request->rect().ytop()
          << " " << request->rect().xbottom() <<" "<<request->rect().ybottom();

    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }

    if(request->rect().xtop() > request->rect().xbottom()) {
        ErrorL << "DragZoomIn xtop max than xbottom";
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "DragZoomIn xtop max than xbottom");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }
    
    Rect rect_info;
    rect_info.xTop = request->rect().xtop();
    rect_info.yTop = request->rect().ytop();
    rect_info.xBottom = request->rect().xbottom();
    rect_info.yBottom = request->rect().ybottom();
    bool b_ret = device->drag_zoom_in(rect_info);
    if(!b_ret) {
        ErrorL << "DragZoomIn failed device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "DragZoomIn failed");
    }

    return grpc::Status::OK;
}

grpc::Status RPCServer::DragZoomOut(grpc::ServerContext* context,
                                       const ::vmr_proto::DragZoomRequest* request,
                                       google::protobuf::Empty* response) {
    InfoL << "device_id=" << request->device_id() 
          << " " << request->rect().xtop() << " "<< request->rect().ytop()
          << " " << request->rect().xbottom() <<" "<<request->rect().ybottom();
    
    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }

    if(request->rect().xtop() >= request->rect().xbottom()) {
        ErrorL << "DragZoomOut xtop max than xbottom";
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "DragZoomOut xtop max than xbottom");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    Rect rect_info;
    rect_info.xTop = request->rect().xtop();
    rect_info.yTop = request->rect().ytop();
    rect_info.xBottom = request->rect().xbottom();
    rect_info.yBottom = request->rect().ybottom();
    bool b_ret = device->drag_zoom_in(rect_info);
    if(!b_ret) {
        ErrorL << "DragZoomOut failed device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "DragZoomOut failed device_id=" + request->device_id());
    }

    return grpc::Status::OK;
}

grpc::Status RPCServer::GotoPosition(::grpc::ServerContext* context,
                                         const ::vmr_proto::GotoPositionRequest* request,
                                         google::protobuf::Empty* response) {
    InfoL << "device_id=" << request->device_id()
          << "," << request->position().pan()
          << "," << request->position().tilt()
          << "," << request->position().zoom();

    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }

    if(request->position().pan() > 3600) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "pan > 3600");
    }
    if(request->position().tilt() > 900) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "tilt > 900");
    }
    if(request->position().zoom() < 10) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "zoom < 10");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    PTZ position{request->position().pan(), request->position().tilt(), request->position().zoom()};
    bool b_ret = device->goto_position(position);
    if(!b_ret) {
        ErrorL << "GotoPosition failed device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "GotoPosition failed");
    }

    return grpc::Status::OK;
}

grpc::Status RPCServer::GetPreset(grpc::ServerContext* context,
                                const vmr_proto::GetPresetRequest* request,
                                vmr_proto::GetPresetResponse* response) {
    InfoL << "device_id=" << request->device_id();
    
    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    std::vector<PresetInfo> preset_list;
    bool b_ret = device->get_preset(preset_list);
    if(!b_ret) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "get preset failed");
    }
    for(auto& it : preset_list) {
        auto preset = response->add_preset_info();
        preset->set_number(it.preset_id);
        preset->set_name(it.preset_name);
        vmr_proto::PTZ* ptz = preset->mutable_pos();
        ptz->set_pan(it.ptz.pan);
        ptz->set_tilt(it.ptz.tilt);
        ptz->set_zoom(it.ptz.zoom);
    }
    return grpc::Status::OK;
}

grpc::Status RPCServer::SetPreset(grpc::ServerContext* context,
                                const vmr_proto::SetPresetRequest* request,
                                google::protobuf::Empty* response) {
    InfoL << "SetPreset device_id=" << request->device_id() << " cmd:" << request->preset_cmd_type() << " index" << request->index();
    
    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }
    
    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    mediakit::PresetCmdType preset_cmd_type = mediakit::PRESET_CMD_SET;

    switch (request->preset_cmd_type()) {
    case vmr_proto::PRESET_CMD_SET:
        preset_cmd_type = mediakit::PRESET_CMD_SET;
        break;
    case vmr_proto::PRESET_CMD_GOTO:
        preset_cmd_type = mediakit::PRESET_CMD_GOTO;
        break;
    case vmr_proto::PRESET_CMD_DEL:
        preset_cmd_type = mediakit::PRESET_CMD_DEL;
        break;
    default:
        return grpc::Status(grpc::StatusCode::UNKNOWN, "cmd not support");;
    }

    device->set_preset(preset_cmd_type, request->index());
    return grpc::Status::OK;
}

grpc::Status RPCServer::PTZControl(grpc::ServerContext* context,
                                const vmr_proto::PTZControlRequest* request,
                                google::protobuf::Empty* response) {
    InfoL << "PTZControl device_id=" << request->device_id() << " cmd:" << request->ptz_cmd_type() << " speed" << request->speed();

    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    mediakit::PTZCmdType ptz_cmd_type = mediakit::PTZ_CMD_STOP;
    switch (request->ptz_cmd_type()) {
        case vmr_proto::PTZ_CMD_RIGHT:
            ptz_cmd_type = mediakit::PTZ_CMD_RIGHT;
            break;
        case vmr_proto::PTZ_CMD_LEFT:
            ptz_cmd_type = mediakit::PTZ_CMD_LEFT;
            break;
        case vmr_proto::PTZ_CMD_UP:
            ptz_cmd_type = mediakit::PTZ_CMD_UP;
            break;
        case vmr_proto::PTZ_CMD_DOWN:
            ptz_cmd_type = mediakit::PTZ_CMD_DOWN;
            break;
        case vmr_proto::PTZ_CMD_LEFT_UP:
            ptz_cmd_type = mediakit::PTZ_CMD_LEFT_UP;
            break;
        case vmr_proto::PTZ_CMD_LEFT_DOWN:
            ptz_cmd_type = mediakit::PTZ_CMD_LEFT_DOWN;
            break;
        case vmr_proto::PTZ_CMD_RIGHT_UP:
            ptz_cmd_type = mediakit::PTZ_CMD_RIGHT_UP;
            break;
        case vmr_proto::PTZ_CMD_RIGHT_DOWN:
            ptz_cmd_type = mediakit::PTZ_CMD_RIGHT_DOWN;
            break;
        case vmr_proto::PTZ_CMD_ZOOM_IN:
            ptz_cmd_type = mediakit::PTZ_CMD_ZOOM_IN;
            break;
        case vmr_proto::PTZ_CMD_ZOOM_OUT:
            ptz_cmd_type = mediakit::PTZ_CMD_ZOOM_OUT;
            break;
        case vmr_proto::PTZ_CMD_STOP:
        default:
            ptz_cmd_type = mediakit::PTZ_CMD_STOP;
            break;
    }

    device->ptz_control(ptz_cmd_type, request->speed());
    
    return grpc::Status::OK;
}

grpc::Status RPCServer::UploadRecord(grpc::ServerContext* context,
                                          const vmr_proto::UploadRecordRequest* request,
                                          vmr_proto::UploadRecordResponse* response) {
    InfoL << "device_id=" << request->device_id() 
          << ",bucket=" << request->bucket_name()
          << ",key=" << request->s3_object_key()
          << ",start_time=" << request->start_time()
          << ",end_time=" << request->end_time();
    
    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }
    
    if(request->start_time() <= 0 || request->end_time() <= 0) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "UploadRecord param error:time <= 0");
    }
              
    if(request->end_time() - request->start_time() <= 3) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "UploadRecord param error:time s >= e");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    std::uint64_t start_time = request->start_time();
    std::uint64_t end_time = request->end_time();
    std::list<mediakit::Frame::Ptr> frame_list;
    bool b_ret = device->download_record(start_time * 1000, end_time * 1000, frame_list);
    if(!b_ret || frame_list.empty()) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "download_record failed");
    }

    std::string url;

    std::chrono::time_point<std::chrono::steady_clock> upload_start_Time = std::chrono::steady_clock::now();
    int ret = RecordUploader::Instance().upload(frame_list,
                                                request->bucket_name(),
                                                request->s3_object_key(),
                                                url);
    std::chrono::time_point<std::chrono::steady_clock> upload_end_Time = std::chrono::steady_clock::now();     
    if(ret != 0) {
        ErrorL << request->device_id() << ",code=" << ret;
        return grpc::Status(grpc::StatusCode::UNKNOWN, std::to_string(ret));
    } 
    InfoL << request->device_id() 
          << ",url=" << url
          << ",time:" << std::chrono::duration_cast<std::chrono::milliseconds>(upload_end_Time - upload_start_Time).count() << "ms";
    
    response->set_url(url);

    return grpc::Status::OK;
}

grpc::Status RPCServer::UpdateStream(grpc::ServerContext* context,
                               const vmr_proto::UpdateStreamRequest* request,
                               google::protobuf::Empty* response) {
    InfoL << "device_id=" << request->device_id()
          << ",record_time_quota=" << request->record_time_quota();
    
    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    device->set_time_quota(request->record_time_quota() * 1000);

    InfoL << request->device_id() << ",UpdateStream success record_time_quota=" << request->record_time_quota();

    return grpc::Status::OK;
}

grpc::Status RPCServer::CapturePicture(grpc::ServerContext* context,
                                            const ::vmr_proto::CapturePictureRequest* request,
                                            vmr_proto::CapturePictureResponse* response) {
    InfoL << "device_id=" << request->device_id() << "," << request->time_stamp();
    
    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }
    
    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device == nullptr) {
        ErrorL << "not found device_id=" << request->device_id();
        return grpc::Status(grpc::StatusCode::UNKNOWN, "not found device!");
    }

    PictureInfo pic_info;
    bool b_ret = device->capture_picture(request->time_stamp(), pic_info);
    if(!b_ret) {
        ErrorL << "device_id=" << request->device_id() << ",capture picture failed";
        return grpc::Status(grpc::StatusCode::UNKNOWN, "capture picture failed");
    }

    InfoL << "device_id=" << request->device_id() << ",picture size=" << pic_info.picture.size();

    //std::fstream picture_file("example.jpg", std::ios::out|std::ios::binary);
    //picture_file << picture;

    response->set_width(pic_info.width);
    response->set_height(pic_info.height);
    response->set_picture(pic_info.picture);
    response->set_ntp_time_stamp(pic_info.ntp_time_stamp);

    return grpc::Status::OK;
}


grpc::Status RPCServer::ActivateDevice(grpc::ServerContext* context,
                                const ::vmr_proto::ActivateDeviceRequest* request, 
                                google::protobuf::Empty* response) {
    DebugL << "device_id=" << request->device_id();   
                         
    if (!server_enabled_) {
        return grpc::Status(grpc::StatusCode::UNKNOWN, "service not enabled");
    }

    if(request->device_id().empty()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "invalid argument");
    }

    IDevice::Ptr device = DeviceManager::Instance().find_device(request->device_id());
    if(device != nullptr) {
        device->activate_device();
    } else {
        DeviceInfo dev_info;
        dev_info.is_activate_device = true;
        bool ret = FlowRPCClient::Instance().get_subdevice_info_v2(request->device_id(), dev_info);
        if(!ret) {
            return grpc::Status(grpc::StatusCode::UNKNOWN, "get_subdevice_info_v2 failed");
        }

        ret = DeviceManager::Instance().add_device(dev_info);
        if (!ret) {
            return grpc::Status(grpc::StatusCode::UNKNOWN, "add_device failed");
        }

        if(ConfigInfo.service.enabled) {
            ret = ServiceManager::Instance().check_device(request->device_id());
            if(!ret) {
                return grpc::Status(grpc::StatusCode::UNKNOWN, "add_device failed");
            }
        }
    }

    return grpc::Status::OK;
}