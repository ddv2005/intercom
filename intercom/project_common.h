#ifndef PROJECT_COMMON_H_
#define PROJECT_COMMON_H_

#include <pjlib.h>
#include <pjlib-util.h>
#include <pj_lock.h>

#include <stdlib.h>
#include <stdio.h>

#define CREATE_CLASS_PROP_FUNCTIONS(type,name) \
		type get_##name() { return m_##name; }\
		void set_##name(type value) { m_##name = value; }

using namespace std;

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

typedef bool pjs_exit_t;

#define ENUM_LIST(l,i) for( i=l.begin(); i != l.end(); i++)
typedef pj_int32_t	pjs_result_t;

#define RC_OK		0
#define RC_ERROR	-1

#define DEBUG_LEVEL	4
#define INFO_LEVEL	3
#define WARN_LEVEL	2
#define ERROR_LEVEL	1

#define PJ_LOG_(level,args)	PJ_LOG(level,args)

#define SWIG_END_DECL
#define SWIG_BEGIN_DECL

#endif /* PROJECT_COMMON_H_ */
