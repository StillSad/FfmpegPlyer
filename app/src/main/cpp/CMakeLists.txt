# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.10.2)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-z,notext -fPIC")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-z,notext -fPIC")

set(jnilibs ${CMAKE_SOURCE_DIR}/libs)
#set(jnilibs E:/workspace/AndroidDemo/ffmpeg/app/src/main/jniLibs)
set(libname ffmpeg_lib)


link_directories(
        ${jnilibs}/${ANDROID_ABI})

file(GLOB include-files
        ${CMAKE_SOURCE_DIR}/*.h)
include_directories(
        include
        include-files
)

file(GLOB src-files
        ${CMAKE_SOURCE_DIR}/*.cpp)

add_library( # Sets the name of the library.
        ${libname}

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${src-files}
        )

set(third-party-libs
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
        ${third-party-libs}
        ${native-libs}
        )