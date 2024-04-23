#include <signal.h>
#include <execinfo.h>

#include <thread>
#include <chrono>

#include "Common/config.h"
#include "Util/logger.h"
#include "Network/TcpServer.h"
#include "Rtsp/RtspSession.h"
#include "Http/HttpSession.h"
#include "Http/WebSocketSession.h"
#include "RPC/RPCServer.h"
#include "EventReport.h"
#include "Device/DeviceManager.h"
#include "Record/RecordUploader.h"
#include "Config.h"
#include "HookServer.h"
#include "ServiceManager.h"

void signal_handler(int signo) {
    int nptrs;
    void *buffer[1024];
    char **strings;
    signal(signo, SIG_DFL);

    nptrs = backtrace(buffer, 1024);
    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
       perror("backtrace_symbols");
       exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nptrs; i++) {
       printf("%s\n", strings[i]);
    }
}

int main(int argc, char* argv[]) {
    using namespace toolkit;
    using namespace mediakit;
    if(argc < 2) {
        std::cerr << "input arg error!" << std::endl;
        return -1;
    }

    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", LInfo));
    
    Config config;
    bool b_ret = config.init(std::string(argv[1]) + "/stream.json");
    if(!b_ret) {
        std::cerr << "config file error!" << std::endl;
        return -1;
    }

    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    Logger::Instance().setLevel((LogLevel)ConfigInfo.log.level);

    if(ConfigInfo.debug.enable_backtrace) {
        signal(SIGSEGV, signal_handler);
        signal(SIGABRT, signal_handler);
    }

    FlowRPCClient::Instance().init();

    HookServer::Instance().init();

    if(ConfigInfo.service.enabled) {
        ServiceManager::Instance().init();
    } else {
        HookServer::Instance().set_enabled(true);
        RPCServer::Instance().set_enabled(true);
    }

    EventPollerPool::setPoolSize(ConfigInfo.network.epoll_size);

    std::string host = "0.0.0.0";
    if(ConfigInfo.network.enabled_ipv6) {
        host = "::0";
    }

    TcpServer::Ptr rtsp_server = std::make_shared<TcpServer>();
    rtsp_server->start<RtspSession>(ConfigInfo.rtsp.port, host);

    TcpServer::Ptr http_server = std::make_shared<TcpServer>();
    http_server->start<WebSocketSession<HttpSession>>(ConfigInfo.http.port, host);

    DeviceManager::Instance().init();

    RecordUploader::Instance().init();

    EventReport::Instance().init();

    RPCServer::Instance().init();
    
    return 0;
}
