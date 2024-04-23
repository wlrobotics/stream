#pragma once

#include <string>

namespace mediakit {

namespace SEIParser {
    
struct SEIPayload {   
    int code = 0;
    std::string msg = "success";
    struct {
        std::uint32_t ptz_current_pos[3] = {0, 0, 0};
        std::uint64_t ntp_time_stamp = 0;
    } data;
};

constexpr int sei_packet_max_size = 40960;

bool demux_sei_frame(unsigned char *annexb_nalu, int annexb_nalu_size, std::string& sei_payload, bool is_h264);

bool generate_sei_frame(std::string& sei_frame, const SEIPayload& sei_payload, bool is_h264, bool is_annexb);

void sei_payload_marshal(const SEIPayload& sei_payload, std::string& sei_payload_json);
}

}
