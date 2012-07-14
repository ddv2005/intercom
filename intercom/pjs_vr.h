#ifndef PJS_VR_H_
#define PJS_VR_H_
#include "project_common.h"
#include "project_global.h"
#include "pjs_utils.h"
#include "pjs_port.h"
#include <string>

SWIG_BEGIN_DECL
typedef pj_int32_t pjs_vr_async_param_t;
SWIG_END_DECL
class pjs_vr;
class pjs_vr_callback
{
public:
	virtual void on_async_completed(pjs_vr *sender, pj_uint8_t state,pjs_vr_async_param_t param1, pjs_vr_async_param_t param2)=0;
	virtual ~pjs_vr_callback(){}
};

//VR effect
#define VRE_NONE		0
#define VRE_FADE_OUT	1

SWIG_BEGIN_DECL
//VR states
#define VRS_IDLE		0
#define VRS_PLAY_FILE	1
#define VRS_WAIT_INPUT	2

class pjs_vr:public pjs_port
{
protected:
	pjs_global &m_global;
	pjs_simple_mutex m_api_lock;
	pjs_simple_mutex m_lock;
	std::string	m_terminate_cond;
	std::string	m_input_buffer;
	std::string	m_sounds_path;
	pj_uint8_t	m_state;
	pj_uint32_t m_ptime;
	pjs_exit_t &m_exit;
	pjs_shared_event &m_exit_event;

	pjs_shared_event m_completed_event;
    bool			 m_async;
	pj_int32_t m_async_param1;
	pj_int32_t m_async_param2;
	void *	   m_user_data;
	pj_int32_t m_user_param;
    pjs_vr_callback *m_callback;

	pjmedia_port *m_getport;
	pj_pool_t	 *m_getpool;
	pj_int16_t	 *m_getbuffer;
	pj_int16_t	  m_getsize;
	pjmedia_resample *m_getresample;
	bool		  m_get_first_time;

	pj_uint8_t	  m_effect;
	pj_timestamp  m_effect_start_time;
	pj_int32_t	  m_effect_param;

	pj_uint8_t    m_wait_input_max;
	pj_timestamp  m_wait_input_start_time;
	pj_timestamp  m_last_digits_time;
	pj_uint32_t	  m_digits_timeout;
	pj_uint32_t	  m_interdigits_timeout;

	virtual pj_status_t get_frame(pjmedia_frame *frame);
	virtual pj_status_t put_frame(const pjmedia_frame *frame);
	void internal_stop();
	bool check_terminate_cond();
	void async_completed(pj_uint8_t state);
	bool internal_wait_completion(pj_int32_t timeout);
	std::string internal_get_input(pj_uint8_t max_digits,const char *terminate_cond,bool lock);
public:
#ifdef SWIG
protected:
#endif
	pjs_vr(pjs_global &global,const char *sounds_path, unsigned sampling_rate,
		   unsigned samples_per_frame,pjs_exit_t &exit,pjs_shared_event &exit_event,pjs_vr_callback *callback,
			void * user_data, pj_int32_t user_param);
	virtual ~pjs_vr();

	void on_input(const char *input);
	void on_input(const char digit);
#ifdef SWIG
public:
#endif

	int	play_file(const char * filename,const char * terminate_cond, bool loop, bool async, pjs_vr_async_param_t async_param1, pjs_vr_async_param_t async_param2);
	int	play_file(const char * filename)
	{
		return play_file(filename,"",false,false,0,0);
	}

	int	play_file_async(const char * filename, pjs_vr_async_param_t async_param1, pjs_vr_async_param_t async_param2)
	{
		return play_file(filename,"",false,true,async_param1,async_param2);
	}

	int	play_file(const char * filename,const char * terminate_cond)
	{
		return play_file(filename,terminate_cond,false,false,0,0);
	}

	int	play_file_async(const char * filename,const char * terminate_cond, pjs_vr_async_param_t async_param1, pjs_vr_async_param_t async_param2)
	{
		return play_file(filename,terminate_cond,false,true,async_param1,async_param2);
	}

	void fade_out(pj_uint32_t time_);

	void clear_input();
	std::string get_input();
	std::string get_input(pj_uint8_t max_digits,const char *terminate_cond)
	{
		return internal_get_input(max_digits,terminate_cond,true);
	}
	std::string wait_input(pj_uint8_t max_digits,const char *terminate_cond,pj_uint32_t timeout,pj_uint32_t inter_digits_timeout,bool async, pjs_vr_async_param_t async_param1, pjs_vr_async_param_t async_param2);
	std::string wait_input(pj_uint8_t max_digits,const char *terminate_cond,pj_uint32_t timeout,pj_uint32_t inter_digits_timeout)
	{
		return wait_input(max_digits,terminate_cond,timeout,3000,false,0,0);
	}
	std::string wait_input(pj_uint8_t max_digits,const char *terminate_cond,pj_uint32_t timeout)
	{
		return wait_input(max_digits,terminate_cond,timeout,3000,false,0,0);
	}

	std::string wait_input_async(pj_uint8_t max_digits,const char *terminate_cond, pjs_vr_async_param_t async_param1, pjs_vr_async_param_t async_param2)
	{
		return wait_input(max_digits,terminate_cond,0,3000,true,async_param1,async_param2);
	}

	std::string wait_input_async(pj_uint8_t max_digits,const char *terminate_cond,pj_uint32_t timeout,pj_uint32_t inter_digits_timeout, pjs_vr_async_param_t async_param1, pjs_vr_async_param_t async_param2)
	{
		return wait_input(max_digits,terminate_cond,timeout,inter_digits_timeout,true,async_param1,async_param2);
	}

	void stop();
	bool wait_async_completion(pj_int32_t timeout);
	pj_uint8_t	get_state() { return m_state; };
	bool is_idle() { return m_state==VRS_IDLE; }
	void *get_user_data() { return m_user_data; }
	pj_int32_t get_user_param() { return m_user_param; }
};

SWIG_END_DECL
#endif /* PJS_VR_H_ */
