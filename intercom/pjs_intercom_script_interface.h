#ifndef PJS_INTERCOM_SCRIPT_INTERFACE_H_
#define PJS_INTERCOM_SCRIPT_INTERFACE_H_
#include "project_common.h"
#include "pjs_lua_common.h"
#include "pjs_external_controller.h"
#include "pjs_audio_switcher.h"
#include <string>

class pjs_call_script_interface_callback
{
public:
	virtual ~pjs_call_script_interface_callback() {}
	virtual void on_connected_to_main_changed(pjsua_call_id callid)=0;
};

typedef struct pjs_script_data_t
{
	std::string user_options;
	pj_int32_t tag;
} pjs_script_data_t;

typedef struct pjs_call_data_t
{
	pjsua_call_id callid;
	bool is_local;
	bool is_inbound;
	bool is_connected_to_main;
	pj_uint8_t sound_connection_type;
	std::string user_options;
	pj_int32_t tag;
	pj_uint32_t timeout;
	pjs_call_script_interface_callback *callback;
} pjs_call_data_t;

void clear_pjs_call_data(pjs_call_data_t &data);

SWIG_BEGIN_DECL
typedef pj_uint32_t pjs_script_id_t;

// Call sound connection type
#define SCT_NONE			0
#define SCT_LISTEN_MAIN		1
#define SCT_CONFERENCE_ON_CONNECTED		2
#define SCT_CONFERENCE		4

#define PJ_ERR_CALL_IS_ENDED	-2000

typedef struct pjs_call_info_t
{
	std::string local_name;
	std::string local_number;
	std::string local_uri;
	std::string remote_name;
	std::string remote_number;
	std::string remote_uri;
} pjs_call_info_t;

class pjs_script_interface: public pjs_lua_interface_object
{
protected:
	pjs_script_data_t &m_script_data;
public:
#ifdef SWIG
protected:
#endif
	DEFINE_LUA_INTERFACE_CLASS_NAME("pjs_script_interface")
	pjs_script_interface(pjs_script_data_t &script_data) :
			m_script_data(script_data)
	{
	}

	virtual ~pjs_script_interface()
	{
	}
	;
#ifdef SWIG
public:
#endif
	pj_int32_t get_tag()
	{
		return m_script_data.tag;
	}
	void set_tag(pj_int32_t v)
	{
		m_script_data.tag = v;
	}
	std::string get_user_options()
	{
		return m_script_data.user_options;
	}
};

class pjs_call_script_interface: public pjs_script_interface
{
protected:
	pjs_call_data_t &m_call_info;
	bool &m_call_is_active;
	pjsua_conf_port_id m_vr_port;
	pjsua_conf_port_id m_call_port;
	pjs_audio_switcher &m_switcher;

	void check_vr_port_connection()
	{
		if (m_call_port && m_vr_port)
		{
			m_switcher.connect(m_vr_port, m_call_port);
		}
	}

	void sound_route_to_other_calls(bool connect)
	{
		pjsua_call_id call_ids[PJSUA_MAX_CALLS];
		unsigned call_cnt = PJ_ARRAY_SIZE(call_ids);
		unsigned i;

		pjsua_enum_calls(call_ids, &call_cnt);

		for (i = 0; i < call_cnt; ++i)
		{
			if (call_ids[i] == m_call_info.callid)
				continue;

			if (!pjsua_call_has_media(call_ids[i]))
				continue;

			pjsua_conf_port_id rem_port = pjsua_call_get_conf_port(call_ids[i]);
			PJ_LOG_(INFO_LEVEL,
					(__FILE__,"conf connect %d to %d",(int)m_call_port,rem_port));
			if (rem_port)
			{
				if (connect)
				{
					m_switcher.connect(m_call_port, rem_port);
					m_switcher.connect(rem_port, m_call_port);
				}
				else
				{
					m_switcher.disconnect(m_call_port, rem_port);
					m_switcher.disconnect(rem_port, m_call_port);
				}
			}
		}
	}

