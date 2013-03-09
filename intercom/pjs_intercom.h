#ifndef PJS_INTERCOM_H_
#define PJS_INTERCOM_H_
#include "project_common.h"
#include "project_global.h"
#include "pjs_gaincontrol.h"
#include "pjs_external_controller.h"
#include <list>
#include <vector>
#include <map>
#include <pjsua.h>
#include <pjmedia.h>
#include <pjmedia_audiodev.h>
#include "echo_canceller.h"
#include "pjs_messages_queue.h"
#include "pjs_intercom_messages.h"
#include "pjs_intercom_script_interface.h"
#include "pjs_lua.h"
#include "pjs_vr.h"
#include "pjs_audio_switcher.h"
#include "pjs_media_sync.h"
#include "pjs_audio_monitor.h"

#define PJS_ECHO_PERIOD_MS			10
#define PJS_MAX_CONTROL_MESSAGES	200
#define PJS_LUA_GLOBAL	"global"
#define PJS_LUA_CALL_PROC "call_main"

class pjs_intercom_system;
class pjs_master_port:public pjs_media_sync
{
protected:
	pjs_global & m_global;
	pj_pool_t *m_pool;
	bool m_tx_mute;
	bool m_rx_mute;

	pj_timestamp m_tx_timestamp;
	pj_timestamp m_rx_timestamp;
	unsigned m_tx_timestamp_inc;
	unsigned m_rx_timestamp_inc;
	pjmedia_aud_stream *m_rx_stream;
	pjmedia_aud_param m_rx_aud_param;
	pjmedia_aud_stream *m_tx_stream;
	pjmedia_aud_param m_tx_aud_param;
	pjmedia_port* m_tx_up_port;
	pjmedia_port* m_rx_up_port;
	pj_bool_t m_ok;
	pjs_agc *m_tx_preprocessor;
	pjs_agc *m_rx_preprocessor;
	pj_int16_t m_tx_power;
	pj_int16_t m_rx_power;
	pjmedia_resample *m_rx_resample;
	pjmedia_resample *m_tx_resample;
	pj_int16_t *m_tx_buffer;
	pj_int16_t *m_rx_buffer;
	PPJ_SemaphoreLock *m_lock;
	pjs_echo_canceller *m_echo;
	pj_uint16_t m_echo_samples;

	pj_thread_t *m_put_thread;
	pjs_event m_put_event;
	pjmedia_circ_buf *m_put_buffer;
	bool m_exit;

	static void* s_put_thread_proc(pjs_master_port *port)
	{
		if (port)
			return port->put_thread_proc();
		else
			return NULL;
	}

	void *put_thread_proc();

public:
	pjs_master_port(pjs_global &global, pjmedia_aud_stream *tx_stream,
			pjmedia_aud_stream *rx_stream, pjmedia_port* tx_up_port,
			pjmedia_port* rx_up_port, pj_uint16_t tx_gain_target,
			pj_uint16_t tx_gain_max, pj_uint16_t tx_gain_min,
			pj_uint16_t rx_gain_target, pj_uint16_t rx_gain_max,
			pj_uint16_t rx_gain_min, pj_uint16_t tail_ms, pj_uint16_t latency_ms,
			bool disable_echo_canceller);
	virtual ~pjs_master_port();
	pj_status_t get_frame(pjmedia_frame *frame);
	pj_status_t put_frame(const pjmedia_frame *frame);

	CREATE_CLASS_PROP_FUNCTIONS(bool, rx_mute)CREATE_CLASS_PROP_FUNCTIONS(bool, tx_mute)
};

class pjs_intercom_script
{
protected:
	pjs_global &m_global;
	pj_pool_t *m_pool;
	pjs_intercom_message_queue m_messages_queue;
	pjs_shared_event m_script_event;
	pjs_simple_mutex m_script_mutex;
	pjs_lua *m_script;
	bool m_script_exit;
	bool m_script_thread_completed;
	pj_thread_t *m_script_thread;
	std::string m_script_name;
	std::string m_script_proc;

	pjs_lua_global *m_lua_global;
	pjs_script_data_t m_script_data;
	pjs_script_id_t m_sid;
	pjs_intercom_script_interface *m_intercom_script_interface;

	static void* s_script_thread_proc(pjs_intercom_script *is)
	{
		if (is)
			return is->script_thread_proc();
		else
			return NULL;
	}
	void* script_thread_proc();
	void terminate_script();
	void post_script_quit_message();
	pj_status_t start_thread();
	virtual int i_put_params_for_main_proc()=0; //return number parameters
	virtual void i_before_terminate()=0;
	virtual void i_after_terminate()=0;
	virtual void i_on_stop()=0;
	virtual void i_on_clean()=0;
	virtual void i_on_thread_end()=0;
	virtual bool is_ready()
	{
		return (m_pool && m_script_thread);
	}
	void clean();
public:
	pjs_intercom_script(pjs_script_id_t sid, pjs_intercom_script_interface *intercom_script_interface,pjs_global &global,
			pjs_lua_global *lua_global, pjs_script_data_t &script_data,
			const char *script_name, const char *script_proc);
	virtual ~pjs_intercom_script();
	void stop();
	void terminate()
	{
		if (m_script_thread)
		{
			stop();
			if (!m_script_thread_completed)
			{
				terminate_script();
			}
		}
	}
	bool is_completed()
	{
		return m_script_thread_completed;
	}
	pj_int32_t get_tag()
	{
		return m_script_data.tag;
	}
};

