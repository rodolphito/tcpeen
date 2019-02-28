cmake_minimum_required(VERSION 3.02)
project(tcpeen LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps/aws-c-common/cmake ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
include(AwsTestHarness)
include(hbGeneral)
include(hbTesting)
enable_testing()

set(HB_TARGET_LINK_LIBS "")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	hb_list_add(HB_TARGET_COMPILE_DEFS -DHB_CMAKE_BUILD_DEBUG)
endif()

if(NOT WIN32)
	set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fPIC")
	set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pie")
else()
	set(CompilerFlags
		CMAKE_CXX_FLAGS
		CMAKE_CXX_FLAGS_DEBUG
		CMAKE_CXX_FLAGS_RELEASE
		CMAKE_C_FLAGS
		CMAKE_C_FLAGS_DEBUG
		CMAKE_C_FLAGS_RELEASE
	)
	foreach(CompilerFlag ${CompilerFlags})
		string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()
	
	if(MSVC)
		add_custom_target(CMake SOURCES CMakeLists.txt)
		hb_list_add(HB_TARGET_COMPILE_DEFS -D_CRT_SECURE_NO_WARNINGS)
	endif()
endif()


#
# argparse dependency
include_directories(${CMAKE_SOURCE_DIR}/deps/argparse)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/argparse)
list(APPEND HB_TARGET_LINK_LIBS argparse)


#
# libuv dependency
set(BUILD_TESTING OFF)
include_directories(${CMAKE_SOURCE_DIR}/deps/libuv/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/libuv)
list(APPEND HB_TARGET_LINK_LIBS uv_a)


#
# aws-c-common dependency
include_directories(${CMAKE_SOURCE_DIR}/deps/aws-c-common/include)
include_directories(${CMAKE_BINARY_DIR}/deps/aws-c-common/generated/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/aws-c-common)
list(APPEND HB_TARGET_LINK_LIBS aws-c-common)


#
# build tcpeen library
include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
set(TCP_LIBRARY_SRCS
	src/log.c
	include/hb/log.h
	src/endpoint.c
	include/hb/endpoint.h
	src/uuid.c
	include/hb/uuid.h
	src/list_aws.c
	src/list.c
	include/hb/list.h
	src/allocator.c
	include/hb/allocator.h
	src/thread.c
	include/hb/thread.h
	src/buffer.c
	include/hb/buffer.h
	src/event.c
	include/hb/event.h
	src/tcp_connection.c
	include/hb/tcp_connection.h
	src/tcp_context.c
	include/hb/tcp_context.h
	src/tcp_service.c
	include/hb/tcp_service.h
	src/tcp_service_internal.c
	src/tcp_service_internal.h
	src/tcp_channel.c
	include/hb/tcp_channel.h
	include/hb/error.h
	include/hb/test_harness.h
	include/hb/config.h
)
add_library(tcpeen ${TCP_LIBRARY_SRCS})
target_link_libraries(tcpeen ${HB_TARGET_LINK_LIBS})
target_compile_definitions(tcpeen PRIVATE ${HB_TARGET_COMPILE_DEFS})
list(APPEND HB_TARGET_LINK_LIBS tcpeen)


#
# build test executables
# TODO: make a cache variable to turn tests on/off
file(GLOB TESTS_C "tests/*.c")
file(GLOB TESTS_H "tests/*.h")
file(GLOB TESTS ${TESTS_H} ${TESTS_C})

add_test_case(test_events)
add_test_case(test_tcp_service)
add_test_case(test_buffer_pool)
add_test_case(test_endpoints)
add_test_case(test_event_list_create)
add_test_case(test_event_list_ops)

generate_test_driver(tcpeen-tests)
target_link_libraries(tcpeen-tests ${HB_TARGET_LINK_LIBS})
target_compile_definitions(tcpeen-tests PRIVATE ${HB_TARGET_COMPILE_DEFS})

# TODO: runner for single test for easy debug step through an impl issue
hb_test_single(test_list test_list_ops "${HB_TARGET_LINK_LIBS}" "${HB_TARGET_COMPILE_DEFS}")

#
# build tcploadgen executable
set(TCP_LOADGEN_SRCS
	src/tcplg/tcplg.c
	include/tcplg/cmdargs.h
)
add_executable(tcplg ${TCP_LOADGEN_SRCS})
target_link_libraries(tcplg ${HB_TARGET_LINK_LIBS})
target_compile_definitions(tcplg PRIVATE ${HB_TARGET_COMPILE_DEFS})


#
# build tcpserver executable
set(TCP_SERVER_SRCS
	src/tcpserv/tcpserv.c
	include/tcpserv/tcpserv.h
)
add_executable(tcpserv ${TCP_SERVER_SRCS})
target_link_libraries(tcpserv ${HB_TARGET_LINK_LIBS})
target_compile_definitions(tcpserv PRIVATE ${HB_TARGET_COMPILE_DEFS})


#
# build tcpserver unity executable
set(TCP_SERVER_SRCS
	src/tcpserv/tcpserv_unity.c
)
add_executable(tcpserv_unity ${TCP_SERVER_SRCS})
target_link_libraries(tcpserv_unity ${HB_TARGET_LINK_LIBS})
target_compile_definitions(tcpserv_unity PRIVATE ${HB_TARGET_COMPILE_DEFS})


#
# build unity plugin
include_directories(${CMAKE_SOURCE_DIR}/deps/unity)
set(TCP_PLUGIN_SRCS
	src/unity/tcp_unity.c
)
add_library(tcp_unity SHARED ${TCP_PLUGIN_SRCS})
target_link_libraries(tcp_unity ${HB_TARGET_LINK_LIBS})
target_compile_definitions(tcp_unity PRIVATE ${HB_TARGET_COMPILE_DEFS})
add_custom_command(
    TARGET tcp_unity 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:tcp_unity>
        ${CMAKE_SOURCE_DIR}/UnityProject/Assets/Plugins/x86_64/$<TARGET_FILE_NAME:tcp_unity>
)
