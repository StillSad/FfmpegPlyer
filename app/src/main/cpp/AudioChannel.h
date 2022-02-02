//
// Created by ice on 2022/1/29.
//

#ifndef FFMPEG_AUDIOCHANNEL_H
#define FFMPEG_AUDIOCHANNEL_H

#include "BaseChannel.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>


extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/mathematics.h>

};

class  AudioChannel: public BaseChannel {
public:
    AudioChannel(int id, AVCodecContext *pContext,AVRational timeBase);

     ~AudioChannel();

     void start();

     void stop();

    void audio_decode();

    void audio_play();

    int getPCM();

    uint8_t *out_buffers = 0;
    //通道数
    int out_channels;
    //
    int out_sample_size;

    int out_sample_rate = 0;
    int out_buffers_size = 0;

private:
    pthread_t pid_audio_decode;
    pthread_t pid_audio_play;
    //引擎
    SLObjectItf engineObject = 0;
    //引擎接口
    SLEngineItf engineInterface = 0;
    //混音器
    SLObjectItf outputMixObject = 0;
    //播放器
    SLObjectItf bqPlayerObject = 0;
    //播放器接口
    SLPlayItf bqPlayerPlay = 0;
    //播放器队列接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = 0;
    //
    SwrContext *swrContext = 0;
};


#endif //FFMPEG_AUDIOCHANNEL_H
