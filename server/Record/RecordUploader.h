#pragma once

#include "Record/S3Client.h"
#include "Extension/Frame.h"
#include "Status.h"

namespace mediakit {
class RecordUploader {
public:
    RecordUploader() = default;
    ~RecordUploader() = default;
    static RecordUploader& Instance();
    bool init();
    Status upload(const std::list<mediakit::Frame::Ptr>& frame_list,
            const std::string& bucket,
            const std::string& key,
            std::string& url);
private:
    static constexpr int FRAME_BUFFER_SIZE = 2 * 1024 * 1024;
    static constexpr int EX_DATA_SIZE = 8192;
    static bool mux_frames_to_mp4(const std::list<mediakit::Frame::Ptr>& frame_list, 
                                  std::shared_ptr<std::iostream> mp4_buffer);
    static bool generate_metadata_file(const std::string& key, const std::string& codec);                          
    std::vector<S3Client::Ptr> s3_clients_;
};
}
