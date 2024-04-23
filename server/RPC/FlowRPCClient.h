#pragma once

#include "Device/IDevice.h"
#include "grpcpp/grpcpp.h"
#include "proto/flow.pb.h"
#include "proto/flow.grpc.pb.h"

class FlowRPCClient {
public:
    FlowRPCClient() = default;
    ~FlowRPCClient() = default;
    static FlowRPCClient& Instance();
    bool init();
    bool create_stream(mediakit::DeviceInfo& dev_info, const MediaDescription sdp = {0,0});
    bool delete_stream(const std::string& device_id);
    bool retrieve_stream(std::uint32_t ssrc,  std::string& device_id);
    bool get_subdevice_info(const std::string& device_id, mediakit::DeviceInfo& dev_info);
    bool get_subdevice_info_v2(const std::string& device_id, mediakit::DeviceInfo& dev_info);
private:
    std::unique_ptr<vmr_proto::FlowService::Stub> stub_;
};
