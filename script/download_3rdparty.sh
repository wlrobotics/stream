#!/bin/bash
set -e

download_url='https://ai-private-devtools.oss-cn-shanghai.aliyuncs.com/supremind/cpp/lib'

library_path=./3rdparty

download_library() {
    wget -P $library_path $download_url/$1
    tar -zxf $library_path/$1 -C $library_path
    rm -rf $library_path/$1
}

library_temp_path=./3rdparty/temp

download_library_v2() {
    wget -P $library_temp_path $download_url/$1
    tar -zxf $library_temp_path/$1 -C $library_temp_path
    rm -rf $library_temp_path/$1
}

if [ -n "$1" ]; then
    download_library ffmpeg-n4.4-aarch64-linux-gnu-gcc-7.5.0.tar.gz
    download_library grpc-v1.30.0-aarch64-linux-gnu-gcc-7.5.0.tar.gz
    download_library jsoncpp-1.9.3-aarch64-linux-gnu-gcc-7.5.0.tar.gz
    download_library zlib-1.2.11-aarch64-linux-gnu-gcc-7.5.0.tar.gz
    download_library openssl-1.1.1g-aarch64-linux-gnu-gcc-7.5.0.tar.gz
    download_library gsoap-2.8.111-aarch64-linux-gnu-gcc-7.5.0.tar.gz
    download_library curl-nossl-7.76.0-aarch64-linux-gnu-gcc-7.6.0.tar.gz
    download_library aws-sdk-cpp-s3-1.8.119-aarch64-linux-gnu-gcc-7.6.0.tar.gz
    download_library onvif-1.0.0-aarch64-linux-gnu-gcc-7.5.0.tar.gz
    download_library_v2 grpc-v1.30.0-linux-amd64-20220718.tar.gz
else
    download_library ffmpeg-n4.4-linux-amd64.tar.gz
    download_library grpc-v1.30.0-linux-amd64-20220718.tar.gz
    download_library HCNetSDK-v6.1.4.42-linux-amd64.tar.gz
    download_library jsoncpp-1.9.3-linux-amd64.tar.gz
    download_library zlib-1.2.11-linux-amd64.tar.gz
    download_library openssl-1.1.1-linux-amd64.tar.gz
    download_library gsoap-2.8.111-linux-amd64.tar.gz
    download_library curl-7.74.0-linux-amd64.tar.gz
    download_library aws-sdk-cpp-s3-1.8.119-linux-amd64.tar.gz
    download_library onvif-1.0.0-linux-amd64.tar.gz
fi
