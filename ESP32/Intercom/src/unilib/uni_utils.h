#ifndef UNI_UTILS_H_
#define UNI_UTILS_H_

#include "uni_common.h"
#include "uni_mutex.h"
#include "uni_lock.h"
#include <list>

typedef enum {
	isUninitialized = 0,
	isInitialized = 1,
	isError = 2
}uni_init_state_t;

class uni_init_state_class
{
protected:
	uni_init_state_t	m_state;
	virtual uni_result_t internal_init() = 0;
	virtual uni_result_t internal_deinit() = 0;

	void setInitState(bool b)
	{
		if(b)
			m_state = ::isInitialized;
		else
			m_state = ::isUninitialized;
	}

	void setErrorState()
	{
		m_state = ::isError;
	}

public:
	virtual ~uni_init_state_class(){}
	uni_init_state_class():m_state(::isUninitialized)
	{
	}

	bool isUninitialized()
	{
		return m_state == ::isUninitialized;
	}

	bool isInitialized()
	{
		return m_state == ::isInitialized;
	}

	virtual uni_result_t init()
	{
		if(!isUninitialized())
			return URC_OK;
		uni_result_t result = internal_init();
		if(result == URC_OK)
			setInitState(true);
		else
			setErrorState();
		return result;
	}

	virtual uni_result_t deinit()
	{
		if(isUninitialized())
			return URC_OK;

		uni_result_t result = internal_deinit();
		setInitState(false);
		return result;
	}
};

class uni_threadsafe_init_state_class:public uni_init_state_class
{
protected:
	uni_simple_mutex m_state_lock;
public:
	virtual uni_result_t init()
	{
		uni_locker lock(m_state_lock);
		return uni_init_state_class::init();
	}

	virtual uni_result_t deinit()
	{
		uni_locker lock(m_state_lock);
		return uni_init_state_class::deinit();
	}
};

template<typename T>
class uni_callback_class
{
protected:
	typedef std::list<T*>	callbacks_list_t;
	typedef typename std::list<T*>::iterator	callbacks_list_iterator_t;

	callbacks_list_t m_callbacks;
	uni_simple_mutex m_callback_lock;

	virtual void on_callback_fire(void * user_data)
	{
	}

	virtual void on_callback(T* cbt, void * user_data)=0;
public:
	virtual ~uni_callback_class(){}
	void add_callback(T *cbt)
	{
		uni_locker lock(m_callback_lock);
		m_callbacks.push_back(cbt);
	}

	void remove_callback(T *cbt)
	{
		uni_locker lock(m_callback_lock);
		m_callbacks.remove(cbt);
	}

	void fire_callbacks(void * user_data)
	{
		uni_locker lock(m_callback_lock);
		on_callback_fire(user_data);
		callbacks_list_iterator_t clitr = m_callbacks.begin();
		for(; clitr != m_callbacks.end(); clitr++)
			on_callback((*clitr),user_data);
	}
};

#endif
