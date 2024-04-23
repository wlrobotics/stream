#pragma once

#include <memory>

#include "Device/IDevice.h"

class RTSPDevice    : public IDevice{
public:
    using Ptr = std::shared_ptr<RTSPDevice>;
    RTSPDevice(const DeviceInfo& info);
    virtual ~RTSPDevice();
    bool media_open() override;
    bool media_close() override;
    
private:
    std::atomic_bool thread_status_{true};
    std::future<void> stream_thread_future_;
};
