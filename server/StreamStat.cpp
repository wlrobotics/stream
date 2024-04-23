#include "StreamStat.h"

#include "Common/config.h"
#include "tinyxml2/tinyxml2.h"
#include "Device/DeviceManager.h"
#include "Rtp/RtpSelector.h"
#include "Network/TcpServer.h"
#include "Rtsp/RtspSession.h"
#include "Http/HttpSession.h"
#include "Util/File.h"
#include "Config.h"

using namespace tinyxml2;

namespace mediakit {

std::unordered_map<std::string, Publisher::Ptr> StreamStat::stream_map_;

std::time_t StreamStat::stat_cool_time_ = 0;

void StreamStat::get_stat_data() {
    if(stat_cool_time_ != 0 && (stat_cool_time_ > std::time(nullptr) - 5)) {
        return ;
    }
    stat_cool_time_ = std::time(nullptr);

    stream_map_.clear();

    std::unordered_map<std::string, std::list<Player::Ptr>> player_map;
    
    DeviceManager::Instance().for_each_device([&](const IDevice::Ptr &device){
        if(device->get_device_type() == DeviceType::GB28181) {
            return ;
        }
        
        auto publisher = std::make_shared<Publisher>();
        publisher->reader_ = device->totalReaderCount();
        publisher->stream_url_ = device->get_origin_url();
        publisher->stream_id_ = device->get_device_id();
        publisher->vhost_ = DEFAULT_VHOST;
        publisher->schema_ = publisher->stream_url_.substr(0, 4);
        publisher->app_ = LIVE_APP;
        device->get_stream_info(publisher->stream_);
        publisher->bytes_in_ = device->get_total_bytes();
        publisher->uptime_ = device->get_uptime();
        publisher->stream_.record_len = device->get_record_len();
        publisher->stream_.record_time = std::to_string(device->get_record_time()/1000);
        stream_map_.emplace(publisher->stream_id_, publisher);
    });
    
    RtpSelector::Instance().for_each_process([&](const string &streamid, RtpProcess::Ptr process) {
        auto publish = std::make_shared<Publisher>();
        for (auto &track : process->getTracks()) {
            if (track->getTrackType() == TrackVideo) {
                auto video_track = dynamic_pointer_cast<VideoTrack>(track);
                if (CodecId::CodecH264 == video_track->getCodecId()) {
                    publish->stream_.codec_ = "H264";
                } else if (CodecId::CodecH265 == video_track->getCodecId()) {
                    publish->stream_.codec_ = "H265";
                } else {
                    publish->stream_.codec_ = "Unknown";
                }
                publish->stream_.width_ = video_track->getVideoWidth();
                publish->stream_.height_ = video_track->getVideoHeight();
                publish->stream_.bits_ = video_track->getBitRate();
                publish->stream_.frame_rate_ = video_track->getVideoFps();
                publish->stream_.byte_rate_ = process->get_byte_rate();
                publish->stream_.rtp_loss_rate = process->get_rtp_loss_rate();
                if (!publish->stream_.bits_) {
                    publish->stream_.bits_ = publish->stream_.byte_rate_ * 8;
                }
                publish->stream_.record_len = process->get_record_len();
                publish->stream_.record_time = std::to_string(process->get_record_time()/1000);
            }
        }
        publish->stream_id_ = streamid;
        publish->vhost_ = DEFAULT_VHOST;
        publish->schema_ = "gb28181";
        publish->app_ = LIVE_APP;
        publish->reader_ = process->totalReaderCount();
        publish->uptime_ = process->get_uptime();
        publish->bytes_in_ = process->get_total_bytes();
        publish->src_ip_ = process->get_peer_ip();
        publish->src_port_ = to_string(process->get_peer_port());

        if (stream_map_.find(publish->stream_id_) == stream_map_.end()) {
            stream_map_.emplace(publish->stream_id_, publish);
        }
    });

    SessionMap::Instance().for_each_session([&](const string &id, const TcpSession::Ptr &session) {
        auto player = std::make_shared<Player>();
        if (session->get_local_port() == ConfigInfo.rtsp.port) {
            RtspSession::Ptr rtsp_session = dynamic_pointer_cast<RtspSession>(session);
            if (!rtsp_session->isPlayer()) {
                auto publish = std::make_shared<Publisher>();
                for (auto &track : rtsp_session->getTracks()) {
                    if (track->getTrackType() == TrackVideo) {
                        auto video_track = dynamic_pointer_cast<VideoTrack>(track);
                        if (CodecId::CodecH264 == video_track->getCodecId()) {
                            publish->stream_.codec_ = "H264";
                        } else if (CodecId::CodecH265 == video_track->getCodecId()) {
                            publish->stream_.codec_ = "H265";
                        } else {
                            publish->stream_.codec_ = "Unknown";
                        }
                        publish->stream_.width_ = video_track->getVideoWidth();
                        publish->stream_.height_ = video_track->getVideoHeight();
                        publish->stream_.bits_ = video_track->getBitRate();
                        publish->stream_.frame_rate_ = video_track->getVideoFps();
                    }
                }
                publish->stream_url_ = rtsp_session->getOriginUrl();
                publish->stream_id_ = rtsp_session->getId();
                publish->vhost_ = rtsp_session->getVhost();
                publish->schema_ = rtsp_session->getSchema();
                publish->app_ = rtsp_session->getApp();
                publish->reader_ = rtsp_session->totalReaderCount();
                publish->uptime_ = rtsp_session->getUptime();
                publish->bytes_in_ = rtsp_session->totalBytesUsage();
                publish->src_ip_ = rtsp_session->get_peer_ip();
                publish->src_port_ = to_string(rtsp_session->get_peer_port());
                stream_map_.emplace(publish->stream_id_, publish);
                return;
            } else {
                //add rtsp player
                player->stream_url_ = rtsp_session->getOriginUrl();
                player->stream_id_ = rtsp_session->getId();
                player->vhost_ = rtsp_session->getVhost();
                player->schema_ = rtsp_session->getSchema();
                player->app_ = rtsp_session->getApp();
                player->uptime_ = rtsp_session->getUptime();
                player->bytes_in_ = rtsp_session->totalBytesUsage();
                player->src_ip_ = rtsp_session->get_peer_ip();
                player->src_port_ = to_string(rtsp_session->get_peer_port());
                player->dst_ip_ = rtsp_session->get_local_ip();
                player->dst_port_ = to_string(rtsp_session->get_local_port());
            }
        } else {
            //add rtmp player
            HttpSession::Ptr http_session = dynamic_pointer_cast<HttpSession>(session);
            if (!http_session || ("rtmp" != http_session->getSchema() &&  "fmp4" != http_session->getSchema())) {
                return;
            }
            player->stream_url_ = http_session->getOriginUrl();
            player->stream_id_ = http_session->getId();
            player->vhost_ = http_session->getVhost();
            player->schema_ = http_session->getSchema();
            player->app_ = http_session->getApp();
            player->uptime_ = http_session->getUptime();
            player->bytes_in_ = http_session->totalBytesUsage();
            player->src_ip_ = http_session->get_peer_ip();
            player->src_port_ = to_string(http_session->get_peer_port());
            player->dst_ip_ = http_session->get_local_ip();
            player->dst_port_ = to_string(http_session->get_local_port());
        }

        auto iter = player_map.find(player->stream_id_);
        if (iter != player_map.end()) {
            iter->second.emplace_back(player);
        } else {
            list<Player::Ptr> play_list;
            play_list.emplace_back(player);
            player_map.emplace(player->stream_id_, play_list);
        }
    });
    

    for (auto it_publish : stream_map_) {
        auto iter = player_map.find(it_publish.second->stream_id_);
        if (iter != player_map.end()) {
            for (auto player : iter->second) {
                it_publish.second->play_list_.emplace_back(player);
                DebugL << "stream " << player->stream_id_ << " publisher add player " << player 
                       << " sz: " << it_publish.second->play_list_.size();
            }
        }
    }
}


std::string StreamStat::format_xml() {
    XMLDocument doc;
    XMLPrinter printer;

    XMLDeclaration *declaration = doc.NewDeclaration(R"(xml version="1.0" encoding="UTF-8"?> <?xml-stylesheet type="text/xsl" href="stat.xsl")");
    doc.InsertFirstChild(declaration);

    std::vector<Publisher::Ptr> live_publish, other_publish;
    uint64_t total_in = 0,  total_out = 0;
    uint64_t total_bw_in = 0, total_bw_out = 0;
    for (auto iter = stream_map_.begin(); iter != stream_map_.end(); iter++)
    {
        auto publish = iter->second;
        if (publish->app_ == LIVE_APP)
            live_publish.emplace_back(publish);
        else
            other_publish.emplace_back(publish);
    }
    XMLElement *vmr = doc.NewElement("vmr");
    doc.InsertEndChild(vmr);

    std::uint64_t sum_record_len = 0u;
    std::uint64_t sum_rtp_loss_rate = 0u;

    {
        XMLElement *server = doc.NewElement("server");
        {
            int app_num = (live_publish.size() ? 1 : 0) + (other_publish.size() ? 1 : 0);
            for (int idx = 0; idx < app_num; idx++)
            {
                XMLElement *server_app = doc.NewElement("app");
                server->InsertEndChild(server_app);
                {
                    XMLElement *app_live;
                    std::vector<Publisher::Ptr> *tmp_publish;
                    if (live_publish.size() && idx == 0)
                    {
                        app_live = doc.NewElement(LIVE_APP);
                        tmp_publish = &live_publish;
                    }
                    else
                    {
                        app_live = doc.NewElement(ANALYZER_APP);
                        tmp_publish = &other_publish;
                    }
                    server_app->InsertEndChild(app_live);
                    for (int lv = 0; lv < tmp_publish->size(); lv++)
                    {
                        XMLElement *live_stream = doc.NewElement("stream");
                        app_live->InsertEndChild(live_stream);
                        {
                            auto publisher = (*tmp_publish)[lv];
                            total_in += publisher->bytes_in_;
                            total_bw_in += publisher->stream_.bits_;

                            XMLElement *stream_name = doc.NewElement("name");
                            stream_name->InsertEndChild(doc.NewText(publisher->stream_id_.c_str()));
                            live_stream->InsertEndChild(stream_name);

                            XMLElement *stream_time = doc.NewElement("time");
                            stream_time->InsertEndChild(doc.NewText(to_string(publisher->uptime_).c_str()));
                            live_stream->InsertEndChild(stream_time);

                            XMLElement *stream_record_len = doc.NewElement("record_len");
                            stream_record_len->InsertEndChild(doc.NewText(std::to_string(publisher->stream_.record_len).c_str()));
                            live_stream->InsertEndChild(stream_record_len);
                            sum_record_len += publisher->stream_.record_len;

                            XMLElement *stream_record_time = doc.NewElement("record_time");
                            stream_record_time->InsertEndChild(doc.NewText(publisher->stream_.record_time.c_str()));
                            live_stream->InsertEndChild(stream_record_time);

                            sum_rtp_loss_rate += publisher->stream_.rtp_loss_rate;
                            XMLElement *stream_rtp_loss_rate = doc.NewElement("rtp_loss_rate");
                            stream_rtp_loss_rate->InsertEndChild(doc.NewText(std::to_string(publisher->stream_.rtp_loss_rate).c_str()));
                            live_stream->InsertEndChild(stream_rtp_loss_rate);

                            XMLElement *stream_bw_in = doc.NewElement("bw_in");
                            stream_bw_in->InsertEndChild(doc.NewText(to_string(publisher->stream_.bits_).c_str()));
                            live_stream->InsertEndChild(stream_bw_in);

                            XMLElement *stream_bytes_in = doc.NewElement("bytes_in");
                            stream_bytes_in->InsertEndChild(doc.NewText(to_string(publisher->bytes_in_).c_str()));
                            live_stream->InsertEndChild(stream_bytes_in);

                            XMLElement *stream_bw_out = doc.NewElement("bw_out");
                            stream_bw_out->InsertEndChild(doc.NewText(to_string(publisher->stream_.bits_ * publisher->reader_).c_str()));
                            live_stream->InsertEndChild(stream_bw_out);
                            total_bw_out += (publisher->stream_.bits_ * publisher->reader_);
                            // XMLElement*  stream_bytes_out= doc.NewElement("bytes_out");
                            // stream_bytes_out->InsertEndChild(doc.NewText("66"));
                            // live_stream->InsertEndChild(stream_bytes_out);

                            XMLElement *stream_bw_video = doc.NewElement("bw_video");
                            stream_bw_video->InsertEndChild(doc.NewText(to_string(publisher->stream_.bits_).c_str()));
                            live_stream->InsertEndChild(stream_bw_video);

                            if (false == publisher->play_list_.empty()) {
                                XMLElement *stream_stat = doc.NewElement("active");
                                live_stream->InsertEndChild(stream_stat);
                            }

                            auto iter = publisher->play_list_.begin();
                            uint64_t publish_out = 0;
                            for (int cl = 0; cl < publisher->play_list_.size() + 1; cl++)
                            {
                                Player::Ptr player;
                                uint64_t uptime;
                                XMLElement *stream_client = doc.NewElement("client");
                                live_stream->InsertEndChild(stream_client);
                                {
                                    XMLElement *client_id = doc.NewElement("id");
                                    client_id->InsertEndChild(doc.NewText(to_string(cl).c_str()));
                                    stream_client->InsertEndChild(client_id);

                                    XMLElement *client_address = doc.NewElement("address");
                                    string address;
                                    string separate;
                                    if (cl)
                                    {
                                        if (iter == publisher->play_list_.end())
                                        {
                                            break;
                                        }
                                        player = *iter;
                                        if (player->dst_ip_ != "" && player->dst_port_ != "")
                                            separate = ":";
                                        address = player->src_ip_ + separate + player->src_port_;
                                        uptime = player->uptime_;
                                        total_out += player->bytes_in_;
                                        publish_out += player->bytes_in_;
                                    }
                                    else
                                    {
                                        if (publisher->src_ip_ != "" && publisher->src_port_ != "")
                                            separate = ":";
                                        address = publisher->src_ip_ + separate + publisher->src_port_;
                                        uptime = publisher->uptime_;
                                    }
                                    client_address->InsertEndChild(doc.NewText(address.c_str()));
                                    stream_client->InsertEndChild(client_address);

                                    XMLElement *client_time = doc.NewElement("time");
                                    client_time->InsertEndChild(doc.NewText(to_string(uptime).c_str()));
                                    stream_client->InsertEndChild(client_time);

                                    XMLElement *client_schema = doc.NewElement("schema");
                                    XMLElement *client_url = doc.NewElement("pageurl");
                                    if (cl)
                                    {
                                        client_schema->InsertEndChild(doc.NewText(player->schema_.c_str()));
                                        string url = (player->schema_.c_str()=="rtsp"?"rtsp":"http");
                                        url += "://streamip:port/" + player->app_ + "/" + player->stream_id_;
                                        if (player->schema_ == "rtmp")
                                            url += ".flv";
                                        else if(player->schema_ == "fmp4")
                                            url += ".mp4";
                                        client_url->InsertEndChild(doc.NewText(url.c_str()));
                                        iter++;
                                    }
                                    else
                                    {
                                        client_schema->InsertEndChild(doc.NewText(publisher->schema_.c_str()));
                                        client_url->InsertEndChild(doc.NewText(publisher->stream_url_.c_str()));
                                    }

                                    stream_client->InsertEndChild(client_schema);
                                    stream_client->InsertEndChild(client_url);

                                    if (!cl)
                                    {
                                        XMLElement *client_publish = doc.NewElement("publishing");
                                        stream_client->InsertEndChild(client_publish);
                                    }
                                    XMLElement *client_active = doc.NewElement("active");
                                    stream_client->InsertEndChild(client_active);
                                }
                            }

                            XMLElement *stream_bytes_out = doc.NewElement("bytes_out");
                            stream_bytes_out->InsertEndChild(doc.NewText(to_string(publish_out).c_str()));
                            live_stream->InsertEndChild(stream_bytes_out);

                            XMLElement *stream_meta = doc.NewElement("meta");
                            live_stream->InsertEndChild(stream_meta);
                            {
                                XMLElement *meta_video = doc.NewElement("video");
                                stream_meta->InsertEndChild(meta_video);
                                {
                                    XMLElement *video_width = doc.NewElement("width");
                                    video_width->InsertEndChild(doc.NewText(to_string(publisher->stream_.width_).c_str()));
                                    meta_video->InsertEndChild(video_width);

                                    XMLElement *video_height = doc.NewElement("height");
                                    video_height->InsertEndChild(doc.NewText(to_string(publisher->stream_.height_).c_str()));
                                    meta_video->InsertEndChild(video_height);

                                    XMLElement *video_frame_rate = doc.NewElement("frame_rate");
                                    video_frame_rate->InsertEndChild(doc.NewText(to_string(publisher->stream_.frame_rate_).c_str()));
                                    meta_video->InsertEndChild(video_frame_rate);

                                    XMLElement *video_codec = doc.NewElement("codec");
                                    video_codec->InsertEndChild(doc.NewText(publisher->stream_.codec_.c_str()));
                                    meta_video->InsertEndChild(video_codec);

                                    XMLElement *video_profile = doc.NewElement("profile");
                                    video_profile->InsertEndChild(doc.NewText(publisher->stream_.profile_.c_str()));
                                    meta_video->InsertEndChild(video_profile);

                                    XMLElement *video_level = doc.NewElement("level");
                                    video_level->InsertEndChild(doc.NewText(publisher->stream_.level_.c_str()));
                                    meta_video->InsertEndChild(video_level);
                                }
                            }
                            XMLElement *stream_nclients = doc.NewElement("nclients");
                            stream_nclients->InsertEndChild(doc.NewText(to_string(publisher->reader_).c_str()));
                            live_stream->InsertEndChild(stream_nclients);
                        }
                    }
                    XMLElement *live_nclients = doc.NewElement("nclients");
                    live_nclients->InsertEndChild(doc.NewText(to_string(tmp_publish->size()).c_str()));
                    app_live->InsertEndChild(live_nclients);
                }
            }
        }

        //XMLElement *vmr_uptime = doc.NewElement("uptime");
        //vmr_uptime->InsertEndChild(doc.NewText(to_string(ticks_.createdTime()).c_str()));
        //vmr->InsertEndChild(vmr_uptime);

        XMLElement *vmr_naccepted = doc.NewElement("naccepted");
        vmr_naccepted->InsertEndChild(doc.NewText(to_string(stream_map_.size()).c_str()));
        vmr->InsertEndChild(vmr_naccepted);

        XMLElement *vmr_bw_in = doc.NewElement("bw_in");
        vmr_bw_in->InsertEndChild(doc.NewText(to_string(total_bw_in).c_str()));
        vmr->InsertEndChild(vmr_bw_in);

        XMLElement *vmr_bytes_in = doc.NewElement("bytes_in");
        vmr_bytes_in->InsertEndChild(doc.NewText(to_string(total_in).c_str()));
        vmr->InsertEndChild(vmr_bytes_in);

        XMLElement *vmr_bw_out = doc.NewElement("bw_out");
        vmr_bw_out->InsertEndChild(doc.NewText(to_string(total_bw_out).c_str()));
        vmr->InsertEndChild(vmr_bw_out);

        XMLElement *vmr_bytes_out = doc.NewElement("bytes_out");
        vmr_bytes_out->InsertEndChild(doc.NewText(to_string(total_out).c_str()));
        vmr->InsertEndChild(vmr_bytes_out);

        XMLElement *vmr_record_len = doc.NewElement("record_len");
        vmr_record_len->InsertEndChild(doc.NewText(std::to_string(sum_record_len).c_str()));
        vmr->InsertEndChild(vmr_record_len);

        XMLElement *vmr_record_time = doc.NewElement("record_time");
        vmr_record_time->InsertEndChild(doc.NewText(std::to_string(ConfigInfo.record.time_quota/1000).c_str()));
        vmr->InsertEndChild(vmr_record_time);

        XMLElement *vmr_rtp_loss_rate = doc.NewElement("rtp_loss_rate");
        vmr_rtp_loss_rate->InsertEndChild(doc.NewText(std::to_string(sum_rtp_loss_rate).c_str()));
        vmr->InsertEndChild(vmr_rtp_loss_rate);

        vmr->InsertEndChild(server);
    } //vmr end

    printer.ClearBuffer();
    doc.Print(&printer);
    return printer.CStr();
}


std::string StreamStat::stat() {
    get_stat_data();
    return format_xml();
}

std::string StreamStat::stat_template() {
    return toolkit::File::loadFile("/etc/supremind/stat.xsl");
}

}
