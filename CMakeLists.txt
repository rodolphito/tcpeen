cmake_minimum_required(VERSION 3.0.2)
# TODO: aws cmake files are not actually 3.0.2 compat
# fix version or fix use of target_sources for them?
project(tcpeen LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps/aws-c-common/cmake ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
include(AwsTestHarness)
include(tnGeneral)
include(tnTesting)
enable_testing()

set(TN_TARGET_LINK_LIBS "")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	tn_list_add(TN_TARGET_COMPILE_DEFS -DTN_CMAKE_BUILD_DEBUG)
endif()

if(NOT WIN32)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -std=c99")
	if(NOT APPLE)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
	endif()
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
		tn_list_add(TN_TARGET_COMPILE_DEFS -D_CRT_SECURE_NO_WARNINGS)
	endif()
endif()


#
# argparse dependency
include_directories(${CMAKE_SOURCE_DIR}/deps/argparse)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/argparse)
list(APPEND TN_TARGET_LINK_LIBS argparse)


#
# libuv dependency
set(BUILD_TESTING OFF)
include_directories(${CMAKE_SOURCE_DIR}/deps/libuv/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/libuv)
list(APPEND TN_TARGET_LINK_LIBS uv_a)


#
# aws-c-common dependency
include_directories(${CMAKE_SOURCE_DIR}/deps/aws-c-common/include)
include_directories(${CMAKE_BINARY_DIR}/deps/aws-c-common/generated/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/aws-c-common)
list(APPEND TN_TARGET_LINK_LIBS aws-c-common)


#
# libpcap/winpcap dependency
include_directories(${CMAKE_SOURCE_DIR}/deps/libpcap)
if(WIN32)
	list(APPEND TN_TARGET_LINK_LIBS ${CMAKE_SOURCE_DIR}/deps/winpcap/lib/x64/wpcap.lib)
else()
	set(DISABLE_DBUS TRUE)
	set(BUILD_SHARED_LIBS FALSE)
	add_subdirectory(${CMAKE_SOURCE_DIR}/deps/libpcap)
	list(APPEND TN_TARGET_LINK_LIBS pcap_static)
endif()


#
# build tcpeen library
include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
set(TCP_LIBRARY_SRCS
	src/log.c
	include/tn/log.h
	src/fslog.c
	include/tn/fslog.h
	src/system.c
	include/tn/system.h
	src/endpoint.c
	include/tn/endpoint.h
	src/uuid.c
	include/tn/uuid.h
	src/time.c
	include/tn/time.h
	src/list_ptr.c
	include/tn/list_ptr.h
	src/list_block.c
	include/tn/list_block.h
	src/map.c
	include/tn/map.h
	src/queue_spsc.c
	include/tn/queue_spsc.h
	src/allocator.c
	include/tn/allocator.h
	src/thread.c
	include/tn/thread.h
	src/mutex.c
	include/tn/mutex.h
	src/atomic.c
	include/tn/atomic.h
	src/buffer.c
	include/tn/buffer.h
	src/buffer_pool.c
	include/tn/buffer_pool.h
	src/event.c
	include/tn/event.h
	src/tcp_connection.c
	include/tn/tcp_connection.h
	src/tcp_context.c
	include/tn/tcp_context.h
	src/tcp_service.c
	include/tn/tcp_service.h
	src/tcp_service_internal.c
	src/tcp_service_internal.h
	src/tcp_channel.c
	include/tn/tcp_channel.h
	include/tn/error.h
	include/tn/test_harness.h
	include/tn/config.h
)
add_library(tcpeen ${TCP_LIBRARY_SRCS})
target_link_libraries(tcpeen ${TN_TARGET_LINK_LIBS})
target_compile_definitions(tcpeen PRIVATE ${TN_TARGET_COMPILE_DEFS})
list(APPEND TN_TARGET_LINK_LIBS tcpeen)


