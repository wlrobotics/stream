/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_HTTPSESSION_H_
#define SRC_HTTP_HTTPSESSION_H_

#include <functional>

#include "Network/TcpSession.h"
#include "Rtmp/RtmpMediaSource.h"
#include "Rtmp/FlvMuxer.h"
#include "HttpRequestSplitter.h"
#include "WebSocketSplitter.h"
#include "Http/FMP4MediaSource.h"
#include "Http/HttpBody.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class HttpSession: public TcpSession,
                          public FlvMuxer,
                          public HttpRequestSplitter,
                          public WebSocketSplitter {
public:
    typedef std::shared_ptr<HttpSession> Ptr;
    typedef StrCaseMap KeyValue;
    friend class AsyncSender;

    HttpSession(const Socket::Ptr &pSock);
    ~HttpSession() override;

    void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override;
    void onManager() override;
    static string urlDecode(const string &str);
    uint64_t getUptime(){ return _ticker.createdTime();};

    const string& getSchema() {return _mediaInfo._schema;};
    const string& getVhost() {return _mediaInfo._vhost;};
    const string& getApp() {return _mediaInfo._app;};
    const string& getId() {return _mediaInfo._streamid;};
    string getOriginUrl() {return _mediaInfo._full_url;};
    uint64_t totalBytesUsage() {return _total_bytes_usage;};
protected:
    //FlvMuxer override
    void onWrite(const Buffer::Ptr &data, bool flush) override ;
    void onDetach() override;
    std::shared_ptr<FlvMuxer> getSharedPtr() override;

    //HttpRequestSplitter override
    int64_t onRecvHeader(const char *data,uint64_t len) override;
    void onRecvContent(const char *data,uint64_t len) override;

    /**
     * 重载之用于处理不定长度的content
     * 这个函数可用于处理大文件上传、http-flv推流
     * @param header http请求头
     * @param data content分片数据
     * @param len content分片数据大小
     * @param totalSize content总大小,如果为0则是不限长度content
     * @param recvedSize 已收数据大小
     */
    virtual void onRecvUnlimitedContent(const Parser &header,
                                        const char *data,
                                        uint64_t len,
                                        uint64_t totalSize,
                                        uint64_t recvedSize){
        shutdown(SockException(Err_shutdown,"http post content is too huge,default closed"));
    }

    /**
     * websocket客户端连接上事件
     * @param header http头
     * @return true代表允许websocket连接，否则拒绝
     */
    virtual bool onWebSocketConnect(const Parser &header){
        WarnP(this) << "http server do not support websocket default";
        return false;
    }

    //WebSocketSplitter override
    /**
     * 发送数据进行websocket协议打包后回调
     * @param buffer websocket协议数据
     */
    void onWebSocketEncodeData(Buffer::Ptr buffer) override;

    /**
     * 接收到完整的一个webSocket数据包后回调
     * @param header 数据包包头
     */
    void onWebSocketDecodeComplete(const WebSocketHeader &header_in) override;

private:
    void Handle_Req_GET(int64_t &content_len);

    bool checkLiveStream(const std::string &schema,
                              const std::string  &url_suffix,
                              const std::function<void(const MediaSource::Ptr &src)> &cb);

    bool checkLiveStreamFlv(const std::function<void()> &cb = nullptr);
    bool checkLiveStreamFMP4(const std::function<void()> &fmp4_list = nullptr);

    bool checkWebSocket();
    void urlDecode(Parser &parser);
    void sendNotFound(bool bClose);
    void sendResponse(const char *pcStatus,
                          bool bClose,
                          const char *pcContentType = nullptr,
                          const HttpSession::KeyValue &header = HttpSession::KeyValue(),
                          const HttpBody::Ptr &body = nullptr,
                          bool no_content_length = false);

private:
    bool _is_live_stream = false;
    bool _live_over_websocket = false;
    std::uint64_t _total_bytes_usage = 0;
    std::string _origin;
    Parser _parser;
    Ticker _ticker;
    MediaInfo _mediaInfo;
    FMP4MediaSource::RingType::RingReader::Ptr _fmp4_reader;
    std::function<bool (const char *data, uint64_t len) > _contentCallBack;
    bool is_fmp4_websocket_ = false;
    bool websocket_first_send_ = true;
};

} /* namespace mediakit */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
