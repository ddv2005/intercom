cmake_minimum_required(VERSION 3.4)

include(FindPkgConfig)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

set( PROJECT_NAME "wapa_httpd" )

## Set our project name
project(${PROJECT_NAME})
FILE(GLOB SRCS "*.cpp" "*.c" "*.cxx" "libconfig/*.c" "libconfig/*.cxx" "uvc/*.cpp" "zeromq/src/*.cpp")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_SOURCE_DIR}/libconfig -I /projects/3rdparty/ptlib/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -I${CMAKE_SOURCE_DIR}/libconfig -I /projects/3rdparty/ptlib/include")

link_directories(/projects/3rdparty/ptlib/lib_linux_aarch64)

add_executable(${PROJECT_NAME} ${SRCS})
target_link_libraries(${PROJECT_NAME}
  pthread
  "-lpt_s -lpthread -lrt -ldl")

