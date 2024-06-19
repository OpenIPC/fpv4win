//
// Created by liangzhuohua on 2022/2/28.
//

#include "JpegEncoder.h"

#include <memory>
inline bool convertToYUV420P(const shared_ptr<AVFrame> &frame, shared_ptr<AVFrame> &yuvFrame) {
    int width = frame->width;
    int height = frame->height;

    // Allocate YUV frame
    yuvFrame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *f){
        av_frame_free(&f);
    });
    if (!yuvFrame) {
        return false;
    }
    yuvFrame->format = AV_PIX_FMT_YUVJ420P;
    yuvFrame->width = width;
    yuvFrame->height = height;

    // Allocate buffer for YUV frame
    int ret = av_frame_get_buffer(yuvFrame.get(), 32);
    if (ret < 0) {
        return false;
    }

    // Convert RGB to YUV420P
    struct SwsContext *sws_ctx = sws_getContext(width, height, static_cast<AVPixelFormat>(frame->format),
                                                width, height, AV_PIX_FMT_YUVJ420P,
                                                0, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
        return false;
    }

    // Perform RGB to YUV conversion
    ret = sws_scale(sws_ctx, frame->data, frame->linesize, 0, height,
                    yuvFrame->data, yuvFrame->linesize);
    if (ret <= 0) {
        sws_freeContext(sws_ctx);
        return false;
    }

    // Cleanup
    sws_freeContext(sws_ctx);

    return true;
}

bool JpegEncoder::encodeJpeg(const string &outFilePath, const shared_ptr<AVFrame> &frame) {
    if (!(frame && frame->height && frame->width && frame->linesize[0])) {
        return false;
    }

    // 编码上下文
    shared_ptr<AVFormatContext> pFormatCtx
        = shared_ptr<AVFormatContext>(avformat_alloc_context(), &avformat_free_context);

    // 设置格式
    pFormatCtx->oformat = av_guess_format("mjpeg", nullptr, nullptr);

    // 初始化上下文
    if (avio_open(&pFormatCtx->pb, outFilePath.c_str(), AVIO_FLAG_READ_WRITE) < 0) {
        return false;
    }

    // 创建流
    AVStream *pAVStream = avformat_new_stream(pFormatCtx.get(), nullptr);
    if (pAVStream == nullptr) {
        return false;
    }

    // 查找编码器
    const AVCodec *pCodec = avcodec_find_encoder(pFormatCtx->oformat->video_codec);
    if (!pCodec) {
        return false;
    }
    // 设置编码参数
    shared_ptr<AVCodecContext> codecCtx = shared_ptr<AVCodecContext>(
        avcodec_alloc_context3(pCodec), [](AVCodecContext *ctx) { avcodec_free_context(&ctx); });
    codecCtx->codec_id = pFormatCtx->oformat->video_codec;
    codecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    codecCtx->pix_fmt = static_cast<AVPixelFormat>(frame->format);
    codecCtx->width = frame->width;
    codecCtx->height = frame->height;
    codecCtx->time_base = AVRational { 1, 25 };

    // Convert frame to YUV420P if it's not already in that format
    shared_ptr<AVFrame> yuvFrame;
    if (
        frame->format != AV_PIX_FMT_YUVJ420P &&
        frame->format != AV_PIX_FMT_YUV420P
        ) {
        if (!convertToYUV420P(frame, yuvFrame)) {
            return false;
        }
        codecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    } else {
        yuvFrame = frame; // If already YUV420P, use as is
    }

    // 打开编码器
    if (avcodec_open2(codecCtx.get(), pCodec, nullptr) < 0) {
        return false;
    }
    // 设置编码器参数
    avcodec_parameters_from_context(pAVStream->codecpar, codecCtx.get());

    // 写文件头
    avformat_write_header(pFormatCtx.get(), nullptr);
    int y_size = (codecCtx->width) * (codecCtx->height);
    // 调整包大小
    shared_ptr<AVPacket> pkt = shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket *pkt) { av_packet_free(&pkt); });
    av_new_packet(pkt.get(), y_size);

    // 发送帧到编码上下文
    int ret = avcodec_send_frame(codecCtx.get(), yuvFrame.get());
    if (ret < 0) {
        return false;
    }
    // 获取已经编码完成的帧
    avcodec_receive_packet(codecCtx.get(), pkt.get());
    // 写文件
    av_write_frame(pFormatCtx.get(), pkt.get());
    // 写文件尾
    av_write_trailer(pFormatCtx.get());
    // 关闭编码器
    avcodec_close(codecCtx.get());
    // 关闭文件
    avio_close(pFormatCtx->pb);
    return true;
}
