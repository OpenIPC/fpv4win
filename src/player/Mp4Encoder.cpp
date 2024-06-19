//
// Created by liangzhuohua on 2022/3/1.
//

#include "Mp4Encoder.h"

Mp4Encoder::Mp4Encoder(const string &saveFilePath) {
    // 分配
    _formatCtx = shared_ptr<AVFormatContext>(avformat_alloc_context(), &avformat_free_context);
    // 设置格式
    _formatCtx->oformat = av_guess_format("mov", nullptr, nullptr);
    // 文件保存路径
    _saveFilePath = saveFilePath;
}

Mp4Encoder::~Mp4Encoder() {
    if (_isOpen) {
        stop();
    }
}

void Mp4Encoder::addTrack(AVStream *stream) {
    AVStream *os = avformat_new_stream(_formatCtx.get(), nullptr);
    if (!os) {
        return;
    }
    int ret = avcodec_parameters_copy(os->codecpar, stream->codecpar);
    if (ret < 0) {
        return;
    }
    os->codecpar->codec_tag = 0;
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        audioIndex = os->index;
        _originAudioTimeBase = stream->time_base;
    } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        videoIndex = os->index;
        _originVideoTimeBase = stream->time_base;
    }
}

bool Mp4Encoder::start() {
    // 初始化上下文
    if (avio_open(&_formatCtx->pb, _saveFilePath.c_str(), AVIO_FLAG_READ_WRITE) < 0) {
        return false;
    }
    // 写输出流头信息
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov", 0);
    int ret = avformat_write_header(_formatCtx.get(), &opts);
    if (ret < 0) {
        return false;
    }
    _isOpen = true;
    return true;
}

void Mp4Encoder::writePacket(const shared_ptr<AVPacket> &pkt, bool isVideo) {
    if (!_isOpen) {
        return;
    }
#ifdef I_FRAME_FIRST
    // 未获取视频关键帧前先忽略音频
    if (videoIndex >= 0 && !writtenKeyFrame && !isVideo) {
        return;
    }
    // 跳过非关键帧，使关键帧前置
    if (!writtenKeyFrame && pkt->flags & AV_PKT_FLAG_KEY) {
        return;
    }
    writtenKeyFrame = true;
#endif
    if (isVideo) {
        pkt->stream_index = videoIndex;
        av_packet_rescale_ts(pkt.get(), _originVideoTimeBase, _formatCtx->streams[videoIndex]->time_base);
    } else {
        pkt->stream_index = audioIndex;
        av_packet_rescale_ts(pkt.get(), _originAudioTimeBase, _formatCtx->streams[audioIndex]->time_base);
    }
    pkt->pos = -1;
    av_write_frame(_formatCtx.get(), pkt.get());
}

void Mp4Encoder::stop() {
    _isOpen = false;
    // 写文件尾
    av_write_trailer(_formatCtx.get());
    // 关闭文件
    avio_close(_formatCtx->pb);
}
