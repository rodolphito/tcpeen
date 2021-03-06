# Copyright 2010-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License").
# You may not use this file except in compliance with the License.
# A copy of the License is located at
#
#  http://aws.amazon.com/apache2.0
#
# or in the "license" file accompanying this file. This file is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
# express or implied. See the License for the specific language governing
# permissions and limitations under the License.

cmake_minimum_required(VERSION 3.0)
project(aws-c-common LANGUAGES C VERSION 0.1.0)
set(TARGET_PREFIX_STR "aws-c-common")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AwsCFlags)
include(AwsSharedLibSetup)
include(AwsFeatureTests)
include(AwsSanitizers)
include(AwsSIMD)

set(GENERATED_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_INCLUDE_DIR "${GENERATED_ROOT_DIR}/include")
set(GENERATED_CONFIG_HEADER "${GENERATED_INCLUDE_DIR}/aws/common/config.h")
set(CONFIG_HEADER_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/include/aws/common/config.h.in")

file(GLOB AWS_COMMON_HEADERS
        "include/aws/common/*.h"
        "include/aws/common/*.inl"
        )
# The generated config header doesn't exist yet, so don't use GLOB to try to find it
# (as it'll skip the file if it doesn't exist yet)
set(AWS_COMMON_HEADERS ${AWS_COMMON_HEADERS} ${GENERATED_CONFIG_HEADER})

file(GLOB AWS_TEST_HEADERS
        "include/aws/testing/*.h"
        )

file(GLOB AWS_COMMON_PRIV_HEADERS
        "include/aws/common/private/*.h"
        "include/aws/common/private/*.c"
        )

file(GLOB AWS_COMMON_SRC
        "source/*.c"
        )

option(PERFORM_HEADER_CHECK "Performs compile-time checks that each header can be included independently. Requires a C++ compiler.")
option(AWS_NUM_CPU_CORES "Number of CPU cores of the target machine. Useful when cross-compiling." 0)

if (WIN32)
    file(GLOB AWS_COMMON_OS_HEADERS
            "include/aws/common/windows/*"
            )
    file(GLOB AWS_COMMON_OS_SRC
            "source/windows/*.c"
            )

    if (MSVC)
        source_group("Header Files\\aws\\common" FILES ${AWS_COMMON_HEADERS})
        source_group("Header Files\\aws\\common\\private" FILES ${AWS_COMMON_PRIV_HEADERS})
        source_group("Header Files\\aws\\testing" FILES ${AWS_TEST_HEADERS})
        source_group("Source Files" FILES ${AWS_COMMON_SRC})
        source_group("Source Files\\windows" FILES ${AWS_COMMON_OS_SRC})
    endif ()

    set(PLATFORM_LIBS BCrypt Kernel32 Ws2_32)
else ()
    file(GLOB AWS_COMMON_OS_HEADERS
        "include/aws/common/posix/*"
        )
    file(GLOB AWS_COMMON_OS_SRC
        "source/posix/*.c"
        )
    if (APPLE)
        find_library(CORE_FOUNDATION_LIB CoreFoundation)
        if (NOT CORE_FOUNDATION_LIB)
            message(FATAL_ERROR "Core Foundation not found")
        endif ()

        set(PLATFORM_LIBS pthread ${CORE_FOUNDATION_LIB})
    elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux") # Android does not link to libpthread nor librt, so this is fine
        set(PLATFORM_LIBS pthread rt)
    endif()
endif()

file(GLOB COMMON_HEADERS
        ${AWS_COMMON_HEADERS}
        ${AWS_COMMON_OS_HEADERS}
        ${AWS_COMMON_PRIV_HEADERS}
        ${AWS_TEST_HEADERS}
        )

file(GLOB COMMON_SRC
        ${AWS_COMMON_SRC}
        ${AWS_COMMON_OS_SRC}
        )

if (PERFORM_HEADER_CHECK)
    set(HEADERS_TO_CHECK ${AWS_COMMON_HEADERS} ${AWS_TEST_HEADERS} ${AWS_COMMON_OS_HEADERS})
    add_subdirectory(cmake/header-tester)
endif()

add_library(${TARGET_PREFIX_STR} ${COMMON_SRC})
aws_set_common_properties(${TARGET_PREFIX_STR} NO_WEXTRA)
aws_prepare_symbol_visibility_args(${TARGET_PREFIX_STR} "AWS_COMMON")

#apple source already includes the definitions we want, and setting this posix source
#version causes it to revert to an older version. So don't turn it on there, we don't need it.
if (UNIX AND NOT APPLE)
    #this only gets applied to aws-c-common (not its consumers).
    target_compile_definitions(${TARGET_PREFIX_STR} PRIVATE -D_POSIX_C_SOURCE=200809L)
endif()

aws_add_sanitizers(${TARGET_PREFIX_STR} BLACKLIST "sanitizer-blacklist.txt")
target_link_libraries(${TARGET_PREFIX_STR} PUBLIC ${PLATFORM_LIBS})

if (AWS_NUM_CPU_CORES)
    target_compile_definitions(${TARGET_PREFIX_STR} PRIVATE -DAWS_NUM_CPU_CORES=${AWS_NUM_CPU_CORES})
endif()

# Our ABI is not yet stable
set_target_properties(${TARGET_PREFIX_STR} PROPERTIES VERSION 1.0.0)
set_target_properties(${TARGET_PREFIX_STR} PROPERTIES SOVERSION 0unstable)

target_include_directories(${TARGET_PREFIX_STR} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
# When we install, the generated header will be at the INSTALL_INTERFACE:include location,
# but at build time we need to explicitly include this here
target_include_directories(${TARGET_PREFIX_STR} PUBLIC
        $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIR}>)

# Enable SIMD encoder if the compiler supports the right features
simd_add_definitions(${TARGET_PREFIX_STR})

if (HAVE_MAY_I_USE OR HAVE_BUILTIN_CPU_SUPPORTS OR HAVE_MSVC_CPUIDEX)
    set(HAVE_SIMD_CPUID TRUE)
endif()

if (HAVE_AVX2_INTRINSICS AND HAVE_SIMD_CPUID)
    target_compile_definitions(${TARGET_PREFIX_STR} PRIVATE -DUSE_SIMD_ENCODING)
    target_sources(${TARGET_PREFIX_STR} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/arch/cpuid.c")
    simd_add_source_avx2(${TARGET_PREFIX_STR} "${CMAKE_CURRENT_SOURCE_DIR}/source/arch/encoding_avx2.c")
    message(STATUS "Building SIMD base64 decoder")
endif()

# Preserve subdirectories when installing headers
foreach(HEADER_SRCPATH IN ITEMS ${AWS_COMMON_HEADERS} ${AWS_COMMON_OS_HEADERS} ${AWS_TEST_HEADERS})
    get_filename_component(HEADER_DIR ${HEADER_SRCPATH} DIRECTORY)
# Note: We need to replace the generated include directory component first, otherwise if the build
# directory is located inside the source tree, we'll partially rewrite the path and fail to replace it
# when we replace the generated include dir.
# We also need to take care to not run the source-directory match if the generated-directory match
# succeeds; otherwise, if we're installing to /foo/aws-c-common-install, and our source directory is
# /foo/aws-c-common, we'll end up installing to /foo/aws-c-common-install-install

    unset(HEADER_DSTDIR)

    foreach(POTENTIAL_PREFIX IN ITEMS ${GENERATED_ROOT_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
        string(LENGTH ${POTENTIAL_PREFIX} _prefixlen)
        string(SUBSTRING ${HEADER_DIR} 0 ${_prefixlen} _actual_prefix)
        if(${_actual_prefix} STREQUAL ${POTENTIAL_PREFIX})
            string(REPLACE "${POTENTIAL_PREFIX}/" "" HEADER_DSTDIR "${HEADER_DIR}")
            break()
        endif()
    endforeach()

    if(NOT HEADER_DSTDIR)
        message(ERROR "Couldn't find source root for header ${HEADER_SRCPATH}")
    endif()

    install(FILES ${HEADER_SRCPATH}
            DESTINATION ${HEADER_DSTDIR}
            COMPONENT Development)
endforeach()

aws_prepare_shared_lib_exports(${TARGET_PREFIX_STR})

configure_file("cmake/${TARGET_PREFIX_STR}-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_PREFIX_STR}-config.cmake"
  @ONLY)

install(EXPORT "${TARGET_PREFIX_STR}-targets"
    DESTINATION "${LIBRARY_DIRECTORY}/${TARGET_PREFIX_STR}/cmake"
    NAMESPACE AWS::
    COMPONENT Development)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_PREFIX_STR}-config.cmake"
    DESTINATION "${LIBRARY_DIRECTORY}/${TARGET_PREFIX_STR}/cmake"
    COMPONENT Development)

list(APPEND EXPORT_MODULES
    "cmake/AwsCFlags.cmake"
    "cmake/AwsSharedLibSetup.cmake"
    "cmake/AwsTestHarness.cmake"
    "cmake/AwsLibFuzzer.cmake"
    "cmake/AwsSanitizers.cmake"
    "cmake/AwsSIMD.cmake"
    )

install(FILES ${EXPORT_MODULES}
        DESTINATION "${LIBRARY_DIRECTORY}/cmake"
        COMPONENT Development)

if (NOT CMAKE_CROSSCOMPILING)
    include(CTest)
    add_subdirectory(tests)
endif()

# This should come last, to ensure all variables defined by cmake will be available for export
configure_file(${CONFIG_HEADER_TEMPLATE}
               ${GENERATED_CONFIG_HEADER}
               ESCAPE_QUOTES)

include(CPackConfig)
