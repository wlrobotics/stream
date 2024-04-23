#include "Device/DeviceManager.h"

#include "Device/DeviceFactory.h"
#include "Config.h"
namespace mediakit {

DeviceManager& DeviceManager::Instance() {
    static std::shared_ptr<DeviceManager> s_instance(new DeviceManager());
    static DeviceManager &s_insteanc_ref = *s_instance;
    return s_insteanc_ref;
}

bool DeviceManager::init() {
    for(int i = ConfigInfo.rtp.start_port; i < ConfigInfo.rtp.end_port; i += 2) {
        rtp_port_pool_.push(i);
    }
    check_device_status();
    return true;
}

bool DeviceManager::add_device(const DeviceInfo& dev_info, bool is_gb28181) {
    std::lock_guard<std::mutex> lck(device_map_mtx_);
    auto it = device_map_.find(dev_info.device_id);
    if (it != device_map_.end()) {
        return true;  //如果设备已存在则返回true，增加播放成功率；
    }

    if(device_map_.size() >= ConfigInfo.service.in_stream_quota) {
        return false;
    }

    IDevice::Ptr device = DeviceFactory::create(dev_info);
    if(device == nullptr) {
        return false;
    }

    bool ret = device->init();
    if(!ret) {
        return false;
    }

    if(!is_gb28181) {
        ret = device->media_open();
        if(!ret) {
            return false;
        }
    }
    
    device_map_[dev_info.device_id] = device;
    return true;
}

void DeviceManager::remove_device(const std::string& device_id) {
    std::lock_guard<std::mutex> lck(device_map_mtx_);
    auto it = device_map_.find(device_id);
    if (it != device_map_.end()) {
        device_map_.erase(it);
    }
}

IDevice::Ptr DeviceManager::find_device(const std::string& device_id) {
    std::lock_guard<std::mutex> lck(device_map_mtx_);
    auto it = device_map_.find(device_id);
    if (it != device_map_.end()) {
        return it->second;
    }
    return nullptr;
}

void DeviceManager::for_each_device(const std::function<void(const IDevice::Ptr &device)> &cb) {
    std::lock_guard<std::mutex> lck(device_map_mtx_);
    for(const auto& p : device_map_) {
        cb(p.second);
    }              
}

bool DeviceManager::open_rtp_server(MediaDescription& sdp, const std::string& device_id) {
    if(!ConfigInfo.rtp.enabled_multi_port) {
        sdp.stream_rtp_port = ConfigInfo.rtp.start_port;
        if(rtp_server_ == nullptr) {
            rtp_server_ = std::make_shared<RtpServer>();
            rtp_server_->start(ConfigInfo.rtp.start_port, ConfigInfo.rtp.start_port + 1);
        }
        return true;
    }
    
    std::lock_guard<std::recursive_mutex> lck(rtp_server_map_mtx_);
    auto it = rtp_server_map_.find(device_id);
    if (it != rtp_server_map_.end()) {
        WarnL << "rtp_server exist " << device_id;
        return false;
    }

    if(rtp_port_pool_.empty()) {
        WarnL << "rtp_port_pool_ empty";
        return false;
    }

    sdp.stream_rtp_port = rtp_port_pool_.front();
    rtp_port_pool_.pop();
    
    RtpServer::Ptr rtp_server = std::make_shared<RtpServer>();
    rtp_server->start(sdp.stream_rtp_port, sdp.stream_rtp_port + 1, device_id);
    rtp_server->setOnDetach([this, device_id](){
        close_rtp_server(device_id);
    });
    rtp_server_map_[device_id] = rtp_server;
    
    return true;
}


void DeviceManager::close_rtp_server(const std::string& device_id) {
    if(!ConfigInfo.rtp.enabled_multi_port) {
        return ;
    }

    std::lock_guard<std::recursive_mutex> lck(rtp_server_map_mtx_);
    auto it = rtp_server_map_.find(device_id);
    if (it == rtp_server_map_.end()) {
        return ;
    }

    rtp_port_pool_.push(it->second->get_rtp_port());
    rtp_server_map_.erase(it);
}

bool DeviceManager::get_device_info(const std::string& device_id, DeviceInfo& info) {
    std::lock_guard<std::mutex> lck(device_map_mtx_);
    auto it = device_map_.find(device_id);
    if (it != device_map_.end()) {
        info = it->second->get_device_info();
        return true;
    }
    return false;
}

void DeviceManager::check_device_status() {
    std::thread([this](){
        while(true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::list<IDevice::Ptr> delete_device_list;
            {
                std::lock_guard<std::mutex> lck(device_map_mtx_);
                for (auto iter = device_map_.begin(); iter != device_map_.end();) {
                    if(iter->second->alive()) {
                        ++iter;
                    } else {
                        InfoL << "device offline " << iter->second->get_device_id();
                        delete_device_list.emplace_back(iter->second);
                        iter = device_map_.erase(iter);
                    }
                }
            }
        }
    }).detach();
}

}
