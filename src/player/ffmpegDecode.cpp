#include "ffmpegDecode.h"
#include <QThread>
#include <iostream>
#include <vector>
#include <QDateTime>



#define MAX_AUDIO_PACKET (2 * 1024 * 1024)

bool FFmpegDecoder::OpenInput(string &inputFile) {
  CloseInput();

  AVDictionary *param = nullptr;

  av_dict_set(&param, "preset", "ultrafast", 0);
  av_dict_set(&param, "tune", "zerolatency", 0);
  av_dict_set(&param, "buffer_size", "425984", 0);
  av_dict_set(&param, "rtsp_transport", "tcp", 0);
  av_dict_set(&param, "protocol_whitelist", "file,udp,rtp", 0);

  // 打开输入
  if (avformat_open_input(&pFormatCtx, inputFile.c_str(), nullptr, &param) !=
      0) {
    CloseInput();
    return false;
  }
  // 超时机制
  static const int timeout = 10;
  auto startTime = std::make_shared<uint64_t >();
  *startTime = QDateTime::currentDateTime().toSecsSinceEpoch();
  pFormatCtx->interrupt_callback.callback = [](void *ctx)->int {
    uint64_t now = QDateTime::currentDateTime().toSecsSinceEpoch();
    return now - *(uint64_t *)ctx > timeout;
  };
  pFormatCtx->interrupt_callback.opaque = startTime.get();


  if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
    CloseInput();
    return false;
  }

  // 分析超时，退出，可能格式不正确
  if(QDateTime::currentDateTime().toSecsSinceEpoch() - *startTime > timeout){
    CloseInput();
    return false;
  }
  pFormatCtx->interrupt_callback.callback = nullptr;
  pFormatCtx->interrupt_callback.opaque = nullptr;

  // 打开视频/音频输入
  hasVideoStream = OpenVideo();
  hasAudioStream = OpenAudio();

  isOpen = true;

  // 转换时间基
  if (videoStreamIndex != -1) {
    videoFramePerSecond =
        av_q2d(pFormatCtx->streams[videoStreamIndex]->r_frame_rate);
    videoBaseTime = av_q2d(pFormatCtx->streams[videoStreamIndex]->time_base);
  }
  if (audioStreamIndex != -1) {
    audioBaseTime = av_q2d(pFormatCtx->streams[audioStreamIndex]->time_base);
  }

  // 创建音频解码缓存
  if (hasAudioStream) {
    audioFifoBuffer = shared_ptr<AVFifoBuffer>(
        av_fifo_alloc(GetAudioFrameSamples() * GetAudioChannelCount() * 10),
        &av_fifo_free);
  }
  return true;
}

bool FFmpegDecoder::CloseInput() {
  isOpen = false;

  lock_guard<mutex> lck(_releaseLock);

  // 关闭流
  CloseVideo();
  CloseAudio();
  if (pFormatCtx) {
    avformat_close_input(&pFormatCtx);
    pFormatCtx = nullptr;
  }

  return true;
}

void freeFrame(AVFrame *f) { av_frame_free(&f); }
void freePkt(AVPacket *f) { av_packet_free(&f); }
void freeSwrCtx(SwrContext *s) { swr_free(&s); }

