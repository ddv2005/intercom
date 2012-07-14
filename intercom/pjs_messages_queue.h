#ifndef PJS_MESSAGES_QUEUE_H_
#define PJS_MESSAGES_QUEUE_H_
#include "pjs_utils.h"
#include "pjs_lua_common.h"


SWIG_BEGIN_DECL

typedef struct pjs_intercom_message_t
{
	pj_uint32_t	message;
	pj_int32_t	param1;
	pj_int32_t	param2;
}pjs_intercom_message_t;

#define clear_intercom_message(m) (memset(&m,0,sizeof(m)))

typedef pjs_list<pjs_intercom_message_t>	pjs_intercom_message_queue_base;
#ifdef SWIG
%template(pjs_intercom_message_queue_base) pjs_list<pjs_intercom_message_t>;
%apply SWIGTYPE **OUTPUT { pjs_intercom_message_t **result };
#endif

class pjs_intercom_message_queue: public pjs_intercom_message_queue_base,public pjs_lua_interface_object
{
public:
#ifdef SWIG
protected:
#endif
	pjs_intercom_message_queue(unsigned int	max_elements):pjs_intercom_message_queue_base(max_elements){}
	virtual ~pjs_intercom_message_queue(){};
	DEFINE_LUA_INTERFACE_CLASS_NAME("pjs_intercom_message_queue")
#ifdef SWIG
public:
#endif

	void get_message(pjs_intercom_message_t **result)
	{
		pjs_intercom_message_t msg;
		if(pop(&msg))
		{
			*result = (pjs_intercom_message_t *)malloc(sizeof(pjs_intercom_message_t ));
			**result = msg;
		}
		else
			*result = NULL;
	}
};

SWIG_END_DECL


#endif /* PJS_MESSAGES_QUEUE_H_ */
