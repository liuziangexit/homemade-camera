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
find_package(OpenCV REQUIRED)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(homecam Threads::Threads)
endif ()
find_package(OpenSSL REQUIRED)

#add_subdirectory(3rd-party/oneTBB)
#find_package(TBB REQUIRED PATHS 3rd-party/oneTBB)
find_package(TBB REQUIRED)

target_link_libraries(homecam ${OpenCV_LIBS} ${OPENSSL_LIBRARIES} TBB::tbb)
include_directories(homecam ./include ./3rd-party ${OPENCV_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${TBB_INCLUDE_DIR})

message(STATUS "building ilclient")
execute_process(COMMAND "./clean.sh&&./build.sh" WORKING_DIRECTORY 3rd-party/ilclient)