#include "HttpClient_curl.h"

#include <utility>
#include <cstdio>

#include "Util/logger.h"

using namespace toolkit;


namespace Infra {


inline size_t onWriteData(void * buffer, size_t size, size_t nmemb, void * userp) {
    std::string* str = dynamic_cast<std::string *>((std::string *)userp);
    str->append((char *)buffer, size * nmemb);
    return nmemb;
}


inline size_t onWriteBuffer(void *contents, size_t size, size_t nmemb, void *userp) {
    struct buffer_temp {
        uint8_t *buf;
        int read_len;
    };
    struct buffer_temp* tmp = (struct buffer_temp*)userp;
    std::memcpy(tmp->buf + tmp->read_len, contents, size * nmemb);
    tmp->read_len += size * nmemb;
    return size * nmemb;
}

int HttpClient::get(
    std::string url,
    HttpKeyValue const * params,
    HttpKeyValue const * headers,
    std::string * response) const {
    CURL * curl = curl_easy_init();
    struct curl_slist * slist = nullptr;
    if (headers) {
        this->appendHeaders(*headers, &slist);
    }
    if (params) {
        this->appendUrlParams(*params, &url);
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) response);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, true);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, socket_timeout_);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, debug_);
    
    int status_code = curl_easy_perform(curl);
    if(status_code != CURLE_OK) {
        ErrorL << "curl_easy_perform failed! status_code" << status_code;
    }

    long http_status_code = 200;
    status_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status_code);
    if(status_code != CURLE_OK) {
        ErrorL << "curl_easy_perform failed! status_code" << status_code;
    }
    
    curl_easy_cleanup(curl);
    curl_slist_free_all(slist);
    
    return status_code;
}


int HttpClient::get(
            std::string url,
            HttpKeyValue const * params,
            HttpKeyValue const * headers,
            void *buf,
            long& http_status_code) const {
    CURL * curl = curl_easy_init();
    struct curl_slist * slist = nullptr;
    if (headers) {
        this->appendHeaders(*headers, &slist);
    }
    if (params) {
        this->appendUrlParams(*params, &url);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWriteBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, true);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, socket_timeout_);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, debug_);

    int status_code = curl_easy_perform(curl);
    if(status_code != CURLE_OK) {
        ErrorL << "curl_easy_perform failed! status_code" << status_code;
    }
    status_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status_code);
    if(status_code != CURLE_OK) {
        ErrorL << "curl_easy_getinfo failed! status_code" << status_code;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(slist);

    return status_code;

}


int HttpClient::post(
    std::string url,
    HttpKeyValue const * params,
    const std::string & body,
    HttpKeyValue const * headers,
    std::string * response) const {
    struct curl_slist * slist = nullptr;
    CURL * curl = curl_easy_init();
    if (headers) {
        this->appendHeaders(*headers, &slist);
    }
    if (params) {
        this->appendUrlParams(*params, &url);
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_POST, true);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) response);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, true);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, socket_timeout_);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, debug_);

    if (isHttp2_) {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    }
    
    int status_code = curl_easy_perform(curl);
    
    curl_easy_cleanup(curl);
    curl_slist_free_all(slist);
    
    return status_code;
}


int HttpClient::post(
    std::string url,
    HttpKeyValue const * params,
    HttpKeyValue const & data,
    HttpKeyValue const * headers,
    std::string * response) const {
    
    std::string body;
    this->makeUrlencodedForm(data, &body);
    return this->post(std::move(url), params, body, headers, response);
}


int HttpClient::post(
    std::string url,
    HttpKeyValue const * params,
    Json::Value const & data,
    HttpKeyValue const * headers,
    std::string * response) const {
    std::string body;
    Json::StreamWriterBuilder swb;
    std::unique_ptr<Json::StreamWriter> writer(swb.newStreamWriter());
    std::ostringstream os;
    writer->write(data, &os);
    body = os.str();
    HttpKeyValue temp_headers;
    if (headers) {
        HttpKeyValue temp_headers(*headers);
    }
    
    temp_headers["Content-Type"] = "application/json";
    return this->post(url, params, body, &temp_headers, response);
}


int HttpClient::post(
    std::string url,
    HttpKeyValue const * params,
    HttpKeyValue const * headers,
    std::string * response) const {
    const static std::string EMPTY_STRING;  
    return this->post(std::move(url), params, EMPTY_STRING, headers, response);
}


void HttpClient::makeUrlencodedForm(
    HttpKeyValue const & params,
    std::string * content) const {
    content->clear();
    HttpKeyValue::const_iterator it;
    for(it=params.begin(); it!=params.end(); it++) {
        char * key = curl_escape(it->first.c_str(), (int) it->first.size());
        char * value = curl_escape(it->second.c_str(),(int) it->second.size());
        *content += key;
        *content += '=';
        *content += value;
        *content += '&';
        curl_free(key);
        curl_free(value);
    }
}

void HttpClient::appendUrlParams(
    HttpKeyValue const & params,
    std::string* url) const {
    if(params.empty()) {
        return;
    }
    std::string content;
    this->makeUrlencodedForm(params, &content);
    bool url_has_param = false;
    for (const auto& ch : *url) {
        if (ch == '?') {
            url_has_param = true;
            break;
        }
    }
    if (url_has_param) {
        url->append("&");
    } else {
        url->append("?");
    }
    url->append(content);
}

void HttpClient::appendHeaders(
    HttpKeyValue const & headers,
    curl_slist ** slist) const {
    std::ostringstream ostr;
    HttpKeyValue::const_iterator it;
    for(it=headers.begin(); it!=headers.end(); it++) {
        ostr << it->first << ":" << it->second;
        *slist = curl_slist_append(*slist, ostr.str().c_str());
        ostr.str("");
    }
}

}
