//
// Created by ice on 2022/1/29.
//

#ifndef FFMPEG_BASECHANNEL_H
#define FFMPEG_BASECHANNEL_H

#include "safe_queue.h"
#include "pthread.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/frame.h"
#include <libavutil/time.h>

};

class BaseChannel {
public:
    BaseChannel(int id, AVCodecContext *avCodecContext,AVRational timeBase) : id(id) ,avCodecContext(avCodecContext), timeBase(timeBase){
        packets.setReleaseCallback(releaseAvPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }

    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
//        avcodec_free_context(avCodecContext);
    }

    /**
     * 释放AVPacket
     * @param frame
     */
    static void releaseAvPacket(AVPacket **packet) {
        if (packet) {
            av_packet_free(packet);
            *packet = 0;
        }
    }

    /**
     * 释放AVFrame
     * @param frame
     */
    static void releaseAVFrame(AVFrame **frame) {
        if (frame) {
            av_frame_free(frame);
            *frame = 0;
        }
    }


    //纯虚函数
    virtual void stop() = 0;

    virtual void start() = 0;

    SafeQueue<AVPacket *> packets;
    SafeQueue<AVFrame *> frames;
    AVCodecContext *avCodecContext;
    int id;
    bool isPlaying = 0;
     AVRational timeBase;
    double audio_time;
};


#endif //FFMPEG_BASECHANNEL_H
