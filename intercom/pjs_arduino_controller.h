
#ifndef PJS_ARDUINO_CONTROLLER_H_
#define PJS_ARDUINO_CONTROLLER_H_
#include "pjs_external_controller.h"
#include "pjs_utils.h"
#include "pjs_intercom_messages.h"
#include "serial/comm_prot_parser.h"
#include "serial/serialib.h"


class	pjs_arduino_controller: public pjs_external_controller
{
protected:
	pjs_event	   		   m_event;
	bool				   m_exit;
	pj_thread_t 		  *m_thread;
	PPJ_MutexLock		  *m_lock;
	pj_pool_t			  *m_pool;
	pj_uint8_t			   m_input;
	pj_uint8_t			   m_last_input;
	pj_uint8_t			   m_relay;
	pj_uint8_t			   m_btn_led;
	pj_uint8_t			   m_btn2_led;
	serialib			  *serial;
	bool 				   just_opened;
	comm_protocol_parser_c parser;

	static void* s_thread_proc(pjs_arduino_controller *object)
	{
		if(object)
			return object->thread_proc();
		else
			return NULL;
	}

	void* thread_proc();
	void check_input();
	void send_relay();
	void send_btn_led();
	void send_btn2_led();
	void send_buffer(void *data, pj_uint16_t size);
public:
	pjs_arduino_controller(pjs_global &global,pjs_external_controller_callback &callback);
	virtual ~pjs_arduino_controller();
	virtual pj_int32_t set_output(pj_uint8_t  output, pj_uint8_t value);
	virtual void update_inputs();
};


#endif /* PJS_ARDUINO_CONTROLLER_H_ */
