# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.4.1)

add_definitions(-DCL_TARGET_OPENCL_VERSION=200)
add_definitions(-DCL_VERSION_2_0)

add_library(native-lib
            SHARED
            native-lib.cpp
            clMathOp_impl.cc
            clMathConst_impl.cc
            GRCLBase.cpp
            clComplexToMag_impl.cc
            clComplexToArg_impl.cc
            clLog_impl.cc
)

find_library(log-lib log)

set(TOOLCHAIN $ENV{HOME}/src/gnuradio-android/toolchain)

target_include_directories(native-lib PUBLIC
        ${TOOLCHAIN}/${ANDROID_ABI}/include
        ${TOOLCHAIN}/${ANDROID_ABI}/include/boost-1_69)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries(native-lib
        ${TOOLCHAIN}/${ANDROID_ABI}/lib/libgnuradio-runtime.so
        ${TOOLCHAIN}/${ANDROID_ABI}/lib/libgnuradio-pmt.so
        ${TOOLCHAIN}/${ANDROID_ABI}/lib/libgnuradio-blocks.so
        ${TOOLCHAIN}/${ANDROID_ABI}/lib/libgnuradio-sched.so
        ${TOOLCHAIN}/${ANDROID_ABI}/lib/libOpenCL.so
        ${TOOLCHAIN}/${ANDROID_ABI}/lib/libvolk.so
        ${log-lib} )