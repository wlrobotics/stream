#include "RPC/FlowRPCClient.h"

#include "Util/logger.h"
#include "Common/config.h"
#include "Config.h"
#include "HttpClient_curl.h"

FlowRPCClient& FlowRPCClient::Instance() {
    static std::shared_ptr<FlowRPCClient> s_instance(new FlowRPCClient());
    static FlowRPCClient &s_insteanc_ref = *s_instance;
    return s_insteanc_ref;
}

bool FlowRPCClient::init() {
    stub_ = vmr_proto::FlowService::NewStub(grpc::CreateChannel(ConfigInfo.vmr.grpc_endpoint,
                                             grpc::InsecureChannelCredentials()));

    return true;
}

bool FlowRPCClient::create_stream(DeviceInfo& dev_info, const MediaDescription sdp) {
    vmr_proto::CreateStreamRequest req;
    req.set_device_id(dev_info.device_id);
    req.set_stream_host(sdp.stream_host);  
    req.set_stream_rtp_port(sdp.stream_rtp_port);
    vmr_proto::CreateStreamResponse resp;
    grpc::ClientContext context;
    grpc::Status status = stub_->CreateStream(&context, req, &resp);
    if (!status.ok()) {
        ErrorL << "CreateStream failed! stream_id=" << dev_info.device_id 
               << ",grpc_code=" << status.error_code() 
               << ": " << status.error_message();
        return false;
    }

    InfoL << "mp4_loop_status=" << resp.mp4_loop_status() << ",mp4_loop_count=" << resp.mp4_loop_count();

    if(!resp.mp4_loop_status()) {
        WarnL << dev_info.device_id << ",mp4_loop_status false";
        return false;
    }
    dev_info.mp4_loop_count = resp.mp4_loop_count();

    switch (resp.device_type()) {
    case vmr_proto::DEVICE_TYPE_HIKVISION: {
        dev_info.dev_type = DeviceType::HKSDK;
        dev_info.channel_id = resp.channel_id();
        dev_info.host = resp.host();
        dev_info.user_name = resp.user_name();
        dev_info.password = resp.password();
        if(ConfigInfo.hksdk.enable_rtsp == 1) {
            dev_info.dev_type = DeviceType::RTSP;
            dev_info.url = "rtsp://" + dev_info.user_name + ":" + dev_info.password + "@" + dev_info.host;
        }
        break;
    }
    case vmr_proto::DEVICE_TYPE_STREAM_ADDR: {
        dev_info.url = resp.stream_url();
        if(dev_info.url.compare(0, 4, "rtsp") == 0) {
            dev_info.dev_type = DeviceType::RTSP;
            if(dev_info.ptz.enabled) {
                dev_info.dev_type = DeviceType::ONVIF;
            }
        } else if (dev_info.url.compare(0, 4, "rtmp") == 0) {
            dev_info.dev_type = DeviceType::RTMP;
        } else if (dev_info.url.compare(0, 4, "http") == 0 || dev_info.url[0] == '/') {
            dev_info.dev_type = DeviceType::HTTP_MP4;
            for(int32_t index = 0; index < resp.extra_url_list_size(); index++){
                dev_info.url_list.emplace_back(resp.extra_url_list(index));
            }
        } else {
            ErrorL << "not found protocol";
            return false;
        }  
        break;
    }
    case vmr_proto::DEVICE_TYPE_GB28181: {
        dev_info.dev_type = DeviceType::GB28181;
        break;
    }
    case vmr_proto::DEVICE_TYPE_ONVIF: {
        dev_info.dev_type = DeviceType::ONVIF;
        dev_info.channel_id = resp.channel_id();
        dev_info.host = resp.host();
        dev_info.user_name = resp.user_name();
        dev_info.password = resp.password();
        break;
    }
    }
    
    InfoL << "CreateStream success, dev_info=" 
          << dev_info.dev_type << "," 
          << dev_info.device_id << ","
          << dev_info.channel_id << ","
          << dev_info.ptz.enabled << ","
          << dev_info.host << ","
          << dev_info.user_name << ","
          << dev_info.password << ","
          << dev_info.url << ","
          << dev_info.record_time_quota;

    return true;
}

bool FlowRPCClient::delete_stream(const std::string& device_id) {
    InfoL << "delete_stream device_id=" << device_id;
    vmr_proto::DeleteStreamRequest req;
    req.set_device_id(device_id);
    google::protobuf::Empty resp;
    grpc::ClientContext context;
    grpc::Status status = stub_->DeleteStream(&context, req, &resp);
    if (!status.ok()) {
         ErrorL << "DeleteStream failed! stream_id " << device_id 
                << ",grpc_code=" << status.error_code() 
                << ": " << status.error_message();
         return false;
    }
    return true;
}

