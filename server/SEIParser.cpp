#include "SEIParser.h"

#include <cstring>

#include "json/json.h"

namespace mediakit {

namespace SEIParser {
    
namespace {

constexpr unsigned char live_sei_uuid[16] = {0xdc, 0x45, 0xe9, 0xbd,
                                             0xe6, 0xd9, 0x48, 0xb7,
                                             0x96, 0x2c, 0xd8, 0x20,
                                             0xd9, 0x23, 0xee, 0xef};

constexpr unsigned char analyzer_sei_uuid[16] = {0xdd, 0x45, 0xe9, 0xbd,
                                                 0xe6, 0xd9, 0x48, 0xb7,
                                                 0x96, 0x2c, 0xd8, 0x20,
                                                 0xd9, 0x23, 0xee, 0xef};

constexpr int sei_uuid_size = 16;
constexpr unsigned char start_sequence[] = { 0x00, 0x00, 0x00, 0x01 };
constexpr int start_sequence_size = 4;
constexpr unsigned char start_sequence_short[] = { 0x00, 0x00, 0x01};
constexpr int start_sequence_short_size = 3;

}

static unsigned int get_h264_sei_nalu_size(unsigned int sei_payload_content_size) {     
    unsigned int sei_payload_size = sei_payload_content_size + sei_uuid_size;    
    unsigned int sei_nalu_size = 1 + 1 + (sei_payload_size / 0xff + (sei_payload_size % 0xff != 0 ? 1 : 0)) + sei_payload_size;     
    unsigned int tail_size = 1;
    sei_nalu_size += tail_size;
    return sei_nalu_size;
}

static unsigned int get_hevc_sei_nalu_size(unsigned int sei_payload_content_size) {
    unsigned int sei_payload_size = sei_payload_content_size + sei_uuid_size;    
    unsigned int sei_nalu_size = 1 + 1 + 1 + (sei_payload_size / 0xff + (sei_payload_size % 0xff != 0 ? 1 : 0)) + sei_payload_size;     
    unsigned int tail_size = 1;
    sei_nalu_size += tail_size;
    return sei_nalu_size;
}

static bool demux_sei_frame_h264(unsigned char *annexb_nalu, int annexb_nalu_size, std::string& sei_payload) {
    //start_sequence(>=3)+nalu_head(1)+sei_payload_type(>=1)+sei_payload_size(>=1)+uuid(16)+data > 24;
    if (annexb_nalu == nullptr || annexb_nalu_size < 24) {
        return false;
    }

    int nalu_head_pos = 0;
    if(std::memcmp(annexb_nalu, start_sequence, start_sequence_size) == 0) {
        nalu_head_pos = start_sequence_size;
    } else if (std::memcmp(annexb_nalu, start_sequence_short, start_sequence_short_size) == 0){
        nalu_head_pos = start_sequence_short_size;
    } else {
        return false;
    }

    if ((annexb_nalu[nalu_head_pos] & 0x1f) != 0x06) {
        return false;
    }
    
    unsigned char *nalu_payload_data = annexb_nalu + nalu_head_pos + 1;
    int nalu_payload_size = annexb_nalu_size - nalu_head_pos - 1;
    unsigned char *nalu_payload_data_temp = nalu_payload_data;
    int sei_payload_type = 0;
    int sei_payload_size = 0;

    do {
        sei_payload_type += *nalu_payload_data_temp;
    } while (*nalu_payload_data_temp++ == 0xff);
    
    if(sei_payload_type != 0x05) {
        return false;
    }
    
    do {
        sei_payload_size += *nalu_payload_data_temp;
    } while (*nalu_payload_data_temp++ == 0xff);
    
    if(sei_payload_size <= sei_uuid_size || sei_payload_size > sei_packet_max_size) {
        return false;
    }
    
    if (std::memcmp(nalu_payload_data_temp, analyzer_sei_uuid, sei_uuid_size) != 0) {
        return false;
    } 
    nalu_payload_data_temp += sei_uuid_size;
    sei_payload_size -= sei_uuid_size;
    
    if(sei_payload_size > (nalu_payload_data + nalu_payload_size - nalu_payload_data_temp)) {
        return false;
    }
    
    sei_payload.assign((char*)nalu_payload_data_temp, sei_payload_size);
    
    return true;
}

static bool demux_sei_frame_h265(unsigned char *annexb_nalu,
                                                       int annexb_nalu_size,
                                                       std::string& sei_payload) {
    //start_sequence(>=3)+nalu_head(1)+sei_payload_type(>=1)+sei_payload_size(>=1)+uuid(16)+data > 24;
    if (annexb_nalu == nullptr || annexb_nalu_size < 24) {
        return false;
    }

    int nalu_head_pos = 0;
    if(std::memcmp(annexb_nalu, start_sequence, start_sequence_size) == 0) {
        nalu_head_pos = start_sequence_size;
    } else if (std::memcmp(annexb_nalu, start_sequence_short, start_sequence_short_size) == 0){
        nalu_head_pos = start_sequence_short_size;
    } else {
        return false;
    }

    if ((((unsigned char)(annexb_nalu[nalu_head_pos]) >> 1) & 0x3f) != 39) {
        return false;
    }

    unsigned char *nalu_payload_data = annexb_nalu + nalu_head_pos + 2;
    int nalu_payload_size = annexb_nalu_size - nalu_head_pos - 1;
    unsigned char *nalu_payload_data_temp = nalu_payload_data;
 
    int sei_payload_type = 0;
    int sei_payload_size = 0;

    do {
        sei_payload_type += *nalu_payload_data_temp;
    } while (*nalu_payload_data_temp++ == 0xff);
    
    if(sei_payload_type != 0x05) {
        return false;
    }

    do {
        sei_payload_size += *nalu_payload_data_temp;
    } while (*nalu_payload_data_temp++ == 0xff);
    if(sei_payload_size <= sei_uuid_size || sei_payload_size > 20480) {
        return false;
    }
    
    if (std::memcmp(nalu_payload_data_temp, analyzer_sei_uuid, sei_uuid_size) != 0) {
        return false;
    } 
    nalu_payload_data_temp += sei_uuid_size;
    sei_payload_size -= sei_uuid_size;
    
    if(sei_payload_size > (nalu_payload_data + nalu_payload_size - nalu_payload_data_temp)) {
        return false;
    }
    sei_payload.assign((char*)nalu_payload_data_temp, sei_payload_size);
    return true;
}

bool demux_sei_frame(unsigned char *annexb_nalu, int annexb_nalu_size, std::string& sei_payload, bool is_h264) {
    if(is_h264) {
        return demux_sei_frame_h264(annexb_nalu, annexb_nalu_size, sei_payload);
    } else {
        return demux_sei_frame_h265(annexb_nalu, annexb_nalu_size, sei_payload);
    }
}

static bool mux_sei_frame_h264(std::string& sei_frame, const std::string& sei_payload, bool is_annexb) {       
    if(sei_payload.empty()) {
        return false;
    }

    if(is_annexb) {
        sei_frame.append((char*)start_sequence, start_sequence_size);
    } else {
        unsigned int sei_nalu_size = SEIParser::get_h264_sei_nalu_size(sei_payload.size());
        sei_frame.push_back((uint8_t) ((sei_nalu_size >> 24) & 0xFF));
        sei_frame.push_back((uint8_t) ((sei_nalu_size >> 16) & 0xFF));
        sei_frame.push_back((uint8_t) ((sei_nalu_size >> 8) & 0xFF));
        sei_frame.push_back((uint8_t) ((sei_nalu_size >> 0) & 0xFF));
    }

    sei_frame.push_back(0x06);
    sei_frame.push_back(0x05);

    int sei_payload_size = sei_payload.size() + sei_uuid_size;
     
    while (true) {
        sei_frame.push_back((sei_payload_size >= 0xff ? 0xff : (unsigned char)sei_payload_size));
        if (sei_payload_size < 0xff) {
            break;
        }
        sei_payload_size -= 0xff;
    }

    sei_frame.append((char*)live_sei_uuid, sei_uuid_size);
    sei_frame.append(sei_payload);
    sei_frame.push_back(0x80);
    return true;
}


static bool mux_sei_frame_h265(std::string& sei_frame, const std::string& sei_payload, bool is_annexb) {
    if(sei_payload.empty()) {
        return false;
    }
    
    if(is_annexb) {
        sei_frame.append((char*)start_sequence, start_sequence_size);
    } else {
        unsigned int sei_nalu_size = SEIParser::get_hevc_sei_nalu_size(sei_payload.size());
        sei_frame.push_back((uint8_t) ((sei_nalu_size >> 24) & 0xFF));
        sei_frame.push_back((uint8_t) ((sei_nalu_size >> 16) & 0xFF));
        sei_frame.push_back((uint8_t) ((sei_nalu_size >> 8) & 0xFF));
        sei_frame.push_back((uint8_t) ((sei_nalu_size >> 0) & 0xFF));
    }

    sei_frame.push_back(0x4e);
    sei_frame.push_back(0x01);
    sei_frame.push_back(0x05);

    int sei_payload_size = sei_payload.size() + sei_uuid_size;
     
    while (true) {
        sei_frame.push_back((sei_payload_size >= 0xff ? 0xff : (unsigned char)sei_payload_size));
        if (sei_payload_size < 0xff) {
            break;
        }
        sei_payload_size -= 0xff;
    }
 
    sei_frame.append((char*)live_sei_uuid, sei_uuid_size);
    sei_frame.append(sei_payload);
    sei_frame.push_back(0x80);
    return true;
}

void sei_payload_marshal(const SEIPayload& sei_payload, std::string& sei_payload_json) {
    Json::Value json_payload;
    json_payload["code"] = sei_payload.code;
    json_payload["msg"] = sei_payload.msg;

    if((sei_payload.data.ptz_current_pos[0] != 0) || 
       (sei_payload.data.ptz_current_pos[1] != 0) ||
       (sei_payload.data.ptz_current_pos[2] != 0) ) {
        json_payload["data"]["ptz_current_pos"].append(sei_payload.data.ptz_current_pos[0]);
        json_payload["data"]["ptz_current_pos"].append(sei_payload.data.ptz_current_pos[1]);
        json_payload["data"]["ptz_current_pos"].append(sei_payload.data.ptz_current_pos[2]);
    }

    if(sei_payload.data.ntp_time_stamp != 0u) {
        json_payload["data"]["ntp_time_stamp"] = sei_payload.data.ntp_time_stamp;
    }

    Json::StreamWriterBuilder builder;
    builder.settings_["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream os;
    writer->write(json_payload, &os);
    sei_payload_json = os.str();
}

bool generate_sei_frame(std::string& sei_frame, const SEIPayload& sei_payload, bool is_h264, bool is_annexb) {
    std::string sei_payload_json;
    sei_payload_marshal(sei_payload, sei_payload_json);
    if(is_h264) {
        return mux_sei_frame_h264(sei_frame, sei_payload_json, is_annexb);
    } else {
        return mux_sei_frame_h265(sei_frame, sei_payload_json, is_annexb);
    }
}

}

}