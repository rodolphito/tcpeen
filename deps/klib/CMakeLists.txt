cmake_minimum_required(VERSION 3.0.2)
project(klib LANGUAGES C)
enable_testing()

set(KLIB_SRCS
    kson.c
    kson.h
    ksort.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test)
add_library(klib STATIC ${KLIB_SRCS})

add_executable(ksort_test test/ksort_test.c)
add_test(ksort_test ksort_test)