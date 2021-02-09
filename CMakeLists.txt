cmake_minimum_required(VERSION 3.1)
project(homecam)
ENABLE_LANGUAGE(CXX)
set(CMAKE_CXX_STANDARD 17)

#[[set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(tools /home/liuziang/x-tools/armv7-rpi4-linux-gnueabihf/)

#[=[set(CMAKE_SYSROOT ${tools})]=]
set(CMAKE_STAGING_PREFIX stage)

set(CMAKE_C_COMPILER ${tools}/bin/armv7-rpi4-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/armv7-rpi4-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)]]

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

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    file(GLOB SRCS "src/*.cpp" "src/*.c" "src/linux/*.cpp" "src/linux/*.c" "src/rpi/*.cpp" "src/rpi/*.c")
else ()
    file(GLOB SRCS "src/*.cpp" "src/*.c")
endif ()
message(STATUS "Source Files : ${SRCS}")
add_executable(homecam ${SRCS})
target_precompile_headers(homecam PUBLIC include/web/web.h include/web/session.h include/web/http_file_handler.h include/web/ssl_context_manager.h include/video/capture.h)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_library(PTHREAD NAMES libpthread.so.0
            HINTS
            ${tools}/lib/arm-linux-gnueabihf
            /usr/local/lib64
            /usr/local/lib
            /opt/local/lib64
            /opt/local/lib
            /usr/lib64
            /usr/lib
            /lib64
            /lib
            )
    if (NOT PTHREAD)
        message(FATAL_ERROR "PTHREAD library not found")
    endif ()
    message(STATUS "PTHREAD: ${PTHREAD}")
    target_link_libraries(homecam ${PTHREAD})

    find_library(GCCLIBATOMIC_LIBRARY NAMES atomic atomic.so.1 libatomic.so.1
            HINTS
            ${tools}/lib/arm-linux-gnueabihf
            /usr/local/lib64
            /usr/local/lib
            /opt/local/lib64
            /opt/local/lib
            /usr/lib64
            /usr/lib
            /lib64
            /lib
            )
    if (NOT GCCLIBATOMIC_LIBRARY)
        message(FATAL_ERROR "GCCLIBATOMIC_LIBRARY library not found")
    endif ()
    message(STATUS "GCCLIBATOMIC_LIBRARY: ${GCCLIBATOMIC_LIBRARY}")
    target_link_libraries(homecam ${GCCLIBATOMIC_LIBRARY})
endif ()

find_package(OpenCV REQUIRED HINTS 3rd-party/opencv/install)
include_directories(homecam ${OPENCV_INCLUDE_DIRS})
target_link_libraries(homecam ${OpenCV_LIBS})

find_library(LIBZ REQUIRED NAMES z HINTS ${tools}/lib)
if (NOT LIBZ)
    message(FATAL_ERROR "LIBZ library not found")
endif ()
message(STATUS "LIBZ: ${LIBZ}")
target_link_libraries(homecam ${LIBZ})

find_library(HARFBUZZ REQUIRED NAMES harfbuzz HINTS 3rd-party/harfbuzz/install/lib/arm-linux-gnueabihf)
if (NOT HARFBUZZ)
    message(FATAL_ERROR "HARFBUZZ library not found")
endif ()
message(STATUS "HARFBUZZ: ${HARFBUZZ}")
target_link_libraries(homecam ${HARFBUZZ})

find_library(LIBPNG REQUIRED NAMES png16 HINTS ${tools}/lib)
if (NOT LIBPNG)
    message(FATAL_ERROR "LIBPNG library not found")
endif ()
message(STATUS "LIBPNG: ${LIBPNG}")
target_link_libraries(homecam ${LIBPNG})

set(OPENSSL_ROOT_DIR 3rd-party/openssl/sslinstall)
find_package(OpenSSL REQUIRED)
target_link_libraries(homecam ${OPENSSL_LIBRARIES})
include_directories(homecam ${OPENSSL_INCLUDE_DIR})

find_package(TBB REQUIRED HINTS 3rd-party/oneTBB/install)
target_link_libraries(homecam TBB::tbb)
include_directories(homecam ${TBB_INCLUDE_DIR})

find_library(TURBOJPEG_LIBRARY turbojpeg HINTS 3rd-party/libjpeg-turbo/install/lib)
if (NOT TURBOJPEG_LIBRARY)
    message(FATAL_ERROR "TURBOJPEG_LIBRARY library not found")
