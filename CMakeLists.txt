cmake_minimum_required(VERSION 3.02)
project(tcpeen LANGUAGES C)
enable_testing()

set(HB_LINK_LIBS "")

#
# prep compile flags
if (NOT WIN32)
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
endif()


#
# argparse dependency
include_directories(${CMAKE_SOURCE_DIR}/deps/argparse)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/argparse)
list(APPEND HB_LINK_LIBS argparse)


#
# libuv dependency
set(BUILD_TESTING OFF)
include_directories(${CMAKE_SOURCE_DIR}/deps/libuv/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/libuv)
list(APPEND HB_LINK_LIBS uv_a)


#
# aws-c-common dependency
include_directories(${CMAKE_SOURCE_DIR}/deps/aws-c-common/include)
include_directories(${CMAKE_BINARY_DIR}/deps/aws-c-common/generated/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/aws-c-common)
list(APPEND HB_LINK_LIBS aws-c-common)


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
	include/hb/error.h
	include/hb/test_harness.h
)
add_library(tcpeen ${TCP_LIBRARY_SRCS})
target_link_libraries(tcpeen ${HB_LINK_LIBS})
list(APPEND HB_LINK_LIBS tcpeen)



#
# build test executables
# TODO: make a cache variable to turn tests on/off
# TODO: build foreach loop before this sections gets out of control
# TODO: output to a separate folder
# TODO: migrate to aws style single target / multiple cases
add_executable(test_endpoint tests/test_endpoint.c)
target_link_libraries(test_endpoint ${HB_LINK_LIBS})
add_test(NAME test_endpoint COMMAND $<TARGET_FILE_NAME:test_endpoint>)

add_executable(test_event tests/test_event.c)
target_link_libraries(test_event ${HB_LINK_LIBS})
add_test(NAME test_event COMMAND $<TARGET_FILE_NAME:test_event>)

add_executable(test_buffer tests/test_buffer.c)
target_link_libraries(test_buffer ${HB_LINK_LIBS})
add_test(NAME test_buffer COMMAND $<TARGET_FILE_NAME:test_buffer>)

add_executable(test_tcp_service tests/test_tcp_service.c)
target_link_libraries(test_tcp_service ${HB_LINK_LIBS})
add_test(NAME test_tcp_service COMMAND $<TARGET_FILE_NAME:test_tcp_service>)


#
# build tcploadgen executable
set(TCP_LOADGEN_SRCS
	src/tcplg/tcplg.c
	include/tcplg/cmdargs.h
)
add_executable(tcplg ${TCP_LOADGEN_SRCS})
target_link_libraries(tcplg ${HB_LINK_LIBS})


#
# build tcpserver executable
set(TCP_SERVER_SRCS
	src/tcpserv/tcpserv.c
	include/tcpserv/tcpserv.h
)
add_executable(tcpserv ${TCP_SERVER_SRCS})
target_link_libraries(tcpserv ${HB_LINK_LIBS})


#
# build unity plugin
include_directories(${CMAKE_SOURCE_DIR}/deps/unity)
set(TCP_PLUGIN_SRCS
	src/unity/tcp_unity.c
)
add_library(tcp_unity SHARED ${TCP_PLUGIN_SRCS})
target_link_libraries(tcp_unity ${HB_LINK_LIBS})
add_custom_command(
    TARGET tcp_unity 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:tcp_unity>
        ${CMAKE_SOURCE_DIR}/UnityProject/Assets/Plugins/x86_64/$<TARGET_FILE_NAME:tcp_unity>
)
