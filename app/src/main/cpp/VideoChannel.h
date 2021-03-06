//
// Created by ice on 2022/1/29.
//

#ifndef FFMPEG_VIDEOCHANNEL_H
#define FFMPEG_VIDEOCHANNEL_H

#include "BaseChannel.h"
#include "macro.h"
#include "AudioChannel.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
};


typedef void (*RenderCallback)(uint8_t *, int, int, int);

class VideoChannel: public BaseChannel {
public:
    VideoChannel(int id, AVCodecContext *pContext,int fps,AVRational timeBase);

    ~VideoChannel();

    void start();

    void stop();

    void video_play();

    void video_decode();

    void serRenderCallback(RenderCallback callback);

    void setAudioChannel(AudioChannel *audioChannel);

private:
    int fps;

    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback renderCallback;
    AudioChannel *audioChannel = 0;
};


#endif //FFMPEG_VIDEOCHANNEL_H
