#include "Device/IDevice.h"


namespace mediakit {

class DeviceFactory {
public:
    static IDevice::Ptr create(const DeviceInfo& info);
};

}
