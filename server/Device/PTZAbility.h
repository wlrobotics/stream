#pragma once

#include <string>
#include <vector>
#include <memory>

namespace mediakit {

struct PTZ {
    PTZ(unsigned int p = 0, unsigned int t = 0, unsigned int z = 0) : pan(p), tilt(t), zoom(z) {}
    unsigned int pan  = 0;
    unsigned int tilt = 0;
    unsigned int zoom = 0;
};

struct Rect{
    uint32_t xTop;
    uint32_t yTop;
    uint32_t xBottom;
    uint32_t yBottom;
};

struct PresetInfo{
    uint32_t preset_id;
    std::string preset_name;
    PTZ ptz;
};

enum PTZControlType {
    PTZ_CONTROL_TYPE_SDK = 1,
    PTZ_CONTROL_TYPE_ONVIF = 2
};

enum PresetCmdType {
    PRESET_CMD_SET = 0,
    PRESET_CMD_GOTO = 1,
    PRESET_CMD_DEL = 2,
};

enum PTZCmdType {
  PTZ_CMD_STOP = 0,
  PTZ_CMD_RIGHT = 1,
  PTZ_CMD_LEFT = 2,
  PTZ_CMD_UP = 3,
  PTZ_CMD_DOWN = 4,
  PTZ_CMD_LEFT_UP = 5,
  PTZ_CMD_LEFT_DOWN = 6,
  PTZ_CMD_RIGHT_UP = 7,
  PTZ_CMD_RIGHT_DOWN = 8,
  PTZ_CMD_ZOOM_IN = 9,
  PTZ_CMD_ZOOM_OUT = 10
};

class PTZAbility     {
public:
    using Ptr = std::shared_ptr<PTZAbility>;
    PTZAbility() {};
    virtual ~PTZAbility() {};
    virtual bool get_position(PTZ& position);
    virtual bool goto_position(const PTZ& position);
    virtual bool get_preset(std::vector<PresetInfo>& preset_list);
    virtual bool set_preset(mediakit::PresetCmdType preset_cmd, uint32_t index);    
    virtual bool drag_zoom_in(const Rect& rect);
    virtual bool drag_zoom_out(const Rect& rect);
    virtual bool ptz_control(mediakit::PTZCmdType ptz_cmd, uint8_t speed);
};
}
