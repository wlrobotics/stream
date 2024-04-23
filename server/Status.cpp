#include "Status.h"

#include <unordered_map>

namespace mediakit {

static std::unordered_map<Status, std::string> error_map = {
    {SUCCESS, "success"},
    {ERROR_UNKNOWN, "error unknow"},
    {ERROR_STREAM_ID_INVALID, "stream_id invalid"},
    {ERROR_OVER_LOAD, "over load"},
    {ERROR_NOT_ENABLED, "not enabled"},
    {ERROR_TIMEOUT, "timeout"},
    {ERROR_INVALID_DATA, "invalid data"},
    {ERROR_ETCD_FAILED, "etcd error"},
    {ERROR_FLOW_FAILED, "flow error"},
    {ERROR_RTP_FAILED, "rtp error"},
    {ERROR_DEVICE_FAILED, "device error"},
    {ERROR_NOT_FOUND, "not found"},
    {ERROR_DOWNLOAD_FAILED, "download failed"},
    {ERROR_OPEN_FAILED, "open failed"},
    {ERROR_MUX_FAILED, "mux failed"},
    {ERROR_UPLOAD_FAILED, "upload failed"},
};

std::string status_to_string(Status status) {
    auto it = error_map.find(status);
    if(it == error_map.end()) {
        return "error unknow";
    }
    return it->second;
}

}