class pjs_intercom_script_final:public pjs_intercom_script
{
protected:
	virtual int i_put_params_for_main_proc()
	{
		if (m_script)
		{
			pjs_script_interface *script_interface = new pjs_script_interface(m_script_data);
			m_script->push_swig_object(script_interface, script_interface->class_name(), true);
		}
		return 1;
	}
	virtual void i_before_terminate(){};
	virtual void i_after_terminate(){};
	virtual void i_on_stop(){};
	virtual void i_on_clean(){};
	virtual void i_on_thread_end(){};
public:
	pjs_intercom_script_final(pjs_script_id_t sid, pjs_intercom_script_interface *intercom_script_interface,pjs_global &global,
			pjs_lua_global *lua_global, pjs_script_data_t &script_data,
			const char *script_name, const char *script_proc):pjs_intercom_script(sid,intercom_script_interface, global,lua_global,script_data,script_name,script_proc)
	{
		pj_status_t status = start_thread();
		if (status != PJ_SUCCESS)
		{
			clean();
		}
	}
	virtual ~pjs_intercom_script_final()
	{
		clean();
	}
};

class pjs_intercom_call: public pjs_intercom_script, public pjs_vr_callback
{
protected:
	pjs_call_data_t m_call_info;
	bool m_call_is_active;
	pjs_vr *m_master_vr;
	pjsua_conf_port_id m_master_vr_id;
	pjs_call_script_interface *m_lua_call;

	virtual int i_put_params_for_main_proc();
	virtual void i_before_terminate();
	virtual void i_after_terminate();
	virtual void i_on_stop();
	virtual void i_on_clean();
	virtual void i_on_thread_end();
	virtual bool is_ready()
	{
		return m_call_is_active && pjs_intercom_script::is_ready();
	}
public:
	pjs_intercom_call(pjs_script_id_t sid,pjs_intercom_script_interface *intercom_script_interface,pjs_global &global, pjs_lua_global *lua_global,
			pjs_script_data_t &script_data,pjs_call_data_t &callinfo, const char *script_name,pjs_audio_switcher &switcher);
	virtual ~pjs_intercom_call();
	bool is_active()
	{
		return m_call_is_active;
	}
	bool is_connected_to_main()
	{
		return m_call_is_active && m_call_info.is_connected_to_main;
	}
	pjsua_call_id get_callid()
	{
		return m_call_info.callid;
	}

	void on_media_connected();
	void on_dtmf_digit(int digit);

	//pjs_vr_callback
	virtual void on_async_completed(pjs_vr *sender, pj_uint8_t state,
			pjs_vr_async_param_t param1, pjs_vr_async_param_t param2);
};

#define PJSUA_CB_V(name,params,eparams) static void s_##name params { if(self) self->name eparams; } \
								void name params;

typedef map<pjsua_call_id, pjs_script_id_t> pjs_intercom_calls_map;
typedef map<pjsua_call_id, pjs_script_id_t>::iterator pjs_intercom_calls_map_itr;


typedef map<pjs_script_id_t, pjs_intercom_script*> pjs_intercom_scripts_map;
typedef map<pjs_script_id_t, pjs_intercom_script*>::iterator pjs_intercom_scripts_map_itr;

typedef list<pjs_intercom_script*> pjs_intercom_scripts_erase_list;
typedef list<pjs_intercom_script*>::iterator pjs_intercom_scripts_erase_list_itr;


#define LGV_DB_PATH	"db_path"

void set_global_to_lua(pjs_lua *lua, pjs_global *global);

