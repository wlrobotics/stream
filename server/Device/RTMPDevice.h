#pragma once

#include <memory>

#include "Device/IDevice.h"


class RTMPDevice final    : public IDevice {
public:
    using Ptr = std::shared_ptr<RTMPDevice>;

    RTMPDevice(const DeviceInfo& info);
    ~RTMPDevice();
    bool media_open() override;
    bool media_close() override;
    
private:
    std::atomic_bool thread_status_{true};
    std::future<void> stream_thread_future_;
};
