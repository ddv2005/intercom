#ifndef PJS_EMULATOR_CONTROLLER_H_
#define PJS_EMULATOR_CONTROLLER_H_
#include "pjs_external_controller.h"
#include "pjs_utils.h"
#include "pjs_intercom_messages.h"


class	pjs_emulator_controller: public pjs_external_controller
{
protected:
	pjs_event	   		   m_event;
	bool				   m_exit;
	pj_thread_t 		  *m_thread;
	PPJ_MutexLock		  *m_lock;
	pj_pool_t			  *m_pool;

	static void* s_thread_proc(pjs_emulator_controller *object)
	{
		if(object)
			return object->thread_proc();
		else
			return NULL;
	}

	void* thread_proc();
public:
	pjs_emulator_controller(pjs_global &global,pjs_external_controller_callback &callback);
	virtual ~pjs_emulator_controller();
	virtual pj_int32_t set_output(pj_uint8_t  output, pj_uint8_t value);
	virtual void update_inputs(){};
};

#endif /* PJS_EMULATOR_CONTROLLER_H_ */
