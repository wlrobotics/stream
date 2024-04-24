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

int HookServer::not_found_stream(const MediaInfo &args) {
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
