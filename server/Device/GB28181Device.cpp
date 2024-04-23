#include "Device/GB28181Device.h"

#include "Device/HKDevice.h"
#include "Device/OnvifDevice.h"

GB28181Device::GB28181Device(const DeviceInfo& info): IDevice(info) {
}

GB28181Device::~GB28181Device() {
}

bool GB28181Device::init() {
    IDevice::init();

    if(device_info_.ptz.enabled) {
        if(device_info_.ptz.protocol_type == PTZControlType::PTZ_CONTROL_TYPE_ONVIF) {
            dev_ = std::make_shared<OnvifDevice>(device_info_);
        } else if (device_info_.ptz.protocol_type == PTZControlType::PTZ_CONTROL_TYPE_SDK) {
            dev_ = std::make_shared<HKDevice>(device_info_);
        } else {
            return false;
        }
        bool ret = dev_->init();
        if(!ret) {
            return false;
        }
    }

    return true;
}

bool GB28181Device::drag_zoom_in(const Rect& rect) {
    return (dev_ != nullptr) && dev_->drag_zoom_in(rect);
}

bool GB28181Device::drag_zoom_out(const Rect& rect) {
    return (dev_ != nullptr) && dev_->drag_zoom_out(rect);
}

bool GB28181Device::get_position(PTZ& position) {
    return (dev_ != nullptr) && dev_->get_position(position);
}

bool GB28181Device::goto_position(const PTZ& position) {
    return (dev_ != nullptr) && dev_->goto_position(position);
}

bool GB28181Device::get_preset(std::vector<PresetInfo>& preset_list) {
    return (dev_ != nullptr) && dev_->get_preset(preset_list); 
}

bool GB28181Device::set_preset(mediakit::PresetCmdType preset_cmd, uint32_t index) {
    return (dev_ != nullptr) && dev_->set_preset(preset_cmd, index);
}

bool GB28181Device::ptz_control(mediakit::PTZCmdType ptz_cmd, uint8_t speed) {
    return (dev_ != nullptr) && dev_->ptz_control(ptz_cmd, speed);
}

bool GB28181Device::capture_picture(const std::uint64_t time_stamp, PictureInfo& pic_info) {
    return (dev_ != nullptr) && dev_->capture_picture(time_stamp, pic_info);
}