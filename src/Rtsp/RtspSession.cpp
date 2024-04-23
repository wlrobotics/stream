#include <atomic>
#include <iomanip>
#include "Common/config.h"
#include "Util/logger.h"
#include "RtspSession.h"
#include "Util/mini.h"
#include "Util/base64.h"
#include "Util/onceToken.h"
#include "Util/TimeTicker.h"
#include "Network/sockutil.h"

using namespace toolkit;

namespace mediakit {

RtspSession::RtspSession(const Socket::Ptr &sock) : TcpSession(sock) {
    DebugP(this);
    GET_CONFIG(uint32_t,keep_alive_sec,Rtsp::kKeepAliveSecond);
    sock->setSendTimeOutSecond(keep_alive_sec);
}

RtspSession::~RtspSession() {
    DebugP(this);
}

void RtspSession::onError(const SockException &err) {
    WarnP(this) << "RTSP " << (_push_src ? "Pusher" : "Player")
                << " Break:" << err.what() << ","
                << _media_info._app << "/"
                << _media_info._streamid << ","
                << _alive_ticker.createdTime() / 1000 << "s,"
                << _bytes_usage / 1024.0 / 1024.0 << "MB";
}

void RtspSession::onManager() {
    GET_CONFIG(uint32_t,handshake_sec,Rtsp::kHandshakeSecond);
    GET_CONFIG(uint32_t,keep_alive_sec,Rtsp::kKeepAliveSecond);

    if (_alive_ticker.createdTime() > handshake_sec * 1000) {
        if (_sessionid.size() == 0) {
            shutdown(SockException(Err_timeout, "illegal connection"));
            return;
        }
    }

    if (_push_src && _alive_ticker.elapsedTime() > keep_alive_sec * 1000) {
        shutdown(SockException(Err_timeout, "rtsp push session timeouted"));
        return;
    }
}

void RtspSession::onRecv(const Buffer::Ptr &buf) {
    _alive_ticker.resetTime();
    _bytes_usage += buf->size();
    HttpRequestSplitter::input(buf->data(), buf->size());    
}

void RtspSession::onWholeRtspPacket(Parser &parser) {
    string method = parser.Method(); //提取出请求命令字
    _cseq = atoi(parser["CSeq"].data());
    if(_content_base.empty() && method != "GET"){
        _content_base = parser.Url();
        _media_info.parse(parser.FullUrl());
        _media_info._schema = RTSP_SCHEMA;
    }

    typedef void (RtspSession::*rtsp_request_handler)(const Parser &parser);
    static unordered_map<string, rtsp_request_handler> s_cmd_functions;
    static onceToken token( []() {
        s_cmd_functions.emplace("OPTIONS",&RtspSession::handleReq_Options);
        s_cmd_functions.emplace("DESCRIBE",&RtspSession::handleReq_Describe);
        s_cmd_functions.emplace("ANNOUNCE",&RtspSession::handleReq_ANNOUNCE);
        s_cmd_functions.emplace("RECORD",&RtspSession::handleReq_RECORD);
        s_cmd_functions.emplace("SETUP",&RtspSession::handleReq_Setup);
        s_cmd_functions.emplace("PLAY",&RtspSession::handleReq_Play);
        s_cmd_functions.emplace("PAUSE",&RtspSession::handleReq_Pause);
        s_cmd_functions.emplace("TEARDOWN",&RtspSession::handleReq_Teardown);
        s_cmd_functions.emplace("SET_PARAMETER",&RtspSession::handleReq_SET_PARAMETER);
        s_cmd_functions.emplace("GET_PARAMETER",&RtspSession::handleReq_SET_PARAMETER);
        s_cmd_functions.emplace("HEARTBEAT",&RtspSession::handleReq_Heartbeat);
    }, []() {});

    auto it = s_cmd_functions.find(method);
    if (it == s_cmd_functions.end()) {
        sendRtspResponse("403 Forbidden");
        shutdown(SockException(Err_shutdown,StrPrinter << "403 Forbidden:" << method));
        return;
    }

    auto &fun = it->second;
    try {
        (this->*fun)(parser);
    }catch (SockException &ex){
        if(ex){
            shutdown(ex);
        }
    }catch (exception &ex){
        shutdown(SockException(Err_shutdown,ex.what()));
    }
    parser.Clear();
}

void RtspSession::onRtpPacket(const char *data, uint64_t len) {
    if(!_push_src){
        return;
    }

    uint8_t interleaved = data[1];
    if(interleaved % 2 == 0){
        auto track_idx = getTrackIndexByInterleaved(interleaved);
        handleOneRtp(track_idx, _sdp_track[track_idx]->_type, _sdp_track[track_idx]->_samplerate, (unsigned char *) data + 4, len - 4);
    }
}

int64_t RtspSession::getContentLength(Parser &parser) {
    if(parser.Method() == "POST"){
        //http post请求的content数据部分是base64编码后的rtsp请求信令包
        return remainDataSize();
    }
    return RtspSplitter::getContentLength(parser);
}

std::vector<Track::Ptr> RtspSession::getTracks() {
    if (_push_src) {
        return _push_src->getTracks();
    } else {
        if(!_play_src.expired()) {                
            auto media_src = _play_src.lock();
            return media_src ? media_src->getTracks() : vector<Track::Ptr>();
        }
    }
}

int RtspSession::totalReaderCount() {
    if (_push_src) {
        return _push_src->totalReaderCount();
    } else {
        if(!_play_src.expired()) {
            auto mdia_src = _play_src.lock();
            return mdia_src?mdia_src->totalReaderCount() : 0;
        }
    }
}

void RtspSession::handleReq_Options(const Parser &parser) {
    sendRtspResponse("200 OK",{
                                "Public",
                                "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, ANNOUNCE, RECORD, SET_PARAMETER, GET_PARAMETER"
                               });
}

void RtspSession::handleReq_Heartbeat(const Parser &parser) {
    sendRtspResponse("200 OK");
}

void RtspSession::handleReq_ANNOUNCE(const Parser &parser) {
    auto src = dynamic_pointer_cast<RtspMediaSource>(MediaSource::find(RTSP_SCHEMA,
                                                                       _media_info._vhost,
                                                                       _media_info._app,
                                                                       _media_info._streamid));
    if(src){
        sendRtspResponse("406 Not Acceptable", {"Content-Type", "text/plain"}, "Already publishing.");
        string err = StrPrinter << "ANNOUNCE:"
                                << "Already publishing:"
                                << _media_info._vhost << " "
                                << _media_info._app << " "
                                << _media_info._streamid << endl;
        throw SockException(Err_shutdown,err);
    }

    if(_media_info._app.empty() || _media_info._streamid.empty()){
        sendRtspResponse("403 Forbidden", {"Content-Type", "text/plain"}, "rtsp url is illegal");
        throw SockException(Err_shutdown, StrPrinter << "rtsp url is illegal");
    }

    SdpParser sdpParser(parser.Content());
    _sessionid = makeRandStr(12);
    _sdp_track = sdpParser.getAvailableTrack();

    _push_src = std::make_shared<RtspMediaSourceImp>(_media_info._vhost, _media_info._app, _media_info._streamid);
    _push_src->setListener(dynamic_pointer_cast<MediaSourceEvent>(shared_from_this()));
    _push_src->setProtocolTranslation();
    _push_src->setSdp(sdpParser.toString());

    sendRtspResponse("200 OK",{"Content-Base", _content_base + "/"});
}

void RtspSession::handleReq_RECORD(const Parser &parser){
    if (_sdp_track.empty() || parser["Session"] != _sessionid) {
        send_SessionNotFound();
        throw SockException(Err_shutdown, _sdp_track.empty() ? "can not find any availabe track when record" : "session not found when record");
    }

    _StrPrinter rtp_info;
    for(auto &track : _sdp_track){
        if (track->_inited == false) {
            //还有track没有setup
            shutdown(SockException(Err_shutdown,"track not setuped"));
            return;
        }
        rtp_info << "url=" << _content_base << "/" << track->_control_surffix << ",";
    }
    rtp_info.pop_back();
    
    sendRtspResponse("200 OK", {"RTP-Info",rtp_info});
}

void RtspSession::handleReq_Describe(const Parser &parser) {
    InfoL << "pull rtsp stream:" << _media_info._streamid;
    TraceP(this);
    weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
    MediaSource::findAsync(_media_info, weakSelf.lock(), [weakSelf](const MediaSource::Ptr &src){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        auto rtsp_src = dynamic_pointer_cast<RtspMediaSource>(src);
        if (!rtsp_src) {
            //未找到相应的MediaSource
            string err = StrPrinter << "no such stream:" << strongSelf->_media_info._app << "/" << strongSelf->_media_info._streamid;
            strongSelf->send_StreamNotFound();
            strongSelf->shutdown(SockException(Err_shutdown,err));
            return;
        }
        //找到了相应的rtsp流
        strongSelf->_sdp_track = SdpParser(rtsp_src->getSdp()).getAvailableTrack();
        if (strongSelf->_sdp_track.empty()) {
            //该流无效
            DebugL << "无trackInfo，该流无效";
            strongSelf->send_StreamNotFound();
            strongSelf->shutdown(SockException(Err_shutdown,"can not find any available track in sdp"));
            return;
        }
        strongSelf->_sessionid = makeRandStr(12);
        strongSelf->_play_src = rtsp_src;
        for(auto &track : strongSelf->_sdp_track){
            track->_ssrc = rtsp_src->getSsrc(track->_type);
            track->_seq = rtsp_src->getSeqence(track->_type);
            track->_time_stamp = rtsp_src->getTimeStamp(track->_type);
        }

        strongSelf->sendRtspResponse("200 OK",
                                     {"Content-Base", strongSelf->_content_base + "/",
                                      "x-Accept-Retransmit","our-retransmit",
                                      "x-Accept-Dynamic-Rate","1"
                                     },rtsp_src->getSdp());
    });
}

inline void RtspSession::send_StreamNotFound() {
    sendRtspResponse("404 Stream Not Found",{"Connection","Close"});
}

inline void RtspSession::send_UnsupportedTransport() {
    sendRtspResponse("461 Unsupported Transport",{"Connection","Close"});
}

inline void RtspSession::send_SessionNotFound() {
    sendRtspResponse("454 Session Not Found",{"Connection","Close"});
}

void RtspSession::handleReq_Setup(const Parser &parser) {
    auto controlSuffix = split(parser.FullUrl(),"/").back();
    if(controlSuffix.front() == '/'){
        controlSuffix = controlSuffix.substr(1);
    }
    int trackIdx = getTrackIndexByControlSuffix(controlSuffix);
    SdpTrack::Ptr &trackRef = _sdp_track[trackIdx];
    if (trackRef->_inited && (_rtp_type != Rtsp::RTP_UDP)) {
        throw SockException(Err_shutdown, "can not setup one track twice");
    }
    trackRef->_inited = true;

    if(parser["Transport"].find("TCP") != string::npos){
        _rtp_type = Rtsp::RTP_TCP;
    } else {
        _rtp_type = Rtsp::RTP_UDP;
        send_UnsupportedTransport(); 
        return ;
    }

    RtspSplitter::enableRecvRtp(true);

    if(_push_src){
        //rtsp推流时，interleaved由推流者决定
        auto key_values =  Parser::parseArgs(parser["Transport"],";","=");
        int interleaved_rtp = -1 , interleaved_rtcp = -1;
        if(2 == sscanf(key_values["interleaved"].data(),"%d-%d",&interleaved_rtp,&interleaved_rtcp)){
            trackRef->_interleaved = interleaved_rtp;
        }else{
            throw SockException(Err_shutdown, "can not find interleaved when setup of rtp over tcp");
        }
    } else {
        //rtsp播放时，由于数据共享分发，所以interleaved必须由服务器决定
        trackRef->_interleaved = 2 * trackRef->_type;
    }
    sendRtspResponse("200 OK",
                     {"Transport", StrPrinter << "RTP/AVP/TCP;unicast;"
                                              << "interleaved=" << (int) trackRef->_interleaved << "-"
                                              << (int) trackRef->_interleaved + 1 << ";"
                                              << "ssrc=" << printSSRC(trackRef->_ssrc),
                      "x-Transport-Options", "late-tolerance=1.400000",
                      "x-Dynamic-Rate", "1"
                     });
}

void RtspSession::handleReq_Play(const Parser &parser) {
    if (_sdp_track.empty() || parser["Session"] != _sessionid) {
        send_SessionNotFound();
        throw SockException(Err_shutdown, _sdp_track.empty() ? "can not find any available track when play" : "session not found when play");
    }
    auto play_src = _play_src.lock();
    if(!play_src){
        send_StreamNotFound();
        shutdown(SockException(Err_shutdown,"rtsp stream released"));
        return;
    }

    _enable_send_rtp = false;

    _StrPrinter rtp_info;
    for (auto &track : _sdp_track) {
        if (track->_inited == false) {
            //还有track没有setup
            shutdown(SockException(Err_shutdown, "track not setuped"));
            return;
        }
        track->_ssrc = play_src->getSsrc(track->_type);
        track->_seq = play_src->getSeqence(track->_type);
        track->_time_stamp = play_src->getTimeStamp(track->_type);

        rtp_info << "url=" << _content_base << "/" << track->_control_surffix << ";"
                 << "seq=" << track->_seq << ";"
                 << "rtptime=" << (int) (track->_time_stamp * (track->_samplerate / 1000)) << ",";
    }

    rtp_info.pop_back();
    
    sendRtspResponse("200 OK",
                    {"Range", StrPrinter << "npt=" << setiosflags(ios::fixed) 
                                          << setprecision(2) 
                                          << play_src->getTimeStamp(TrackInvalid) / 1000.0,
                     "RTP-Info",rtp_info
                    });

    _enable_send_rtp = true;

    if (!_play_reader) {
        std::weak_ptr<RtspSession> weakSelf = dynamic_pointer_cast<RtspSession>(shared_from_this());
        _play_reader = play_src->getRing()->attach(getPoller());
        _play_reader->setDetachCB([weakSelf]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            strongSelf->shutdown(SockException(Err_shutdown, "rtsp ring buffer detached"));
        });
        _play_reader->setReadCB([weakSelf](const RtspMediaSource::RingDataType &pack) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            if (strongSelf->_enable_send_rtp) {
                strongSelf->sendRtpPacket(pack);
            }
        });
    }
}

