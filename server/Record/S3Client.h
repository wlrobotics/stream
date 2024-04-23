#pragma once

#include <string>
#include "aws/s3/S3Client.h"

namespace mediakit {
class S3Client {
public:
    using Ptr = std::shared_ptr<S3Client>;
    S3Client();
    ~S3Client();

    struct S3Info {
        std::string endpoint;
        std::string outter_endpoint;
        std::string ak;
        std::string sk;
        std::string object_tags;
        std::string bucket;
        bool use_virtual_address;
    };

    bool init(const S3Info& s3_info);
    
    bool put_object(const std::string& bucket,
                const std::string& key, 
                std::shared_ptr<std::iostream> body,
                std::string& codec,
                std::string& url);
private:
    S3Info s3_info_;
    std::unique_ptr<Aws::S3::S3Client> s3_client_;
};

}
