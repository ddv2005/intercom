#ifndef UNI_THREAD_H_
#define UNI_THREAD_H_

#include "uni_common.h"
#include "uni_lock.h"
#include "uni_mutex.h"
#include "uni_event.h"

#ifdef __POSIX
#include <sys/types.h>
#ifndef ESP32
#include <sys/syscall.h>
#else
#ifndef _WIN32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#ifndef PTHREAD_STACK_MIN
#define	PTHREAD_STACK_MIN	4192
#endif
#endif
#endif
#include <unistd.h>
#endif

#ifdef __WINDOWS
#include <process.h>
#endif

enum uni_thread_states
{
	utsRunning = 1, utsStopped = 2
};

#ifdef __WINDOWS
typedef DWORD uni_thread_id;
#else
#ifdef __POSIX
typedef pid_t uni_thread_id;
#endif
#endif

class uni_thread_os
{
public:
	static void sleep(uint32_t timeout);
	static uint64_t getTickCount();
	static uni_thread_id getCurrentThreadID()
	{
#ifdef __WINDOWS
		return GetCurrentThreadId();
#else
#ifdef __POSIX
#ifdef ESP32
		return (uni_thread_id) xTaskGetCurrentTaskHandle();
#else
		return syscall(SYS_gettid);
#endif
#endif
#endif
	}
};

class uni_thread_base:public uni_thread_os
{
protected:
	uni_simple_mutex m_lock;
	uint32_t m_stackSize;
	uni_thread_states m_state;
	bool m_deleteOnClose;
	bool m_onDestroyExecuted;
	void onThreadExit();
	virtual void onStop(){};
	virtual void onStart(){};
	virtual int internalStart()=0;
	virtual int internalStop()
	{
		onStop();
		return URC_OK;
	}
	virtual void internalWaitForTermination()=0;
	virtual void internalOnThreadExit()=0;
	virtual uni_thread_id getThreadID()=0;
	void onDestroy()
	{
		if(!m_onDestroyExecuted)
		{
			m_onDestroyExecuted = true;
			stop();
			waitForTermination();
		}
	}
public:
	uni_thread_base(uint32_t stackSize = 0) :
			m_stackSize(stackSize), m_state(utsStopped),m_deleteOnClose(false),m_onDestroyExecuted(false)
	{
	}
	virtual ~uni_thread_base()
	{
	}
	int start();
	int stop();
	void waitForTermination();
	virtual void run()=0;
	void setDeleteOnClose(bool value) { m_deleteOnClose = value; }

	bool isTerminated()
	{
		uni_locker lock(m_lock);
		return m_state==utsStopped;
	}

	bool isRunning()
	{
		uni_locker lock(m_lock);
		return m_state==utsRunning;
	}
};

class uni_thread_impl: public uni_thread_base
{
protected:
	uni_thread_id m_threadID;
#ifdef __POSIX
	pthread_t m_thread;

	static void *run_s(void *data)
	{
		if (data)
		{
			uni_thread_impl *th = (uni_thread_impl *) data;
			th->m_threadID = getCurrentThreadID();
			try
			{
				th->run();
			} catch (...)
			{
			}
			bool deleteOnClose = th->m_deleteOnClose;
			th->onThreadExit();
			if(deleteOnClose)
				delete th;

		}

		return NULL;
	}
#endif
#ifdef __WINDOWS
	HANDLE m_thread;
	static DWORD WINAPI run_s(void *data)
	{
		if (data)
		{
			uni_thread_impl *th = (uni_thread_impl *) data;
			th->m_threadID = getCurrentThreadID();
			try
			{
				th->run();
			}
			catch (...)
			{
			}
			bool deleteOnClose = th->m_deleteOnClose;
			th->onThreadExit();
			if(deleteOnClose)
				delete th;
		}

		_endthreadex(0);
		return (DWORD)NULL;
	}
#endif
	virtual void internalOnThreadExit();
	virtual void onStop()
	{
	}
	virtual void onStart()
	{
	}
	virtual int internalStart();
	virtual void internalWaitForTermination();
public:
	uni_thread_impl(uint32_t stackSize = 0);
	virtual ~uni_thread_impl(){}
	virtual uni_thread_id getThreadID() { return m_threadID; }
};


class uni_thread: public uni_thread_impl
{
public:
	uni_thread(uint32_t stackSize = 0):uni_thread_impl(stackSize){}
	virtual ~uni_thread()
	{
		onDestroy();
	}
};

// nd - NO DESTROY
template<class T>
class uni_thread_with_signal_template_nd: public T
{
protected:
	uni_event m_signal;
	bool m_exit;

	virtual void onStart()
	{
		m_exit = false;
	}

	virtual void onStop()
	{
		m_exit = true;
		m_signal.signal();
	}

public:

	uni_thread_with_signal_template_nd(uint32_t stackSize = 0) :
			T(stackSize)
	{
		m_exit = false;
	}

	virtual ~uni_thread_with_signal_template_nd(){}

	uni_event &getSignal()
	{
		return m_signal;
	}

	bool isStopped()
	{
		return m_exit;
	}
};

template<class T>
class uni_thread_with_signal_template: public uni_thread_with_signal_template_nd<T>
{
public:
	uni_thread_with_signal_template(uint32_t stackSize = 0):uni_thread_with_signal_template_nd<T>(stackSize){}
	virtual ~uni_thread_with_signal_template()
	{
		uni_thread_with_signal_template_nd<T>::onDestroy();
	}
};

typedef uni_thread_with_signal_template<uni_thread_impl> uni_thread_with_signal;
typedef uni_thread_with_signal_template_nd<uni_thread_impl> uni_thread_with_signal_nd;

template<typename T, class T1>
class uni_thread_function_template: public uni_thread_with_signal_template_nd<T1>
{
public:
	typedef void (T::*thread_func_t)(uni_thread_function_template<T, T1> *thread);
protected:
	thread_func_t m_func;
	T &m_owner;
public:

	uni_thread_function_template(T &owner, thread_func_t func,
			uint32_t stackSize = 0) :
			uni_thread_with_signal_template_nd<T1>(stackSize), m_func(func), m_owner(
					owner)
	{
	}

	virtual ~uni_thread_function_template()
	{
		uni_thread_with_signal_template_nd<T1>::onDestroy();
	}

	virtual void run()
	{
		(m_owner.*m_func)(this);
	}
};


template<typename T>
class uni_thread_function: public uni_thread_with_signal_template_nd<uni_thread_impl>
{
public:
	typedef void (T::*thread_func_t)(uni_thread_function<T> *thread);
protected:
	thread_func_t m_func;
	T &m_owner;
public:

	uni_thread_function(T &owner, thread_func_t func, uint32_t stackSize = 0) :
			uni_thread_with_signal_template_nd<uni_thread_impl>(stackSize), m_func(func), m_owner(
					owner)
	{
	}

	virtual ~uni_thread_function()
	{
		onDestroy();
	}

	virtual void run()
	{
		(m_owner.*m_func)(this);
	}
};
#endif /* UNI_THREAD_H_ */
