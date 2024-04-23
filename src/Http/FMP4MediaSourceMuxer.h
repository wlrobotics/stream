#ifndef ZLMEDIAKIT_FMP4MEDIASOURCEMUXER_H
#define ZLMEDIAKIT_FMP4MEDIASOURCEMUXER_H

#include "Http/FMP4MediaSource.h"
#include "Http/MP4Muxer.h"

namespace mediakit {

class FMP4MediaSourceMuxer : public MP4MuxerMemory,
                                          public MediaSourceEventInterceptor,
                                          public std::enable_shared_from_this<FMP4MediaSourceMuxer> {
public:
    using Ptr = std::shared_ptr<FMP4MediaSourceMuxer>;

    FMP4MediaSourceMuxer(const string &vhost,
                                const string &app,
                                const string &stream_id) {
        _media_src = std::make_shared<FMP4MediaSource>(vhost, app, stream_id);
    }

    ~FMP4MediaSourceMuxer() override = default;

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        setDelegate(listener);
        _media_src->setListener(shared_from_this());
    }

    int readerCount() const{
        return _media_src->readerCount();
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    void inputFrame(const Frame::Ptr &frame) override {
        MP4MuxerMemory::inputFrame(frame);
    }

    bool isEnabled() {
        return true;
    }

    void onAllTrackReady() {
        _media_src->set_mse_mime_type(mse_mime_type_);
        _media_src->setInitSegment(getInitSegment());
    }

protected:
    void onSegmentData(const std::string &buf, uint32_t stamp, bool key_frame) override {
        if (buf.empty()) {
            return;
        }
        FMP4Packet::Ptr packet = std::make_shared<FMP4Packet>(std::move(buf));
        packet->time_stamp = stamp;
        _media_src->onWrite(std::move(packet), key_frame);
    }

private:
    FMP4MediaSource::Ptr _media_src;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_FMP4MEDIASOURCEMUXER_H
