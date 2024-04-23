#pragma once

#include "Device/IDevice.h"

class GB28181Device final : public IDevice {
public:
    using Ptr = std::shared_ptr<GB28181Device>;
    GB28181Device(const DeviceInfo& info);
    ~GB28181Device();

    bool init() override;

    bool drag_zoom_in(const Rect& rect) override;
    bool drag_zoom_out(const Rect& rect) override;

    bool get_position(PTZ& position) override;
    bool goto_position(const PTZ& position) override;

    bool get_preset(std::vector<PresetInfo>& preset_list) override;
    bool set_preset(mediakit::PresetCmdType preset_cmd, uint32_t index) override;
    bool ptz_control(mediakit::PTZCmdType ptz_cmd, uint8_t speed) override;
    bool capture_picture(const std::uint64_t time_stamp, PictureInfo& pic_info) override;

private:
    IDevice::Ptr dev_ = nullptr;
};