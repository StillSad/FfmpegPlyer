
#include <cstdio>
#include <cstring>
#include <jni.h>
#include <string>
#include <android//native_window_jni.h>
#include "NEFFmpeg.h"


JavaVM *javaVm = 0;
JavaCallHelper *javaCallHelper = 0;
NEFFmpeg *ffmpeg = 0;
ANativeWindow *window = 0;
//静态初始化mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    javaVm = vm;
    return JNI_VERSION_1_6;
}

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavfilter/avfilter.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

}


extern "C"
jstring getError(JNIEnv *env, int errnum) {
    char errorMsgBuffer[1024 * 4] = {0};
    av_strerror(errnum, errorMsgBuffer, sizeof(errorMsgBuffer));
    return env->NewStringUTF(errorMsgBuffer);
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_ice_ffmpeg_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    strcat(strBuffer, "\nlibavformat : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFORMAT_VERSION));
    strcat(strBuffer, "\nlibavutil : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVUTIL_VERSION));
    strcat(strBuffer, "\nlibavfilter : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFILTER_VERSION));
    strcat(strBuffer, "\nlibswresample : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWRESAMPLE_VERSION));
    strcat(strBuffer, "\nlibswscale : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWSCALE_VERSION));
    strcat(strBuffer, "\navcodec_configure : \n");
    strcat(strBuffer, avcodec_configuration());
    strcat(strBuffer, "\navcodec_license : ");
    strcat(strBuffer, avcodec_license());

    return env->NewStringUTF(strBuffer);
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_ice_ffmpeg_IcePlayer_native_1start(JNIEnv *env, jobject thiz,
                                                  jstring jpath,
                                                  jobject surface) {
    const char *path = env->GetStringUTFChars(jpath, 0);

    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);

    //初始化网络模块
    avformat_network_init();
    //总上下文
    AVFormatContext *formatContext = avformat_alloc_context();
    //设置超时时间
    AVDictionary *dictionary = NULL;
    av_dict_set(&dictionary, "timeout", "3000000", 0);
    //打开视频流
    int ret = avformat_open_input(&formatContext, path, NULL, &dictionary);
    if (ret) return getError(env, ret);

    //视频流
    avformat_find_stream_info(formatContext, NULL);
    //视频流索引
    int video_stream_idx = -1;
    for (int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }

    //解码器参数
    AVCodecParameters *codecpar = formatContext->streams[video_stream_idx]->codecpar;

    //解码器
    AVCodec *dec = avcodec_find_decoder(codecpar->codec_id);
    //解码器上下文
    AVCodecContext *codecContext = avcodec_alloc_context3(dec);
    //将解码器参数copy到解码器上下问
    avcodec_parameters_to_context(codecContext, codecpar);
    //打开解码器
    avcodec_open2(codecContext, dec, NULL);

    AVPacket *packet = av_packet_alloc();

    //转换上下文
    SwsContext *swsContext = sws_getContext(
            //输入源参数
            codecContext->width, codecContext->height, codecContext->pix_fmt,
            //输出参数
            codecContext->width, codecContext->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, 0, 0, 0);
    //
    ANativeWindow_setBuffersGeometry(nativeWindow, codecContext->width, codecContext->height,
                                     WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer outbuffer;
    //从视频流中读取数据包
    while (av_read_frame(formatContext, packet) >= 0) {
        ret = avcodec_send_packet(codecContext, packet);

        //packet数据转frame
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);

        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret < 0) {
            break;
        }

        //接受的容器
        uint8_t *dst_data[4];
        //每一行的 首地址
        int dst_linesize[4];
        //创建容器和首地址数组
        av_image_alloc(dst_data, dst_linesize, codecContext->width, codecContext->height,
                       AV_PIX_FMT_RGBA, 1);

        //yuv->rgba
        sws_scale(swsContext,
                //输入
                  frame->data, frame->linesize, 0, frame->height,
                //输出
                  dst_data, dst_linesize);
        ANativeWindow_lock(nativeWindow, &outbuffer, NULL);


        //渲染
        uint8_t *firstWindow = static_cast<uint8_t *>(outbuffer.bits);
        //输入源（rgba）的
        uint8_t *src_data = dst_data[0];
        //拿到一行又多少个字节RGBA
        int destStride = outbuffer.stride * 4;
        int src_linesize = dst_linesize[0];


        for (int i = 0; i < outbuffer.height; i++) {
            //内存拷贝来进行渲染
            memcpy(firstWindow + i * destStride, src_data + i * src_linesize, destStride);
        }


        ANativeWindow_unlockAndPost(nativeWindow);

    }

    if (ret < 0) return getError(env, ret);


    env->ReleaseStringUTFChars(jpath, path);

    return env->NewStringUTF("finish");
}


void renderFrame(uint8_t *src_data, int src_lineSize, int width, int height) {
    LOGD("renderFrame");

    pthread_mutex_lock(&mutex);
    if (!window) {
        pthread_mutex_unlock(&mutex);
        return;
    }
    ANativeWindow_setBuffersGeometry(window,width,height,WINDOW_FORMAT_RGBA_8888);

    ANativeWindow_Buffer windowBuffer;

    if (ANativeWindow_lock(window,&windowBuffer,0)) {
        ANativeWindow_release(window);
        window = 0;
        pthread_mutex_unlock(&mutex);
        return;
    }

    uint8_t *dst_data = static_cast<uint8_t *>(windowBuffer.bits);
    //输入源（rgba）的
    //拿到一行又多少个字节RGBA
    int dst_lineSize = windowBuffer.stride * 4;

    for (int i = 0; i < windowBuffer.height; i++) {
        //内存拷贝来进行渲染
        memcpy(dst_data + i * dst_lineSize, src_data + i * src_lineSize, dst_lineSize);
    }

    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&mutex);


}


extern "C"
JNIEXPORT void JNICALL
Java_com_ice_ffmpeg_IcePlayer_prepareNative(JNIEnv *env, jobject thiz, jstring dataSource_) {
    char *dataSource = const_cast<char *>(env->GetStringUTFChars(dataSource_, 0));

    JavaCallHelper *javaCallHelper = new JavaCallHelper(javaVm, env, thiz);

    ffmpeg = new NEFFmpeg(javaCallHelper, dataSource);
    ffmpeg->setRenderCallBack(renderFrame);
    ffmpeg->prepare();

    env->ReleaseStringUTFChars(dataSource_, dataSource);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ice_ffmpeg_IcePlayer_startNative(JNIEnv *env, jobject thiz) {
    if (!ffmpeg) return;
    ffmpeg->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ice_ffmpeg_IcePlayer_setSurfaceNative(JNIEnv *env, jobject thiz, jobject surface) {
    pthread_mutex_lock(&mutex);

    //先释放之前的显示窗口
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }

    //创建新的窗口用于视频显示
    window = ANativeWindow_fromSurface(env, surface);

    pthread_mutex_unlock(&mutex);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_ice_ffmpeg_IcePlayer_releaseNative(JNIEnv *env, jobject thiz) {
    pthread_mutex_lock(&mutex);
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
    pthread_mutex_unlock(&mutex);
    DELETE(ffmpeg)
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ice_ffmpeg_IcePlayer_stopNative(JNIEnv *env, jobject thiz) {
    if (ffmpeg) {
        ffmpeg->stop();
    }
}