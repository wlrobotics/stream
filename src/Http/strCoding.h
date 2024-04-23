/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_STRCODING_H_
#define SRC_HTTP_STRCODING_H_

#include <iostream>
#include <string>

using namespace std;

namespace mediakit {

class strCoding {
public:
    static string UrlEncode(const string &str); //urlutf8 编码
    static string UrlDecode(const string &str); //urlutf8解码
private:
    strCoding(void);
    virtual ~strCoding(void);
};

} /* namespace mediakit */

#endif /* SRC_HTTP_STRCODING_H_ */
