#include "Device/DeviceFactory.h"

#include "Device/HKDevice.h"
#include "Device/OnvifDevice.h"
#include "Device/RTSPDevice.h"
#include "Device/RTMPDevice.h"
#include "Device/HTTPMP4Device.h"
#include "Device/GB28181Device.h"

namespace mediakit {

IDevice::Ptr DeviceFactory::create(const DeviceInfo& info) {
    switch (info.dev_type) {
    case DeviceType::HKSDK:{
        return std::make_shared<HKDevice>(info);
    }
    case DeviceType::ONVIF:{
        return std::make_shared<OnvifDevice>(info);
    }
    case DeviceType::RTSP:{
        return std::make_shared<RTSPDevice>(info);
    }
    case DeviceType::RTMP:{
        return std::make_shared<RTMPDevice>(info);
    }
    case DeviceType::HTTP_MP4:{
        return std::make_shared<HTTPMP4Device>(info);
    }
    case DeviceType::GB28181:{
        return std::make_shared<GB28181Device>(info);
    }
    default: {
        return nullptr;
    }

    return nullptr;
}
}

}
