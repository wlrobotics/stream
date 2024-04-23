#include "EventReport.h"

#include "Config.h"
#include "HttpClient_curl.h"
#include "Util/logger.h"
#include "Poller/EventPoller.h"
#include "Common/MediaSource.h"
#include "RPC/proto/event.pb.h"
#include "RPC/proto/event.grpc.pb.h"
#include "grpcpp/grpcpp.h"
#include "Rtp/RtpSelector.h"

namespace mediakit {

EventReport::EventReport() {

}
EventReport::~EventReport() {

}

EventReport& EventReport::Instance() {
    static std::shared_ptr<EventReport> s_instance(new EventReport());
    static EventReport &s_insteanc_ref = *s_instance;
    return s_insteanc_ref;
}


bool EventReport::init() {
    using namespace toolkit;
    
    event_queue_.init(1024);
    event_thread_future_ = std::async([this]() {
        Infra::HttpClient http_client;
        while (true) {
            Json::Value evt = event_queue_.pop();
            std::string response;
            int status_code = http_client.post(ConfigInfo.vmr.http_endpoint + "/v1/event/collection", nullptr, evt, nullptr, &response);
            if(status_code != CURLE_OK) {
                ErrorL << "http_client->post failed!";
            }
        }
    });
    static auto event_stub = vmr_proto::EventService::NewStub(
                            grpc::CreateChannel(ConfigInfo.vmr.grpc_endpoint,
                            grpc::InsecureChannelCredentials()));

    std::thread([](){
        while(true) {
            vmr_proto::ReportStreamInfoRequest req;
            MediaSource::for_each_media([&req](const MediaSource::Ptr &media) {
                if (media->getSchema().compare("rtsp") == 0) {
                    req.set_host("127.0.0.1");
                    auto info = req.add_infos();
                    auto bytes_rate = media->getBytesSpeed();
                    info->set_device_id(media->getId());
                    info->set_duration("12");
                    for (auto &track : media->getTracks()) {
                        if (track->getTrackType() == TrackVideo) {
                            auto video_track = dynamic_pointer_cast<VideoTrack>(track);
                            info->mutable_video()->set_height(video_track->getVideoHeight());
                            info->mutable_video()->set_width(video_track->getVideoWidth());
                            info->mutable_video()->set_frame_rate(video_track->getVideoFps());
                            auto gb = RtpSelector::Instance().getProcess(media->getId(), false);
                            if(gb != nullptr) {
                                info->mutable_video()->set_drop_frame_rate(gb->get_rtp_loss_rate());
                            }
                            info->mutable_video()->set_codec((video_track->getCodecId() == CodecH264) ? 
                                                            vmr_proto::CODEC_TYPE_H264 : vmr_proto::CODEC_TYPE_H265);
                        }
                    }
                    info->mutable_video()->set_rate(bytes_rate);
                    info->set_status(bytes_rate ? vmr_proto::STREAM_STATUS_OK : vmr_proto::STREAM_STATUS_ERROR);
                }
            });
            if(req.infos_size() > 0) {
                grpc::ClientContext context;
                google::protobuf::Empty resp;
                event_stub->ReportStreamInfo(&context, req, &resp);
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }).detach();
}

//report 异步且线程安全，event_queue_.push满后，event_queue_会清空
bool EventReport::report(const std::string& stream_id,
                           const EventType event,
                           const std::string& details) {
    WarnL  << "report event stream_id=" << stream_id
           << ",event=" << event_map_[event]
           << ",details=" << details;

    Json::Value json_data;
    json_data["product"] = "vmr";
    json_data["service"] = "stream";
    json_data["type"] = "media";
    json_data["meta"]["resource"] = "device";
    json_data["meta"]["resource_id"] = stream_id;
    json_data["message"]["info"] = event_map_[event];
    json_data["message"]["reason"] = details;
    if(event < 20) {
        json_data["level"] = "TRACE";
    } else if(event < 40) {
        json_data["level"] = "INFO";
    } else if(event < 60) {
        json_data["level"] = "WARN";
    } else if(event < 80) {
        json_data["level"] = "CRITICAL";
    }
    json_data["time"] = std::time(nullptr);
    event_queue_.push(json_data);
}

}
