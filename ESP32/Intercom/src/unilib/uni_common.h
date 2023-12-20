#ifndef UNI_COMMON_H_
#define UNI_COMMON_H_

#ifdef _WIN32
#define __WINDOWS
#define __MSVC
#undef	__LINUX
#undef	__GCC
#undef  __POSIX
#else
#undef	__WINDOWS
#undef	__MSVC
#define	__LINUX
#define __GCC
#define __POSIX
#endif

#ifdef __WINDOWS
#include <winsock2.h>
#include <windows.h>
#ifdef _MSC_VER
#if _MSC_VER <= 1500
#include "ms_stdint.h"
#else //VER
#include <stdint.h>
#endif //VER
#if _MSC_VER < 1700
#define ETIMEDOUT       60
#endif
#else
#include <stdint.h>
#endif
#else //__WINDOWS
#include <stdint.h>
#endif //__WINDOWS

#define URC_OK	0
#define URC_ERROR -1
#define URC_BAD_STATE -2
typedef int32_t uni_result_t;
#endif /* UNI_COMMON_H_ */
