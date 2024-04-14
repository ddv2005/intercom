cmake_minimum_required(VERSION 3.4)

include(FindPkgConfig)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

## Use the variable PROJECT_NAME for changing the target name
set( PROJECT_NAME "intercom" )

## Set our project name
project(${PROJECT_NAME})


## Use all the *.cpp files we found under this folder for the project
FILE(GLOB SRCS "sqlite3/*.c" "*.cpp" "*.c" "*.cxx" "crypto/*.c" "libconfig/*.c" "libconfig/*.cpp"
    "lua/*.c" "lua/bitop/*.c" "lua/luasocket/*.c" "lua/src/*.c"
    "pjsupport/src/*.cpp" "serial/*.cpp" "swig/nativesrc/*.cpp"
    "zeromq/src/*.cpp"
    )

link_directories(${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjlib/lib
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjlib-util/lib
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjmedia/lib
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjnath/lib
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjsip/lib
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/third_party/lib
    )

add_executable(${PROJECT_NAME} ${SRCS})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPJ_AUTOCONF=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -O3 -fno-exceptions -fmessage-length=0 -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -DPJ_AUTOCONF=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 -O3 -fno-exceptions -fmessage-length=0 -fvisibility=hidden -fvisibility-inlines-hidden -fdata-sections -ffunction-sections -fomit-frame-pointer -ftree-vectorize")

include_directories(${CMAKE_SOURCE_DIR}/lua/src
    ${CMAKE_SOURCE_DIR}/pjsupport/include
    ${CMAKE_SOURCE_DIR}/zeromq/include
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjlib/include
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjlib-util/include
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjmedia/include
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjnath/include
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/pjsip/include
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/third_party/speex/include
    ${CMAKE_SOURCE_DIR}/../3rdparty/pjproject/third_party/build/speex
    )

#set(prefix ${CMAKE_HOST_SYSTEM_PROCESSOR}-unknown-linux-gnu)
execute_process(COMMAND bash "-c" "echo -n $MACHTYPE" OUTPUT_VARIABLE MACHTYPE)
set(prefix ${MACHTYPE})
target_link_libraries(${PROJECT_NAME} 
  "-lpjsua-${prefix} -lpjsip-ua-${prefix} -lpjsip-simple-${prefix} -lpjsip-${prefix} -lpjnath-${prefix} -lpjmedia-codec-${prefix} -lpjmedia-audiodev-${prefix} -lpjmedia-${prefix} -lspeex-${prefix} -lsrtp-${prefix} -lresample-${prefix} -lpjlib-util-${prefix} -lpj-${prefix} -lrt -lasound -lpthread")

set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-s -fwhole-program  -Wl,--gc-sections -static-libstdc++") 