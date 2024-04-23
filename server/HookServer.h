#pragma once

#include <string>
#include <tuple>

#include "Common/MediaSource.h"
#include "Common/Parser.h"
#include "Http/HttpSession.h"
class HookServer {
public:
    HookServer() = default;
    ~HookServer() = default;

    static HookServer& Instance();

    bool init();

    void set_enabled(bool enable);
    int not_found_stream(const mediakit::MediaInfo &args);
    bool none_stream_reader(mediakit::MediaSource &sender);
    bool retrieve_stream(std::uint32_t ssrc, std::string& stream_id);
    std::tuple<std::string, HttpSession::KeyValue, std::string> 
            http_request(const mediakit::Parser &parser);

private:
    std::atomic_bool server_enabled_{false};
};
