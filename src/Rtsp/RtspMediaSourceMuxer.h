#ifndef ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H

#include "RtspMuxer.h"
#include "Rtsp/RtspMediaSource.h"

namespace mediakit {

class RtspMediaSourceMuxer : public RtspMuxer, public MediaSourceEventInterceptor,
                             public std::enable_shared_from_this<RtspMediaSourceMuxer> {
public:
    typedef std::shared_ptr<RtspMediaSourceMuxer> Ptr;

    RtspMediaSourceMuxer(const string &vhost,
                         const string &strApp,
                         const string &strId,
                         const TitleSdp::Ptr &title = nullptr) : RtspMuxer(title){
        _media_src = std::make_shared<RtspMediaSource>(vhost,strApp,strId);
        getRtpRing()->setDelegate(_media_src);
    }

    ~RtspMediaSourceMuxer() override{}

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        setDelegate(listener);
        _media_src->setListener(shared_from_this());
    }

    int readerCount() const{
        return _media_src->readerCount();
    }

    void setTimeStamp(uint32_t stamp){
        _media_src->setTimeStamp(stamp);
    }

    void onAllTrackReady(){
        _media_src->setSdp(getSdp());
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    void inputFrame(const Frame::Ptr &frame) override {
        RtspMuxer::inputFrame(frame);
    }

    bool isEnabled() {
        return true;
    }

private:
    RtspMediaSource::Ptr _media_src;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
