#include "RtpServer.h"

#include "RtpSelector.h"
#include "RtpSession.h"

using namespace toolkit;


namespace mediakit{

RtpServer::RtpServer() {
}

RtpServer::~RtpServer() {
    rtp_udp_server_->setOnRead(nullptr);
    rtcp_server_->setOnRead(nullptr);
    for (auto& udp_svr : vec_udp_server_) {
        udp_svr->setOnRead(nullptr);
    }
    if (rtp_process_) {
        //RtpSelector::Instance().delProcess(device_id_, rtp_process_.get());
    }
}

void RtpServer::start(std::uint32_t rtp_port, std::uint32_t rtcp_port) {
    rtp_port_ = rtp_port;
    rtcp_port_ = rtcp_port;

    EventPollerPool::Instance().for_each([&](const TaskExecutor::Ptr &executor) {
        EventPoller::Ptr poller = dynamic_pointer_cast<EventPoller>(executor); 
        Socket::Ptr udp_server = std::make_shared<Socket>(poller, false);
        if (!udp_server->bindUdpSock(rtp_port, local_ip_)) {
            ErrorL << "bindUdpSock on " << local_ip_ << ":" << rtp_port << " failed:" << get_uv_errmsg(true);
            return ;
        }
        SockUtil::setRecvBuf(udp_server->rawFD(), 8 * 1024 * 1024);
        auto &ref = RtpSelector::Instance();
        toolkit::Socket::Ptr udp_server_r = udp_server;
        udp_server->setOnRead([&ref, udp_server_r](const Buffer::Ptr &buf, struct sockaddr *addr, int) {
            ref.inputRtp(udp_server_r, buf->data(), buf->size(), addr);
        });
        vec_udp_server_.emplace_back(udp_server);
    });

    start_rtcp_server(rtcp_port);

    rtp_tcp_server_ = std::make_shared<TcpServer>();
    rtp_tcp_server_->start<RtpSession>(rtp_port, local_ip_);
}


void RtpServer::start(std::uint32_t rtp_port, std::uint32_t rtcp_port, const std::string& device_id) {    
    rtp_port_ = rtp_port;
    rtcp_port_ = rtcp_port;
    device_id_ = device_id;

    rtp_udp_server_ = std::make_shared<Socket>(nullptr, true);
    if (!rtp_udp_server_->bindUdpSock(rtp_port, local_ip_)) {
        ErrorL << "bindUdpSock on " << rtp_port << " failed:" << get_uv_errmsg(true);
        return ;
    }
    SockUtil::setRecvBuf(rtp_udp_server_->rawFD(), 8 * 1024 * 1024);


    rtp_process_ = RtpSelector::Instance().getProcess(device_id);
    rtp_udp_server_->setOnRead([this](const Buffer::Ptr &buf,
                                                            struct sockaddr *addr,
                                                            int addr_len) {
        rtp_process_->inputRtp(true, rtp_udp_server_, buf->data(), buf->size(), addr);
    });

    start_rtcp_server(rtcp_port);
    
    rtp_tcp_server_ = std::make_shared<TcpServer>();
    (*rtp_tcp_server_)[RtpSession::kStreamID] = device_id;
    rtp_tcp_server_->start<RtpSession>(rtp_port, local_ip_);
}

void RtpServer::setOnDetach(const std::function<void()> &cb){
    if(rtp_process_){
        rtp_process_->setOnDetach(cb);
    }
}

void RtpServer::start_rtcp_server(std::uint32_t port) {
    rtcp_server_ = std::make_shared<Socket>(nullptr, true);
    if (!rtcp_server_->bindUdpSock(port, local_ip_)) {
        ErrorL << "bindUdpSock on 0.0.0.0:" << port << " failed:" << get_uv_errmsg(true);
        return ;
    }
    SockUtil::setRecvBuf(rtcp_server_->rawFD(), 8 * 1024 * 1024);
    rtcp_server_->setOnRead([this](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len){
        InfoL << "rtcp packet size =" << buf->size();
        const char * rtp_buf = buf->data();
        int rtp_buf_len = buf->size();
        if (rtp_buf_len < 12) {
            return ;
        }

        uint32_t ssrc = 0;
        std::memcpy(&ssrc, rtp_buf + 4, 4);
        static const char s_cname[] = "test";
        uint8_t aui8Rtcp[32 + 10 + sizeof(s_cname) + 1] = {0};
        uint8_t *pui8Rtcp_RR = aui8Rtcp, *pui8Rtcp_SDES = pui8Rtcp_RR + 32;
                    
        pui8Rtcp_RR[0] = 0x81;/* 1 report block */
        pui8Rtcp_RR[1] = 0xC9;//RTCP_RR
        pui8Rtcp_RR[2] = 0x00;
        pui8Rtcp_RR[3] = 0x07;/* length in words - 1 */

        uint32_t own_ssrc = htonl(ssrc + 1);
        // our own SSRC: we use the server's SSRC + 1 to avoid conflicts
        std::memcpy(&pui8Rtcp_RR[4], &own_ssrc, 4);
        
        ssrc = htonl(ssrc);
        // server SSRC
        std::memcpy(&pui8Rtcp_RR[8], &ssrc, 4);

        // CNAME
        pui8Rtcp_SDES[0] = 0x81;
        pui8Rtcp_SDES[1] = 0xCA;
        pui8Rtcp_SDES[2] = 0x00;
        pui8Rtcp_SDES[3] = 0x06;

        std::memcpy(&pui8Rtcp_SDES[4], &ssrc, 4);

        pui8Rtcp_SDES[8] = 0x01;
        pui8Rtcp_SDES[9] = 0x0f;
        std::memcpy(&pui8Rtcp_SDES[10], s_cname, sizeof(s_cname));
        pui8Rtcp_SDES[10 + sizeof(s_cname)] = 0x00;

        rtcp_server_->send((char *)aui8Rtcp, sizeof(aui8Rtcp), addr, addr_len);
    });
}

                                                  
}//namespace mediakit
