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
#include "Config.h"
#include "HookServer.h"

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

    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", LInfo));
    
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    Logger::Instance().setLevel((LogLevel)ConfigInfo.log.level);

    if(ConfigInfo.debug.enable_backtrace) {
        signal(SIGSEGV, signal_handler);
        signal(SIGABRT, signal_handler);
    }

    HookServer::Instance().init();

    EventPollerPool::setPoolSize(ConfigInfo.network.epoll_size);

    std::string host = "0.0.0.0";

    TcpServer::Ptr rtsp_server = std::make_shared<TcpServer>();
    rtsp_server->start<RtspSession>(ConfigInfo.rtsp.port, host);

    TcpServer::Ptr http_server = std::make_shared<TcpServer>();
    http_server->start<WebSocketSession<HttpSession>>(ConfigInfo.http.port, host);


    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(100));
    }
    
    return 0;
}
