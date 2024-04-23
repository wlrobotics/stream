#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "json/json.h"

struct config_info {
    struct {
        std::string  grpc_endpoint;
        std::string  http_endpoint;
        std::string  http_token;
        bool enable_get_subdevice_info_v2;
    } vmr;
    
    struct {
        unsigned int stream_not_found_timeout;
        unsigned int stream_none_reader_timeout;
        int trace_fps_print_rate;
    } preview;

    struct {
        std::string extra_host;
        std::string intra_host;
        unsigned int epoll_size;
        bool enabled_ipv6;
    } network;

    struct {
        unsigned int port;
    } grpc;
    
    struct {
        unsigned int port = 554;
        std::unordered_map<std::string, std::string> ffmpeg_options;
    } rtsp;

    struct {
        bool enabled_multi_port = true;
        unsigned int start_port;
        unsigned int end_port;
        unsigned int timeout;
        std::string dumpdir;
    } rtp;

    struct {
        unsigned int port = 8088;
    } http;

    struct {
        bool enable_backtrace = false;
    } debug;

    struct {
        unsigned int timeout;
        bool enable_rtsp = false;
        bool enable_port_range;
        unsigned int start_port;
        unsigned int end_port;
    } hksdk;

    struct {
        unsigned int frame_interval;
        unsigned int zoom_max;
    } ptz;

    struct {
        unsigned int level;
    } log;

    struct {
        bool ntp_time_enable = false;
    } time_stamp;

    struct {
        bool modify_stamp = true;
        bool trace_fps = false;
    } live;

    struct {
        bool modify_stamp = true;
        bool trace_fps = false;
    } analyzer;

    struct {
        bool enabled = true;
        std::uint64_t memory_quota;
        std::uint32_t time_quota;
        bool enabled_time_quota = false;
        std::string storage_type;
        std::string local_path;
        bool enabled_sei_data;
        struct {
            std::string endpoint;
            std::string outter_endpoint;
            std::string accesskey;
            std::string secretkey;
            std::string bucket;
            std::string object_tags;
            bool use_host_style_addr = false;
        } s3[10];
    } record;

    struct {
        bool enabled;
        std::string ectd_endpoint;
        std::string etcd_root_path;
        int service_ttl;
        int device_ttl;
        int in_stream_quota;
    } service;
};

extern struct config_info ConfigInfo;

class Config {
public:
    Config() = default;
    ~Config() = default;
    bool init(const std::string& config_file);

private:
    Json::Value config_;
};
