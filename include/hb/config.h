#ifndef HB_CONFIG_H
#define HB_CONFIG_H

#if _MSC_VER
#	ifdef _DEBUG
#		define HB_BUILD_DEBUG
#	endif
#elif defined(HB_CMAKE_BUILD_DEBUG)
#	define HB_BUILD_DEBUG
#endif

#endif