endif ()
target_link_libraries(homecam ${TURBOJPEG_LIBRARY})
include_directories(homecam 3rd-party/libjpeg-turbo/install/include)

include_directories(homecam ./include ./3rd-party)

#rpi specific
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_library(MMAL REQUIRED NAMES mmal HINTS ${tools}/lib)
    if (NOT MMAL)
        message(FATAL_ERROR "MMAL library not found")
    endif ()
    message(STATUS "MMAL: ${MMAL}")
    target_link_libraries(homecam ${MMAL})

    find_library(MMAL_COMPONENTS REQUIRED NAMES mmal_components HINTS ${tools}/lib)
    if (NOT MMAL_COMPONENTS)
        message(FATAL_ERROR "MMAL_COMPONENTS library not found")
    endif ()
    message(STATUS "MMAL_COMPONENTS: ${MMAL_COMPONENTS}")
    target_link_libraries(homecam ${MMAL_COMPONENTS})

    find_library(MMAL_CORE REQUIRED NAMES mmal_core HINTS ${tools}/lib)
    if (NOT MMAL_CORE)
        message(FATAL_ERROR "MMAL_CORE library not found")
    endif ()
    message(STATUS "MMAL_CORE: ${MMAL_CORE}")
    target_link_libraries(homecam ${MMAL_CORE})

    find_library(MMAL_UTIL REQUIRED NAMES mmal_util HINTS ${tools}/lib)
    if (NOT MMAL_UTIL)
        message(FATAL_ERROR "MMAL_UTIL library not found")
    endif ()
    message(STATUS "MMAL_UTIL: ${MMAL_UTIL}")
    target_link_libraries(homecam ${MMAL_UTIL})

    find_library(MMAL_VC_CLIENT REQUIRED NAMES mmal_vc_client HINTS ${tools}/lib)
    if (NOT MMAL_VC_CLIENT)
        message(FATAL_ERROR "MMAL_VC_CLIENT library not found")
    endif ()
    message(STATUS "MMAL_VC_CLIENT: ${MMAL_VC_CLIENT}")
    target_link_libraries(homecam ${MMAL_VC_CLIENT})

    find_library(ILCLIENT_LIBRARY NAMES ilclient libilclient.a
            HINTS
            ${tools}/lib/)
    if (NOT ILCLIENT_LIBRARY)
        message(FATAL_ERROR "ILCLIENT_LIBRARY library not found")
    endif ()
    include_directories(homecam 3rd-party/raspberrypi 3rd-party/ilclient)

    find_library(BCM_HOST_LIBRARY bcm_host HINTS 3rd-party/vc/lib)
    if (NOT BCM_HOST_LIBRARY)
        message(FATAL_ERROR "BCM_HOST_LIBRARY library not found")
    endif ()
    find_library(VCHIQ_ARM_LIBRARY vchiq_arm HINTS 3rd-party/vc/lib)
    if (NOT VCHIQ_ARM_LIBRARY)
        message(FATAL_ERROR "VCHIQ_ARM_LIBRARY library not found")
    endif ()
    find_library(VCHOSTIF_LIBRARY vchostif HINTS 3rd-party/vc/lib)
    if (NOT VCHOSTIF_LIBRARY)
        message(FATAL_ERROR "VCHOSTIF_LIBRARY library not found")
    endif ()
    find_library(VCILCS_LIBRARY vcilcs HINTS 3rd-party/vc/lib)
    if (NOT VCILCS_LIBRARY)
        message(FATAL_ERROR "VCILCS_LIBRARY library not found")
    endif ()
    find_library(VCOS_LIBRARY vcos HINTS 3rd-party/vc/lib)
    if (NOT VCOS_LIBRARY)
        message(FATAL_ERROR "VCOS_LIBRARY library not found")
    endif ()
    find_library(VCSM_LIBRARY vcsm HINTS 3rd-party/vc/lib)
    if (NOT VCSM_LIBRARY)
        message(FATAL_ERROR "VCSM_LIBRARY library not found")
    endif ()
    find_library(CONTAINERS containers HINTS 3rd-party/vc/lib)
    if (NOT CONTAINERS)
        message(FATAL_ERROR "CONTAINERS library not found")
    endif ()
    target_link_libraries(homecam ${ILCLIENT_LIBRARY} ${BCM_HOST_LIBRARY} ${VCHIQ_ARM_LIBRARY} ${VCHOSTIF_LIBRARY} ${VCILCS_LIBRARY} ${VCOS_LIBRARY} ${VCSM_LIBRARY} ${CONTAINERS})
endif ()
