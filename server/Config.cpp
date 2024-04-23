#include "Config.h"

#include <fstream>
#include <algorithm>

#include "Util/logger.h"

using namespace toolkit;

struct config_info ConfigInfo;

bool Config::init(const std::string& config_file) {
    std::fstream f(config_file, std::ios::in);
    if (!f.is_open()) {
        InfoL << "open failed! config_file=" << config_file;
        return false;
    }

    Json::CharReaderBuilder rbuilder;
    JSONCPP_STRING errs;
    bool ret = Json::parseFromStream(rbuilder, f, &config_, &errs);
    if(!ret) {
        InfoL << "Json::parseFromStream ERROR!";
        return false;
    }

    InfoL << config_.toStyledString();

    ConfigInfo.vmr.grpc_endpoint = config_["vmr"]["grpc_endpoint"].asString();
    ConfigInfo.vmr.http_endpoint = config_["vmr"]["http_endpoint"].asString();
    ConfigInfo.vmr.http_token = config_["vmr"]["http_token"].asString();
    ConfigInfo.vmr.enable_get_subdevice_info_v2 = config_["vmr"]["enable_get_subdevice_info_v2"].asBool();

    ConfigInfo.preview.stream_not_found_timeout = config_["preview"]["stream_not_found_timeout"].asUInt();
    ConfigInfo.preview.stream_none_reader_timeout = config_["preview"]["stream_none_reader_timeout"].asUInt();
    ConfigInfo.preview.trace_fps_print_rate = config_["preview"]["trace_fps_print_rate"].asInt();

    ConfigInfo.network.extra_host = config_["network"]["extra_host"].asString();
    ConfigInfo.network.intra_host = config_["network"]["intra_host"].asString();
    ConfigInfo.network.epoll_size = config_["network"]["epoll_size"].asUInt();
    ConfigInfo.network.enabled_ipv6 = config_["network"]["enabled_ipv6"].asBool();

    ConfigInfo.grpc.port = config_["grpc"]["port"].asUInt();
    
    ConfigInfo.rtsp.port = config_["rtsp"]["port"].asUInt();
    for (auto it = config_["rtsp"]["ffmpeg_options"].begin(); it != config_["rtsp"]["ffmpeg_options"].end(); it++){
        ConfigInfo.rtsp.ffmpeg_options.emplace(it.key().asString(), it->asString());
    }

    ConfigInfo.rtp.enabled_multi_port = config_["rtp"]["enabled_multi_port"].asBool();
    ConfigInfo.rtp.start_port = config_["rtp"]["start_port"].asUInt();
    ConfigInfo.rtp.end_port = config_["rtp"]["end_port"].asUInt();
    ConfigInfo.rtp.timeout = config_["rtp"]["timeout"].asUInt();
    ConfigInfo.rtp.dumpdir = config_["rtp"]["dumpdir"].asString();

    ConfigInfo.http.port = config_["http"]["port"].asUInt();

    ConfigInfo.debug.enable_backtrace = config_["debug"]["enable_backtrace"].asBool();

    ConfigInfo.hksdk.timeout = config_["hksdk"]["timeout"].asUInt();
    ConfigInfo.hksdk.enable_rtsp = config_["hksdk"]["enable_rtsp"].asBool();
    ConfigInfo.hksdk.enable_port_range = config_["hksdk"]["enable_port_range"].asBool();
    ConfigInfo.hksdk.start_port = config_["hksdk"]["start_port"].asUInt();
    ConfigInfo.hksdk.end_port = config_["hksdk"]["end_port"].asUInt();

    ConfigInfo.ptz.frame_interval = config_["ptz"]["frame_interval"].asUInt();
    ConfigInfo.ptz.zoom_max = config_["ptz"]["zoom_max"].asUInt();

    ConfigInfo.log.level = config_["log"]["level"].asUInt();

    ConfigInfo.time_stamp.ntp_time_enable = config_["time_stamp"]["ntp_time_enable"].asBool();

    ConfigInfo.live.modify_stamp = config_["live"]["modify_stamp"].asBool();
    ConfigInfo.live.trace_fps = config_["live"]["trace_fps"].asBool();

    ConfigInfo.analyzer.modify_stamp = config_["analyzer"]["modify_stamp"].asBool();
    ConfigInfo.analyzer.trace_fps = config_["analyzer"]["trace_fps"].asBool();

    ConfigInfo.record.enabled = config_["record"]["enabled"].asBool();
    std::uint64_t memory_quota_temp = config_["record"]["memory_quota"].asUInt();
    ConfigInfo.record.memory_quota = memory_quota_temp * 1024 * 1024;
    ConfigInfo.record.time_quota = config_["record"]["time_quota"].asUInt() * 1000;
    ConfigInfo.record.enabled_time_quota = config_["record"]["enabled_time_quota"].asBool();
    ConfigInfo.record.enabled_sei_data = config_["record"]["enabled_sei_data"].asBool();
    ConfigInfo.record.storage_type = config_["record"]["storage_type"].asString();
    ConfigInfo.record.local_path = config_["record"]["local_path"].asString();
    for(int i = 0; i < std::min((int)config_["record"]["s3"].size(), 10); i++) {
        ConfigInfo.record.s3[i].endpoint = config_["record"]["s3"][i]["endpoint"].asString();
        ConfigInfo.record.s3[i].outter_endpoint = config_["record"]["s3"][i]["outter_endpoint"].asString();
        ConfigInfo.record.s3[i].accesskey = config_["record"]["s3"][i]["accesskey"].asString();
        ConfigInfo.record.s3[i].secretkey = config_["record"]["s3"][i]["secretkey"].asString();
        ConfigInfo.record.s3[i].bucket = config_["record"]["s3"][i]["bucket"].asString();
        ConfigInfo.record.s3[i].object_tags = config_["record"]["object_tags"].asString();
        ConfigInfo.record.s3[i].use_host_style_addr = config_["record"]["use_host_style_addr"].asBool();
    }

    ConfigInfo.service.enabled = config_["service"]["enabled"].asBool();
    ConfigInfo.service.ectd_endpoint = config_["service"]["ectd_endpoint"].asString();
    ConfigInfo.service.etcd_root_path = config_["service"]["etcd_root_path"].asString();
    ConfigInfo.service.service_ttl = config_["service"]["service_ttl"].asInt();
    ConfigInfo.service.device_ttl = config_["service"]["device_ttl"].asInt();
    ConfigInfo.service.in_stream_quota = config_["service"]["in_stream_quota"].asInt();

    return true;
}