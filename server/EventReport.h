#pragma once

#include <string>
#include <unordered_map>
#include <future>
#include "BlockingQueue.h"
#include "json/json.h"

namespace mediakit {

class EventReport {
public:   
    enum EventType {
        //INFO
        open_success = 20,

        //WARN
        network_offline = 40,
        resolution_is_change
    };
        
    EventReport();
    ~EventReport();
    static EventReport& Instance();
    bool init();
    bool report(const std::string& stream_id,
                  const EventType event,
                  const std::string& details = "");

private:
    std::future<void> event_thread_future_;
    tool::BlockingQueue<Json::Value> event_queue_;
    std::unordered_map<EventType, std::string> event_map_ = {
        {open_success, "open success"},
        {network_offline, "network offline"},
        {resolution_is_change, "resolution is change"}
    };
};

}
