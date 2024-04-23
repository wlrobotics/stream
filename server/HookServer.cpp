#include "HookServer.h"

#include "Util/logger.h"
#include "StreamStat.h"
#include "Device/DeviceManager.h"
#include "Config.h"
#include "ServiceManager.h"

using namespace mediakit;
using namespace toolkit;


HookServer& HookServer::Instance() {
    static std::shared_ptr<HookServer> s_instance(new HookServer());
    static HookServer &s_insteanc_ref = *s_instance;
    return s_insteanc_ref;
}

bool HookServer::init() {
    return true;
}

void HookServer::set_enabled(bool enable) {
    server_enabled_ = enable;
}

Status HookServer::not_found_stream(const MediaInfo &args) {
    std::thread::id tid = std::this_thread::get_id();
    std::string stream_id = args._streamid;
    InfoL << "stream_id=" << stream_id;
    if(args._app != LIVE_APP || stream_id.empty()) {
        WarnL << "stream invalid:" << stream_id;
        return ERROR_STREAM_ID_INVALID;
    }

    if (!server_enabled_) {
        return ERROR_NOT_ENABLED;
    }

    InfoL << std::hex << tid << "," << args._streamid  << ",service enabled";
    bool ret = false;

    if(ConfigInfo.service.enabled) {
        ret = ServiceManager::Instance().check_device(stream_id);
        if(!ret) {
            return ERROR_ETCD_FAILED;
        }

        InfoL << std::hex << tid << "," << args._streamid  << ",keep alive success";
    }

    DeviceInfo dev_info;
    dev_info.device_id = stream_id;
    if(ConfigInfo.vmr.enable_get_subdevice_info_v2) {
        ret = FlowRPCClient::Instance().get_subdevice_info_v2(stream_id, dev_info);
    } else {
        ret = FlowRPCClient::Instance().get_subdevice_info(stream_id, dev_info);
    }
    if(!ret) {
        return ERROR_FLOW_FAILED;
    }
    InfoL << std::hex << tid << "," << args._streamid  << ",get_subdevice_info success," << (dev_info.dev_type == DeviceType::GB28181);

    if(dev_info.dev_type == DeviceType::GB28181) {
        ret = DeviceManager::Instance().add_device(dev_info, true);
        if (!ret) {
            return ERROR_DEVICE_FAILED;
        }
        InfoL << std::hex << tid << "," << args._streamid  << ",gb28181 add_device success";
        
        MediaDescription sdp;
        sdp.stream_host = ConfigInfo.network.extra_host;
        ret = DeviceManager::Instance().open_rtp_server(sdp, dev_info.device_id);
        if(!ret) {
            return ERROR_RTP_FAILED;
        }
        InfoL << std::hex << tid << "," << args._streamid  << ",gb28181 open_rtp_server success, sdp.stream_rtp_port=" << sdp.stream_rtp_port;

        ret = FlowRPCClient::Instance().create_stream(dev_info, sdp);
        if (!ret) {
            return ERROR_FLOW_FAILED;
        }
        InfoL << std::hex << tid << "," << args._streamid  << ",gb28181 create_stream success";
    } else {
        ret = FlowRPCClient::Instance().create_stream(dev_info);
        if (!ret) {
            return ERROR_FLOW_FAILED;
        }
        InfoL << std::hex << tid << "," << args._streamid  << ",create_stream success";

        ret = DeviceManager::Instance().add_device(dev_info);
        if (!ret) {
            return ERROR_DEVICE_FAILED;
        }
        InfoL << std::hex << tid << "," << args._streamid  << ",add_device success";
    }

    InfoL << std::hex << tid << "," << args._streamid  << ",end";

    return SUCCESS;
}

bool HookServer::none_stream_reader(MediaSource &sender) {
    std::string stream_id = sender.getId();
    InfoL << "stream_id=" << stream_id;
    if (sender.getApp() != LIVE_APP || stream_id.empty()) {
        WarnL << "stream invalid:" << stream_id;
        return false;
    }

    DeviceInfo dev_info;

    {
        IDevice::Ptr device = DeviceManager::Instance().find_device(stream_id);
        if(device == nullptr) {
            sender.close(false);
            return false;
        }

        dev_info = device->get_device_info();
        if(dev_info.is_activate_device) {
            if(device->is_activate()) {
                return true;
            }
        }
    }

    sender.close(false);

    FlowRPCClient::Instance().delete_stream(stream_id);

    if(dev_info.dev_type == DeviceType::GB28181) {
        DeviceManager::Instance().close_rtp_server(stream_id);
    }
    
    DeviceManager::Instance().remove_device(stream_id);
    return false;
}

bool HookServer::retrieve_stream(std::uint32_t ssrc, std::string& stream_id) {
    if (!server_enabled_) {
        return false;
    }
    InfoL << "RetrieveStream ssrc=" << ssrc;
    return FlowRPCClient::Instance().retrieve_stream(ssrc, stream_id);
}

std::tuple<std::string, HttpSession::KeyValue, std::string>
HookServer::http_request(const Parser &parser) {
    //InfoL << "http request url=" << parser.Url();
    if (parser.Url() == "/stat") {
        HttpSession::KeyValue header;
        header["Content-Type"] = "text/xml";
        return {"200 OK", header, StreamStat::stat()};
    } else if (parser.Url() == "/stat.xsl") {
        HttpSession::KeyValue header;
        header["Content-Type"] = "text/plain";
        return {"200 OK", header, StreamStat::stat_template()};
    } else if (parser.Url() == "/healthcheck/livez") {
        return {"200 OK", HttpSession::KeyValue(), "200 OK"};
    }

    return {"404 Not Found", HttpSession::KeyValue(), "404 Not Found"};
}
