# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

set(SNPEIncludeDir ${CMAKE_SOURCE_DIR}/snpe-sdk/include/zdl)
set(cfftIncludeDir ${CMAKE_SOURCE_DIR}/arm_cfft/Include)
set(cfftCoreIncludeDir ${CMAKE_SOURCE_DIR}/arm_cfft_core/Include)
set(fftSourceDir ${CMAKE_SOURCE_DIR}/fft)

add_subdirectory(${fftSourceDir})

add_library(
        native-lib
        SHARED

        native-lib.cpp
        doa.cpp
        CreateUserBuffer.cpp
        CheckRuntime.cpp
        GNSetBuilderOptions.cpp
        LoadContainer.cpp
        LoadInputTensor.cpp
        PreprocessInput.cpp
        SaveOutputTensor.cpp
        SetBuilderOptions.cpp
        udlExample.cpp
        Util.cpp)

target_include_directories(native-lib PRIVATE
        include
        ${cfftIncludeDir}
        ${cfftCoreIncludeDir}
        ${SNPEIncludeDir})


set(SNPELibDir ${CMAKE_SOURCE_DIR}/snpe-sdk/libs)
set(SNPELibs
        ${SNPELibDir}/${ANDROID_ABI}/libPlatformValidatorShared.so
        ${SNPELibDir}/${ANDROID_ABI}/libSNPE.so
        ${SNPELibDir}/${ANDROID_ABI}/libc++_shared.so
        ${SNPELibDir}/${ANDROID_ABI}/libcalculator.so
        ${SNPELibDir}/${ANDROID_ABI}/libcalculator_domains.so
        ${SNPELibDir}/${ANDROID_ABI}/libsnpe_adsp.so
        ${SNPELibDir}/${ANDROID_ABI}/libsnpe_dsp_domains.so
        ${SNPELibDir}/${ANDROID_ABI}/libsnpe_dsp_domains_system.so
        ${SNPELibDir}/${ANDROID_ABI}/libsnpe_dsp_domains_v2.so
        ${SNPELibDir}/${ANDROID_ABI}/libsnpe_dsp_domains_v2_system.so
        ${SNPELibDir}/${ANDROID_ABI}/libhta.so)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        native-lib
        fft-lib
        ${SNPELibs}
        ${log-lib} )
