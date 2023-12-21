#include "uni_log.h"

#ifdef NO_UNI_LOG
#include "uni_common.h"
void uni_set_log_thread(bool value)
{
}

void uni_set_log_level(int level)
{
}

int  uni_get_log_level()
{
	return 0;
}

void uni_log(int level,const char * file,int line, const char * function,const char *format,...)
{
}
int uni_logger::uni_log_add_func(uni_log_func_t func){ return URC_OK;}
int uni_logger::uni_log_remove_func(uni_log_func_t func){return URC_OK;}
#else
#include <stdio.h>
#include <stdarg.h>
#include "uni_lock.h"
#include "uni_mutex.h"
#include <time.h>
#include "uni_thread.h"
#include <list>
#include <string.h>

#define MAX_LOG_LINE	512

class uni_logger_internal:public uni_logger
{
#ifndef ESP32	
private:
	typedef uni_thread_function<uni_logger_internal> uni_log_thread_c;
	void thread_func(uni_log_thread_c *thread);
#endif	
public:
	typedef std::list<uni_log_func_t> uni_log_funcions_list_t;
	typedef uni_log_funcions_list_t::iterator uni_log_functions_list_itr_t;
	uni_log_funcions_list_t log_functions_list;
	uni_simple_mutex log_lock;
	char log_buffer[MAX_LOG_LINE];
	int log_level;
#ifndef ESP32	
	uni_log_thread_c *thread;
	uni_simple_mutex log_thread_lock;
	uni_event log_thread_event;
	std::list<char *> log_thread_list;
	std::list<char *> log_thread_free;
	bool use_thread;
#endif	

	uni_logger_internal()
	{
		log_level = 1;
#ifndef ESP32
		use_thread = true;		
		thread = new uni_log_thread_c(*this,&uni_logger_internal::thread_func);
		thread->start();
#endif		
	}

	~uni_logger_internal()
	{
#ifndef ESP32		
		thread->stop();
		thread->waitForTermination();
		delete thread;
		while(log_thread_free.size())
		{
			char *data =log_thread_free.front();
			log_thread_free.pop_front();
			free(data);
		}
		while(log_thread_list.size())
		{
			char *data =log_thread_list.front();
			log_thread_list.pop_front();
			free(data);
		}
#endif		
	}
#ifndef ESP32
	void push(const char*str)
	{
		log_thread_lock.lock();
		char *data = NULL;
		if(log_thread_free.size())
		{
			data = log_thread_free.front();
			log_thread_free.pop_front();
		}
		if(!data)
			data = (char*)malloc(MAX_LOG_LINE);
		strncpy(data,str,MAX_LOG_LINE);
		log_thread_list.push_back(data);
		log_thread_event.signal();
		log_thread_lock.unlock();
	}
#endif	
	static void check_logger();
	static void clear_logger();
};

static uni_logger_internal *logger=NULL;

#ifndef ESP32
void uni_logger_internal::thread_func(uni_log_thread_c *thread)
{
#ifdef __POSIX
    int policy = SCHED_OTHER;
    struct sched_param param;

    param.sched_priority = -1;
    pthread_setschedparam(pthread_self(), policy, &param);
#else
#ifdef __WINDOWS
    SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
#endif
	while (!thread->isStopped())
	{
		log_thread_lock.lock();
		if(log_thread_list.size())
		{
			while(log_thread_list.size())
			{
				char *data = log_thread_list.front();
				log_thread_list.pop_front();
				log_thread_lock.unlock();
				for(uni_logger_internal::uni_log_functions_list_itr_t itr=logger->log_functions_list.begin();itr!=logger->log_functions_list.end();itr++)
					(*itr)(data);
				log_thread_lock.lock();
				if(log_thread_free.size()<=100)
					log_thread_free.push_back(data);
				else
					free(data);
			}
			log_thread_lock.unlock();
		}
		else
		{
			log_thread_lock.unlock();
			log_thread_event.wait(100);
		}
	}
}
#endif
void uni_logger_internal::check_logger()
{
	if(!logger)
		logger = new uni_logger_internal();
}

void uni_logger_internal::clear_logger()
{
	if(logger)
	{
		delete logger;
		logger = NULL;
	}
}

void uni_clear_logger()
{
	uni_logger_internal::clear_logger();
}

void uni_set_log_level(int level)
{
	uni_logger_internal::check_logger();
	logger->log_level = level;
}
#ifndef ESP32
void uni_set_log_thread(bool value)
{
	uni_logger_internal::check_logger();
	logger->use_thread = value;
}
#endif
int  uni_get_log_level()
{
	uni_logger_internal::check_logger();
	return logger->log_level;
}

int uni_logger::uni_log_add_func(uni_log_func_t func)
{
	uni_logger_internal::check_logger();
	uni_locker lock(logger->log_lock);
	logger->log_functions_list.push_back(func);
	return URC_OK;
}

int uni_logger::uni_log_remove_func(uni_log_func_t func)
{
	uni_logger_internal::check_logger();
	uni_locker lock(logger->log_lock);
	logger->log_functions_list.remove(func);
	return URC_OK;
}


#ifdef	__WINDOWS	
#define vsnprintf_(a,b,c,d) vsnprintf_s(a,b,b,c,d)
#define snprintf_  _snprintf
#pragma warning(disable : 4996)
#else
#define vsnprintf_ vsnprintf
#define snprintf_  snprintf
#endif

void uni_log(int level,const char * file,int line, const char * function,const char *format,...)
{
	uni_logger_internal::check_logger();
	if(level<=logger->log_level)
	{
#ifdef __POSIX
		struct tm * timeinfo;
		timeval now;
		gettimeofday(&now, NULL);
		timeinfo = localtime(&now.tv_sec);
#else
#ifdef __WINDOWS
		SYSTEMTIME now;
		GetLocalTime(&now);
#endif
#endif

		va_list ap;
		va_start (ap, format);
		uni_locker lock(logger->log_lock);
#ifdef __POSIX
		size_t size = strftime(logger->log_buffer,sizeof(logger->log_buffer),"%H:%M:%S",timeinfo);
		size+=snprintf_(&logger->log_buffer[size],sizeof(logger->log_buffer)-size,".%03d",(int)now.tv_usec / 1000);
#else
#ifdef __WINDOWS

		size_t size = _snprintf_s(logger->log_buffer,sizeof(logger->log_buffer),"%02d:%02d:%02d.%03d",now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
#endif
#endif
		size+=snprintf_(&logger->log_buffer[size],sizeof(logger->log_buffer)-size," [%d]\t[%d] ",(unsigned int)uni_thread::getCurrentThreadID(),level);
		vsnprintf_(&logger->log_buffer[size],sizeof(logger->log_buffer)-size,format,ap);
		va_end (ap);
#ifndef ESP32
		if(logger->use_thread)
			logger->push(logger->log_buffer);
		else
#endif		
			for(uni_logger_internal::uni_log_functions_list_itr_t itr=logger->log_functions_list.begin();itr!=logger->log_functions_list.end();itr++)
				(*itr)(logger->log_buffer);
	}
}
#endif


