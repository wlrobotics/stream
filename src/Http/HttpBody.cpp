#include "HttpBody.h"

namespace mediakit {

HttpStringBody::HttpStringBody(const string &str){
    _str = str;
}

uint64_t HttpStringBody::remainSize() {
    return _str.size();
}

Buffer::Ptr HttpStringBody::readData(uint32_t size) {
    size = _str.size();
    if(size <= 0){
        return nullptr;
    }
    return std::make_shared<BufferString>(_str, 0, size);
}

}//namespace mediakit