#
# build test executables
# TODO: make a cache variable to turn tests on/off
set(TN_TESTS_C
	"tests/test_buffer.c"
	"tests/test_endpoint.c"
	"tests/test_event.c"
	"tests/test_list_block.c"
	"tests/test_list_ptr.c"
	"tests/test_tcp_service.c"
	"tests/test_queue_spsc.c"
)
set(TESTS ${TN_TESTS_C})

add_test_case(test_events)
add_test_case(test_tcp_service)
add_test_case(test_buffer_pool)
add_test_case(test_endpoints)
add_test_case(test_list_block_create)
add_test_case(test_list_block_ops)
add_test_case(test_list_ptr_create)
add_test_case(test_list_ptr_ops)
add_test_case(test_queue_spsc_stress)
add_test_case(test_queue_spsc_empty)
add_test_case(test_queue_spsc_full)
add_test_case(test_queue_spsc_npot)

generate_test_driver(tcpeen-tests)
target_link_libraries(tcpeen-tests ${TN_TARGET_LINK_LIBS})
target_compile_definitions(tcpeen-tests PRIVATE ${TN_TARGET_COMPILE_DEFS})

# runner for single test for easy debug step through an impl issue
#tn_test_single(test_event test_events "${TN_TARGET_LINK_LIBS}" "${TN_TARGET_COMPILE_DEFS}")

#
# build tcploadgen executable
set(TCP_LOADGEN_SRCS
	src/tcplg/tcplg.c
	include/tcplg/cmdargs.h
)
add_executable(tcplg ${TCP_LOADGEN_SRCS})
target_link_libraries(tcplg ${TN_TARGET_LINK_LIBS})
target_compile_definitions(tcplg PRIVATE ${TN_TARGET_COMPILE_DEFS})


#
# build tcpserver executable
set(TCP_SERVER_SRCS
	src/tcpserv/tcpserv.c
	include/tcpserv/tcpserv.h
)
add_executable(tcpserv ${TCP_SERVER_SRCS})
target_link_libraries(tcpserv ${TN_TARGET_LINK_LIBS})
target_compile_definitions(tcpserv PRIVATE ${TN_TARGET_COMPILE_DEFS})


#
# build tcpserver unity executable
set(TCP_SERVER_SRCS
	src/tcpserv/tcpserv_unity.c
)
add_executable(tcpserv_unity ${TCP_SERVER_SRCS})
target_link_libraries(tcpserv_unity ${TN_TARGET_LINK_LIBS})
target_compile_definitions(tcpserv_unity PRIVATE ${TN_TARGET_COMPILE_DEFS})


#
# build sniff executable
set(SNIFF_SRCS
	src/sniff/sniff.c
)
add_executable(sniff ${SNIFF_SRCS})
target_link_libraries(sniff ${TN_TARGET_LINK_LIBS})
target_compile_definitions(sniff PRIVATE ${TN_TARGET_COMPILE_DEFS})


#
# build unity plugin
include_directories(${CMAKE_SOURCE_DIR}/deps/unity)
set(TCP_PLUGIN_SRCS
	src/unity/tcpeen_unity.c
)
add_library(tcpeen_unity MODULE ${TCP_PLUGIN_SRCS})
target_link_libraries(tcpeen_unity ${TN_TARGET_LINK_LIBS})
target_compile_definitions(tcpeen_unity PRIVATE ${TN_TARGET_COMPILE_DEFS})

if(NOT APPLE)
	add_custom_command(
		TARGET tcpeen_unity
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			$<TARGET_FILE:tcpeen_unity>
			${CMAKE_SOURCE_DIR}/UnityProject/Assets/TCPeen/Plugins/x86_64/$<TARGET_FILE_NAME:tcpeen_unity>
	)
else()
	set_target_properties(tcpeen_unity PROPERTIES BUNDLE TRUE)
	add_custom_command(
		TARGET tcpeen_unity
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			$<TARGET_FILE:tcpeen_unity>
			${CMAKE_SOURCE_DIR}/UnityProject/Assets/TCPeen/Plugins/x86_64/$<TARGET_FILE_NAME:tcpeen_unity>.bundle
	)
endif()