class pjs_intercom_system: public pjs_external_controller_callback,
		public pjs_intercom_script_interface,
		public pjs_vr_callback,
		public pjs_call_script_interface_callback,
		public pjs_audio_switcher
{
protected:
	pjs_config_t m_config;
	pjs_global *m_global;
	pj_pool_t *m_pool;
	PPJ_MutexLock *m_lock;
	pjsua_config m_pjsua_cfg;
	pjsua_logging_config m_log_cfg;
	pjsua_media_config m_media_cfg;
	pjsua_transport_config m_udp_cfg;
	pjsua_transport_config m_rtp_cfg;
	unsigned m_acc_cnt;
	pjsua_acc_id m_acc_id[MAX_ACCOUNTS];
	pj_uint8_t m_init_stage;
	pjsua_transport_id m_udp_transport_id;

	pj_timestamp m_last_check_scripts;
	pj_timestamp m_last_log_flush;
	pj_timer_entry *m_timer;
	pjs_master_port *m_master_port;
	pjmedia_aud_stream *m_play_stream;
	pjmedia_aud_stream *m_rec_stream;
	pjs_external_controller *m_controller;
	pjs_intercom_message_queue m_messages_queue;

	pjs_shared_event m_script_event;
	pjs_simple_mutex m_script_mutex;
	pjs_lua *m_script;
	bool m_script_exit;
	pj_thread_t *m_script_thread;

	pjs_vr *m_master_vr;
	pjsua_conf_port_id m_master_vr_id;

	pjs_vr *m_back_vr;
	pjsua_conf_port_id m_back_vr_id;

	pjs_script_id_t m_scripts_id;
	pjs_intercom_calls_map m_calls;
	pjs_intercom_scripts_map m_scripts;
	pjs_simple_mutex m_scripts_mutex;
	pjs_lua_global m_lua_global;

	bool m_started;

	pjs_audio_monitor *m_audio_monitor;

	static pjs_intercom_system *self;

	static void* s_script_thread_proc(pjs_intercom_system *ics)
	{
		if (ics)
			return ics->script_thread_proc();
		else
			return NULL;
	}
	void* script_thread_proc();

	static void timer_callback_s(pj_timer_heap_t *timer_heap,
			struct pj_timer_entry *entry)
	{
		if (entry)
		{
			void * ud = entry->user_data;
			if (ud)
				((pjs_intercom_system *) ud)->timer();
		}
	}

	static pj_status_t null_rec_cb_s(void *user_data, pjmedia_frame *frame)
	{
		return PJ_SUCCESS;
	}

	static pj_status_t rec_cb_s(void *user_data, pjmedia_frame *frame)
	{
		if (user_data)
		{
			if (((pjs_intercom_system *) user_data)->m_master_port)
				return ((pjs_intercom_system *) user_data)->m_master_port->put_frame(
						frame);
		}
		return PJ_SUCCESS;
	}

	static pj_status_t null_play_cb_s(void *user_data, pjmedia_frame *frame)
	{
		return PJ_SUCCESS;
	}

	static pj_status_t play_cb_s(void *user_data, pjmedia_frame *frame)
	{
		if (user_data)
		{
			if (((pjs_intercom_system *) user_data)->m_master_port)
				return ((pjs_intercom_system *) user_data)->m_master_port->get_frame(
						frame);
		}
		return PJ_SUCCESS;
	}

	PJSUA_CB_V(on_call_state,(pjsua_call_id call_id, pjsip_event *e),(call_id,e))
	;PJSUA_CB_V(on_incoming_call,(pjsua_acc_id acc_id, pjsua_call_id call_id,pjsip_rx_data *rdata),(acc_id,call_id,rdata))
	;PJSUA_CB_V(on_call_media_state,(pjsua_call_id call_id),(call_id))
	;PJSUA_CB_V(on_dtmf_digit,(pjsua_call_id call_id, int digit),(call_id,digit))
	;PJSUA_CB_V(on_call_transfer_request,(pjsua_call_id call_id,const pj_str_t *dst,pjsip_status_code *code),(call_id,dst,code))
	;

	void timer();
	void schedule_timer();

	void clean_system();
	bool is_started()
	{
		return m_started;
	}

	void post_script_quit_message();
	void terminate_script();
	pj_status_t create_call(pjs_script_data_t &script_data,pjs_call_data_t &call_info);
	pjs_intercom_call *get_call(pjsua_call_id call_id);
	pjs_intercom_script *get_script(pjs_script_id_t script_id);
	pj_uint32_t internal_get_connected_to_main_count();
public:
	pjs_intercom_system();
	virtual ~pjs_intercom_system();
	// start/stop is NOT thread safe
	void set_config(pjs_config_t &config);
	pjs_result_t start();
	void stop();

	pjs_media_sync* get_media_sync()
	{
		if(m_master_port)
			return m_master_port;
		else
			return NULL;
	}

	virtual pj_int32_t on_ext_controller_message(pjs_intercom_message_t &message);

	//pjs_intercom_script_interface implementation
	virtual pjs_external_controller *get_controller()
	{
		return m_controller;
	}
	virtual pj_uint32_t get_connected_to_main_count();

	//pjs_vr_callback
	virtual void on_async_completed(pjs_vr *sender, pj_uint8_t state,
			pjs_vr_async_param_t param1, pjs_vr_async_param_t param2);

	//pjs_call_script_interface_callback
	virtual void on_connected_to_main_changed(pjsua_call_id callid);
	virtual int make_call(int account_idx, const char* uri, pj_uint32_t timeout,
			const char *user_options, pj_int32_t tag, bool is_raw_uri);
	virtual unsigned hangup_calls_by_tag(pj_int32_t tag, unsigned code);
	virtual bool call_is_active(int callid);
	virtual pjs_script_id_t create_thread(const char *script,const char *proc, const char *user_options, pj_int32_t tag);

	// pjs_audio_switcher
	virtual pj_status_t connect(pjsua_conf_port_id source,
			pjsua_conf_port_id sink);
	virtual pj_status_t disconnect(pjsua_conf_port_id source,
			pjsua_conf_port_id sink);

};

#endif /* PJS_INTERCOM_H_ */
