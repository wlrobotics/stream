#include "ServiceManager.h"

#include <thread>
#include <ios>

#include "Config.h"
#include "Common/MediaSource.h"
#include "RPC/RPCServer.h"
#include "HookServer.h"

ServiceManager& ServiceManager::Instance() {
    static std::shared_ptr<ServiceManager> s_instance(new ServiceManager());
    static ServiceManager &s_insteanc_ref = *s_instance;
    return s_insteanc_ref;
}

bool ServiceManager::init() {
    //确保旧的stream和device都已经到期
    std::this_thread::sleep_for(std::chrono::seconds(ConfigInfo.service.service_ttl + ConfigInfo.service.device_ttl + 3));

    init_service_info();

    etcd_client_ = std::make_unique<EtcdRPCClient>();
    etcd_client_->init();

    std::thread(&ServiceManager::register_service_thread, this).detach();
    std::thread(&ServiceManager::register_device_thread, this).detach();
    
    return true;
}

void ServiceManager::init_service_info() {
    Json::Value json_service_info;
    json_service_info["extra_host"] = ConfigInfo.network.extra_host;
    json_service_info["intra_host"] = ConfigInfo.network.intra_host;
    json_service_info["rtsp_port"] = ConfigInfo.rtsp.port;
    json_service_info["grpc_port"] = ConfigInfo.grpc.port;
    json_service_info["http_port"] = ConfigInfo.http.port;

    Json::StreamWriterBuilder builder;
    builder.settings_["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream os;
    writer->write(json_service_info, &os);
    service_info_ = os.str();
}

bool ServiceManager::keep_alive_service() {
    return etcd_client_->lease_keep_alive(service_lease_id_);
}

bool ServiceManager::keep_alive_device(const std::string& device_id, std::int64_t lease_id) {
    if(lease_id != 0) {
        device_map_[device_id] = lease_id;
    }
    return etcd_client_->lease_keep_alive(device_map_[device_id]);
}

bool ServiceManager::register_service() {
    std::int64_t lease_id = etcd_client_->lease_grant(ConfigInfo.service.service_ttl);
    if(lease_id < 0) {
        return false;
    }
    service_lease_id_ = lease_id;

    std::stringstream key;
    key << ConfigInfo.service.etcd_root_path << std::hex << service_lease_id_;

    bool ret = etcd_client_->put(key.str(), service_info_, service_lease_id_);
    if(!ret) {
        return false;
    }

    InfoL << "service register success,k=" << key.str() << ",v=" << service_info_;
    return true;
}


bool ServiceManager::check_device(std::string stream_id) {
    std::string key = "/stream_cluster/device/" + stream_id;
    std::string value;
    bool ret = etcd_client_->get(key, value);
    if (!ret) {
        ErrorL << "check_device failed,stream_id=" << stream_id << ",key=" << key;
        return ret;
    }

    Json::Reader reader;
    Json::Value json_value;
    if (!reader.parse(value, json_value)) {
        return false;
    }

    std::int64_t stream_id_in_etcd = std::stoll(json_value["stream_id"].asString().c_str(), nullptr, 16);
    if(service_lease_id_ != stream_id_in_etcd) {
        ErrorL << "check_device failed,stream_id=" << stream_id << ",stream_id_in_etcd=" << stream_id_in_etcd;
        return false;
    }

    std::int64_t token_in_etcd = std::stoll(json_value["token"].asString().c_str(), nullptr, 16);

    InfoL << "check_device success,stream_id=" << stream_id << ",token=" << token_in_etcd;

    return keep_alive_device(stream_id, token_in_etcd);
}

void ServiceManager::set_enabled_service(bool enabled) {
    RPCServer::Instance().set_enabled(enabled);
    HookServer::Instance().set_enabled(enabled);
    
    if(!enabled) {
        mediakit::MediaSource::for_each_media([this](const MediaSource::Ptr &media) {
            media->close(true);
        });
    }
}

void ServiceManager::register_service_thread() {
    bool register_status = false;
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));

        register_status = (register_status || register_service()) ;
        if(!register_status) {
            ErrorL << "register_service failed!";
            continue;
        }

        set_enabled_service(true);

        register_status = keep_alive_service();
        if(!register_status) {
            ErrorL << "keep_alive_service failed!";
            set_enabled_service(false);
        }
    }
}

void ServiceManager::register_device_thread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));

        mediakit::MediaSource::for_each_media([this](const MediaSource::Ptr &media) {
            if(media->getSchema() == "rtsp" && media->getApp() == LIVE_APP) {
                if(!keep_alive_device(media->getId())) {
                    media->close(true);
                }
            }
        });
    }
}