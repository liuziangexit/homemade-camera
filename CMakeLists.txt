cmake_minimum_required(VERSION 3.1)
project(homecam)
ENABLE_LANGUAGE(CXX)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g")
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif ()

#disable MSVC language extensions
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Za")
endif ()

message(STATUS "System            : ${CMAKE_SYSTEM_NAME}")
message(STATUS "Project           : ${PROJECT_NAME}")
message(STATUS "Project Directory : ${PROJECT_SOURCE_DIR}")

add_executable(homecam src/main.cpp)
#OPENCV
find_package(OpenCV REQUIRED)

#PTHREAD
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(homecam Threads::Threads)
endif ()

#OPENSSL
find_package(OpenSSL REQUIRED)

#Intel TBB
#add_subdirectory(3rd-party/oneTBB)
#find_package(TBB REQUIRED PATHS 3rd-party/oneTBB)
find_package(TBB REQUIRED)

target_link_libraries(homecam ${OpenCV_LIBS} ${OPENSSL_LIBRARIES} TBB::tbb)

#FFMPEG
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    find_library(FOUNDATION Foundation)
    if (NOT FOUNDATION)
        message(FATAL_ERROR "Foundation not found")
    endif ()
    target_link_libraries(homecam ${FOUNDATION})

    find_library(AV_FOUNDATION AVFoundation)
    if (NOT AV_FOUNDATION)
        message(FATAL_ERROR "AVFoundation not found")
    endif ()
    target_link_libraries(homecam ${AV_FOUNDATION})

    find_library(CORE_FOUNDATION CoreFoundation)
    if (NOT CORE_FOUNDATION)
        message(FATAL_ERROR "CoreFoundation not found")
    endif ()
    target_link_libraries(homecam ${CORE_FOUNDATION})

    find_library(CORE_MEDIA CoreMedia)
    if (NOT CORE_MEDIA)
        message(FATAL_ERROR "CoreMedia not found")
    endif ()
    target_link_libraries(homecam ${CORE_MEDIA})

    find_library(CORE_VIDEO CoreVideo)
    if (NOT CORE_VIDEO)
        message(FATAL_ERROR "CoreVideo not found")
    endif ()
    target_link_libraries(homecam ${CORE_VIDEO})

    find_library(AUDIO_TOOLBOX AudioToolbox)
    if (NOT AUDIO_TOOLBOX)
        message(FATAL_ERROR "AudioToolbox not found")
    endif ()
    target_link_libraries(homecam ${AUDIO_TOOLBOX})

    find_library(VIDEO_TOOLBOX VideoToolbox)
    if (NOT VIDEO_TOOLBOX)
        message(FATAL_ERROR "VideoToolbox not found")
    endif ()
    target_link_libraries(homecam ${VIDEO_TOOLBOX})

    find_library(CORE_GRAPHICS CoreGraphics)
    if (NOT CORE_GRAPHICS)
        message(FATAL_ERROR "CoreGraphics not found")
    endif ()
    target_link_libraries(homecam ${CORE_GRAPHICS})

    find_library(CORE_ANIMATION QuartzCore)
    if (NOT CORE_ANIMATION)
        message(FATAL_ERROR "CoreAnimation not found")
    endif ()
    target_link_libraries(homecam ${CORE_ANIMATION})

    find_library(APP_KIT AppKit)
    if (NOT APP_KIT)
        message(FATAL_ERROR "AppKit not found")
    endif ()
    target_link_libraries(homecam ${APP_KIT})

    find_library(SECURITY Security)
    if (NOT SECURITY)
        message(FATAL_ERROR "Security not found")
    endif ()
    target_link_libraries(homecam ${SECURITY})

endif ()

find_package(ZLIB)
target_link_libraries(homecam ZLIB::ZLIB)

find_library(ICONV iconv)
if (NOT ICONV)
    message(FATAL_ERROR "iconv not found")
endif ()
target_link_libraries(homecam ${ICONV})

find_library(BZ2 bz2)
if (NOT BZ2)
    message(FATAL_ERROR "bz2 not found")
endif ()
target_link_libraries(homecam ${BZ2})

find_library(OPENGL opengl)
if (NOT OPENGL)
    message(FATAL_ERROR "opengl not found")
endif ()
target_link_libraries(homecam ${OPENGL})

find_library(SDL2 sdl2)
if (NOT SDL2)
    message(FATAL_ERROR "sdl2 not found")
endif ()
target_link_libraries(homecam ${SDL2})

find_library(LZMA lzma)
if (NOT LZMA)
    message(FATAL_ERROR "lzma not found")
endif ()
target_link_libraries(homecam ${LZMA})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__STDC_CONSTANT_MACROS")

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)

find_path(AVFILTER_INCLUDE_DIR libavfilter/avfilter.h)
find_library(AVFILTER_LIBRARY avfilter)

find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)
find_library(SWSCALE_LIBRARY swscale)

find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h)
find_library(SWRESAMPLE_LIBRARY swresample)

target_link_libraries(homecam ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${AVFILTER_LIBRARY} ${SWSCALE_LIBRARY} ${SWRESAMPLE_LIBRARY})
include_directories(homecam ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR} ${AVFILTER_INCLUDE_DIR})

include_directories(homecam ./include ./3rd-party ${OPENCV_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${TBB_INCLUDE_DIR})