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

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    file(GLOB SRCS "src/*.cpp" "src/*.c" "src/linux/*.cpp" "src/linux/*.c")
else ()
    file(GLOB SRCS "src/*.cpp" "src/*.c")
endif ()
message(STATUS "Source Files : ${SRCS}")
add_executable(homecam ${SRCS})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(homecam Threads::Threads)

    FIND_LIBRARY(GCCLIBATOMIC_LIBRARY NAMES atomic atomic.so.1 libatomic.so.1
            HINTS
            /usr/local/lib64
            /usr/local/lib
            /opt/local/lib64
            /opt/local/lib
            /usr/lib64
            /usr/lib
            /lib64
            /lib
            )
    target_link_libraries(homecam ${GCCLIBATOMIC_LIBRARY})
endif ()

find_package(OpenCV REQUIRED HINTS 3rd-party/opencv/install)
include_directories(homecam ${OPENCV_INCLUDE_DIRS})
target_link_libraries(homecam ${OpenCV_LIBS})
set(OPENSSL_ROOT_DIR 3rd-party/openssl/sslinstall)
find_package(OpenSSL REQUIRED)
target_link_libraries(homecam ${OPENSSL_LIBRARIES})
include_directories(homecam ${OPENSSL_INCLUDE_DIR})

find_package(TBB REQUIRED HINTS 3rd-party/oneTBB/install)
target_link_libraries(homecam TBB::tbb)
include_directories(homecam ${TBB_INCLUDE_DIR})

find_library(TURBOJPEG_LIBRARY turbojpeg HINTS 3rd-party/libjpeg-turbo/install/lib)
target_link_libraries(homecam ${TURBOJPEG_LIBRARY})
include_directories(homecam 3rd-party/libjpeg-turbo/install/include)

include_directories(homecam ./include ./3rd-party)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    include_directories(homecam 3rd-party/raspberrypi 3rd-party/ilclient)
    find_library(ILCLIENT_LIBRARY ilclient HINTS 3rd-party/ilclient)
    find_library(BCM_HOST_LIBRARY bcm_host HINTS /opt/vc/lib)
    find_library(VCHIQ_ARM_LIBRARY vchiq_arm HINTS /opt/vc/lib)
    find_library(VCHOSTIF_LIBRARY vchostif HINTS /opt/vc/lib)
    find_library(VCILCS_LIBRARY vcilcs HINTS /opt/vc/lib)
    find_library(VCOS_LIBRARY vcos HINTS /opt/vc/lib)
    find_library(VCSM_LIBRARY vcsm HINTS /opt/vc/lib)
    target_link_libraries(homecam ${ILCLIENT_LIBRARY} ${BCM_HOST_LIBRARY} ${VCHIQ_ARM_LIBRARY} ${VCHOSTIF_LIBRARY} ${VCILCS_LIBRARY} ${VCOS_LIBRARY} ${VCSM_LIBRARY})
endif ()
