#pragma once

#include <memory>

#include "Device/IDevice.h"

class HTTPMP4Device final: public IDevice {
public:
    using Ptr = std::shared_ptr<HTTPMP4Device>;

    HTTPMP4Device(const DeviceInfo& info);
    ~HTTPMP4Device();
    bool media_open() override;
    bool media_close() override;
    
private:
    std::atomic_bool thread_status_{true};
    std::future<void> stream_thread_future_;
};
