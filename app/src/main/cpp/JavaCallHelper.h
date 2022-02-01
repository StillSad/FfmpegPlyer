//
// Created by ice on 2022/1/29.
//

#ifndef FFMPEG_JAVACALLHELPER_H
#define FFMPEG_JAVACALLHELPER_H

#include <jni.h>
#include "macro.h"

class JavaCallHelper {
public:
    JavaCallHelper(JavaVM *javaVm, JNIEnv *env, jobject instance);

    ~JavaCallHelper();

    void onPrepared(int threadMode);

    void onError(int threadMode,int errorCode);

private:
    JavaVM *javaVm;
    JNIEnv *env;
    jobject instance;
    jmethodID jmd_error;
    jmethodID jmd_prepared;
};


#endif //FFMPEG_JAVACALLHELPER_H


