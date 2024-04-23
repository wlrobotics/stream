#pragma once

#include <mutex>
#include <unordered_map>
#include <string>
#include <queue>

#include "Device/IDevice.h"
#include "Rtp/RtpServer.h"

namespace mediakit {
class DeviceManager {
public:
    DeviceManager() = default;
    ~DeviceManager() = default;
    static DeviceManager& Instance();
    
    bool init();
    
    bool add_device(const DeviceInfo& dev_info, bool is_gb28181 = false);
    void remove_device(const std::string& device_id);
    IDevice::Ptr find_device(const std::string& device_id);
    void for_each_device(const std::function<void(const IDevice::Ptr &device)> &cb);

    bool open_rtp_server(MediaDescription& sdp, const std::string& device_id);
    void close_rtp_server(const std::string& device_id);

    /*
    与find_device + device->get_device_info相比，
    DeviceManager::get_device_info能确保device对象不会暴露到外部，从而避免device对象的生命周期被延长
    */
    bool get_device_info(const std::string& device_id, DeviceInfo& info);

private:
    void check_device_status();
    
    std::mutex device_map_mtx_;
    std::unordered_map<std::string, IDevice::Ptr> device_map_;

    RtpServer::Ptr rtp_server_;
    std::recursive_mutex rtp_server_map_mtx_;
    std::queue<int> rtp_port_pool_;
    std::unordered_map<std::string, RtpServer::Ptr> rtp_server_map_;
};

}
