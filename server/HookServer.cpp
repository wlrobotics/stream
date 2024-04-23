#include "HookServer.h"

#include "Util/logger.h"
#include "Config.h"

using namespace mediakit;
using namespace toolkit;


HookServer& HookServer::Instance() {
    static std::shared_ptr<HookServer> s_instance(new HookServer());
    static HookServer &s_insteanc_ref = *s_instance;
    return s_insteanc_ref;
}

bool HookServer::init() {
    return true;
}

void HookServer::set_enabled(bool enable) {
    server_enabled_ = enable;
}

int HookServer::not_found_stream(const MediaInfo &args) {
    std::thread::id tid = std::this_thread::get_id();
    std::string stream_id = args._streamid;
    InfoL << "stream_id=" << stream_id;
    if(args._app != LIVE_APP || stream_id.empty()) {
        return 1;
    }

    return 1;
}

bool HookServer::none_stream_reader(MediaSource &sender) {
    return true;
}

bool HookServer::retrieve_stream(std::uint32_t ssrc, std::string& stream_id) {
    return true;
}

std::tuple<std::string, HttpSession::KeyValue, std::string>
HookServer::http_request(const Parser &parser) {
    return {"404 Not Found", HttpSession::KeyValue(), "404 Not Found"};
}
