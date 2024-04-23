#ifndef SESSION_RTSPSESSION_H_
#define SESSION_RTSPSESSION_H_

#include <set>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "Util/util.h"
#include "Common/config.h"
#include "Network/TcpSession.h"
#include "RtspMediaSource.h"
#include "RtspSplitter.h"
#include "RtpReceiver.h"
#include "RtspMediaSourceImp.h"
#include "Common/Stamp.h"

namespace mediakit {

class RtspSession : public TcpSession, public RtspSplitter, public RtpReceiver, public MediaSourceEvent
{
public:
    typedef std::shared_ptr<RtspSession> Ptr;
    RtspSession(const Socket::Ptr &sock);
    virtual ~RtspSession();
    void onRecv(const Buffer::Ptr &buf) override;
    void onError(const SockException &err) override;
    void onManager() override;
    bool isPlayer() { return !_push_src; };
    uint64_t getUptime() { return _alive_ticker.createdTime(); };
    std::vector<Track::Ptr> getTracks();
    const string &getSchema() { return _media_info._schema;};
    const string &getVhost() { return _media_info._vhost;};
    const string &getApp() { return _media_info._app; };
    const string &getId() { return _media_info._streamid;};
    string getOriginUrl() { return _media_info._full_url; };
    int totalReaderCount();
    uint64_t totalBytesUsage() { return _bytes_usage; };

protected:
    /////RtspSplitter override/////
    //收到完整的rtsp包回调，包括sdp等content数据
    void onWholeRtspPacket(Parser &parser) override;
    //收到rtp包回调
    void onRtpPacket(const char *data, uint64_t len) override;
    //从rtsp头中获取Content长度
    int64_t getContentLength(Parser &parser) override;

    ////RtpReceiver override////
    void onRtpSorted(const RtpPacket::Ptr &rtp, int track_idx) override;

    ///////MediaSourceEvent override///////
    bool close(MediaSource &sender, bool force) override;
    int totalReaderCount(MediaSource &sender) override;
    MediaOriginType getOriginType(MediaSource &sender) const override;
    string getOriginUrl(MediaSource &sender) const override;
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;

    /////TcpSession override////
    int send(Buffer::Ptr pkt) override;
    
private:
    //处理options方法,获取服务器能力
    void handleReq_Options(const Parser &parser);
    //处理describe方法，请求服务器rtsp sdp信息
    void handleReq_Describe(const Parser &parser);
    //处理ANNOUNCE方法，请求推流，附带sdp
    void handleReq_ANNOUNCE(const Parser &parser);
    //处理record方法，开始推流
    void handleReq_RECORD(const Parser &parser);
    //处理setup方法，播放和推流协商rtp传输方式用
    void handleReq_Setup(const Parser &parser);
    //处理play方法，开始或恢复播放
    void handleReq_Play(const Parser &parser);
    //处理pause方法，暂停播放
    void handleReq_Pause(const Parser &parser);
    //处理teardown方法，结束播放
    void handleReq_Teardown(const Parser &parser);
    //处理SET_PARAMETER、GET_PARAMETER方法，一般用于心跳
    void handleReq_SET_PARAMETER(const Parser &parser);
    //海康私有信令HEARTBEAT，一般用于心跳
    void handleReq_Heartbeat(const Parser &parser);
    //rtsp资源未找到
    void send_StreamNotFound();
    //不支持的传输模式
    void send_UnsupportedTransport();
    //会话id错误
    void send_SessionNotFound();
    //一般rtsp服务器打开端口失败时触发
    void send_NotAcceptable();
    //获取track下标
    int getTrackIndexByTrackType(TrackType type);
    int getTrackIndexByControlSuffix(const string &control_suffix);
    int getTrackIndexByInterleaved(int interleaved);
    void sendRtpPacket(const RtspMediaSource::RingDataType &pkt);;
    bool sendRtspResponse(const string &res_code, const std::initializer_list<string> &header, const string &sdp = "", const char *protocol = "RTSP/1.0");
    bool sendRtspResponse(const string &res_code, const StrCaseMap &header = StrCaseMap(), const string &sdp = "", const char *protocol = "RTSP/1.0");
private:
    bool _enable_send_rtp = false;
    Rtsp::eRtpType _rtp_type = Rtsp::RTP_Invalid;
    int _cseq = 0;
    //rtsp推流起始时间戳，目的是为了同步
    int64_t _start_stamp[2] = {-1, -1};
    //消耗的总流量
    uint64_t _bytes_usage = 0;
    //ContentBase
    string _content_base;
    //Session号
    string _sessionid;
    //用于判断客户端是否超时
    Ticker _alive_ticker;

    //url解析后保存的相关信息
    MediaInfo _media_info;
    //rtsp推流相关绑定的源
    RtspMediaSourceImp::Ptr _push_src;
    //rtsp播放器绑定的直播源
    std::weak_ptr<RtspMediaSource> _play_src;
    //直播源读取器
    RtspMediaSource::RingType::RingReader::Ptr _play_reader;
    //sdp里面有效的track,包含音频或视频
    vector<SdpTrack::Ptr> _sdp_track;
};
} /* namespace mediakit */

#endif /* SESSION_RTSPSESSION_H_ */
