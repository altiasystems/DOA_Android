# Sets the minimum version of CMake required to build the native
# library. You should either keep the default value or only pass a
# value of 3.4.0 or lower.

cmake_minimum_required(VERSION 3.4.1)


if (${ANDROID_ABI} STREQUAL "armeabi-v7a")
  set(neon_flags -mfpu=neon)
else ()
  set(neon_flags)
endif ()


add_library( fft-lib
   STATIC

   arm_bitreversal2.c
   arm_cfft_f32.c
   arm_cfft_init_f32.c
   arm_cfft_radix8_f32.c
   arm_common_tables.c
   arm_const_structs.c)

target_include_directories(fft-lib PRIVATE
        ../arm_cfft/Include
        ../arm_cfft_core/Include)

