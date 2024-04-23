#pragma once

#include <unordered_map>
#include <mutex>
#include <list>
#include <string>

#include "Util/util.h"
#include "Util/TimeTicker.h"

namespace mediakit
{
    typedef struct
    {
        string codec_;
        string profile_;
        string level_;
        uint32_t frame_rate_;
        uint32_t byte_rate_;
        uint32_t bits_;
        uint32_t time_stamps_;
        uint32_t width_;
        uint32_t height_;
        uint32_t channels_;
        uint32_t sample_rates_;
        string meta_data_;
        std::uint64_t record_len;
        std::string record_time;
        std::uint32_t rtp_loss_rate = 0;
    } StreamInfo;

    class Monitor
    {
    public:
        string stream_url_;
        uint64_t bytes_in_;
        uint64_t bytes_in_audio_;
        uint64_t bytes_in_video_;
        uint64_t uptime_;
        uint32_t reader_;
        string schema_;
        string vhost_;
        string app_;
        string stream_id_;
        string src_ip_;
        string src_port_;
        string dst_ip_;
        string dst_port_;
        std::string rtp_loss_rate;

        StreamInfo stream_;
    };

    class Player : public Monitor
    {
    public:
        typedef std::shared_ptr<Player> Ptr;

    private:
    };

    class Publisher : public Monitor
    {
    public:
        typedef std::shared_ptr<Publisher> Ptr;
        list<Player::Ptr> play_list_;
    };

    class StreamStat
    {
    public:
        static std::string stat();
        static std::string stat_template();
        
    private:
        static void get_stat_data();
        static std::string format_xml();
        static std::unordered_map<string, Publisher::Ptr> stream_map_;
        static std::time_t stat_cool_time_;
    };
}
