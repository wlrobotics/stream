#pragma once

#include <string>

namespace mediakit {

enum Status {
    SUCCESS = 0,
    ERROR_UNKNOWN,
    ERROR_STREAM_ID_INVALID,
    ERROR_OVER_LOAD,
    ERROR_NOT_ENABLED,
    ERROR_TIMEOUT,
    ERROR_INVALID_DATA,
    ERROR_ETCD_FAILED,
    ERROR_FLOW_FAILED,
    ERROR_RTP_FAILED,
    ERROR_DEVICE_FAILED,
    ERROR_NOT_FOUND,
    ERROR_DOWNLOAD_FAILED,
    ERROR_OPEN_FAILED,
    ERROR_MUX_FAILED,
    ERROR_UPLOAD_FAILED
};

std::string status_to_string(Status status);

}