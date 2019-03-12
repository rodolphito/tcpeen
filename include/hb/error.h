#ifndef HB_ERROR_H
#define HB_ERROR_H

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

/* general return status */
#define HB_SUCCESS 0
#define HB_ERROR -1
#define HB_ERROR_AGAIN EAGAIN
#define HB_ERROR_NOMEM ENOMEM
#define HB_ERROR_INVAL EINVAL

/* queue return status */
#define HB_QUEUE_EMPTY -100
#define HB_QUEUE_FULL -101
#define HB_QUEUE_AGAIN -102

/* event return status */
#define HB_EVENT_ERROR -200
#define HB_EVENT_NOCHAN -201
#define HB_EVENT_NOSPAN -202
#define HB_EVENT_SPANREAD -203

/* send / IO write return status */
#define HB_SEND_ERROR -300
#define HB_SEND_NOREQ -301
#define HB_SEND_NOBUF -302
#define HB_SEND_PUSH -303

#define HB_RECV_ERROR -400
#define HB_RECV_EBUF -401
#define HB_RECV_EVTPOP -402
#define HB_RECV_EVTPUSH -403
#define HB_RECV_E2BIG -404

#define HB_ASSERT(expr) assert(expr)
#define HB_GUARD(expr) if ((expr)) return HB_ERROR
#define HB_GUARD_NULL(expr) if (!(expr)) return HB_ERROR
#define HB_GUARD_GOTO(lbl, expr) { if ((expr)) goto lbl; }
#define HB_GUARD_CLEANUP(expr) HB_GUARD_GOTO(cleanup, expr)
#define HB_GUARD_NULL_CLEANUP(expr) HB_GUARD_GOTO(cleanup, !(expr))

#ifdef _WIN32
#	define HB_PLATFORM_WINDOWS 1

#	ifdef _WIN64
#	else
#	endif
#elif __APPLE__
#	include <TargetConditionals.h>
#	if TARGET_IPHONE_SIMULATOR
#		define HB_PLATFORM_IOS 1
#	elif TARGET_OS_IPHONE
#		define HB_PLATFORM_IOS 1
#	elif TARGET_OS_MAC
#		define HB_PLATFORM_OSX 1
#	else
#   	error "Unknown Apple platform"
#	endif
#elif __ANDROID__
#	define HB_PLATFORM_ANDROID 1
#elif __linux__
#	define HB_PLATFORM_POSIX 1
#elif __unix__ // all unices not caught above
#	define HB_PLATFORM_POSIX 1
#elif defined(_POSIX_VERSION)
#	define HB_PLATFORM_POSIX 1
#else
#   error "Unknown compiler"
#endif

#endif