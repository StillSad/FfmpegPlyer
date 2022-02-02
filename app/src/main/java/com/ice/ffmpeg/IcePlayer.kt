package com.ice.ffmpeg

import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView


class IcePlayer : SurfaceHolder.Callback {


    private var mDataSource: String? = null


    fun setDataSource(dataSource: String) {
        mDataSource = dataSource
    }

    private var mSurfaceHolder: SurfaceHolder? = null


    fun setSurfaceView(surfaceView: SurfaceView) {
        mSurfaceHolder?.removeCallback(this)
        mSurfaceHolder = surfaceView.holder
        mSurfaceHolder?.addCallback(this)

    }


    override fun surfaceCreated(p0: SurfaceHolder) {

    }

    override fun surfaceChanged(surfaceHolder: SurfaceHolder, p1: Int, p2: Int, p3: Int) {
        setSurfaceNative(surfaceHolder.surface)
    }

    override fun surfaceDestroyed(p0: SurfaceHolder) {
    }

    fun start() {
        startNative()
    }

    /**
     * 播放准备工作
     */
    fun prepare() {
        if (mDataSource == null) return
        prepareNative(mDataSource!!)
    }



     fun onError(errorCode:Int) {
        listener?.onError(errorCode)
         Log.e(TAG,"onError")
    }

    /**
     * 播放器准备好了可以开始播放
     */
    fun onPrepared() {
        prepareListener?.prepare()
    }

    var listener:MyErrorListener? = null

    interface MyErrorListener {
        fun onError(errorCode:Int)
    }


    var prepareListener:OnPrepareListener? = null

    interface  OnPrepareListener{
        fun prepare()
    }


    /**
     * 停止播放
     */
    fun  stop() {
        stopNative()
    }

    fun release() {
        mSurfaceHolder?.removeCallback(this)
        releaseNative()
    }

    private external fun prepareNative(dataSource: String)


    external fun native_start(path: String, surface: Surface): String

    external fun startNative()

    external fun setSurfaceNative(surface: Surface)

    external fun stopNative()

    external fun releaseNative()

    companion object {
        private val TAG = "IcePlayer"

        init {
            System.loadLibrary("ffmpeg_lib")
        }
    }

}