void RtspSession::handleReq_Pause(const Parser &parser) {
    if (parser["Session"] != _sessionid) {
        send_SessionNotFound();
        throw SockException(Err_shutdown,"session not found when pause");
    }

    sendRtspResponse("200 OK");
    _enable_send_rtp = false;
}

void RtspSession::handleReq_Teardown(const Parser &parser) {
    sendRtspResponse("200 OK");
    throw SockException(Err_shutdown,"rtsp player send teardown request");
}

void RtspSession::handleReq_SET_PARAMETER(const Parser &parser) {
    sendRtspResponse("200 OK");
}

inline void RtspSession::send_NotAcceptable() {
    sendRtspResponse("406 Not Acceptable",{"Connection","Close"});
}

void RtspSession::onRtpSorted(const RtpPacket::Ptr &rtp, int track_idx) {
    if (_start_stamp[track_idx] == -1) {
        //记录起始时间戳
        _start_stamp[track_idx] = rtp->timeStamp;
    }
    //时间戳增量
    rtp->timeStamp -= _start_stamp[track_idx];
    _push_src->onWrite(rtp, false);
}

static string dateStr(){
    char buf[64];
    time_t tt = time(NULL);
    strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

bool RtspSession::sendRtspResponse(const string &res_code, const StrCaseMap &header_const, const string &sdp, const char *protocol){
    auto header = header_const;
    header.emplace("CSeq",StrPrinter << _cseq);
    if(!_sessionid.empty()){
        header.emplace("Session", _sessionid);
    }

    header.emplace("Server",SERVER_NAME);
    header.emplace("Date",dateStr());

    if(!sdp.empty()){
        header.emplace("Content-Length",StrPrinter << sdp.size());
        header.emplace("Content-Type","application/sdp");
    }

    _StrPrinter printer;
    printer << protocol << " " << res_code << "\r\n";
    for (auto &pr : header){
        printer << pr.first << ": " << pr.second << "\r\n";
    }

    printer << "\r\n";

    if(!sdp.empty()){
        printer << sdp;
    }
    return send(std::make_shared<BufferString>(std::move(printer))) > 0 ;
}

int RtspSession::send(Buffer::Ptr pkt){
    _bytes_usage += pkt->size();
    return TcpSession::send(std::move(pkt));
}

bool RtspSession::sendRtspResponse(const string &res_code,
                                        const std::initializer_list<string> &header,
                                        const string &sdp,
                                        const char *protocol) {
    string key;
    StrCaseMap header_map;
    int i = 0;
    for(auto &val : header){
        if(++i % 2 == 0){
            header_map.emplace(key,val);
        }else{
            key = val;
        }
    }
    return sendRtspResponse(res_code,header_map,sdp,protocol);
}

inline int RtspSession::getTrackIndexByTrackType(TrackType type) {
    for (unsigned int i = 0; i < _sdp_track.size(); i++) {
        if (type == _sdp_track[i]->_type) {
            return i;
        }
    }
    if(_sdp_track.size() == 1){
        return 0;
    }
    throw SockException(Err_shutdown, StrPrinter << "no such track with type:" << (int) type);
}

inline int RtspSession::getTrackIndexByControlSuffix(const string &controlSuffix) {
    for (unsigned int i = 0; i < _sdp_track.size(); i++) {
        if (controlSuffix == _sdp_track[i]->_control_surffix) {
            return i;
        }
    }
    if(_sdp_track.size() == 1){
        return 0;
    }
    throw SockException(Err_shutdown, StrPrinter << "no such track with suffix:" << controlSuffix);
}

inline int RtspSession::getTrackIndexByInterleaved(int interleaved){
    for (unsigned int i = 0; i < _sdp_track.size(); i++) {
        if (_sdp_track[i]->_interleaved == interleaved) {
            return i;
        }
    }
    if(_sdp_track.size() == 1){
        return 0;
    }
    throw SockException(Err_shutdown, StrPrinter << "no such track with interleaved:" << interleaved);
}

bool RtspSession::close(MediaSource &sender, bool force) {
    //此回调在其他线程触发
    if(!_push_src || (!force && _push_src->totalReaderCount())){
        return false;
    }
    string err = StrPrinter << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    safeShutdown(SockException(Err_shutdown,err));
    return true;
}

int RtspSession::totalReaderCount(MediaSource &sender) {
    return _push_src ? _push_src->totalReaderCount() : sender.readerCount();
}

MediaOriginType RtspSession::getOriginType(MediaSource &sender) const{
    return MediaOriginType::rtsp_push;
}

string RtspSession::getOriginUrl(MediaSource &sender) const {
    return _media_info._full_url;
}

std::shared_ptr<SockInfo> RtspSession::getOriginSock(MediaSource &sender) const {
    return const_cast<RtspSession *>(this)->shared_from_this();
}

void RtspSession::sendRtpPacket(const RtspMediaSource::RingDataType &pkt) {
    int i = 0;
    int size = pkt->size();
    setSendFlushFlag(false);
    pkt->for_each([&](const RtpPacket::Ptr &rtp) {
        if (++i == size) {
            setSendFlushFlag(true);
        }
        send(rtp);
    });
}

}
