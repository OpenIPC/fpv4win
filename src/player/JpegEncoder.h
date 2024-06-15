//
// Created by liangzhuohua on 2022/2/28.
//

#ifndef CTRLCENTER_JPEGENCODER_H
#define CTRLCENTER_JPEGENCODER_H
#include "ffmpegInclude.h"
#include <string>
#include <memory>

using namespace std;
class JpegEncoder {
public:
    static bool encodeJpeg(const string& outFilePath,const shared_ptr<AVFrame>& frame);
};


#endif //CTRLCENTER_JPEGENCODER_H

