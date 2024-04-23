#pragma once

#include <string>
#include <memory>

#include "Device/IDevice.h"
#include "Device/RTSPDevice.h"

class OnvifDevice final: public RTSPDevice  {
public:
    using Ptr = std::shared_ptr<OnvifDevice>;
    OnvifDevice(const DeviceInfo& info);
    ~OnvifDevice();
    bool init() override;
    bool media_open() override;
    bool get_position(mediakit::PTZ& position) override;

    bool get_preset(std::vector<PresetInfo>& preset_list) override;
    bool set_preset(mediakit::PresetCmdType preset_cmd, uint32_t index) override;
    bool ptz_control(mediakit::PTZCmdType ptz_cmd, uint8_t speed) override;
    bool goto_position(const PTZ& position) override;
private:
    bool get_stream_uri(std::string &stream_uri);
    std::string media_service_address_;
    std::string media2_service_address_;
    std::string ptz_service_address_;
    std::string profile_token_;
};
