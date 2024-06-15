
#include "QQuickRealTimePlayer.h"
#include "JpegEncoder.h"
#include <QDir>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <QStandardPaths>
#include <SDL2/SDL.h>
#include <future>
#include <sstream>
// GIF默认帧率
#define DEFAULT_GIF_FRAMERATE 10

//************TaoItemRender************//
class TItemRender : public QQuickFramebufferObject::Renderer {
public:
  TItemRender();

  void render() override;
  QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
  void synchronize(QQuickFramebufferObject *) override;

private:
  RealTimeRenderer m_render;
  QQuickWindow *m_window = nullptr;
};

TItemRender::TItemRender() { m_render.init(); }

void TItemRender::render() {
  m_render.paint();
  m_window->resetOpenGLState();
}

QOpenGLFramebufferObject *
TItemRender::createFramebufferObject(const QSize &size) {
  QOpenGLFramebufferObjectFormat format;
  format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
  format.setSamples(4);
  m_render.resize(size.width(), size.height());
  return new QOpenGLFramebufferObject(size, format);
}

void TItemRender::synchronize(QQuickFramebufferObject *item) {

  auto *pItem = qobject_cast<QQuickRealTimePlayer *>(item);
  if (pItem) {
    if (!m_window) {
      m_window = pItem->window();
    }
    if (pItem->infoDirty()) {
      m_render.updateTextureInfo(pItem->videoWidth(), pItem->videoHeght(),
                                 pItem->videoFormat());
      pItem->makeInfoDirty(false);
    }
    if (pItem->playStop) {
      m_render.clear();
      return;
    }
    bool got = false;
    shared_ptr<AVFrame> frame = pItem->getFrame(got);
    if (got && frame->linesize[0] && frame->linesize[1] && frame->linesize[2]) {
      m_render.updateTextureData(frame);
    }
  }
}

//************QQuickRealTimePlayer************//
QQuickRealTimePlayer::QQuickRealTimePlayer(QQuickItem *parent)
    : QQuickFramebufferObject(parent) {
  SDL_Init(SDL_INIT_AUDIO);
  // 按每秒60帧的帧率更新界面
  startTimer(1000 / 100);
}

void QQuickRealTimePlayer::timerEvent(QTimerEvent *event) {
  Q_UNUSED(event);
  update();
}

shared_ptr<AVFrame> QQuickRealTimePlayer::getFrame(bool &got) {
  got = false;
  shared_ptr<AVFrame> frame;
  {
    lock_guard<mutex> lck(mtx);
    // 帧缓冲区已被清空,跳过渲染
    if (videoFrameQueue.empty()) {
      return {};
    }
    // 从帧缓冲区取出帧
    frame = videoFrameQueue.back();
    got = true;
    // 缓冲区出队被渲染的帧
    videoFrameQueue.pop();
  }
  // 计算一帧的显示时间
  auto frameDuration = 1000 / decoder->GetFps();
  // 缓冲，追帧机制
  if (videoFrameQueue.size() < 5) {
    double scale = videoFrameQueue.size() * frameDuration / 100.0;
    std::this_thread::sleep_for(
        std::chrono::milliseconds((int)(frameDuration / scale)));
  }
  _lastFrame = frame;
  return frame;
}

void QQuickRealTimePlayer::onVideoInfoReady(int width, int height, int format) {
  if (m_videoWidth != width) {
    m_videoWidth = width;
    makeInfoDirty(true);
  }
  if (m_videoHeight != height) {
    m_videoHeight = height;
    makeInfoDirty(true);
  }
  if (m_videoFormat != format) {
    m_videoFormat = format;
    makeInfoDirty(true);
  }
}

QQuickFramebufferObject::Renderer *
QQuickRealTimePlayer::createRenderer() const {
  return new TItemRender;
}