shared_ptr<AVFrame> FFmpegDecoder::GetNextFrame() {
  // 加锁，避免在此方法执行过程中解码器释放，导致崩溃
  lock_guard<mutex> lck(_releaseLock);
  shared_ptr<AVFrame> res;
  if (videoStreamIndex == -1 && audioStreamIndex == -1) {
    return res;
  }
  if (!isOpen) {
    return res;
  }

  // 读输入流
  while (true) {
    if (!pFormatCtx) {
      throw runtime_error("分配解析器出错");
    }
    shared_ptr<AVPacket> packet =
        shared_ptr<AVPacket>(av_packet_alloc(), &freePkt);
    int ret = av_read_frame(pFormatCtx, packet.get());
    if (ret < 0) {
      char errStr[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(ret, errStr, AV_ERROR_MAX_STRING_SIZE);
      throw runtime_error("解析视频出错 " + string(errStr));
    }
    // 计算码率
    {
      bytesSecond += packet->size;
      uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
      if (now - lastCountBitrateTime >= 1000) {
        // 计算码率定时器
        bitrate = bytesSecond * 8 * 1000 / (now - lastCountBitrateTime);
        bytesSecond = 0;
        if (onBitrate) {
          onBitrate(bitrate);
        }
        lastCountBitrateTime = now;
      }
    }
    if (packet->stream_index == videoStreamIndex) {
      // 回调nalu
      if (_gotPktCallback) {
        _gotPktCallback(packet);
      }
      // 处理视频数据
      shared_ptr<AVFrame> pVideoYuv =
          shared_ptr<AVFrame>(av_frame_alloc(), &freeFrame);
      // 解码视频祯
      bool isDecodeComplite = DecodeVideo(packet.get(), pVideoYuv);
      if (isDecodeComplite) {
        res = pVideoYuv;
      }
      // 回调frame
      if (_gotFrameCallback) {
        _gotFrameCallback(pVideoYuv);
      }
      break;
    } else if (packet->stream_index == audioStreamIndex) {
      // 回调nalu
      if (_gotPktCallback) {
        _gotPktCallback(packet);
      }
      // 处理音频数据
      if (packet->dts != AV_NOPTS_VALUE) {
        int audioFrameSize = MAX_AUDIO_PACKET;
        shared_ptr<uint8_t> pFrameAudio =
            shared_ptr<uint8_t>(new uint8_t[audioFrameSize]);
        // 解码音频祯
        int nDecodedSize = DecodeAudio(audioStreamIndex, packet.get(),
                                       pFrameAudio.get(), audioFrameSize);
        // 解码成功，解码数据写入音频缓存
        if (nDecodedSize > 0) {
          writeAudioBuff(pFrameAudio.get(), nDecodedSize);
        }
      }
      if (!HasVideo()) {
        return res;
      }
    }
  }
  return res;
}

bool FFmpegDecoder::OpenVideo() {
  bool res = false;

  if (pFormatCtx) {
    videoStreamIndex = -1;

    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
      if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        videoStreamIndex = i;
        const AVCodec *codec =
            avcodec_find_decoder(pFormatCtx->streams[i]->codecpar->codec_id);

        if (codec) {
          pVideoCodecCtx = avcodec_alloc_context3(codec);
          if (pVideoCodecCtx) {
            if (avcodec_parameters_to_context(
                    pVideoCodecCtx, pFormatCtx->streams[i]->codecpar) >= 0) {
              res = !(avcodec_open2(pVideoCodecCtx, codec, nullptr) < 0);
              if (res) {
                width = pVideoCodecCtx->width;
                height = pVideoCodecCtx->height;
              }
            }
          }
        }

        break;
      }
    }

    if (!res) {
      CloseVideo();
    }
  }

  return res;
}

bool FFmpegDecoder::DecodeVideo(const AVPacket *avpkt,
                                const shared_ptr<AVFrame>& pOutFrame) {
  bool res = false;

  if (pVideoCodecCtx && avpkt && pOutFrame) {
    int ret = avcodec_send_packet(pVideoCodecCtx, avpkt);
    if (ret < 0) {
      char errStr[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(ret, errStr, AV_ERROR_MAX_STRING_SIZE);
      throw runtime_error("发送视频包出错 " + string(errStr));
    }

    ret = avcodec_receive_frame(pVideoCodecCtx, pOutFrame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      // No output available right now or end of stream
      res = false;
    } else if (ret < 0) {
      char errStr[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(ret, errStr, AV_ERROR_MAX_STRING_SIZE);
      throw runtime_error("解码视频出错 " + string(errStr));
    } else {
      // Successfully decoded a frame
      res = true;
    }
  }

  return res;
}

bool FFmpegDecoder::OpenAudio() {
  bool res = false;

  if (pFormatCtx) {
    audioStreamIndex = -1;

    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
      if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        audioStreamIndex = i;
        const AVCodec *codec =
            avcodec_find_decoder(pFormatCtx->streams[i]->codecpar->codec_id);

        if (codec) {
          pAudioCodecCtx = avcodec_alloc_context3(codec);
          if (pAudioCodecCtx) {
            if (avcodec_parameters_to_context(
                    pAudioCodecCtx, pFormatCtx->streams[i]->codecpar) >= 0) {
              res = !(avcodec_open2(pAudioCodecCtx, codec, nullptr) < 0);
            }
          }
        }

        break;
      }
    }

    if (!res) {
      CloseAudio();
    }
  }

  return res;
}

