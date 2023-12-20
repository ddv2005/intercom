#ifndef UNI_LOG_H_
#define UNI_LOG_H_

#define DEBUG_LEVEL	4
#define INFO_LEVEL	3
#define WARN_LEVEL	2
#define ERROR_LEVEL	1

#include <stdlib.h>

#ifdef __DEBUG
#define UASSIGN(b) if(!b){ULOG(1,"ASSIGN ERROR");abort();}
#else
#define UASSIGN(b) if(!b){ULOG(1,"ASSIGN ERROR");}
#endif

class uni_logger
{
public:
	typedef void (*uni_log_func_t)(const char * text);
	static int uni_log_add_func(uni_log_func_t func);
	static int uni_log_remove_func(uni_log_func_t func);
};
void uni_set_log_thread(bool value);
void uni_set_log_level(int level);
int  uni_get_log_level();
void uni_clear_logger();
#ifdef UNI_PJLOG
#include <pjlib.h>
#define ULOG(level,format,...) PJ_LOG(level,(__FILE__,format, ##__VA_ARGS__))
#else // UNI_PJLOG
void uni_log(int level,const char * file,int line, const char * function,const char *format,...);
#define ULOG(level,format,...) uni_log(level,__FILE__,__LINE__,__FUNCTION__, format, ##__VA_ARGS__)
#endif // UNI_PJLOG

#endif /* UNI_LOG_H_ */
