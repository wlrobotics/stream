/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <algorithm>
#include "Common/config.h"
#include "strCoding.h"
#include "HttpSession.h"
#include "Util/base64.h"
#include "Util/SHA1.h"
#include "HookServer.h"
#include "json/json.h"

using namespace toolkit;

namespace mediakit {

HttpSession::HttpSession(const Socket::Ptr &pSock) : TcpSession(pSock) {
    TraceP(this);
    pSock->setSendTimeOutSecond(15);
}

HttpSession::~HttpSession() {
    TraceP(this);
}

int64_t HttpSession::onRecvHeader(const char *header,uint64_t len) {
    typedef void (HttpSession::*HttpCMDHandle)(int64_t &);
    static unordered_map<string, HttpCMDHandle> s_func_map;
    static onceToken token([]() {
        s_func_map.emplace("GET",&HttpSession::Handle_Req_GET);
    }, nullptr);

    _parser.Parse(header);
    urlDecode(_parser);
    string cmd = _parser.Method();
    auto it = s_func_map.find(cmd);
    if (it == s_func_map.end()) {
        WarnP(this) << "不支持该命令:" << cmd;
        sendResponse("405 Not Allowed", true);
        return 0;
    }

    //跨域
    _origin = _parser["Origin"];

    //默认后面数据不是content而是header
    int64_t content_len = 0;
    auto &fun = it->second;
    try {
        (this->*fun)(content_len);
    }catch (exception &ex){
        shutdown(SockException(Err_shutdown,ex.what()));
    }

    //清空解析器节省内存
    _parser.Clear();
    //返回content长度
    return content_len;
}

void HttpSession::onRecvContent(const char *data,uint64_t len) {
    if(_contentCallBack){
        if(!_contentCallBack(data,len)){
            _contentCallBack = nullptr;
        }
    }
}

void HttpSession::onRecv(const Buffer::Ptr &pBuf) {
    _ticker.resetTime();
    input(pBuf->data(),pBuf->size());
}

void HttpSession::onError(const SockException& err) {
    if(_is_live_stream){
        uint64_t duration = _ticker.createdTime()/1000;
        WarnP(this) << "HTTP Player Break:"
                    << err.what() << ","
                    << _mediaInfo._app << "/"
                    << _mediaInfo._streamid << ","
                    << duration << "s,"
                    << _total_bytes_usage / 1024.0 / 1024.0 << "MB";
        return;
    }

    if(_ticker.createdTime() < 10 * 1000){
        TraceP(this) << err.what();
    }else{
        WarnP(this) << err.what();
    }
}

void HttpSession::onManager() {
    if(_ticker.elapsedTime() > 15 * 1000){
        shutdown(SockException(Err_timeout,"session timeouted"));
    }
}

bool HttpSession::checkWebSocket(){
    auto Sec_WebSocket_Key = _parser["Sec-WebSocket-Key"];
    if (Sec_WebSocket_Key.empty()) {
        return false;
    }
    auto Sec_WebSocket_Accept = encodeBase64(SHA1::encode_bin(Sec_WebSocket_Key 
                                                              + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

    KeyValue headerOut;
    headerOut["Upgrade"] = "websocket";
    headerOut["Connection"] = "Upgrade";
    headerOut["Sec-WebSocket-Accept"] = Sec_WebSocket_Accept;
    if (!_parser["Sec-WebSocket-Protocol"].empty()) {
        headerOut["Sec-WebSocket-Protocol"] = _parser["Sec-WebSocket-Protocol"];
    }

    auto res_cb = [this, headerOut]() {
        _live_over_websocket = true;
        sendResponse("101 Switching Protocols", false, nullptr, headerOut, nullptr, true);
    };

    if (checkLiveStreamFlv(res_cb)) {
        return true;
    }

    is_fmp4_websocket_ = true;
    if (checkLiveStreamFMP4(res_cb)) {
        return true;
    }

    if (!onWebSocketConnect(_parser)) {
        sendResponse("501 Not Implemented", true, nullptr, headerOut);
        return true;
    }
    sendResponse("101 Switching Protocols", false, nullptr, headerOut, nullptr, true);
    return true;
}

bool HttpSession::checkLiveStream(const string &schema, const string  &url_suffix, const function<void(const MediaSource::Ptr &src)> &cb){
    auto pos = strcasestr(_parser.Url().data(), url_suffix.data());
    if (!pos || pos + url_suffix.size() != 1 + &_parser.Url().back()) {
        return false;
    }

    _mediaInfo.parse(schema + "://" + _parser["Host"] + _parser.FullUrl());
    if (_mediaInfo._app.empty() || _mediaInfo._streamid.size() < url_suffix.size() + 1) {
        return false;
    }
    bool close_flag = !strcasecmp(_parser["Connection"].data(), "close");
    _mediaInfo._streamid.erase(_mediaInfo._streamid.size() - url_suffix.size());
    std::weak_ptr<HttpSession> weak_self = dynamic_pointer_cast<HttpSession>(shared_from_this());

    MediaSource::findAsync(_mediaInfo, weak_self.lock(), [weak_self, close_flag, cb](const MediaSource::Ptr &src) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        if (!src) {
            strong_self->sendNotFound(close_flag);
            return;
        }
        strong_self->_is_live_stream = true;
        cb(src);
    });
    
    return true;
}

static std::string generate_first_message(std::string mse_mime_type, bool old) {
    if(old) {
        std::string::size_type pos = mse_mime_type.find("avc1");
        if(pos != std::string::npos) {
            return mse_mime_type.substr(pos + 5, 6);
        } else {
            return "h265";
        }
    } else {
        Json::Value json_payload;
        json_payload["code"] = 0;
        json_payload["msg"] = "success";
        json_payload["data"]["mime_type"] = mse_mime_type;
        json_payload["data"]["redirect"] = false;
        Json::StreamWriterBuilder builder;
        builder.settings_["indentation"] = "";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        std::ostringstream os;
        writer->write(json_payload, &os);
        return os.str();
    }
}

bool HttpSession::checkLiveStreamFMP4(const function<void()> &cb){
    return checkLiveStream(FMP4_SCHEMA, ".mp4", [this, cb](const MediaSource::Ptr &src) {
        auto fmp4_src = dynamic_pointer_cast<FMP4MediaSource>(src);
        assert(fmp4_src);
        if (!cb) {
            //找到源，发送http头，负载后续发送
            sendResponse("200 OK", false, "video/mp4", KeyValue(), nullptr, true);
        } else {
            //自定义发送http头
            cb();
        }

        if(is_fmp4_websocket_) {
            std::string first_message = generate_first_message(fmp4_src->get_mse_mime_type(),
            _mediaInfo._param_strs.find("version") == std::string::npos);
            onWrite(std::make_shared<BufferString>(first_message), true);
        }

        onWrite(std::make_shared<BufferString>(fmp4_src->getInitSegment()), true);
        weak_ptr<HttpSession> weak_self = dynamic_pointer_cast<HttpSession>(shared_from_this());
        _fmp4_reader = fmp4_src->getRing()->attach(getPoller());
        _fmp4_reader->setDetachCB([weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->shutdown(SockException(Err_shutdown, "fmp4 ring buffer detached"));
        });
        _fmp4_reader->setReadCB([weak_self](const FMP4MediaSource::RingDataType &fmp4_list) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            int i = 0;
            int size = fmp4_list->size();
            fmp4_list->for_each([&](const FMP4Packet::Ptr &ts) {
                strong_self->onWrite(ts, ++i == size);
            });
        });
    });
}

bool HttpSession::checkLiveStreamFlv(const function<void()> &cb){
    return checkLiveStream(RTMP_SCHEMA, ".flv", [this, cb](const MediaSource::Ptr &src) {
        auto rtmp_src = dynamic_pointer_cast<RtmpMediaSource>(src);
        assert(rtmp_src);
        if (!cb) {
            //找到源，发送http头，负载后续发送
            sendResponse("200 OK",
                         false,
                         "video/x-flv",
                         KeyValue(),
                         nullptr,
                         true);
        } else {
            //自定义发送http头
            cb();
        }

        start(getPoller(), rtmp_src);
    });
}

void HttpSession::Handle_Req_GET(int64_t &content_len) {
    if (checkWebSocket()) {
        InfoL << "pull websocket stream:" << _mediaInfo._streamid;
        content_len = -1;
        _contentCallBack = [this](const char *data, uint64_t len) {
            WebSocketSplitter::decode((uint8_t *) data, len);
            //_contentCallBack是可持续的，后面还要处理后续数据
            return true;
        };
        return;
    }

    if (checkLiveStreamFMP4()) {
        InfoL << "pull http-mp4 stream:" << _mediaInfo._streamid;
        return;
    }

    if (checkLiveStreamFlv()) {
        InfoL << "pull http-flv stream:" << _mediaInfo._streamid;
        return;
    }

    auto [code, header, body] = HookServer::Instance().http_request(_parser);
    sendResponse(code.data(), true, nullptr, header, std::make_shared<HttpStringBody>(body));
}

static string dateStr() {
    char buf[64];
    time_t tt = time(NULL);
    strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

class AsyncSenderData {
public:
    friend class AsyncSender;
    typedef std::shared_ptr<AsyncSenderData> Ptr;
    AsyncSenderData(const TcpSession::Ptr &session, const HttpBody::Ptr &body, bool close_when_complete) {
        _session = dynamic_pointer_cast<HttpSession>(session);
        _body = body;
        _close_when_complete = close_when_complete;
    }
    ~AsyncSenderData() = default;
private:
    std::weak_ptr<HttpSession> _session;
    HttpBody::Ptr _body;
    bool _close_when_complete;
    bool _read_complete = false;
};

class AsyncSender {
public:
    typedef std::shared_ptr<AsyncSender> Ptr;
    static bool onSocketFlushed(const AsyncSenderData::Ptr &data) {
        if (data->_read_complete) {
            if (data->_close_when_complete) {
                //发送完毕需要关闭socket
                shutdown(data->_session.lock());
            }
            return false;
        }

        GET_CONFIG(uint32_t, sendBufSize, Http::kSendBufSize);
        data->_body->readDataAsync(sendBufSize, [data](const Buffer::Ptr &sendBuf) {
            auto session = data->_session.lock();
            if (!session) {
                return;
            }
            session->async([data, sendBuf]() {
                auto session = data->_session.lock();
                if (!session) {
                    return;
                }
                onRequestData(data, session, sendBuf);
            }, false);
        });
        return true;
    }

private:
    static void onRequestData(const AsyncSenderData::Ptr &data, const std::shared_ptr<HttpSession> &session, const Buffer::Ptr &sendBuf) {
        session->_ticker.resetTime();
        if (sendBuf && session->send(sendBuf) != -1) {
            //文件还未读完，还需要继续发送
            if (!session->isSocketBusy()) {
                //socket还可写，继续请求数据
                onSocketFlushed(data);
            }
            return;
        }
        //文件写完了
        data->_read_complete = true;
        if (!session->isSocketBusy() && data->_close_when_complete) {
            shutdown(session);
        }
    }

    static void shutdown(const std::shared_ptr<HttpSession> &session) {
        if(session){
            session->shutdown(SockException(Err_shutdown, StrPrinter << "close connection after send http body completed."));
        }
    }
};

static const string kDate = "Date";
static const string kServer = "Server";
static const string kConnection = "Connection";
static const string kKeepAlive = "Keep-Alive";
static const string kContentType = "Content-Type";
static const string kContentLength = "Content-Length";
static const string kAccessControlAllowOrigin = "Access-Control-Allow-Origin";
static const string kAccessControlAllowCredentials = "Access-Control-Allow-Credentials";

void HttpSession::sendResponse(const char *pcStatus,
                               bool bClose,
                               const char *pcContentType,
                               const HttpSession::KeyValue &header,
                               const HttpBody::Ptr &body,
                               bool no_content_length ){
    //body默认为空
    int64_t size = 0;
    if (body && body->remainSize()) {
        //有body，获取body大小
        size = body->remainSize();
    }

    if(no_content_length){
        //http-flv直播是Keep-Alive类型
        bClose = false;
    }else if(size >= INT64_MAX){
        //不固定长度的body，那么发送完body后应该关闭socket，以便浏览器做下载完毕的判断
        bClose = true;
    }

    HttpSession::KeyValue &headerOut = const_cast<HttpSession::KeyValue &>(header);
    headerOut.emplace(kDate, dateStr());
    headerOut.emplace(kServer, SERVER_NAME);
    headerOut.emplace(kConnection, bClose ? "close" : "keep-alive");
    if(!bClose){
        string keepAliveString = "timeout=";
        keepAliveString += std::to_string(15);
        keepAliveString += ", max=100";
        headerOut.emplace(kKeepAlive,std::move(keepAliveString));
    }

    if(!_origin.empty()){
        //设置跨域
        headerOut.emplace(kAccessControlAllowOrigin,_origin);
        headerOut.emplace(kAccessControlAllowCredentials, "true");
    }

    if(!no_content_length && size >= 0 && size < INT64_MAX){
        //文件长度为固定值,且不是http-flv强制设置Content-Length
        headerOut[kContentLength] = to_string(size);
    }

    if(size && !pcContentType){
        //有body时，设置缺省类型
        pcContentType = "text/plain";
    }

    //if(size && pcContentType)
    if(pcContentType){
        //有body时，设置文件类型
        string strContentType = pcContentType;
        strContentType += "; charset=utf-8";
        headerOut.emplace(kContentType,std::move(strContentType));
    }

    //发送http头
    string str;
    str.reserve(256);
    str += "HTTP/1.1 " ;
    str += pcStatus ;
    str += "\r\n";
    for (auto &pr : header) {
        str += pr.first ;
        str += ": ";
        str += pr.second;
        str += "\r\n";
    }
    str += "\r\n";
    SockSender::send(std::move(str));
    _ticker.resetTime();

    if(!size){
        //没有body
        if(bClose){
            shutdown(SockException(Err_shutdown,StrPrinter << "close connection after send http header completed with status code:" << pcStatus));
        }
        return;
    }

    //发送http body
    AsyncSenderData::Ptr data = std::make_shared<AsyncSenderData>(shared_from_this(),body,bClose);
    getSock()->setOnFlush([data](){
        return AsyncSender::onSocketFlushed(data);
    });
    AsyncSender::onSocketFlushed(data);
}

string HttpSession::urlDecode(const string &str){
    auto ret = strCoding::UrlDecode(str);
    return ret;
}

void HttpSession::urlDecode(Parser &parser){
    parser.setUrl(urlDecode(parser.Url()));
    for(auto &pr : _parser.getUrlArgs()){
        const_cast<string &>(pr.second) = urlDecode(pr.second);
    }
}

void HttpSession::sendNotFound(bool bClose) {
    sendResponse("404 Not Found", bClose,"text/html",KeyValue(),std::make_shared<HttpStringBody>("404 Not Found"));
}

void HttpSession::onWrite(const Buffer::Ptr &buffer, bool flush) {
    if(flush){
        //需要flush那么一次刷新缓存
        HttpSession::setSendFlushFlag(true);
    }

    _ticker.resetTime();
    if (!_live_over_websocket) {
        if(!(buffer->data()[0u] == 0x7b && buffer->data()[1] == 0x22)) {
            send(buffer);
        }
    } else {
        WebSocketHeader header;
        header._fin = true;
        header._reserved = 0;
        if(is_fmp4_websocket_) {
            if(websocket_first_send_) {
                header._opcode = WebSocketHeader::TEXT;
                websocket_first_send_ = false;
            } else {
                if(buffer->data()[0u] == 0x7b && buffer->data()[1] == 0x22) {
                    header._opcode = WebSocketHeader::TEXT;
                } else {
                    header._opcode = WebSocketHeader::BINARY;
                }
            }
        } else {
            header._opcode = WebSocketHeader::BINARY;
        }
        header._mask_flag = false;
        WebSocketSplitter::encode(header, buffer);
    }

    if (flush) {
        //本次刷新缓存后，下次不用刷新缓存
        HttpSession::setSendFlushFlag(false);
    }

    _total_bytes_usage += buffer->size();
}

void HttpSession::onWebSocketEncodeData(Buffer::Ptr buffer){
    _total_bytes_usage += buffer->size();
    send(std::move(buffer));
}

void HttpSession::onWebSocketDecodeComplete(const WebSocketHeader &header_in){
    WebSocketHeader& header = const_cast<WebSocketHeader&>(header_in);
    header._mask_flag = false;

    switch (header._opcode) {
        case WebSocketHeader::CLOSE: {
            encode(header, nullptr);
            shutdown(SockException(Err_shutdown, "recv close request from client"));
            break;
        }

        default : break;
    }
}

void HttpSession::onDetach() {
    shutdown(SockException(Err_shutdown,"rtmp ring buffer detached"));
}

std::shared_ptr<FlvMuxer> HttpSession::getSharedPtr(){
    return dynamic_pointer_cast<FlvMuxer>(shared_from_this());
}

} /* namespace mediakit */
