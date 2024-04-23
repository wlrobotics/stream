#include "Device/PTZAbility.h"

namespace mediakit {

bool PTZAbility::get_position(PTZ& position) {
    return false;
}

bool PTZAbility::goto_position(const PTZ& position) {
    return false;
}

bool PTZAbility::get_preset(std::vector<PresetInfo>& preset_list) {
    return false;
}

bool PTZAbility::set_preset(mediakit::PresetCmdType preset_cmd, uint32_t index){
    return false;
}

bool PTZAbility::drag_zoom_in(const Rect& rect) {
    return false;
}

bool PTZAbility::drag_zoom_out(const Rect& rect){
    return false;
}

bool PTZAbility::ptz_control(mediakit::PTZCmdType ptz_cmd, uint8_t speed) {
    return false;
}
}
