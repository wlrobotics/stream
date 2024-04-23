#ifndef IDEVICE_H_
#define IDEVICE_H_

#include <vector>
#include <string>
#include <future>
#include <vector>
#include "Common/MultiMediaSourceMuxer.h"
#include "Common/MediaSource.h"
#include "StreamStat.h"
#include "Device/RecordAbility.h"
#include "Device/PTZAbility.h"

#define TraceID(ptr) TraceL << ptr->device_info_.device_id << " "
#define DebugID(ptr) DebugL << ptr->device_info_.device_id << " "
#define InfoID(ptr) InfoL <<  ptr->device_info_.device_id << " "
#define WarnID(ptr) WarnL <<  ptr->device_info_.device_id << " "
#define ErrorID(ptr) ErrorL << ptr->device_info_.device_id << " "

namespace mediakit {

enum DeviceType {
    HKSDK = 1,
    ONVIF,
    RTSP,
    RTMP,
    HTTP_MP4,
    GB28181
};

enum ManufacturerType {
    HIKVISION = 1,
    DAHUA
};

struct DeviceInfo {
    DeviceType dev_type;
    std::string device_id;
    std::uint32_t channel_id = 1;
    std::string host;
    std::string user_name;
    std::string password;
    std::string url;
    std::vector<std::string> url_list;
    std::uint32_t record_time_quota = 0;
    bool is_activate_device = false;
    ManufacturerType manufacturer_type = ManufacturerType::HIKVISION;
    struct {
        bool enabled = false;
        PTZControlType protocol_type;
        int max_zoom = -1;
        int max_elevation = 0;
    } ptz;
    int mp4_loop_count = 0;
};

struct MediaDescription {
    std::uint32_t stream_rtp_port = 0;
    int stream_trans_type = 0;
    std::string stream_host;
};

class IDevice : public RecordAbility,
                     public PTZAbility,
                     public MediaSourceEvent,
                     public std::enable_shared_from_this<IDevice>{
public:
    using Ptr = std::shared_ptr<IDevice>;
    IDevice(const DeviceInfo& info);
    virtual ~IDevice();
    virtual bool init();
    
    virtual bool media_open();
    virtual bool media_close();
    bool close(MediaSource &sender, bool force) override;

    std::string get_origin_url();

    void get_stream_info(StreamInfo& stream_info);
    std::uint64_t get_uptime();
    int totalReaderCount();
    std::uint64_t get_total_bytes();
    bool alive();

    bool get_ptz_enabled() { return device_info_.ptz.enabled; };

    void keep_alive();
    
    DeviceInfo get_device_info() {return device_info_;}
    std::string get_device_id() {return device_info_.device_id;}
    DeviceType get_device_type() {return device_info_.dev_type;}

    bool input_frame(const Frame::Ptr &frame);

    void activate_device();
    bool is_activate();
        
protected:
    int totalReaderCount(MediaSource &sender) override;

    uint64_t total_bytes_ = 0;
    uint64_t bits_ = 0;
    BytesSpeed speed_;

    CodecId codec_id_ = CodecInvalid;
    
    DeviceInfo device_info_;
    StreamInfo stream_info_;

    MultiMediaSourceMuxer::Ptr muxer_;

private:
    toolkit::Ticker _last_frame_time;
    toolkit::Ticker activate_device_timer_;
};

}

#endif
