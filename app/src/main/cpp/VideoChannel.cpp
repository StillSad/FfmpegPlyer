//
// Created by ice on 2022/1/29.
//

#include "VideoChannel.h"

VideoChannel::VideoChannel(int id, AVCodecContext *avCodecContext, int fps) : BaseChannel(id,avCodecContext) {
    this->fps = fps;
}

VideoChannel::~VideoChannel() {

}


void *task_video_decode(void *args) {
    VideoChannel *channel = static_cast<VideoChannel *>(args);
    channel->video_decode();
    return 0;
}

void *task_video_play(void *args) {
    VideoChannel *channel = static_cast<VideoChannel *>(args);
    channel->video_play();
    return 0;
}


void VideoChannel::start() {
    isPlaying = 1;
    //设置队列状态为工作状态
    packets.setWork(1);
    frames.setWork(1);
    //解码
    pthread_create(&pid_video_decode, 0, task_video_decode, this);
    //播放
    pthread_create(&pid_video_play, 0, task_video_play, this);

}

void VideoChannel::stop() {

}

/**
 * 真正视频解码
 */
void VideoChannel::video_decode() {
    AVPacket *packet = 0;

    while (isPlaying) {


        int ret = packets.pop(packet);
        if (!isPlaying) {
            //停止播放跳出循环释放packet
            break;
        }

        if (!ret) {
            LOGE("取数据包失败");
            continue;
        }
        //拿到了视频数据包(编码压缩了的),需要把数据包给解码器进行解码
        ret = avcodec_send_packet(avCodecContext, packet);
        if (ret) {
            //往解码器发数据失败跳出循环
            break;
        }
        releaseAvPacket(&packet);


        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            //重来
            continue;
        } else if (ret) {
            break;
        }

        while (isPlaying && frames.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        //ret == 0 数据收发正常，成功获取到了解码后的视频原始数据包AvFramg
        frames.push(frame);
    }

    releaseAvPacket(&packet);
}


void VideoChannel::video_play() {

    AVFrame *frame = NULL;
    SwsContext *swsContext = sws_getContext(
            //输入源参数
            avCodecContext->width, avCodecContext->height, avCodecContext->pix_fmt,
            //输出参数
            avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, 0, 0, 0);

    //接受的容器
    uint8_t *dst_data[4];
    //每一行的 首地址
    int dst_linesize[4];
    //创建容器和首地址数组
    av_image_alloc(dst_data, dst_linesize, avCodecContext->width, avCodecContext->height,
                   AV_PIX_FMT_RGBA, 1);

    //根据fps控制每一帧的延时时间单位毫秒
    double delay_time_pre_frame = 1.0 / fps;
    while (isPlaying) {

        int ret = frames.pop(frame);
        if (!isPlaying) {
            //停止播放跳出循环释放frame
            break;
        }
        if (!ret) {
            //取数据失败
            continue;
        }



        //yuv->rgba
        sws_scale(swsContext,
                //输入
                  frame->data, frame->linesize, 0, frame->height,
                //输出
                  dst_data, dst_linesize);

        //进行休眠(微秒)
        //平均时间 + 每一帧额外时间
        double extra_delay = frame->repeat_pict / (2 * fps);
        double real_delay = delay_time_pre_frame + extra_delay;
        av_usleep(real_delay * 1000000);
        //渲染，回调去native-lib里
        //todo
        renderCallback(dst_data[0], dst_linesize[0], avCodecContext->width, avCodecContext->height);
        releaseAVFrame(&frame);
    }
    releaseAVFrame(&frame);
    isPlaying = 0;
    av_freep(&dst_data[0]);
    sws_freeContext(swsContext);
}

void VideoChannel::serRenderCallback(RenderCallback callback) {
    this->renderCallback = callback;
}

