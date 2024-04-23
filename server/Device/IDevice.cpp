#include "Device/IDevice.h"

#include "Config.h"

namespace mediakit {

IDevice::IDevice(const DeviceInfo& info) : device_info_(info),
                                              RecordAbility(info.device_id) {
}

IDevice::~IDevice() {
}

bool IDevice::close(MediaSource &sender, bool force) {
    if(!force && totalReaderCount(sender) > 0){
        return false;
    }
    media_close();
}

bool IDevice::init() {
    set_time_quota(device_info_.record_time_quota);
    return true;
}

bool IDevice::media_open() {
    muxer_ = std::make_shared<MultiMediaSourceMuxer>(DEFAULT_VHOST, LIVE_APP, device_info_.device_id);
    muxer_->setMediaListener(shared_from_this());
    return true;
}

bool IDevice::media_close() {
    return false;
}

bool IDevice::alive() {
    return (_last_frame_time.elapsedTime() < 8000);
}

void IDevice::keep_alive() {
    _last_frame_time.resetTime();
}

std::string IDevice::get_origin_url() {
    return device_info_.url;
}

void IDevice::get_stream_info(StreamInfo& stream_info) {
    stream_info_.bits_ = speed_.getSpeed() * 8;
    stream_info = stream_info_;
}

std::uint64_t IDevice::get_uptime() {
    return _last_frame_time.createdTime();
}

std::uint64_t IDevice::get_total_bytes() {
    return total_bytes_;
}

int IDevice::totalReaderCount() {
    return muxer_->totalReaderCount();
}

int IDevice::totalReaderCount(MediaSource &sender) {
    return muxer_->totalReaderCount();
}

bool IDevice::input_frame(const Frame::Ptr &frame) {
    frame->set_ntp_stamp();

    if(ConfigInfo.record.enabled) {
        RecordAbility::record_frame(frame);
    }

    if(ConfigInfo.time_stamp.ntp_time_enable && (frame->sei_payload.data.ntp_time_stamp == 0)) {
        frame->sei_enabled = true;
        frame->sei_payload.data.ntp_time_stamp = frame->get_ntp_stamp();
    }

    return muxer_->input_frame(frame);
}

void IDevice::activate_device() {
    activate_device_timer_.resetTime();
}

bool IDevice::is_activate() {
    return (activate_device_timer_.elapsedTime() < 8000);
}

}
