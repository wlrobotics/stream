#ifndef ZLMEDIAKIT_RTPSERVER_H
#define ZLMEDIAKIT_RTPSERVER_H

#include <memory>
#include <vector>

#include "Network/Socket.h"
#include "Network/TcpServer.h"
#include "Rtp/RtpProcess.h"


namespace mediakit{

class RtpServer {
public:
    typedef std::shared_ptr<RtpServer> Ptr;

    RtpServer();
    ~RtpServer();

    void start(std::uint32_t rtp_port, std::uint32_t rtcp_port);

    void start(std::uint32_t   rtp_port, std::uint32_t rtcp_port, const std::string& device_id);
    
    std::uint32_t get_rtp_port()  { return rtp_port_;}

    void setOnDetach(const function<void()> &cb);
    
private:
    void start_rtcp_server(std::uint32_t port);
    
    toolkit::TcpServer::Ptr rtp_tcp_server_;
    toolkit::Socket::Ptr rtp_udp_server_;
    toolkit::Socket::Ptr rtcp_server_;

    RtpProcess::Ptr rtp_process_;

    std::uint32_t rtp_port_;
    std::uint32_t rtcp_port_;

    std::string device_id_;

    std::string local_ip_ = "0.0.0.0";

    std::vector<toolkit::Socket::Ptr> vec_udp_server_;
};

}
#endif
