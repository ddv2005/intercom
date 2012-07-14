#ifndef	__PJ_SUPPORT_H
#define	__PJ_SUPPORT_H

#include <pjlib.h>
#if PJ_ANDROID==1
extern "C" pj_status_t set_android_thread_priority(int priority);
#endif
extern "C" pj_status_t set_max_relative_thread_priority(int relative);


#ifndef	_DEBUG
#define	PJS_ASSERT(expr)	\
	{\
		if(!expr)\
		{\
			pj_log_1("PJ","Assertion failed: %s",(const char *)#expr);\
		}\
	}
#else
#define	PJS_ASSERT(expr)	\
	{\
		if(!expr)\
		{\
			pj_log_1("PJ","Assertion failed: %s, file %s, line: %d",(const char *)#expr,(const char *)__FILE__, (int) __LINE__);\
		}\
	}
#endif

#endif	
