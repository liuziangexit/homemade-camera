cmake_minimum_required(VERSION 3.1)
project(homecam)
ENABLE_LANGUAGE(CXX)
set(CMAKE_CXX_STANDARD 17)

#options
set(OPTIMIZATION FALSE CACHE BOOL "Enable optimization")
set(DEBUG TRUE CACHE BOOL "Build with debug information")

#set optimization flags
if (OPTIMIZATION)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
endif ()

#debug flags
if (DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif ()

#set warning level: enable all warning
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

#disable MSVC language extensions
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Za")
endif ()

message(STATUS "Debug             : ${DEBUG}")
message(STATUS "Opt               : ${OPTIMIZATION}")
message(STATUS "System            : ${CMAKE_SYSTEM_NAME}")
message(STATUS "Project           : ${PROJECT_NAME}")
message(STATUS "Project Directory : ${PROJECT_SOURCE_DIR}")

add_executable(homecam src/main.cpp)
find_package(OpenCV REQUIRED)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(homecam Threads::Threads)
endif ()
find_package(OpenSSL REQUIRED)

target_link_libraries(homecam ${OpenCV_LIBS} ${OPENSSL_LIBRARIES})
include_directories(homecam ./include ./3rd-party ${OPENCV_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
