//
// Created by ice on 2022/1/29.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int id, AVCodecContext *avCodecContext, AVRational timeBase)
        : BaseChannel(id, avCodecContext, timeBase) {
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    out_sample_rate = 44100;
    //2（通道数） * 2 （16bit = 2字节） * 44100（采样率）
    out_buffers_size = out_channels * out_sample_size * out_sample_rate;
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));
    memset(out_buffers, 0, out_buffers_size);

    //创建重采样上下文
    swrContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                                    out_sample_rate,
                                    avCodecContext->channel_layout,
                                    avCodecContext->sample_fmt,
                                    avCodecContext->sample_rate, 0, 0);
    //初始化重采样上下文
    swr_init(swrContext);

}

AudioChannel::~AudioChannel() {
    if (swrContext) {
        swr_free(&swrContext);
        swrContext = 0;
    }

    DELETE(out_buffers)

}


void *task_audio_decode(void *args) {
    AudioChannel *channel = static_cast<AudioChannel *>(args);
    channel->audio_decode();
    return 0;
}

void *task_audio_play(void *args) {
    AudioChannel *channel = static_cast<AudioChannel *>(args);
    channel->audio_play();
    return 0;
}


void AudioChannel::start() {
    isPlaying = 1;
    packets.setWork(1);
    frames.setWork(1);
    //解码
    pthread_create(&pid_audio_decode, 0, task_audio_decode, this);
    //播放
    pthread_create(&pid_audio_play, 0, task_audio_play, this);

}


void AudioChannel::stop() {
    isPlaying = 0;
    packets.setWork(0);
    frames.setWork(0);

    pthread_join(pid_audio_decode,0);
    pthread_join(pid_audio_play,0);

    //设置播放器状态为停止
    if (bqPlayerPlay) {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
    }
    //销毁播放器
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;
        bqPlayerBufferQueue = 0;
    }
    //销毁混音器
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }
    //销毁引擎
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineInterface = 0;
    }



}

void AudioChannel::audio_decode() {
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
        //PCM数据包
        frames.push(frame);
    }

    releaseAvPacket(&packet);

}

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(context);

    int pcm_size = audioChannel->getPCM();

    if (pcm_size > 0) {
        (*bq)->Enqueue(bq, audioChannel->out_buffers, pcm_size);
    }

}


void AudioChannel::audio_play() {
    //创建引擎并获取引擎接口
    SLresult result;

    //创建引擎对象
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //初始化引擎
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //获取引擎接口
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //设置混音
    //创建混音器
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //初始化混音器
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //音频配置
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN};

    //数据源 将配置信息放到这个数据源中
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    //配置音轨（输出）
    //设置混音
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    //需要的接口 操作队列的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    //创建播放器
    //3.3 创建播放器
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &audioSrc,
                                                   &audioSnk, 1, ids, req);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //初始化播放器
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //获取播放器接口
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //设置播放回调函数
    //获取播放器队列接口
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
    //设置回调
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

    //设置播发器状态
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    //手动激活回调函数
    bqPlayerCallback(bqPlayerBufferQueue, this);


}

/**
 * 获取pcm数据
 * @return 数据大小
 */
int AudioChannel::getPCM() {
    int pcm_data_size = 0;
    AVFrame *frame = 0;

//    SwrContext *swrContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
//                                                out_sample_rate,
//                                                avCodecContext->channel_layout,
//                                                avCodecContext->sample_fmt,
//                                                avCodecContext->sample_rate, 0, 0);
//
//    //初始化重采样上下文
//    swr_init(swrContext);

    while (isPlaying) {
        int ret = frames.pop(frame);
        if (!isPlaying) {
            //停止播放跳出循环释放packet
            break;
        }

        if (!ret) {
            LOGE("取数据包失败");
            continue;
        }
        //pcm数据再frame中
        // 获取的pcm数据有可能和播放器中设置的pcm格式不一样
        //重采样
        //两个数据之间的时间间隔
        int64_t delay = swr_get_delay(swrContext, frame->sample_rate);
        //输出缓冲取能容纳的最大数据量
        int64_t out_max_samples = av_rescale_rnd(frame->nb_samples + delay, frame->sample_rate,
                                                 out_sample_rate, AV_ROUND_UP);


        int out_samples = swr_convert(swrContext,
                                      &out_buffers, out_max_samples,
                                      (const uint8_t **) (frame->data), frame->nb_samples);

        pcm_data_size = out_samples * out_sample_size * out_channels;
        //获取音频时间
        audio_time = frame->best_effort_timestamp * av_q2d(timeBase);

        break;

    }

    releaseAVFrame(&frame);

    return pcm_data_size;
}

