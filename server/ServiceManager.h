#pragma once

#include <mutex>

#include "RPC/EtcdRPCClient.h"

class ServiceManager {
public:
    struct ServiceInfo {   
        std::string extra_host;
        std::string intra_host;
        int rtsp_port;
        int http_port;
        int grpc_port;
    };
    
    ServiceManager() = default;
    ~ServiceManager() = default;
    static ServiceManager& Instance();

    bool init();

    bool keep_alive_device(const std::string& device_id, std::int64_t lease_id = 0);

    bool check_device(std::string stream_id);
    
private:
    void init_service_info();
    bool register_service();
    bool keep_alive_service();
    void set_enabled_service(bool enabled);

    void register_service_thread();
    void register_device_thread();

    std::int64_t service_lease_id_;
    std::string service_info_;

    std::unique_ptr<EtcdRPCClient> etcd_client_;

    std::mutex device_map_mtx_;
    std::unordered_map<std::string, std::int64_t> device_map_;
};