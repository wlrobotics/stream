#include <arpa/inet.h>

#include "RtpSession.h"
#include "RtpSelector.h"
#include "Network/TcpServer.h"
#include "Network/sockutil.h"

namespace mediakit{

static std::string printAddress(const struct sockaddr *addr) {
    return StrPrinter << toolkit::SockUtil::inet_ntoa(((struct sockaddr_in *) addr)->sin_addr) 
                      << ":" 
                      << ntohs(((struct sockaddr_in *) addr)->sin_port);
}

const string RtpSession::kStreamID = "stream_id";

void RtpSession::attachServer(const TcpServer &server) {
    _stream_id = const_cast<TcpServer &>(server)[kStreamID];
}

RtpSession::RtpSession(const Socket::Ptr &sock) : TcpSession(sock) {
    DebugP(this);
    socklen_t addr_len = sizeof(addr);
    getpeername(sock->rawFD(), &addr, &addr_len);
}
RtpSession::~RtpSession() {
    DebugP(this);
    if(_process){
        RtpSelector::Instance().delProcess(_process->getIdentifier(), _process.get());
    }
}

void RtpSession::onRecv(const Buffer::Ptr &data) {
    try {
        RtpSplitter::input(data->data(), data->size());
    } catch (SockException &ex) {
        shutdown(ex);
    } catch (std::exception &ex) {
        shutdown(SockException(Err_other, ex.what()));
    }
}

void RtpSession::onError(const SockException &err) {
    WarnL << "stream_id=" << _stream_id << printAddress(&addr) << " " << err.what();
}

void RtpSession::onManager() {
    if(_process && !_process->alive()){
        shutdown(SockException(Err_timeout, "RtpSession timeout " + printAddress(&addr)));
    }

    if(!_process && _ticker.createdTime() > 10 * 1000){
        shutdown(SockException(Err_timeout, "illegal connection " + printAddress(&addr)));
    }
}

void RtpSession::onRtpPacket(const char *data, uint64_t len) {
    if (len > 1024 * 2) {
        //throw SockException(Err_shutdown, "rtp包长度异常，发送端可能缓存溢出并覆盖");
    }
    if (!_process) {
        uint32_t ssrc;
        if (!RtpSelector::getSSRC(data, len, ssrc)) {
            return;
        }
        if(!_stream_id.empty()) {
            _process = RtpSelector::Instance().getProcess(_stream_id);
        } else {
            _process = RtpSelector::Instance().getProcess(ssrc);
        }
        if(_process == nullptr) {
            return;
        }
        _process->setListener(dynamic_pointer_cast<RtpSession>(shared_from_this()));
    }
    _process->inputRtp(false, getSock(), data, len, &addr);
    _ticker.resetTime();
}

bool RtpSession::close(MediaSource &sender, bool force) {
    if(!_process || (!force && _process->totalReaderCount())){
        return false;
    }
    string err = StrPrinter << "close media:" << sender.getSchema() << "/" 
                                              << sender.getVhost() << "/" 
                                              << sender.getApp() << "/" 
                                              << sender.getId() << " " << force;
    safeShutdown(SockException(Err_shutdown,err));
    return true;
}

int RtpSession::totalReaderCount(MediaSource &sender) {
    return _process ? _process->totalReaderCount() : sender.totalReaderCount();
}

}//namespace mediakit