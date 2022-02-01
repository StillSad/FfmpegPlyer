//
// Created by ice on 2022/1/29.
//

#include "JavaCallHelper.h"


JavaCallHelper::JavaCallHelper(JavaVM *javaVm, JNIEnv *env, jobject instance) {
    this->javaVm = javaVm;
    this->env = env;
    this->instance = env->NewGlobalRef(instance);
    //需要多线程操作，不能直接赋值
//    this->instance = instance;
    jclass clazz = env->GetObjectClass(instance);
    jmd_error = env->GetMethodID(clazz, "onError", "(I)V");
    jmd_prepared = env->GetMethodID(clazz, "onPrepared", "()V");

}


JavaCallHelper::~JavaCallHelper() {
    javaVm = 0;
    env->DeleteGlobalRef(instance);
}

void JavaCallHelper::onPrepared(int threadMode) {
    if (threadMode == THREAD_MAIN) {
        //主线程
        env->CallVoidMethod(instance, jmd_prepared);
    } else {
        //子线程
        JNIEnv *env_child;
        javaVm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(instance, jmd_prepared);
        javaVm->DetachCurrentThread();
    }
}

void JavaCallHelper::onError(int threadMode, int errorCode) {
    if (threadMode == THREAD_MAIN) {
        //主线程
        env->CallVoidMethod(instance, jmd_error, errorCode);
    } else {
        //子线程
        JNIEnv *env_child;
        javaVm->AttachCurrentThread(&env_child, 0);
        env_child->CallVoidMethod(instance, jmd_error, errorCode);
        javaVm->DetachCurrentThread();
    }
}

