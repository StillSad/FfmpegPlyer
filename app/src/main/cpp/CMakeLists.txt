cmake_minimum_required(VERSION 3.10.2)


project("ffmpeg")

set(libname ffmpeg_lib)

include_directories(include)

link_directories(${CMAKE_SOURCE_DIR}//libs/${CMAKE_ANDROID_ARCH_ABI})

file(GLOB src-files ${CMAKE_SOURCE_DIR}/*.cpp)

add_library( # Sets the name of the library.
        ${libname}

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${src-files})


set(ffmpeg-libs
        avformat
        avcodec
        avfilter
        swresample
        swscale
        avutil
        )
set(native-libs
        android
        EGL
        GLESv3
        OpenSLES
        log
        m
        z
        )




target_link_libraries( # Specifies the target library.
        ${libname}


        # Links the target library to the log library
        # included in the NDK.
        ${log-lib}
        ${ffmpeg-libs}
        ${native-libs}
        )