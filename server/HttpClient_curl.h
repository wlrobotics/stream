#pragma once

#include <sstream>
#include <string>
#include <map>

#include "json/json.h"
#include "curl/curl.h"

namespace Infra {
    
class HttpClient {
public:
    using HttpKeyValue = std::map<std::string, std::string>;
    
    HttpClient() = default;
    HttpClient(const HttpClient &) = delete;
    HttpClient & operator=(const HttpClient &) = delete;
    
    void setConnectTimeout(int connect_timeout) {
        connect_timeout_ = connect_timeout;
    }
    
    void setUseHttp2(bool isUseHttp2) {
        isHttp2_ = isUseHttp2;
    }
    
    void setSocketTimeout(int socket_timeout) {
        socket_timeout_ = socket_timeout;
    }
    
    void setDebug(bool debug) {
        debug_ = debug;
    }
    
    int get(
            std::string url,
            HttpKeyValue const * params,
            HttpKeyValue const * headers,
            std::string * response) const;

    int get(
            std::string url,
            HttpKeyValue const * params,
            HttpKeyValue const * headers,
            void *buf,
            long& http_status_code) const;
    
    int post(
             std::string url,
             HttpKeyValue const * params,
             const std::string & body,
             HttpKeyValue const * headers,
             std::string * response) const;
    
    int post(
             std::string url,
             HttpKeyValue const * params,
             HttpKeyValue const & data,
             HttpKeyValue const * headers,
             std::string * response) const;
    
    int post(
             std::string url,
             HttpKeyValue const * params,
             Json::Value const & data,
             HttpKeyValue const * headers,
             std::string * response) const;
    
    int post(
             std::string url,
             HttpKeyValue const * params,
             HttpKeyValue const * headers,
             std::string * response) const;
             
private:
    bool debug_ = false;
    bool isHttp2_ = false;
    int connect_timeout_ = 3000;
    int socket_timeout_ = 3000;
    
    void makeUrlencodedForm(
        HttpKeyValue const & params,
        std::string * content) const;
        
    void appendUrlParams(
        HttpKeyValue const & params,
        std::string* url) const;
        
    void appendHeaders(
        HttpKeyValue const & headers,
        curl_slist ** slist) const;
};
}
