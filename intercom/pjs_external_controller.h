#ifndef PJS_EXTERNAL_CONTROLLER_H_
#define PJS_EXTERNAL_CONTROLLER_H_
#include "project_common.h"
#include "project_global.h"
#include "pjs_messages_queue.h"

typedef	pj_int32_t pjs_ecc_param_t;
class	pjs_external_controller_callback
{
public:
	virtual ~pjs_external_controller_callback(){};
	virtual pj_int32_t	on_ext_controller_message(pjs_intercom_message_t &message)=0;
};

SWIG_BEGIN_DECL
//External controller outputs
#define	ECO_MAIN_LED	0
#define ECO_BELL		1
#define	ECO_SECOND_LED	2

class	pjs_external_controller
{
protected:
	pjs_global &m_global;
	pjs_external_controller_callback &m_callback;
	pj_int32_t	fire_on_message(pjs_intercom_message_t &message)
	{
		return m_callback.on_ext_controller_message(message);
	}
public:
#ifdef SWIG
protected:
#endif
	pjs_external_controller(pjs_global &global,pjs_external_controller_callback &callback);
	virtual ~pjs_external_controller();
#ifdef SWIG
public:
#endif
	virtual pj_int32_t set_output(pj_uint8_t  output, pj_uint8_t value)=0;
	virtual void update_inputs()=0;
};

SWIG_END_DECL

#endif /* PJS_EXTERNAL_CONTROLLER_H_ */
