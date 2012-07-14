#ifndef COMMON_H_
#define COMMON_H_


#include <pjlib.h>
#include <pjlib-util.h>
#include <pj_lock.h>

#include <stdlib.h>
#include <stdio.h>

using namespace std;
#define SWIG_BEGIN_DECL
#define SWIG_END_DECL

#ifdef _WIN32
#define __WINDOWS
#define __MSVC
#undef	__LINUX
#undef	__GCC
#else
#undef	__WINDOWS
#undef	__MSVC
#define	__LINUX
#define __GCC
#endif

#define DEBUG_LEVEL	4
#define INFO_LEVEL	3
#define WARN_LEVEL	2
#define ERROR_LEVEL	1

#define PJ_LOG_(level,args)	PJ_LOG(level,args)

#endif /* COMMON_H_ */
