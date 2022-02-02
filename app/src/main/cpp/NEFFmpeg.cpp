//
// Created by ice on 2022/1/29.
//

#include <cstring>
#include "NEFFmpeg.h"
#include "macro.h"
#include "pthread.h"

NEFFmpeg::NEFFmpeg(JavaCallHelper *javaCallHelper, char *dataSource) {
    this->javaCallHelper = javaCallHelper;

    this->dataSource = new char[strlen(dataSource) + 1];
    strcpy(this->dataSource, dataSource);
}

NEFFmpeg::~NEFFmpeg() {
    DELETE(dataSource)
    DELETE(javaCallHelper)
}

/**
 * 准备线程pid_prepare真正执行的函数
 * @param args
 * @return
 */
void *task_prepare(void *args) {
    NEFFmpeg *ffmpeg = static_cast<NEFFmpeg *>(args);
    ffmpeg->_prepare();
    return 0;
}


void *task_start(void *args) {
    NEFFmpeg *ffmpeg = static_cast<NEFFmpeg *>(args);
    ffmpeg->_start();
    return 0;
}


void NEFFmpeg::_prepare() {
    formatContext = avformat_alloc_context();

    //设置超时时间10s
    AVDictionary *dictionary = NULL;
    av_dict_set(&dictionary, "timeout", "3000000", 0);
    int ret = avformat_open_input(&formatContext, this->dataSource, NULL, &dictionary);
    av_dict_free(&dictionary);

    if (ret) {
        //失败，回调给java
        LOGE("\n打开媒体失败:\ndataSource:%s\nerror:%s", this->dataSource, av_err2str(ret));
        return;
    }
    LOGE("打开媒体成功");
    //视频流
    ret = avformat_find_stream_info(formatContext, NULL);
    if (ret < 0) {
        LOGE("流信息获取失败:%s", av_err2str(ret));
    }

    for (int i = 0; i < formatContext->nb_streams; ++i) {
        //获取媒体流（音频或视频）
        AVStream *stream = formatContext->streams[i];
        //获取编解码这段流的参数
        AVCodecParameters *parameters = stream->codecpar;
        //通过参数中的id（编解码的方式），来查找当前流的解码器
        AVCodec *avCodec = avcodec_find_decoder(parameters->codec_id);
        if (!avCodec) {
            LOGE("没有找到解码器");
            return;
        }

        //创建解码器上下文
        AVCodecContext *codecContext = avcodec_alloc_context3(avCodec);
        //设置解码器上下文参数
        ret = avcodec_parameters_to_context(codecContext, parameters);
        if (ret < 0) {
            LOGE("设置解码器上下文参数失败");
            return;
        }
        //打开解码器
        ret = avcodec_open2(codecContext, avCodec, NULL);
        if (ret) {
            LOGE("解码器打开失败");
            return;
        }
        AVRational tiemBase = stream->time_base;
        //判断流类型
        switch (parameters->codec_type) {
            case AVMEDIA_TYPE_VIDEO: {
                //视频
                AVRational frame_rate = stream->avg_frame_rate;
                //计算fps
                int fps = av_q2d(frame_rate);
                videoChannel = new VideoChannel(i, codecContext, fps, tiemBase);
                videoChannel->serRenderCallback(renderCallback);
            }
            break;
            case AVMEDIA_TYPE_AUDIO: {
                //音频
                audioChannel = new AudioChannel(i, codecContext, tiemBase);
            }
            break;
        }


    }

    if (!audioChannel && !videoChannel) {
        LOGE("没有音频也没有视频");
//        javaCallHelper->onError(THREAD_CHILD, -1);
        return;
    }


    LOGE("准备完成");
    if (javaCallHelper) {
        javaCallHelper->onPrepared(THREAD_CHILD);
    }

}

/**
 * 播放准备
 */
void NEFFmpeg::prepare() {
    //创建子线程
    pthread_create(&pid_prepare, 0, task_prepare, this);
}


/**
 * 开始播放
 */
void NEFFmpeg::start() {
    isPlaying = 1;
    if (videoChannel) {
        videoChannel->setAudioChannel(audioChannel);
        videoChannel->start();
    }
    if (audioChannel)
        audioChannel->start();
    pthread_create(&pid_start, 0, task_start, this);
}

void NEFFmpeg::_start() {
    while (isPlaying) {
        if (videoChannel && videoChannel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        AVPacket *packet = av_packet_alloc();

        int ret = av_read_frame(formatContext, packet);
        LOGD("av_read_frame:%d", ret);

        if (!ret) {
            //ret为0表示成功
            if (videoChannel && packet->stream_index == videoChannel->id) {
                //往视频编码数据包队列中添加数据
                videoChannel->packets.push(packet);
            } else if (audioChannel && packet->stream_index == audioChannel->id) {
                //往音频编码数据包队列中添加数据
                audioChannel->packets.push(packet);
            }
        } else if (ret == AVERROR_EOF) {
            //表示读完了
            //要考虑读完了，是否播完了的情况
            //todo
            break;
        } else {
            LOGE("读取音视频数据包失败");
            break;
        }
    }
    LOGD("isPlaying:%d", isPlaying);

    isPlaying = 0;
    videoChannel->stop();
    audioChannel->stop();
    //停止解码播放（音频和视频）
}

void NEFFmpeg::setRenderCallBack(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}