void QQuickRealTimePlayer::play(const QString &playUrl) {
  playStop = false;
  if (analysisThread.joinable()) {
    analysisThread.join();
  }
  // 启动分析线程
  analysisThread = std::thread([this, playUrl]() {
    auto decoder_ = make_shared<FFmpegDecoder>();
    url = playUrl.toStdString();
    // 打开并分析输入
    bool ok = decoder_->OpenInput(url);
    if (!ok) {
      emit onError("视频加载出错", -2);
      return;
    }
    decoder = decoder_;
    // 启动解码线程
    decodeThread = std::thread([this]() {
      while (!playStop) {
        try {
          // 循环解码
          auto frame = decoder->GetNextFrame();
          if (!frame) {
            continue;
          }
          {
            // 解码获取到视频帧,放入帧缓冲队列
            lock_guard<mutex> lck(mtx);
            if(videoFrameQueue.size()>10) {
              videoFrameQueue.pop();
            }
            videoFrameQueue.push(frame);
          }
        } catch (const exception &e) {
          emit onError(e.what(), -2);
          // 出错，停止
          break;
        }
      }
      playStop = true;
      // 解码已经停止，触发信号
      emit onPlayStopped();
    });
    decodeThread.detach();

    if (!isMuted && decoder->HasAudio()) {
      // 开启音频
      enableAudio();
    }
    // 是否存在音频
    emit onHasAudio(decoder->HasAudio());

    if (decoder->HasVideo()) {
      onVideoInfoReady(decoder->GetWidth(), decoder->GetHeight(),
                       decoder->GetVideoFrameFormat());
    }

    // 码率计算回调
    decoder->onBitrate = [this](uint64_t bitrate) {
      emit onBitrate(static_cast<long>(bitrate));
    };
  });
  analysisThread.detach();
}

void QQuickRealTimePlayer::stop() {
  playStop = true;
  if(decoder&&decoder->pFormatCtx) {
    decoder->pFormatCtx->interrupt_callback.callback = [](void*) {
      return 1;
    };
  }
  if (analysisThread.joinable()) {
    analysisThread.join();
  }
  if (decodeThread.joinable()) {
    decodeThread.join();
  }
  while (!videoFrameQueue.empty()) {
    lock_guard<mutex> lck(mtx);
    // 清空缓冲
    videoFrameQueue.pop();
  }
  SDL_CloseAudio();
  if (decoder) {
    decoder->CloseInput();
  }

}

void QQuickRealTimePlayer::setMuted(bool muted) {
  if (!decoder->HasAudio()) {
    return;
  }
  if (!muted && decoder) {
    decoder->ClearAudioBuff();
    // 初始化声音
    if (!enableAudio()) {
      return;
    }
  } else {
    disableAudio();
  }
  isMuted = muted;
  emit onMutedChanged(muted);
}

QQuickRealTimePlayer::~QQuickRealTimePlayer() { stop(); }

QString QQuickRealTimePlayer::captureJpeg() {
  if (!_lastFrame) {
    return "";
  }
  QString dirPath = QFileInfo("jpg/l").absolutePath();
  QDir dir(dirPath);
  if (!dir.exists()) {
    dir.mkpath(dirPath);
  }
  stringstream ss;
  ss << "jpg/";
  ss << std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count()
     << ".jpg";
  auto ok = JpegEncoder::encodeJpeg(ss.str(), _lastFrame);
  // 截图
  return ok?QString(ss.str().c_str()):"";
}

bool QQuickRealTimePlayer::startRecord() {
  if (playStop&&!_lastFrame) {
    return false;
  }
  QString dirPath = QFileInfo("mp4/l").absolutePath();
  QDir dir(dirPath);
  if (!dir.exists()) {
    dir.mkpath(dirPath);
  }
  // 保存路径
  stringstream ss;
  ss<< "mp4/";
  ss << std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count()
     << ".mp4";
  // 创建MP4编码器
  _mp4Encoder = make_shared<Mp4Encoder>(ss.str());

  // 添加音频流
  if (decoder->HasAudio()) {
    _mp4Encoder->addTrack(
        decoder->pFormatCtx->streams[decoder->audioStreamIndex]);
  }
  // 添加视频流
  if (decoder->HasVideo()) {
    _mp4Encoder->addTrack(
        decoder->pFormatCtx->streams[decoder->videoStreamIndex]);
  }
  if (!_mp4Encoder->start()) {
    return false;
  }
  // 设置获得NALU回调
  decoder->_gotPktCallback = [this](const shared_ptr<AVPacket> &packet) {
    // 输入编码器
    _mp4Encoder->writePacket(packet,
                             packet->stream_index == decoder->videoStreamIndex);
  };
  // 启动编码器
  return true;
}

