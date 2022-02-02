package com.ice.ffmpeg
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import com.ice.ffmpeg.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private  val icePlayer  = IcePlayer()


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)


//        binding.tvStart.text = stringFromJNI()

        binding.tvStart.setOnClickListener {
            val file = File(externalCacheDir,"input.mp4")
            icePlayer.setDataSource(file.absolutePath)
            icePlayer.prepare()
//            icePlayer.start(file.absolutePath)
        }


        icePlayer.setSurfaceView(binding.surfaceView)

        icePlayer.listener = object:IcePlayer.MyErrorListener {
            override fun onError(errorCode: Int) {

            }

        }

        icePlayer.prepareListener = object :IcePlayer.OnPrepareListener {
            override fun prepare() {
                binding.tvStart.text = "准备完成"
                icePlayer.start()
            }

        }

    }


//    external fun stringFromJNI():String


//    companion object {
//        private val TAG = "IcePlayer"
//
//        init {
//            System.loadLibrary("ffmpeg_lib")
//        }
//    }

    override fun onResume() {
        super.onResume()
//        icePlayer.prepare()
    }

    override fun onStop() {
        super.onStop()
        icePlayer.stop()
    }


    override fun onDestroy() {
        super.onDestroy()
        icePlayer.release()
    }



}