bool FlowRPCClient::retrieve_stream(std::uint32_t ssrc, std::string& device_id) {
    vmr_proto::RetrieveStreamRequest req;
    req.set_ssrc(ssrc);
    vmr_proto::RetrieveStreamResponse resp;
    grpc::ClientContext context;
    grpc::Status status = stub_->RetrieveStream(&context, req, &resp);
    if (!status.ok()) {
         ErrorL << status.error_code() << ": " << status.error_message() << ",ssrc=" << ssrc;
         return false;
    }
    device_id = resp.device_id();
    return true;
}

bool FlowRPCClient::get_subdevice_info(const std::string& device_id, mediakit::DeviceInfo& dev_info)
{
    vmr_proto::GetSubDeviceInfoRequest req;
    vmr_proto::GetSubDeviceInfoResponse resp;
    grpc::ClientContext context;
    req.set_sub_device_id(device_id);
    dev_info.dev_type = DeviceType(0);
    dev_info.ptz.protocol_type = PTZControlType(0);
    grpc::Status status = stub_->GetSubDeviceInfo(&context, req, &resp);
    if (!status.ok()) {
         ErrorL << status.error_code() << ": " << status.error_message() << ",device_id=" << device_id;
         return false;
    }

    switch (resp.device_type()) {
        case vmr_proto::DEVICE_TYPE_HIKVISION:
            dev_info.dev_type = mediakit::HKSDK;
            break;
        case vmr_proto::DEVICE_TYPE_GB28181:
            dev_info.dev_type = mediakit::GB28181;
            break;
        case vmr_proto::DEVICE_TYPE_ONVIF:
            dev_info.dev_type = mediakit::ONVIF;
            break;
        case vmr_proto::DEVICE_TYPE_STREAM_ADDR:
        default:
            break;
    };

    dev_info.device_id = device_id;
    dev_info.ptz.enabled = resp.camera_type() == vmr_proto::CameraType::CAMERA_TYPE_DOME ? true : false;
    dev_info.host = resp.ip_address();
    dev_info.user_name = resp.account();
    dev_info.password = resp.password();

    if(resp.protocol_type() == vmr_proto::DeviceProtocolType::DEVICE_PROTOCOL_TYPE_ONVIF) {
        dev_info.ptz.protocol_type = mediakit::PTZ_CONTROL_TYPE_ONVIF;
    } else {
        dev_info.ptz.protocol_type = mediakit::PTZ_CONTROL_TYPE_SDK;
    }

    if(dev_info.ptz.enabled && (dev_info.host.empty() || dev_info.user_name.empty() || dev_info.password.empty())) {
        dev_info.ptz.enabled = false;
        ErrorL << "dome ipc, but no host and user password";
    }

    if(resp.record_time_quota() < 0) {
        InfoL << dev_info.device_id << ",record_time_quota < 0, " << resp.record_time_quota();
        dev_info.record_time_quota = ConfigInfo.record.time_quota;
    } else {
        InfoL << dev_info.device_id << ",record_time_quota >= 0, " << resp.record_time_quota();
        dev_info.record_time_quota = resp.record_time_quota() * 1000;
    }

    if(ConfigInfo.record.enabled_time_quota) {
        dev_info.record_time_quota = ConfigInfo.record.time_quota;
    }

    InfoL << "get_subdevice_info success type" 
            << dev_info.dev_type 
            <<", dev="<< dev_info.device_id
            << ", chn="<< dev_info.channel_id
            << ", ptz="<< dev_info.ptz.enabled
            << ", host="<< dev_info.host
            << ", name="<< dev_info.user_name
            << ", pass="<< dev_info.password
            << ", ptz_type="<< dev_info.ptz.protocol_type
            << ", record_time_quota="<< dev_info.record_time_quota;
    return true;
}