void FFmpegDecoder::CloseVideo() {
  if (pVideoCodecCtx) {
    avcodec_close(pVideoCodecCtx);
    pVideoCodecCtx = nullptr;
    pVideoCodec = nullptr;
    videoStreamIndex = 0;
  }
}

void FFmpegDecoder::CloseAudio() {
  if (pAudioCodecCtx) {
    avcodec_close(pAudioCodecCtx);
    pAudioCodecCtx = nullptr;
    pAudioCodec = nullptr;
    audioStreamIndex = 0;
  }
}

int FFmpegDecoder::DecodeAudio(int nStreamIndex, const AVPacket *avpkt,
                               uint8_t *pOutBuffer, size_t nOutBufferSize) {
  int decodedSize = 0;

  int packetSize = avpkt->size;
  const uint8_t *pPacketData = avpkt->data;

  while (packetSize > 0) {
    int sizeToDecode = nOutBufferSize;
    uint8_t *pDest = pOutBuffer + decodedSize;
    AVFrame *audioFrame = av_frame_alloc();
    if (!audioFrame) {
      throw std::runtime_error("Failed to allocate audio frame");
      return 0;
    }

    int packetDecodedSize = avcodec_receive_frame(pAudioCodecCtx, audioFrame);

    if (packetDecodedSize >= 0) {
      if (audioFrame->format != AV_SAMPLE_FMT_S16) {
        // Convert frame to AV_SAMPLE_FMT_S16 if needed
        if (!swrCtx) {
          auto ptr = swr_alloc_set_opts(
              nullptr, pAudioCodecCtx->channel_layout, AV_SAMPLE_FMT_S16,
              pAudioCodecCtx->sample_rate, pAudioCodecCtx->channel_layout,
              static_cast<AVSampleFormat>(audioFrame->format),
              pAudioCodecCtx->sample_rate, 0, nullptr);

          auto ret = swr_init(ptr);
          if (ret < 0) {
            char errStr[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errStr, AV_ERROR_MAX_STRING_SIZE);
            throw runtime_error("解码音频出错 " + string(errStr));
            return 0;
          }
          swrCtx = shared_ptr<SwrContext>(ptr, &freeSwrCtx);
        }

        // Convert audio frame to S16 format
        int samples = swr_convert(swrCtx.get(), &pDest, audioFrame->nb_samples,
                                  (const uint8_t **)audioFrame->data,
                                  audioFrame->nb_samples);
        sizeToDecode = samples * pAudioCodecCtx->channels *
                       av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
      } else {
        // Copy S16 audio data directly
        sizeToDecode = av_samples_get_buffer_size(
            nullptr, pAudioCodecCtx->channels, audioFrame->nb_samples,
            AV_SAMPLE_FMT_S16, 1);
        memcpy(pDest, audioFrame->data[0], sizeToDecode);
      }
    }

    av_frame_free(&audioFrame);

    if (packetDecodedSize < 0) {
      decodedSize = 0;
      break;
    }

    packetSize -= packetDecodedSize;
    pPacketData += packetDecodedSize;

    if (sizeToDecode <= 0) {
      continue;
    }

    decodedSize += sizeToDecode;
  }

  return decodedSize;
}

void FFmpegDecoder::writeAudioBuff(uint8_t *aSample, size_t aSize) {
  lock_guard<mutex> lck(abBuffMtx);
  if (av_fifo_space(audioFifoBuffer.get()) < aSize) {
    std::vector<uint8_t> tmp;
    tmp.resize(aSize);
    av_fifo_generic_read(audioFifoBuffer.get(), tmp.data(), aSize, nullptr);
  }
  av_fifo_generic_write(audioFifoBuffer.get(), aSample, aSize, nullptr);
}

size_t FFmpegDecoder::ReadAudioBuff(uint8_t *aSample, size_t aSize) {
  lock_guard<mutex> lck(abBuffMtx);
  if (av_fifo_size(audioFifoBuffer.get()) < aSize) {
    return 0;
  }
  av_fifo_generic_read(audioFifoBuffer.get(), aSample, aSize, nullptr);
  return aSize;
}
void FFmpegDecoder::ClearAudioBuff() {
  lock_guard<mutex> lck(abBuffMtx);
  av_fifo_reset(audioFifoBuffer.get());
}
