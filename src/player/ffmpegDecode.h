/*
    It is FFmpeg decoder class. Sample for article from unick-soft.ru
*/

#ifndef __FFMPEG_DECODER__
#define __FFMPEG_DECODER__

#include "ffmpegInclude.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>

class QQuickRealTimePlayer;

using namespace std;

class FFmpegDecoder {
public:
    FFmpegDecoder()
        : pImgConvertCtx(nullptr)
        , audioBaseTime(0.0)
        , videoBaseTime(0.0)
        , videoFramePerSecond(0.0)
        , isOpen(false)
        , audioStreamIndex(-1)
        , videoStreamIndex(-1)
        , pAudioCodecCtx(nullptr)
        , pVideoCodecCtx(nullptr)
        , pFormatCtx(nullptr) {
        ;
    }

public:
    virtual ~FFmpegDecoder() { FFmpegDecoder::CloseInput(); }

    // 打开输入
    virtual bool OpenInput(std::string &inputFile);

    // 关闭输入并释放资源
    virtual bool CloseInput();

    // 获取下一帧
    virtual shared_ptr<AVFrame> GetNextFrame();

    // 获取宽度
    int GetWidth() const { return width; }

    // 获取高度
    int GetHeight() const { return height; }

    // 获取FPS
    double GetFps() const { return videoFramePerSecond; }

    // 输入流是否存在音频
    bool HasAudio() const { return hasAudioStream; }

    // 输入流是否存在视频
    bool HasVideo() const { return hasVideoStream; }

    // 读音频fifo
    size_t ReadAudioBuff(uint8_t *aSample, size_t aSize);
    // 清空音频fifo
    void ClearAudioBuff();
    // 音频采样率
    int GetAudioSampleRate() const { return pAudioCodecCtx->sample_rate; }
    // 音频声道数
    int GetAudioChannelCount() const { return pAudioCodecCtx->ch_layout.nb_channels; }
    // 音频样本格式
    AVSampleFormat GetAudioSampleFormat() const { return AV_SAMPLE_FMT_S16; }
    // 视频帧格式
    AVPixelFormat GetVideoFrameFormat() const {
        if (isHwDecoderEnable) {
            return AV_PIX_FMT_NV12;
        }
        return pVideoCodecCtx->pix_fmt;
    }
    // 获取音频frame大小
    int GetAudioFrameSamples() { return pAudioCodecCtx->sample_rate * 2 / 25; }
    // 有元
    friend class QQuickRealTimePlayer;

private:
    // 打开视频流
    bool OpenVideo();

    // 打开音频流
    bool OpenAudio();

    // 关闭视频流
    void CloseVideo();

    // 关闭音频流
    void CloseAudio();

    // 解码音频帧
    int DecodeAudio(int nStreamIndex, const AVPacket *avpkt, uint8_t *pOutBuffer, size_t nOutBufferSize);

    // 解码视频祯
    bool DecodeVideo(const AVPacket *avpkt, shared_ptr<AVFrame> &pOutFrame);

    // 向音频fifo写入数据
    void writeAudioBuff(uint8_t *aSample, size_t aSize);

    // 获取到NALU回调
    std::function<void(const shared_ptr<AVPacket> &packet)> _gotPktCallback = nullptr;
    // 获取到已经解码图像回调
    std::function<void(const shared_ptr<AVFrame> &frame)> _gotFrameCallback = nullptr;

    // 初始化硬件解码器
    bool hwDecoderInit(AVCodecContext *ctx, enum AVHWDeviceType type);

    // FFmpeg 解封装上下文
    AVFormatContext *pFormatCtx = nullptr;

    // FFmpeg 视频编码上下文
    AVCodecContext *pVideoCodecCtx = nullptr;

    // FFmpeg音频编码上下文
    AVCodecContext *pAudioCodecCtx = nullptr;

    // FFmpeg 音频样本格式转换
    shared_ptr<SwrContext> swrCtx;

    // 视频轨道顺序
    int videoStreamIndex = 0;

    // 音轨顺序
    int audioStreamIndex = 0;

    // 输入源是否成功打开
    volatile bool isOpen = false;

    // Video 帧率
    double videoFramePerSecond = 0;

    // FFmpeg 视频时间基
    double videoBaseTime = 0;

    // FFmpeg 音频时间基
    double audioBaseTime = 0;

    // FFmpeg 视频格式转换
    struct SwsContext *pImgConvertCtx = nullptr;

    // 解码器全局释放锁
    mutex _releaseLock;

    // 是否存在视频流
    bool hasVideoStream {};
    // 是否存在音频流
    bool hasAudioStream {};

    // 视频宽度
    int width {};

    // 视频高度
    int height {};

    volatile uint64_t bytesSecond = 0;
    uint64_t bitrate = 0;
    uint64_t lastCountBitrateTime = 0;
    function<void(uint64_t bitrate)> onBitrate;

    // 音频队列
    mutex abBuffMtx;
    shared_ptr<AVFifo> audioFifoBuffer;

    // 硬件解码
    enum AVHWDeviceType hwDecoderType;
    bool isHwDecoderEnable = false;
    enum AVPixelFormat hwPixFmt;
    AVBufferRef *hwDeviceCtx = nullptr;
    volatile bool dropCurrentVideoFrame = false;
    // Hardware frame
    shared_ptr<AVFrame> hwFrame;
};

#endif
