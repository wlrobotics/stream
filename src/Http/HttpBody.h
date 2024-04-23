/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FILEREADER_H
#define ZLMEDIAKIT_FILEREADER_H

#include <cstdlib>
#include <memory>

#include "Network/Buffer.h"
#include "Common/Parser.h"
#include "Util/mini.h"
#include "strCoding.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class HttpArgs : public map<std::string, toolkit::variant, StrCaseCompare>  {
public:
    HttpArgs(){}
    virtual ~HttpArgs(){}
    string make() const {
        string ret;
        for(auto &pr : *this){
            ret.append(pr.first);
            ret.append("=");
            ret.append(strCoding::UrlEncode(pr.second));
            ret.append("&");
        }
        if(ret.size()){
            ret.pop_back();
        }
        return ret;
    }
};

class HttpBody : public std::enable_shared_from_this<HttpBody>{
public:
    typedef std::shared_ptr<HttpBody> Ptr;
    HttpBody(){
    }
    virtual ~HttpBody(){}

    virtual uint64_t remainSize() { return 0;};

    virtual Buffer::Ptr readData(uint32_t size) { return nullptr;};

    virtual void readDataAsync(uint32_t size,const function<void(const Buffer::Ptr &buf)> &cb){
        cb(readData(size));
    }
};

class HttpStringBody : public HttpBody{
public:
    typedef std::shared_ptr<HttpStringBody> Ptr;
    HttpStringBody(const string &str);
    virtual ~HttpStringBody(){}
    uint64_t remainSize() override ;
    Buffer::Ptr readData(uint32_t size) override ;
private:
    mutable std::string _str;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_FILEREADER_H
