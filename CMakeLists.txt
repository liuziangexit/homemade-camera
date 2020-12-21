cmake_minimum_required(VERSION 3.1)
project(homecam)
ENABLE_LANGUAGE(CXX)

#options
set(OPTIMIZATION FALSE CACHE BOOL "Enable optimization")
set(DEBUG TRUE CACHE BOOL "Build with debug information")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
#set optimization flags
if(OPTIMIZATION)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
endif()

#debug flags
if(DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

#set warning level: enable all warning
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

#disable MSVC language extensions
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Za")
endif()

message(STATUS "Debug             : ${DEBUG}")
message(STATUS "Opt               : ${OPTIMIZATION}")
message(STATUS "System            : ${CMAKE_SYSTEM_NAME}")
message(STATUS "Project           : ${PROJECT_NAME}")
message(STATUS "Project directory : ${PROJECT_SOURCE_DIR}")

find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

add_executable(homecam src/main.cpp)
target_link_libraries(homecam Threads::Threads ${OpenCV_LIBS})
include_directories(homecam ./include ./3rd-party ${OPENCV_INCLUDE_DIRS})
