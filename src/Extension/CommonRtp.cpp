#include <cmath>

#include "CommonRtp.h"
#include "Util/logger.h"

using namespace toolkit;

CommonRtpDecoder::CommonRtpDecoder(CodecId codec, int max_frame_size ){
    _codec = codec;
    _max_frame_size = max_frame_size;
    obtainFrame();
}

CodecId CommonRtpDecoder::getCodecId() const {
    return _codec;
}

void CommonRtpDecoder::obtainFrame() {
    _frame = ResourcePoolHelper<FrameImp>::obtainObj();
    _frame->_buffer.clear();
    _frame->_prefix_size = 0;
    _frame->_dts = 0;
    _frame->_codec_id = _codec;
}

bool CommonRtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool){
    record_rtp_sequence(rtp->sequence);

    auto payload = rtp->data() + rtp->offset;
    auto size = rtp->size() - rtp->offset;
    if (size <= 0) {
        return false;
    }

    if (_frame->_dts != rtp->timeStamp || _frame->_buffer.size() > _max_frame_size) {
        statistics_rtp_loss_rate();

        if (!_frame->_buffer.empty()) {
            RtpCodec::inputFrame(_frame);
        }

        obtainFrame();
        _frame->_dts = rtp->timeStamp;
        _drop_flag = false;
    } else if (_last_seq != 0 && (uint16_t)(_last_seq + 1) != rtp->sequence) {
        WarnL << "rtp loss:" << _last_seq << " -> " << rtp->sequence;
        _drop_flag = true;
        _frame->_buffer.clear();
    }

    if (!_drop_flag) {
        _frame->_buffer.append(payload, size);
    }

    _last_seq = rtp->sequence;
    return false;
}

void CommonRtpDecoder::record_rtp_sequence(std::uint16_t rtp_sequence) {
    seq_sei_.emplace(rtp_sequence);
}

void CommonRtpDecoder::statistics_rtp_loss_rate() {
    if(seq_sei_.empty()) {
        return ;
    }
    int real_rtp_num = seq_sei_.size();
    int nominal_rtp_num = *seq_sei_.rbegin() - *seq_sei_.begin() + 1;

    if(nominal_rtp_num < 0 || nominal_rtp_num > 60000) {
        nominal_rtp_num_ = 0;
        real_rtp_num_ = 0;
        seq_sei_.clear();
        return ;
    }

    real_rtp_num_ += real_rtp_num;
    nominal_rtp_num_ += nominal_rtp_num;
    
    if (nominal_rtp_num_ >= 1000) {
        rtp_loss_rate_ = std::round(((nominal_rtp_num_ - real_rtp_num_) * 1000) / (nominal_rtp_num_ * 1.0));
        nominal_rtp_num_ = 0;
        real_rtp_num_ = 0;
    }
    seq_sei_.clear();
}

int CommonRtpDecoder::get_rtp_loss_rate() {
    return rtp_loss_rate_;
}

////////////////////////////////////////////////////////////////

CommonRtpEncoder::CommonRtpEncoder(CodecId codec, uint32_t ssrc, uint32_t mtu_size,
                                   uint32_t sample_rate,  uint8_t payload_type, uint8_t interleaved)
        : CommonRtpDecoder(codec), RtpInfo(ssrc, mtu_size, sample_rate, payload_type, interleaved) {
}

void CommonRtpEncoder::inputFrame(const Frame::Ptr &frame){
    GET_CONFIG(uint32_t, cycleMS, Rtp::kCycleMS);
    auto stamp = frame->dts() % cycleMS;
    auto ptr = frame->data() + frame->prefixSize();
    auto len = frame->size() - frame->prefixSize();
    auto remain_size = len;
    const auto max_rtp_size = _ui32MtuSize - 20;

    while (remain_size > 0) {
        auto rtp_size = remain_size > max_rtp_size ? max_rtp_size : remain_size;
        RtpCodec::inputRtp(makeRtp(getTrackType(), ptr, rtp_size, false, stamp), false);
        ptr += rtp_size;
        remain_size -= rtp_size;
    }
}