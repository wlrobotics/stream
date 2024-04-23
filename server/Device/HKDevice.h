#ifndef DEVICE_DEVICEHK_H_
#define DEVICE_DEVICEHK_H_

#include <sys/time.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "StreamStat.h"
#include "Device/IDevice.h"

using namespace toolkit;

namespace mediakit {
class HKDevice final : public IDevice {
public:
    using Ptr = std::shared_ptr<HKDevice>;
    HKDevice(const DeviceInfo& info);
    ~HKDevice();

    bool login();
    void logout();

    bool init() override;
    bool media_open() override;
    bool media_close() override;

    bool drag_zoom_in(const Rect& rect) override;
    bool drag_zoom_out(const Rect& rect) override;

    bool get_position(PTZ& position) override;
    bool goto_position(const PTZ& position) override;

    bool get_preset(std::vector<PresetInfo>& preset_list) override;
    bool set_preset(mediakit::PresetCmdType preset_cmd, uint32_t index) override;
    bool ptz_control(mediakit::PTZCmdType ptz_cmd, uint8_t speed) override;
    // bool capture_picture(std::string& picture, std::uint64_t& ntp_time_stamp) override;

private:
    int frame_count_ = 0;
    int sdk_handle_ = -1;
    int alarm_handle_ = -1;
    int realplay_handle_ = -1;
    int drop_frame_count_ = 0;
    bool drop_frame_ = false;
};
}

#endif
