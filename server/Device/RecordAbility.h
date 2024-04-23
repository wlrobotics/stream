#pragma once

#include <mutex>
#include <list>
#include <memory>

#include "Extension/Frame.h"


struct PictureInfo {
    std::uint32_t width;
    std::uint32_t height;
    std::string picture;
    std::uint64_t ntp_time_stamp;
};

class RecordAbility {
public:
    using Ptr = std::shared_ptr<RecordAbility>;
    RecordAbility(std::string device_id);
    virtual ~RecordAbility();
    virtual void record_frame(const mediakit::Frame::Ptr &frame);
    virtual bool download_record(std::uint64_t start_time,
                        std::uint64_t end_time,
                        std::list<mediakit::Frame::Ptr>& result_list);

    virtual void set_time_quota(const std::uint32_t time_quota);

    virtual std::uint64_t get_record_len();
    virtual std::uint64_t get_record_time();

    virtual bool capture_picture(const std::uint64_t time_stamp, PictureInfo& pic_info);
                        
private:
    std::string device_id_;
    std::mutex capture_picture_mtx_;
    std::mutex frame_list_mtx_;
    std::list<mediakit::Frame::Ptr> frame_list_;
    std::uint64_t memory_used_ = 0;
    std::uint32_t time_quota_ = 60000;
};
