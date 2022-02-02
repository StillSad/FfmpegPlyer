//
// Created by ice on 2022/1/29.
//

#ifndef FFMPEG_NEFFMPEG_H
#define FFMPEG_NEFFMPEG_H

#include "JavaCallHelper.h"
#include "AudioChannel.h"
#include "VideoChannel.h"


extern "C" {
#include <libavformat/avformat.h>
}


class NEFFmpeg {
    friend void *task_stop(void *args);
public:
    NEFFmpeg(JavaCallHelper *javaCallHelper, char *dataSource);

    ~NEFFmpeg();

    void prepare();

    void _prepare();

    void start();

    void _start();


    void setRenderCallBack(RenderCallback renderCallback);

    void stop();

private:
    JavaCallHelper *javaCallHelper = 0;
    AudioChannel *audioChannel = 0;
    VideoChannel *videoChannel = 0;
    AVFormatContext *formatContext;
    RenderCallback renderCallback;
    char *dataSource = 0;
    pthread_t pid_prepare;
    pthread_t pid_start;
    pthread_t pid_stop;
    bool isPlaying;
};


#endif //FFMPEG_NEFFMPEG_H
