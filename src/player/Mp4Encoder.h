//
// Created by liangzhuohua on 2022/3/1.
//

#ifndef CTRLCENTER_MP4ENCODER_H
#define CTRLCENTER_MP4ENCODER_H
#include "ffmpegInclude.h"
#include <string>
#include <memory>

using namespace std;
class Mp4Encoder {
public:
    explicit Mp4Encoder(const string& saveFilePath);
    ~Mp4Encoder();
    //开启
    bool start();
    //关闭
    void stop();
    //增加轨道
    void addTrack(AVStream* stream);
    //写packet
    void writePacket(const shared_ptr<AVPacket>& pkt,bool isVideo);
    //音视频index
    int videoIndex = -1;
    int audioIndex = -1;
    //文件保存路径
    string _saveFilePath;
private:
    //是否已经初始化
    bool _isOpen = false;
    //编码上下文
    shared_ptr<AVFormatContext> _formatCtx;
    //原始视频流时间基
    AVRational _originVideoTimeBase{};
    //原始音频流时间基
    AVRational _originAudioTimeBase{};
    //已经写入关键帧
    bool writtenKeyFrame = false;
};


#endif //CTRLCENTER_MP4ENCODER_H