bool FlowRPCClient::get_subdevice_info_v2(const std::string& device_id, mediakit::DeviceInfo& dev_info) {
    Infra::HttpClient http_client;
    Infra::HttpClient::HttpKeyValue headers = {
        {"X-Vmr-Token", ConfigInfo.vmr.http_token}
    };

    dev_info.device_id = device_id;

    std::string http_uri = "http://" + ConfigInfo.vmr.http_endpoint + "/v2/sub_device/" + device_id;
    std::string response;
    http_client.get(http_uri, nullptr, &headers, &response);
    
    InfoL << "get_subdevice_info_v2 response: " << response;
    
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(response, root)) {
        ErrorL << "parse response failed: " << response;
        return false;
    }

    DebugL << "get_subdevice_info_v2 response: " << root.toStyledString();

    if (root["code"].asInt() != 0) {
        return false;
    }

    Json::Value data = root["data"];
    if (data.isNull()) {
        return false;
    }

    Json::Value attribute = data["attribute"];
    if(attribute.isNull()) {
        return false;
    }

    int ptz_type = data["ptz_type"].asInt();
    if(ptz_type == 1) {
        dev_info.ptz.enabled = true;
    }

    int main_type = data["type"]["value"].asInt();
    if(main_type == 1) { //直连摄像机
        Json::Value discovery_protocol = attribute["discovery_protocol"];
        if(discovery_protocol.isNull()) {
            return false;
        }

        int value = discovery_protocol["value"].asInt();
        if(value == 1) { //ONVIF
            dev_info.dev_type = mediakit::ONVIF;
            dev_info.host = attribute["ip"].asString();
            dev_info.password = attribute["password"].asString();
            dev_info.user_name = attribute["account"].asString();
        } else if (value == 2) { //流地址
            dev_info.url = attribute["upstream_url"].asString();
            if(dev_info.url.compare(0, 4, "rtsp") == 0) {
                dev_info.dev_type = DeviceType::RTSP;
                if(dev_info.ptz.enabled) {
                    dev_info.dev_type = DeviceType::ONVIF;
                }
            } else if (dev_info.url.compare(0, 4, "rtmp") == 0) {
                dev_info.dev_type = DeviceType::RTMP;
            } else if (dev_info.url.compare(0, 4, "http") == 0 || dev_info.url[0] == '/') {
                dev_info.dev_type = DeviceType::HTTP_MP4;
            } else {
                return false;
            }
        } else if(value == 3) { //海康SDK
            dev_info.dev_type = mediakit::HKSDK;
            dev_info.host = attribute["ip"].asString();
            dev_info.password = attribute["password"].asString();
            dev_info.user_name = attribute["account"].asString();
        }
    } else if (main_type == 2) { //平台方式
        dev_info.dev_type = mediakit::GB28181;
    }

    dev_info.record_time_quota = data["capability"]["cache_time"].asInt() * 1000;

    //扩展属性字段，user_data
    if(data.isMember("user_data")) {
        std::string str_user_data = data["user_data"].asString();
        Json::Value user_data;
        if (reader.parse(str_user_data, user_data)) {
            if(user_data.isMember("protocol_type")) {
                std::string protocol_type = user_data["protocol_type"].asString();
                if(protocol_type == "SDK") {
                    dev_info.ptz.protocol_type = PTZControlType::PTZ_CONTROL_TYPE_SDK;
                } else if (protocol_type == "Onvif") {
                    dev_info.ptz.protocol_type = PTZControlType::PTZ_CONTROL_TYPE_ONVIF;
                }
            }
            if(user_data.isMember("ip_address")) {
                dev_info.host = user_data["ip_address"].asString();
            }
            if(user_data.isMember("account")) {
                dev_info.user_name = user_data["account"].asString();
            }
            if(user_data.isMember("password")) {
                dev_info.password = user_data["password"].asString();
            }
            if(user_data.isMember("manufacturer_type")) {
                std::string mtype = user_data["manufacturer_type"].asString();
                if(mtype == "HikVision") {
                    dev_info.manufacturer_type = ManufacturerType::HIKVISION;
                } else if (mtype == "Dahua") {
                    dev_info.manufacturer_type = ManufacturerType::DAHUA;
                }
            }
            if(user_data.isMember("max_elevation")) {
                dev_info.ptz.max_elevation = user_data["max_elevation"].asInt();
            }
            if(user_data.isMember("max_zoom")) {
                dev_info.ptz.max_zoom = user_data["max_zoom"].asInt();
            } else {
                dev_info.ptz.max_zoom = ConfigInfo.ptz.zoom_max;
            }
        }
    }

    InfoL << "get_subdevice_info_v2 success type"
        << dev_info.dev_type
        <<", dev="<< dev_info.device_id
        << ", chn="<< dev_info.channel_id
        << ", ptz="<< dev_info.ptz.enabled
        << ", host="<< dev_info.host
        << ", name="<< dev_info.user_name
        << ", pass="<< dev_info.password
        << ", ptz_type="<< dev_info.ptz.protocol_type
        << ", record_time_quota="<< dev_info.record_time_quota;

    return true;
}