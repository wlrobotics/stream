#include "Record/S3Client.h"

#include "aws/core/Aws.h"
#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/s3/model/PutObjectRequest.h"

#include "Util/onceToken.h"
#include "Util/logger.h"

using namespace toolkit;

namespace mediakit {

S3Client::S3Client() {
    static onceToken token([](){
        Aws::SDKOptions options;
        options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Off;
        Aws::InitAPI(options);
    }, []() {
        Aws::SDKOptions options;
        options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Off;
        Aws::ShutdownAPI(options);
    });
}

S3Client::~S3Client() {
}

bool S3Client::init(const S3Info& s3_info) {
    s3_info_ = s3_info;
    Aws::Client::ClientConfiguration cfg;
    cfg.endpointOverride = s3_info_.endpoint;
    cfg.scheme = Aws::Http::Scheme::HTTP;
    cfg.verifySSL = false;
    cfg.httpRequestTimeoutMs = 10000;

    Aws::Auth::AWSCredentials cred(s3_info_.ak, s3_info_.sk);
    s3_client_ = std::make_unique<Aws::S3::S3Client>(cred,
                                                     cfg,
                                                     Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
                                                     s3_info.use_virtual_address);
    return true;
}

bool S3Client::put_object(const std::string& bucket,
                     const std::string& key,
                    std::shared_ptr<std::iostream> body,
                    std::string& codec,
                    std::string& url) {
    Aws::S3::Model::PutObjectRequest object_request;
    std::string buck = s3_info_.bucket;
    if(!bucket.empty()) {
        buck = bucket;
    }
    
    object_request.SetBucket(buck);
    object_request.SetTagging(s3_info_.object_tags);
    object_request.SetKey(key);
    object_request.SetBody(body);
    object_request.AddMetadata("codec", codec);
    //object_request.SetContentType("video/mp4");

    auto put_object_outcome = s3_client_->PutObject(object_request);
    if (!put_object_outcome.IsSuccess()) {
        auto error = put_object_outcome.GetError();
        ErrorL << "ERROR: " << error.GetExceptionName() 
               << ": " << error.GetMessage();
        return false;
    }

    url = "http://" + s3_info_.outter_endpoint + "/" + buck + "/" + key;

    return true;
}

}

