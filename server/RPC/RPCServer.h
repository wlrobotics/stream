#pragma once

#include "stream.pb.h"
#include "stream.grpc.pb.h"
#include "grpcpp/grpcpp.h"

class RPCServer final : public vmr_proto::Stream::Service {
public:
    static RPCServer& Instance();
    bool init();

    void set_enabled(bool enable);

    grpc::Status GetPTZInfo(grpc::ServerContext* context,
                                const vmr_proto::GetPTZInfoRequest* request,
                                vmr_proto::GetPTZInfoResponse* response) override;

    grpc::Status DragZoomIn(grpc::ServerContext* context, 
                                const ::vmr_proto::DragZoomRequest* 
                                request, google::protobuf::Empty* response) override;

    grpc::Status DragZoomOut(grpc::ServerContext* context, 
                                const ::vmr_proto::DragZoomRequest* request, 
                                google::protobuf::Empty* response) override;

    grpc::Status GotoPosition(::grpc::ServerContext* context,
                                const ::vmr_proto::GotoPositionRequest* request, 
                                google::protobuf::Empty* response) override;

    grpc::Status GetPreset(grpc::ServerContext* context,
                                    const vmr_proto::GetPresetRequest* request,
                                    vmr_proto::GetPresetResponse* response) override;

    grpc::Status SetPreset(grpc::ServerContext* context,
                                    const vmr_proto::SetPresetRequest* request,
                                    google::protobuf::Empty* response) override;

    grpc::Status PTZControl(grpc::ServerContext* context,
                                    const vmr_proto::PTZControlRequest* request,
                                    google::protobuf::Empty* response) override;
    
    grpc::Status UploadRecord(grpc::ServerContext* context,
                                   const vmr_proto::UploadRecordRequest* request,
                                   vmr_proto::UploadRecordResponse* response) override;

    grpc::Status UpdateStream(grpc::ServerContext* context,
                                   const vmr_proto::UpdateStreamRequest* request,
                                   google::protobuf::Empty* response) override;
    
    grpc::Status CapturePicture(grpc::ServerContext* context,
                                    const ::vmr_proto::CapturePictureRequest* request, 
                                    vmr_proto::CapturePictureResponse* response) override;

    grpc::Status ActivateDevice(grpc::ServerContext* context,
                                    const ::vmr_proto::ActivateDeviceRequest* request, 
                                    google::protobuf::Empty* response) override;
private:
    std::atomic_bool server_enabled_{false};
    std::unique_ptr<grpc::Server> server_;
};
