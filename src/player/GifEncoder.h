//
// Created by liangzhuohua on 2022/4/22.
//

#ifndef CTRLCENTER_GIFENCODER_H
#define CTRLCENTER_GIFENCODER_H
#include "ffmpegInclude.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace std;
class GifEncoder {
public:
    ~GifEncoder();
    // 初始化编码器
    bool open(int width, int height, AVPixelFormat pixelFormat, int frameRate, const string &outputPath);
    // 编码帧
    bool encodeFrame(const shared_ptr<AVFrame> &frame);
    // 关闭编码器
    void close();
    // 帧率
    int getFrameRate() const { return _frameRate; }
    // 上次编码时间
    uint64_t getLastEncodeTime() const { return _lastEncodeTime; }
    // 是否已经打开
    bool isOpened();

protected:
    mutex _encodeMtx;
    // 编码上下文
    shared_ptr<AVFormatContext> _formatCtx;
    // 编码上下文
    shared_ptr<AVCodecContext> _codecCtx;
    // 色彩空间转换
    SwsContext *_imgConvertCtx;
    // 颜色转换临时frame
    shared_ptr<AVFrame> _tmpFrame;
    vector<uint8_t> _buff;
    // 最后编码时间
    uint64_t _lastEncodeTime = 0;
    // 帧率
    int _frameRate = 0;
    // 是否打开
    volatile bool _opened = false;
};

#endif // CTRLCENTER_GIFENCODER_H
