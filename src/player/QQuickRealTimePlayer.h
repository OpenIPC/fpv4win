#pragma once
#include <QQuickFramebufferObject>
#include <QQuickItem>
#include <memory>
#include "RealTimeRenderer.h"
#include "ffmpegDecode.h"
#include <queue>
#include <memory>
#include <thread>

#include "Mp4Encoder.h"
#include "GifEncoder.h"

class TItemRender;

class QQuickRealTimePlayer : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(bool isMuted READ getMuted WRITE setMuted NOTIFY onMutedChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY onHasAudio )
public:
    explicit QQuickRealTimePlayer(QQuickItem* parent = nullptr);
    ~QQuickRealTimePlayer() override;
    void timerEvent(QTimerEvent* event) override;

    shared_ptr<AVFrame> getFrame(bool& got);

    bool infoDirty() const{
        return m_infoChanged;
    }
    void makeInfoDirty(bool dirty){
        m_infoChanged = dirty;
    }
    int videoWidth() const{
        return m_videoWidth;
    }
    int videoHeght() const{
        return m_videoHeight;
    }
    int videoFormat() const{
        return m_videoFormat;
    }
    bool getMuted() const{
        return isMuted;
    }
    //播放
    Q_INVOKABLE void play(const QString& playUrl);
    //停止
    Q_INVOKABLE void stop();
    //静音
    Q_INVOKABLE void setMuted(bool muted = false);
    //截图
    Q_INVOKABLE QString captureJpeg();
    //录像
    Q_INVOKABLE bool startRecord();
    Q_INVOKABLE QString stopRecord();
    //录制GIF
    Q_INVOKABLE bool startGifRecord();
    Q_INVOKABLE void stopGifRecord();
    //获取视频宽度
    Q_INVOKABLE int getVideoWidth();
    //获取视频高度
    Q_INVOKABLE int getVideoHeight();

signals:
    //播放已经停止
    void onPlayStopped();
    //出错
    void onError(QString msg,int code);
    //获取录音音量
    void gotRecordVol(double vol);
    //获得码率
    void onBitrate(long bitrate);
    //静音
    void onMutedChanged(bool muted);
    //是否有音频
    void onHasAudio(bool has);

friend class TItemRender;
protected:
    //ffmpeg解码器
    shared_ptr<FFmpegDecoder> decoder;
    //播放地址
    string url;
    //播放标记位
    volatile bool playStop = true;
    //静音标记位
    volatile bool isMuted = true;
    //帧队列
    std::queue<shared_ptr<AVFrame>> videoFrameQueue;
    mutex mtx;
    //解码线程
    std::thread decodeThread;
    //分析线程
    std::thread analysisThread;
    //最后输出的帧
    shared_ptr<AVFrame> _lastFrame;
    //视频是否ready
    void onVideoInfoReady(int width, int height, int format);
    //播放音频
    bool enableAudio();
    //停止播放音频
    void disableAudio();
    //MP4录制器
    shared_ptr<Mp4Encoder> _mp4Encoder;
    //GIF录制器
    shared_ptr<GifEncoder> _gifEncoder;
    //是否有声音
    bool hasAudio(){
        if(!decoder){
            return false;
        }
        return decoder->HasAudio();
    }

public:
    Renderer* createRenderer() const override;
    int m_videoWidth{};
    int m_videoHeight{};
    int m_videoFormat{};
    bool m_infoChanged = false;
};