QString QQuickRealTimePlayer::stopRecord() {
  if(!_mp4Encoder){
    return {};
  }
  _mp4Encoder->stop();
  decoder->_gotPktCallback = nullptr;
  return {_mp4Encoder->_saveFilePath.c_str()};
}

int QQuickRealTimePlayer::getVideoWidth() {
  if (!decoder) {
    return 0;
  }
  return decoder->width;
}

int QQuickRealTimePlayer::getVideoHeight() {
  if (!decoder) {
    return 0;
  }
  return decoder->height;
}

bool QQuickRealTimePlayer::enableAudio() {
  if (!decoder->HasAudio()) {
    return false;
  }
  // 音频参数
  SDL_AudioSpec audioSpec;
  audioSpec.freq = decoder->GetAudioSampleRate();
  audioSpec.format = AUDIO_S16;
  audioSpec.channels = decoder->GetAudioChannelCount();
  audioSpec.silence = 1;
  audioSpec.samples = decoder->GetAudioFrameSamples();
  audioSpec.padding = 0;
  audioSpec.size = 0;
  audioSpec.userdata = this;
  // 音频样本读取回调
  audioSpec.callback = [](void *Thiz, Uint8 *stream, int len) {
    auto *pThis = static_cast<QQuickRealTimePlayer *>(Thiz);
    SDL_memset(stream, 0, len);
    pThis->decoder->ReadAudioBuff(stream, len);
    if (pThis->isMuted) {
      SDL_memset(stream, 0, len);
    }
  };
  // 关闭音频
  SDL_CloseAudio();
  // 开启声音
  if (SDL_OpenAudio(&audioSpec, nullptr) == 0) {
    // 播放声音
    SDL_PauseAudio(0);
  } else {
    emit onError("开启音频出错，如需听声音请插入音频外设\n" +
                     QString(SDL_GetError()),
                 -1);
    return false;
  }
  return true;
}

void QQuickRealTimePlayer::disableAudio() { SDL_CloseAudio(); }

bool QQuickRealTimePlayer::startGifRecord() {
  if (playStop) {
    return false;
  }
  // 保存路径
  stringstream ss;
  ss << QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
            .toStdString()
     << "/";
  ss << std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count()
     << ".gif";
  if (!(decoder && decoder->HasVideo())) {
    return false;
  }
  // 创建gif编码器
  _gifEncoder = make_shared<GifEncoder>();
  if (!_gifEncoder->open(decoder->width, decoder->height,
                         decoder->GetVideoFrameFormat(), DEFAULT_GIF_FRAMERATE,
                         ss.str())) {
    return false;
  }
  // 设置获得解码帧回调
  decoder->_gotFrameCallback = [this](const shared_ptr<AVFrame> &frame) {
    if (!_gifEncoder) {
      return;
    }
    if (!_gifEncoder->isOpened()) {
      return;
    }
    // 根据GIF帧率跳帧
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
    if (_gifEncoder->getLastEncodeTime() + 1000 / _gifEncoder->getFrameRate() >
        now) {
      return;
    }
    // 编码
    _gifEncoder->encodeFrame(frame);
  };

  return true;
}

void QQuickRealTimePlayer::stopGifRecord() {
  decoder->_gotFrameCallback = nullptr;
  if (!_gifEncoder) {
    return;
  }
  _gifEncoder->close();
}