	void check_main_sound_connection()
	{
		if (m_call_port)
		{
			if (m_call_info.is_connected_to_main)
			{
				sound_route_to_other_calls(
						(m_call_info.sound_connection_type & SCT_CONFERENCE)
								|| (m_call_info.sound_connection_type
										& SCT_CONFERENCE_ON_CONNECTED));
				m_switcher.connect(m_call_port, 0);
				m_switcher.connect(0, m_call_port);
			}
			else
			{
				sound_route_to_other_calls(
						m_call_info.sound_connection_type & SCT_CONFERENCE);
				m_switcher.disconnect(m_call_port, 0);
				if (m_call_info.sound_connection_type & SCT_LISTEN_MAIN)
					m_switcher.connect(0, m_call_port);
				else
					m_switcher.disconnect(0, m_call_port);
			}
		}
	}

public:
#ifdef SWIG
protected:
#endif
	DEFINE_LUA_INTERFACE_CLASS_NAME("pjs_call_script_interface")
	pjs_call_script_interface(pjs_script_data_t &script_data,pjs_call_data_t &call_info, bool &call_is_active, pjs_audio_switcher &switcher) :
			pjs_script_interface(script_data), m_call_info(call_info), m_call_is_active(
					call_is_active),m_switcher(switcher)
	{
		m_vr_port = 0;
		m_call_port = 0;
		m_call_info.is_connected_to_main = false;
	}

	virtual ~pjs_call_script_interface()
	{
	}
	;

	void set_vr_port(pjsua_conf_port_id port)
	{
		m_vr_port = port;
		check_vr_port_connection();
	}

	void set_call_port(pjsua_conf_port_id port)
	{
		m_call_port = port;
		check_vr_port_connection();
		check_main_sound_connection();
	}
#ifdef SWIG
public:
#endif

	pj_status_t answer(unsigned code, const char *reason)
	{
		if (m_call_is_active)
		{
			pj_str_t str;
			pj_str_t *pstr = NULL;
			if (reason)
			{
				pj_cstr(&str, reason);
				pstr = &str;
			}
			return pjsua_call_answer(m_call_info.callid, code, pstr, NULL);
		}
		else
			return PJ_ERR_CALL_IS_ENDED;
	}

	pj_status_t answer(unsigned code)
	{
		return answer(code, NULL);
	}

	bool connect_main()
	{
		m_call_info.is_connected_to_main = true;
		check_main_sound_connection();
		if (m_call_is_active && m_call_info.callback)
		{
			m_call_info.callback->on_connected_to_main_changed(m_call_info.callid);
		}
		return m_call_port != 0;
	}

	bool disconnect_main()
	{
		m_call_info.is_connected_to_main = false;
		check_main_sound_connection();
		if (m_call_is_active && m_call_info.callback)
		{
			m_call_info.callback->on_connected_to_main_changed(m_call_info.callid);
		}
		return m_call_port != 0;
	}

	void set_sound_connection_type(pj_uint8_t sct)
	{
		m_call_info.sound_connection_type = sct;
		check_main_sound_connection();
	}

	pj_uint8_t get_sound_connection_type()
	{
		return m_call_info.sound_connection_type;
	}

	pj_status_t hangup(unsigned code)
	{
		if (m_call_is_active)
		{
			return pjsua_call_hangup(m_call_info.callid, code, NULL, NULL);
		}
		else
			return PJ_ERR_CALL_IS_ENDED;
	}

	bool is_inbound()
	{
		return m_call_info.is_inbound;
	}
	bool is_local()
	{
		return m_call_info.is_local;
	}
	pjs_call_info_t get_call_info();
	unsigned get_callid()
	{
		return m_call_info.callid;
	}
	unsigned get_state()
	{
		if (m_call_is_active)
		{
			pjsua_call_info call_info;
			pjsua_call_get_info(m_call_info.callid, &call_info);
			return call_info.state;
		}
		else
			return PJSIP_INV_STATE_DISCONNECTED;
	}
};

class pjs_intercom_script_interface: public pjs_lua_interface_object
{
public:
#ifdef SWIG
protected:
#endif
	DEFINE_LUA_INTERFACE_CLASS_NAME("pjs_intercom_script_interface")
	pjs_intercom_script_interface()
	{
	}
	;
	virtual ~pjs_intercom_script_interface()
	{
	}
	;
#ifdef SWIG
public:
#endif
	virtual pjs_external_controller *get_controller()=0;
	virtual pj_uint32_t get_connected_to_main_count()=0;
	virtual int make_call(int account_idx, const char* uri, pj_uint32_t timeout,
			const char *user_options, pj_int32_t tag, bool is_raw_uri)=0;
	virtual unsigned hangup_calls_by_tag(pj_int32_t tag, unsigned code)=0;
	virtual bool call_is_active(int callid)=0;
	virtual pjs_script_id_t create_thread(const char *script,const char *proc, const char *user_options, pj_int32_t tag)=0;
};


SWIG_END_DECL

#endif /* PJS_INTERCOM_SCRIPT_INTERFACE_H_ */
