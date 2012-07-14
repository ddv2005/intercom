#include "pjs_intercom.h"
#include "pjs_arduino_controller.h"
#include "pjs_emulator_controller.h"
#include <unistd.h>
#include "pjs_lua_libs.h"

#define THIS_FILE	"pjs_intercom.cpp"

#define	MAX_CONF_SLOTS	10
//#define DEBUG_MASTER_PORT 1

template <class T,class T1>
T1 safe_cast (T v) {
 return v?static_cast<T1>(v):NULL;
}

#define SAFE_CAST_SCRIPT_TO_CALL(s) (safe_cast<pjs_intercom_script *,pjs_intercom_call *>(s))

#ifdef __LINUX
void pjs_sleep(pj_uint32_t usec)
{
	struct timespec ts;
	ts.tv_sec = usec / 1000000L;
	ts.tv_nsec = (usec % 1000000L) * 1000L;
	nanosleep(&ts, &ts);
}
#else
void pjs_sleep(pj_uint32_t usec)
{
	Sleep(usec/1000)
}
#endif

pjs_intercom_system *pjs_intercom_system::self = NULL;
pjs_intercom_system::pjs_intercom_system() :
		m_messages_queue(PJS_MAX_CONTROL_MESSAGES)
{
	self = this;
	pjs_config_init(m_config);
	m_pool = NULL;
	m_lock = NULL;
	m_global = NULL;
	m_acc_cnt = 0;
	m_started = false;
	m_udp_transport_id = -1;
	m_play_stream = m_rec_stream = NULL;
	m_master_port = NULL;
	m_timer = NULL;
	m_controller = NULL;
	m_script = NULL;
	m_script_thread = NULL;
	m_master_vr = NULL;
	m_master_vr_id = m_back_vr_id = 0;
	m_back_vr = NULL;
	m_script_exit = false;
	m_scripts_id = 0;
	m_last_check_scripts.u64 = 0;
}

pjs_intercom_system::~pjs_intercom_system()
{
	stop();
}

pj_int32_t pjs_intercom_system::on_ext_controller_message(
		pjs_intercom_message_t &message)
{
	m_messages_queue.push(message, false);
	return RC_OK;
}

void pjs_intercom_system::set_config(pjs_config_t &config)
{
	if (!is_started())
		m_config = config;
}

void set_global_to_lua(pjs_lua *lua, pjs_global *global)
{
	if (lua && global)
	{
		lua->set_global(LGV_DB_PATH, global->get_config().system.db_path);
	}
}

void pjs_intercom_system::timer()
{
	if (is_started())
	{
		pj_timestamp now;
		pj_get_timestamp(&now);
		if (pj_elapsed_msec(&m_last_check_scripts, &now) >= 900)
		{
			pj_get_timestamp(&m_last_check_scripts);
			pjs_intercom_scripts_erase_list scripts_to_erase;
			m_scripts_mutex.lock();
			for (pjs_intercom_scripts_map_itr citr = m_scripts.begin();
					citr != m_scripts.end();)
			{
				pjs_intercom_script *script = citr->second;
				if (script)
				{
					if (script->is_completed())
					{
						scripts_to_erase.push_back(script);
						m_scripts.erase(citr++);
					}
					else
					{
						citr++;
					}
				}
				else
				{
					m_scripts.erase(citr++);
				}
			}
			m_scripts_mutex.unlock();

			for (pjs_intercom_scripts_erase_list_itr itr = scripts_to_erase.begin();
					itr != scripts_to_erase.end(); itr++)
			{
				delete (*itr);
			}
		}
	}

	schedule_timer();
}

void pjs_intercom_system::schedule_timer()
{
	if (m_timer)
	{
		pj_time_val delay;
		delay.sec = 1;
		delay.msec = 0;
		pj_timer_heap_schedule(m_global->get_timer_heap(), m_timer, &delay);
	}
}

#define ASSERT_PJ(p) {status=p;if(status!=PJ_SUCCESS){PJ_LOG_(ERROR_LEVEL, (__FILE__,"" #p " error %d",status)); goto on_error;}}

pjs_result_t pjs_intercom_system::start()
{
	pj_status_t status;
	pjs_result_t result = RC_ERROR;
	if (!is_started())
	{
		char tmp[80];
		unsigned i;
		int dev_count;
		pjmedia_aud_param params;
		pjmedia_aud_dev_index dev_idx;

		m_init_stage = 0;

		m_messages_queue.clear();
		m_lua_global.clear();

		ASSERT_PJ(pjsua_create());
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Starting PJSIS"));
		m_init_stage = 1;

		m_global = new pjs_global(pjsua_get_pool_factory(), &m_config);

		m_pool = pj_pool_create(m_global->get_pool_factory(), "pjsis system", 128,
				128, NULL);
		m_lock = new PPJ_MutexLock(m_pool, NULL);

		pjsua_config_default(&m_pjsua_cfg);
		pj_ansi_sprintf(tmp, "PJSIS v%s %s", pj_get_version(),
				pj_get_sys_info()->info.ptr);
		pj_strdup2_with_null(m_pool, &m_pjsua_cfg.user_agent, tmp);

		pjsua_logging_config_default(&m_log_cfg);
		m_log_cfg.decor |= PJ_LOG_HAS_THREAD_ID;
		pjsua_media_config_default(&m_media_cfg);
		pjsua_transport_config_default(&m_udp_cfg);
		m_udp_cfg.port = 5060;
		pjsua_transport_config_default(&m_rtp_cfg);
		m_rtp_cfg.port = 4000;

		m_pjsua_cfg.cb.on_call_state = s_on_call_state;
		m_pjsua_cfg.cb.on_incoming_call = s_on_incoming_call;
		m_pjsua_cfg.cb.on_call_media_state = s_on_call_media_state;
		m_pjsua_cfg.cb.on_dtmf_digit = s_on_dtmf_digit;
		m_pjsua_cfg.cb.on_call_transfer_request = s_on_call_transfer_request;

		if (strlen(m_config.network.stun) > 0)
		{
			pj_str_t stun = pj_str(m_config.network.stun);
			pj_strdup(m_pool, &m_pjsua_cfg.stun_srv[0], &stun);
			m_pjsua_cfg.stun_srv_cnt = 1;
		}

		m_log_cfg.level = m_log_cfg.console_level = m_config.logs.level;
		m_log_cfg.decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_SENDER
				| PJ_LOG_HAS_THREAD_ID | PJ_LOG_HAS_TIME | PJ_LOG_HAS_MICRO_SEC;
		m_log_cfg.msg_logging = PJ_FALSE;
		m_log_cfg.log_filename = pj_str(m_config.logs.file_name);
		m_log_cfg.log_file_flags |= PJ_O_APPEND;

		m_media_cfg.clock_rate = m_config.sound_subsystem.clock_rate;
		m_media_cfg.channel_count = 1;
		m_media_cfg.ptime = m_media_cfg.audio_frame_ptime =
				m_config.sound_subsystem.ptime;
		m_media_cfg.enable_turn = PJ_FALSE;
		m_media_cfg.enable_ice = PJ_FALSE;
		m_media_cfg.quality = 4;
		m_media_cfg.no_vad = PJ_TRUE;

		ASSERT_PJ(pjsua_init(&m_pjsua_cfg, &m_log_cfg, &m_media_cfg));

		ASSERT_PJ(pjmedia_aud_subsys_init(m_global->get_pool_factory()));
		m_init_stage = 2;

		dev_count = pjmedia_aud_dev_count();
		PJ_LOG_(INFO_LEVEL, (THIS_FILE,"Got %d audio devices", dev_count));

		for (dev_idx = 0; dev_idx < dev_count; dev_idx++)
		{
			pjmedia_aud_dev_info info;

			status = pjmedia_aud_dev_get_info(dev_idx, &info);
			if (status == PJ_SUCCESS)
			{
				PJ_LOG_( INFO_LEVEL,
						(THIS_FILE,"%d. %s(%s) (in=%d, out=%d), caps=%d", dev_idx, info.name,info.driver, info.input_count, info.output_count,info.caps));
			}
		}

		pj_uint32_t frame_length = m_config.sound_subsystem.ptime;

		PJ_LOG_(INFO_LEVEL, (__FILE__,"Starting snd port"));
		pj_bzero(&params, sizeof(params));
		pjmedia_aud_dev_default_param(m_config.sound.play_idx, &params);
		params.dir = PJMEDIA_DIR_PLAYBACK;
		params.rec_id = m_config.sound.rec_idx;
		params.play_id = m_config.sound.play_idx;
		params.clock_rate = m_config.sound.play_clock_rate;
		params.channel_count = 1;
		params.samples_per_frame = frame_length * m_config.sound.play_clock_rate
				/ 1000;
		params.bits_per_sample = 16;
		params.flags = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY
				| PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
		params.input_latency_ms = 60;
		params.output_latency_ms = 60;
		status = pjmedia_aud_stream_create(&params, &null_rec_cb_s, &play_cb_s,
				this, &m_play_stream);

		if (status == PJ_SUCCESS)
		{
			pjmedia_aud_dev_default_param(m_config.sound.rec_idx, &params);
			params.dir = PJMEDIA_DIR_CAPTURE;
			params.rec_id = m_config.sound.rec_idx;
			params.play_id = m_config.sound.play_idx;
			params.clock_rate = m_config.sound.rec_clock_rate;
			params.channel_count = 1;
			params.samples_per_frame = frame_length * m_config.sound.rec_clock_rate
					/ 1000;
			params.bits_per_sample = 16;
			params.flags = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY
					| PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
			params.input_latency_ms = 60;
			params.output_latency_ms = 60;

			status = pjmedia_aud_stream_create(&params, &rec_cb_s, &null_play_cb_s,
					this, &m_rec_stream);
			if (status == PJ_SUCCESS)
			{
				pjmedia_port* master_port = pjsua_set_no_snd_dev();
				if (master_port)
				{

					m_master_port = new pjs_master_port(*m_global, m_rec_stream,
							m_play_stream, master_port, master_port,
							m_config.sound_subsystem.rec_gain_target,
							m_config.sound_subsystem.rec_gain_max,
							m_config.sound_subsystem.rec_gain_min,
							m_config.sound_subsystem.play_gain_target,
							m_config.sound_subsystem.play_gain_max,
							m_config.sound_subsystem.play_gain_min,
							m_config.sound.echo_tail_ms, m_config.sound.latency_ms,
							m_config.sound.disable_echo_canceller);
					m_master_port->set_rx_mute(true);
					m_master_port->set_tx_mute(true);
					usleep(1000000);
					status = pjmedia_aud_stream_start(m_play_stream);
					if (status == PJ_SUCCESS)
					{
						status = pjmedia_aud_stream_start(m_rec_stream);
						if (status == PJ_SUCCESS)
						{
							pj_get_timestamp(&m_last_check_scripts);
							m_timer = (pj_timer_entry *) calloc(1, sizeof(*m_timer));
							pj_timer_entry_init(m_timer, 0, this, &timer_callback_s);
							schedule_timer();

							// Init UDP transport
							pjsua_acc_id aid;
							pjsip_transport_type_e type = PJSIP_TRANSPORT_UDP;

							ASSERT_PJ(
									pjsua_transport_create(type, &m_udp_cfg, &m_udp_transport_id));

							/* Add local account */
							if (m_config.sip_accounts.count == 0)
							{
								pjsua_acc_add_local(m_udp_transport_id, PJ_FALSE, &aid);
								pjsua_acc_set_online_status(aid, PJ_TRUE);
							}

							m_acc_cnt = m_config.sip_accounts.count;

							/* Add accounts */
							for (i = 0; i < m_config.sip_accounts.count; ++i)
							{
								pj_char_t id[MAX_CFG_STRING * 2 + 16], registrar[MAX_CFG_STRING
										+ 32];
								pjsua_acc_config acc_cfg;
								memset(&acc_cfg, 0, sizeof(acc_cfg));
								pjsua_acc_config_default(&acc_cfg);

								snprintf(id, sizeof id, "sip:%s@%s",
										m_config.sip_accounts.accounts[i].phone,
										m_config.sip_accounts.accounts[i].server);
								snprintf(registrar, sizeof registrar, "sip:%s:%d",
										m_config.sip_accounts.accounts[i].server,
										m_config.sip_accounts.accounts[i].port);

								acc_cfg.id = pj_str(id);
								acc_cfg.reg_uri = pj_str(registrar);
								acc_cfg.cred_count = 1;
								acc_cfg.cred_info[0].scheme = pj_str("Digest");
								acc_cfg.cred_info[0].realm = pj_str("*");
								acc_cfg.cred_info[0].username = pj_str(
										m_config.sip_accounts.accounts[i].user);
								acc_cfg.cred_info[0].data_type = 0;
								acc_cfg.cred_info[0].data = pj_str(
										m_config.sip_accounts.accounts[i].password);

								acc_cfg.reg_retry_interval = 300;
								acc_cfg.reg_first_retry_interval = 60;

								if (m_config.sip_accounts.accounts[i].is_local_network)
								{
									acc_cfg.allow_contact_rewrite = 0;
								}
								else
								{
									acc_cfg.allow_contact_rewrite = 1;
								}

								status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &m_acc_id[i]);
								if (status != PJ_SUCCESS)
								{
									m_acc_id[i] = -1;
									PJ_LOG_(ERROR_LEVEL,
											(__FILE__,"pjsua_acc_add error %d",status));
								}
								else
								{
									pjsua_acc_set_online_status(m_acc_id[i], PJ_TRUE);
									if (m_config.sip_accounts.accounts[i].register_account)
									{
										pjsua_acc_set_registration(m_acc_id[i], PJ_TRUE);
									}
								}
							}
							ASSERT_PJ( pjsua_media_transports_create(&m_rtp_cfg));

							ASSERT_PJ(pjsua_start());
							m_init_stage = 3;

							m_master_port->set_rx_mute(false);
							m_master_port->set_tx_mute(false);

							switch (m_config.controller.type)
							{
								case PJS_ECT_ARDUINO:
								{
									m_controller = new pjs_arduino_controller(*m_global,
											*(pjs_external_controller_callback *) this);
									break;
								}
								case PJS_ECT_EMULATOR:
								{
									m_controller = new pjs_emulator_controller(*m_global,
											*(pjs_external_controller_callback *) this);
									break;
								}

							}

							m_script_exit = false;

							m_master_vr = new pjs_vr(*m_global, m_config.system.sounds_path,
									m_media_cfg.clock_rate,
									m_media_cfg.ptime * m_media_cfg.clock_rate / 1000,
									m_script_exit, m_script_event, this, NULL, -1);
							if (pjsua_conf_add_port(m_pool, m_master_vr->get_mediaport(),
									&m_master_vr_id) == PJ_SUCCESS)
							{
								pjsua_conf_connect(m_master_vr_id, 0);
							}
							else
								m_master_vr_id = 0;

							m_back_vr = new pjs_vr(*m_global, m_config.system.sounds_path,
									m_media_cfg.clock_rate,
									m_media_cfg.ptime * m_media_cfg.clock_rate / 1000,
									m_script_exit, m_script_event, this, NULL, -1);
							if (pjsua_conf_add_port(m_pool, m_back_vr->get_mediaport(),
									&m_back_vr_id) == PJ_SUCCESS)
							{
								pjsua_conf_connect(m_back_vr_id, 0);
							}
							else
								m_back_vr_id = 0;

							status = pj_thread_create(m_pool, "mscript 0x%x",
									(pj_thread_proc*) &s_script_thread_proc, this,
									PJ_THREAD_DEFAULT_STACK_SIZE, 0, &m_script_thread);
							if (status == PJ_SUCCESS)
							{
								result = RC_OK;
							}

						}
						else
						{
							pjmedia_aud_stream_stop(m_play_stream);
							PJ_LOG_( ERROR_LEVEL,
									(__FILE__,"pjmedia_aud_stream_start play error %d",status));
						}
					}
					else
					{
						PJ_LOG_( ERROR_LEVEL,
								(__FILE__,"pjmedia_aud_stream_start rec error %d",status));
					}
				}
			}
			else
				PJ_LOG_( ERROR_LEVEL,
						(__FILE__,"pjmedia_snd_port_create record error %d",status));
		}
		else
			PJ_LOG_( ERROR_LEVEL,
					(__FILE__,"pjmedia_snd_port_create play error %d",status));

	}
	if (result != RC_OK)
		clean_system();
	else
		m_started = true;
	return result;

	on_error: clean_system();
	return RC_ERROR;

}

void pjs_intercom_system::post_script_quit_message()
{
	PJ_LOG_(INFO_LEVEL, (__FILE__,"Post quit message to script"));
	pjs_intercom_message_t m;
	clear_intercom_message(m);
	m.message = IM_QUIT;
	m_messages_queue.push(m, true);
}

void pjs_intercom_system::terminate_script()
{
	m_script_mutex.lock();
	if (m_script)
	{
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Terminate script"));
		m_script->terminate();
	}
	m_script_mutex.unlock();
}

bool pjs_intercom_system::call_is_active(int callid)
{
	return pjsua_call_is_active(callid);
}

unsigned pjs_intercom_system::hangup_calls_by_tag(pj_int32_t tag, unsigned code)
{
	unsigned result = 0;
	pjsua_call_id cids[PJSUA_MAX_CALLS];
	m_scripts_mutex.lock();
	for (pjs_intercom_calls_map_itr citr = m_calls.begin();
			citr != m_calls.end(); citr++)
	{
		pjs_intercom_call *call = SAFE_CAST_SCRIPT_TO_CALL(get_script(citr->second));
		if (call)
		{
			if (call->is_active() && (call->get_tag() == tag))
			{
				cids[result++] = call->get_callid();
			}
		}
	}
	m_scripts_mutex.unlock();
	for (unsigned i = 0; i < result; i++)
	{
		pjsua_call_hangup(cids[i], code, NULL, NULL);
	}

	return result;
}

pjs_script_id_t pjs_intercom_system::create_thread(const char *script,const char *proc, const char *user_options, pj_int32_t tag)
{
	pjs_script_id_t result = 0;
	if (script && proc && is_started())
	{
		pjs_script_data_t sdata;
		sdata.tag = tag;
		sdata.user_options = user_options;
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Create thread %s:%s",script,proc));

		m_scripts_mutex.lock();
		pjs_intercom_script *iscript = new pjs_intercom_script_final(++m_scripts_id,
				(pjs_intercom_script_interface*)this, *m_global, &m_lua_global, sdata, script, proc);
		m_scripts[m_scripts_id] = iscript;
		m_scripts_mutex.unlock();
		result = m_scripts_id;
	}
	return result;
}

int pjs_intercom_system::make_call(int account_idx, const char* uri,
		pj_uint32_t timeout, const char *user_options, pj_int32_t tag,
		bool is_raw_uri)
{
	pjsua_call_id result = -1;
	if ((account_idx >= 0) && ((unsigned) account_idx < m_acc_cnt))
	{
		char buffs[128];
		pj_str_t suri;
		pjs_call_data_t ci;
		pjs_script_data_t script_data;
		clear_pjs_call_data(ci);

		ci.is_inbound = false;
		ci.callback = this;
		script_data.user_options = user_options;
		script_data.tag = tag;
		ci.timeout = timeout;

		if (is_raw_uri)
			suri = pj_str((char*) uri);
		else
		{
			snprintf(buffs, sizeof(buffs), "sip:%s@%s", uri,
					m_config.sip_accounts.accounts[account_idx].server);
			suri = pj_str((char*) buffs);
		}
		PJ_LOG_(INFO_LEVEL,
				(__FILE__,"Make call to %d:%s",account_idx,suri.ptr));

		pj_status_t status = pjsua_call_make_call(m_acc_id[account_idx], &suri, 0,
				NULL, NULL, &ci.callid);
		if (status == PJ_SUCCESS)
		{
			if (create_call(script_data, ci) != PJ_SUCCESS)
			{
				pjsua_call_hangup(ci.callid, 200, NULL, NULL);
			}
			else
			{
				result = ci.callid;
			}
		}
	}
	return result;
}

void pjs_intercom_system::on_connected_to_main_changed(pjsua_call_id callid)
{
	PJ_LOG_(INFO_LEVEL, (__FILE__,"Connected to main is changed"));
	pjs_intercom_message_t msg;
	clear_intercom_message(msg);
	msg.message = IM_CONNECTED_CHANGED;
	m_messages_queue.push(msg, false);
}

void pjs_intercom_system::on_async_completed(pjs_vr *sender, pj_uint8_t state,
		pjs_vr_async_param_t param1, pjs_vr_async_param_t param2)
{
	PJ_LOG_(INFO_LEVEL,
			(__FILE__,"Async VR operation completed %d,%d,%d",(int)state,(int)param1,(int)param2));
	pjs_intercom_message_t msg;
	clear_intercom_message(msg);
	switch (state)
	{
		case VRS_PLAY_FILE:
		{
			msg.message = IM_ASYNC_PLAY_COMPLETED;
			break;
		}
		default:
		{
			msg.message = IM_ASYNC_COMPLETED;
			break;
		}
	};
	msg.param1 = param1;
	msg.param2 = param2;
	m_messages_queue.push(msg, false);
}

void pjs_intercom_system::clean_system()
{
	m_started = false;

	if (m_init_stage >= 3)
	{
		pjsua_call_hangup_all();
	}

	PJ_LOG_(INFO_LEVEL, (__FILE__,"Stoping all scripts"));
	pjs_intercom_scripts_erase_list scripts_to_erase;
	m_scripts_mutex.lock();
	m_calls.clear();
	for (pjs_intercom_scripts_map_itr citr = m_scripts.begin();
			citr != m_scripts.end(); citr++)
	{
		scripts_to_erase.push_back(citr->second);
		citr->second->stop();
	}
	m_scripts_mutex.unlock();

	m_scripts_mutex.lock();
	m_scripts.clear();
	m_scripts_mutex.unlock();

	PJ_LOG_(INFO_LEVEL, (__FILE__,"Delete all scripts"));
	for (pjs_intercom_scripts_erase_list_itr itr = scripts_to_erase.begin();
			itr != scripts_to_erase.end(); itr++)
	{
		delete (*itr);
	}
	scripts_to_erase.clear();

	PJ_LOG_(INFO_LEVEL, (__FILE__,"All calls scripts deleted"));

	if (m_script_thread)
	{
		pj_timestamp st;
		pj_timestamp now;

		PJ_LOG_(INFO_LEVEL, (__FILE__,"Stop script thread"));
		m_script_exit = true;
		m_script_event.signal();
		post_script_quit_message();
		if (m_back_vr)
			m_back_vr->stop();
		if (m_master_vr)
			m_master_vr->stop();

		pj_get_timestamp(&st);
		now = st;
		while (m_script_exit && pj_elapsed_msec(&st, &now) < 5000)
		{
			pj_get_timestamp(&now);
			usleep(10000);
		}
		if (m_script_exit)
			terminate_script();
		if (m_back_vr)
			m_back_vr->stop();
		if (m_master_vr)
			m_master_vr->stop();
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Join script thread"));
		pj_thread_join(m_script_thread);
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Destroy script thread"));
		pj_thread_destroy(m_script_thread);
		m_script_thread = NULL;
	}

	if (m_back_vr)
	{
		if (m_back_vr_id)
			pjsua_conf_remove_port(m_back_vr_id);
		delete m_back_vr;
		m_back_vr_id = 0;
		m_back_vr = NULL;
	}

	if (m_master_vr)
	{
		if (m_master_vr_id)
			pjsua_conf_remove_port(m_master_vr_id);
		delete m_master_vr;
		m_master_vr_id = 0;
		m_master_vr = NULL;
	}

	if (m_controller)
	{
		delete m_controller;
		m_controller = NULL;
	}
	if (m_play_stream)
	{
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Stoping play audio stream"));
		pjmedia_aud_stream_stop(m_play_stream);
		pjmedia_aud_stream_destroy(m_play_stream);
		m_play_stream = NULL;
	}

	if (m_rec_stream)
	{
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Stoping record audio stream"));
		pjmedia_aud_stream_stop(m_rec_stream);
		pjmedia_aud_stream_destroy(m_rec_stream);
		m_rec_stream = NULL;
	}

	if (m_master_port)
	{
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Destroy master port"));
		delete m_master_port;
		m_master_port = NULL;
	}

	if (m_lock)
	{
		delete m_lock;
		m_lock = NULL;
	}
	if (m_pool)
	{
		pj_pool_release(m_pool);
		m_pool = NULL;
	}
	if (m_global)
	{
		delete m_global;
		m_global = NULL;
	}

	if (m_init_stage >= 2)
		pjmedia_aud_subsys_shutdown();

	if (m_init_stage >= 1)
		pjsua_destroy();
	m_init_stage = 0;
}

void pjs_intercom_system::stop()
{
	if (is_started())
	{
		m_started = false;
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Stopping PJSIS"));
		clean_system();
	}
}

void* pjs_intercom_system::script_thread_proc()
{
	PJ_LOG_(DEBUG_LEVEL, (__FILE__,"script_thread_proc start"));
	m_script_mutex.lock();
	m_script = NULL;
	m_script_mutex.unlock();
	while (!m_script_exit)
	{
		pjs_lua *script;

		m_script_mutex.lock();
		m_script = script = new pjs_lua();
		m_script_mutex.unlock();

		if (m_script)
		{
			char main_script[256];
			snprintf(main_script, sizeof(main_script), "%s/%s",
					m_config.script.scripts_path, m_config.script.main_script);
			m_script->set_path(m_config.script.scripts_path);
			if (m_script->load_script(main_script) == 0)
			{
				pjs_lua_utils *lua_utils = new pjs_lua_utils(m_script_exit,
						m_script_event);
				add_pjs_lua_global(&m_lua_global, m_script->get_state(),
						PJS_LUA_GLOBAL);
				set_global_to_lua(m_script, m_global);
				m_script->add_interface("utils", lua_utils, lua_utils, true);
				m_script->add_interface("msg_queue", &m_messages_queue,
						&m_messages_queue, false);
				m_script->add_interface("pjsis", (pjs_intercom_script_interface*)this,
						(pjs_intercom_script_interface*)this, false);
				int Error = m_script->run();
				if (!Error)
				{
					lua_getglobal(m_script->get_state(), "intercom");
					if (lua_isfunction(m_script->get_state(),-1))
					{
						m_script->push_swig_object((pjs_intercom_script_interface*) this,
								((pjs_intercom_script_interface *)this)->class_name());
						m_script->push_swig_object(m_master_vr, "pjs_vr *");
						m_script->push_swig_object(m_back_vr, "pjs_vr *");

						Error = lua_pcall(m_script->get_state(),3,LUA_MULTRET,0);
						if (Error)
						{
							PJ_LOG_(ERROR_LEVEL,
									(__FILE__,"LUA script error: %s",lua_tostring(m_script->get_state(),-1)));
							lua_pop(m_script->get_state(), 1);
						}
					}
					else
					{
						PJ_LOG_(ERROR_LEVEL, (__FILE__,"Can't find main function"));
					}
				}
				else
				{
					PJ_LOG_(ERROR_LEVEL,
							(__FILE__,"LUA run script error: %s",lua_tostring(m_script->get_state(),-1)));
					lua_pop(m_script->get_state(), 1);
				}
			}
			else
			{
				PJ_LOG_(ERROR_LEVEL,
						(__FILE__,"Can't load script file: %s. Error %s",m_config.script.main_script,lua_tostring(m_script->get_state(),-1)));
			}
		}
		else
		{
			PJ_LOG_(ERROR_LEVEL, (__FILE__,"Script is NULL"));
		}

		m_script_mutex.lock();
		m_script = NULL;
		m_script_mutex.unlock();
		delete script;

		if (m_master_vr)
			m_master_vr->stop();
		if (m_back_vr)
			m_back_vr->stop();

		if (!m_script_exit)
			m_script_event.wait(1000);
	}
	PJ_LOG_(DEBUG_LEVEL, (__FILE__,"script_thread_proc stop"));
	m_script_mutex.lock();
	m_script = NULL;
	m_script_mutex.unlock();
	m_script_exit = false;
	return NULL;
}

pj_uint32_t pjs_intercom_system::internal_get_connected_to_main_count()
{
	if (is_started())
	{
		pj_uint32_t result = 0;
		for (pjs_intercom_calls_map_itr citr = m_calls.begin();
				citr != m_calls.end(); citr++)
		{
			pjs_intercom_call *call = SAFE_CAST_SCRIPT_TO_CALL(get_script(citr->second));
			if (call && call->is_connected_to_main())
				result++;
		}
		return result;
	}
	else
		return 0;
}

pj_uint32_t pjs_intercom_system::get_connected_to_main_count()
{
	pj_uint32_t result;
	m_scripts_mutex.lock();
	result = internal_get_connected_to_main_count();
	m_scripts_mutex.unlock();
	return result;
}

pjs_intercom_script *pjs_intercom_system::get_script(pjs_script_id_t script_id)
{
	pjs_intercom_scripts_map_itr itr = m_scripts.find(script_id);
	if (itr != m_scripts.end())
		return itr->second;
	else
		return NULL;
}

pjs_intercom_call *pjs_intercom_system::get_call(pjsua_call_id call_id)
{
	pjs_intercom_calls_map_itr itr = m_calls.find(call_id);
	if (itr != m_calls.end())
	{
		pjs_intercom_script *script = get_script(itr->second);
		if(script)
			return static_cast<pjs_intercom_call *>(script);
		else
		{
			m_calls.erase(call_id);
			return NULL;
		}
	}
	else
		return NULL;
}

pj_status_t pjs_intercom_system::create_call(pjs_script_data_t &script_data,pjs_call_data_t &call_info)
{
	if (is_started())
	{
		pjs_intercom_call *old_call = NULL;
		PJ_LOG_(INFO_LEVEL,
				(__FILE__,"create call %d, local: %s, inbound: %s ",(int)call_info.callid,call_info.is_local?"true":"false",call_info.is_inbound?"true":"false"));
		m_scripts_mutex.lock();
		pjs_intercom_call *call = new pjs_intercom_call(++m_scripts_id,(pjs_intercom_script_interface*)this,*m_global, &m_lua_global,
				script_data,call_info, m_config.script.call_script);
		m_scripts[m_scripts_id] = call;
		if (m_calls.find(call_info.callid) != m_calls.end())
		{
			PJ_LOG_(ERROR_LEVEL,
					(__FILE__,"Call %d already exist",(int)call_info.callid));
			old_call = SAFE_CAST_SCRIPT_TO_CALL(get_script(m_calls[call_info.callid]));
			if(old_call)
				old_call->stop();
		}
		m_calls[call_info.callid] = m_scripts_id;
		m_scripts_mutex.unlock();
		return PJ_SUCCESS;
	}
	else
		return -1;
}

void pjs_intercom_system::on_call_transfer_request(pjsua_call_id call_id,
		const pj_str_t *dst, pjsip_status_code *code)
{
	PJ_LOG_(INFO_LEVEL, (__FILE__,"REJECT xfer for call %d",(int)call_id));
	*code = PJSIP_SC_NOT_ACCEPTABLE;
}

void pjs_intercom_system::on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
	pjsua_call_info call_info;

	PJ_UNUSED_ARG(e);

	pjsua_call_get_info(call_id, &call_info);

	if (call_info.state == PJSIP_INV_STATE_DISCONNECTED)
	{
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Disconnected call %d",(int)call_id));
		m_scripts_mutex.lock();
		pjs_intercom_call *call = get_call(call_id);
		if (call)
			call->stop();
		m_calls.erase(call_id);
		m_scripts_mutex.unlock();
		on_connected_to_main_changed(call_id);
	}
}

void pjs_intercom_system::on_dtmf_digit(pjsua_call_id call_id, int digit)
{
	m_scripts_mutex.lock();
	pjs_intercom_call *call = get_call(call_id);
	if (call)
		call->on_dtmf_digit(digit);
	m_scripts_mutex.unlock();
}

void pjs_intercom_system::on_incoming_call(pjsua_acc_id acc_id,
		pjsua_call_id call_id, pjsip_rx_data *rdata)
{
	bool b = false;
	pjs_call_data_t ci;
	pjs_script_data_t script_data;
	clear_pjs_call_data(ci);

	script_data.tag = 0;
	ci.callid = call_id;
	ci.is_inbound = true;
	ci.callback = this;
	for (unsigned i = 0; i < m_acc_cnt; i++)
	{
		if (m_acc_id[i] == acc_id)
		{
			ci.is_local = m_config.sip_accounts.accounts[i].is_local;
			b = true;
			break;
		}
	}
	if (b && (create_call(script_data, ci) != PJ_SUCCESS))
	{
		pjsua_call_hangup(call_id, 503, NULL, NULL);
	}
}

void pjs_intercom_system::on_call_media_state(pjsua_call_id call_id)
{
	pjsua_call_info call_info;

	pjsua_call_get_info(call_id, &call_info);

	/* Connect ports appropriately when media status is ACTIVE or REMOTE HOLD,
	 * otherwise we should NOT connect the ports.
	 */
	if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE
			|| call_info.media_status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
	{
		/*
		 pjsua_call_id call_ids[PJSUA_MAX_CALLS];
		 unsigned call_cnt = PJ_ARRAY_SIZE(call_ids);
		 unsigned i;


		 pjsua_enum_calls(call_ids, &call_cnt);

		 for (i = 0; i < call_cnt; ++i) {
		 if (call_ids[i] == call_id)
		 continue;

		 if (!pjsua_call_has_media(call_ids[i]))
		 continue;

		 pjsua_conf_connect(call_info.conf_slot,
		 pjsua_call_get_conf_port(call_ids[i]));
		 pjsua_conf_connect(pjsua_call_get_conf_port(call_ids[i]),
		 call_info.conf_slot);
		 }

		 pjsua_conf_connect(call_info.conf_slot, 0);
		 pjsua_conf_connect(0, call_info.conf_slot);
		 */
		m_scripts_mutex.lock();
		pjs_intercom_call *call = get_call(call_id);
		if (call)
			call->on_media_connected();
		m_scripts_mutex.unlock();
	}

	/* Handle media status */
	switch (call_info.media_status)
	{
		case PJSUA_CALL_MEDIA_ACTIVE:
			PJ_LOG(3, (THIS_FILE, "Media for call %d is active", call_id));
			break;

		case PJSUA_CALL_MEDIA_LOCAL_HOLD:
			PJ_LOG(3,
					(THIS_FILE, "Media for call %d is suspended (hold) by local", call_id));
			break;

		case PJSUA_CALL_MEDIA_REMOTE_HOLD:
			PJ_LOG(3,
					(THIS_FILE, "Media for call %d is suspended (hold) by remote", call_id));
			break;

		case PJSUA_CALL_MEDIA_ERROR:
			PJ_LOG(3, (THIS_FILE, "Media has reported error, disconnecting call"));
			{
				pj_str_t reason = pj_str("ICE negotiation failed");
				pjsua_call_hangup(call_id, 500, &reason, NULL);
			}
			break;

		case PJSUA_CALL_MEDIA_NONE:
			PJ_LOG(3, (THIS_FILE, "Media for call %d is inactive", call_id));
			break;

		default:
			break;
	}
}

//========================================================
pjs_master_port::pjs_master_port(pjs_global &global,
		pjmedia_aud_stream *tx_stream, pjmedia_aud_stream *rx_stream,
		pjmedia_port* tx_up_port, pjmedia_port* rx_up_port,
		pj_uint16_t tx_gain_target, pj_uint16_t tx_gain_max,
		pj_uint16_t tx_gain_min, pj_uint16_t rx_gain_target,
		pj_uint16_t rx_gain_max, pj_uint16_t rx_gain_min, pj_uint16_t tail_ms,
		pj_uint16_t latency_ms, bool disable_echo_canceller) :
		m_global(global)
{
	pj_status_t status;
	m_lock = NULL;
	m_rx_mute = m_tx_mute = false;
	m_rx_up_port = rx_up_port;
	m_tx_up_port = tx_up_port;
	m_rx_stream = rx_stream;
	m_tx_stream = tx_stream;
	m_tx_resample = NULL;
	m_rx_resample = NULL;
	m_ok = PJ_FALSE;
	m_tx_buffer = NULL;
	m_rx_buffer = NULL;
	m_pool = NULL;
	m_echo = NULL;
	m_put_thread = NULL;
	m_put_buffer = NULL;
	m_exit = false;
	m_tx_timestamp.u64 = m_rx_timestamp.u64 = 0;
	m_rx_timestamp_inc = m_rx_up_port->info.samples_per_frame
			/ m_rx_up_port->info.channel_count;
	m_tx_timestamp_inc = m_tx_up_port->info.samples_per_frame
			/ m_tx_up_port->info.channel_count;

	if ((pjmedia_aud_stream_get_param(m_rx_stream, &m_rx_aud_param) == PJ_SUCCESS)
			&& (pjmedia_aud_stream_get_param(m_tx_stream, &m_tx_aud_param)
					== PJ_SUCCESS))
	{

		m_pool = pj_pool_create(m_global.get_pool_factory(), "pjs_master_port", 512,
				512, NULL);
		m_lock = new PPJ_SemaphoreLock(m_pool, NULL, 1, 1);

		if (!disable_echo_canceller)
		{
			status = pjmedia_circ_buf_create(m_pool,
					m_tx_up_port->info.samples_per_frame * 4, &m_put_buffer);
			if (status == PJ_SUCCESS)
			{
				status = pj_thread_create(m_pool, "master put thread",
						(pj_thread_proc*) &s_put_thread_proc, this,
						PJ_THREAD_DEFAULT_STACK_SIZE, 0, &m_put_thread);

				if (status != PJ_SUCCESS)
					m_put_thread = NULL;
			}
			else
				m_put_buffer = NULL;
		}

		m_echo_samples = m_tx_up_port->info.clock_rate * PJS_ECHO_PERIOD_MS / 1000;
		if (!disable_echo_canceller)
			m_echo = new pjs_echo_canceller(m_pool, m_tx_up_port->info.clock_rate,
					m_echo_samples, tail_ms, latency_ms, PJMEDIA_ECHO_USE_SIMPLE_FIFO);
		else
			m_echo = NULL;

		if (m_tx_up_port->info.clock_rate != m_tx_aud_param.clock_rate)
		{
			m_tx_buffer = (pj_int16_t*) pj_pool_alloc(m_pool,
					m_tx_up_port->info.samples_per_frame * 2);
			pj_bool_t high_quality = PJ_FALSE;
			pj_bool_t large_filter = PJ_FALSE;

			status = pjmedia_resample_create(m_pool, high_quality, large_filter,
					m_tx_aud_param.channel_count, m_tx_aud_param.clock_rate,/* Rate in */
					m_tx_up_port->info.clock_rate, /* Rate out */
					m_tx_aud_param.samples_per_frame, &m_tx_resample);
			if (status != PJ_SUCCESS)
				return;
		}

		if (m_rx_up_port->info.clock_rate != m_rx_aud_param.clock_rate)
		{
			m_rx_buffer = (pj_int16_t*) pj_pool_alloc(m_pool,
					m_rx_up_port->info.samples_per_frame * 2);
			pj_bool_t high_quality = PJ_FALSE;
			pj_bool_t large_filter = PJ_FALSE;

			status = pjmedia_resample_create(m_pool, high_quality, large_filter,
					m_rx_up_port->info.channel_count, m_rx_up_port->info.clock_rate,/* Rate in */
					m_rx_aud_param.clock_rate, /* Rate out */
					m_rx_up_port->info.samples_per_frame, &m_rx_resample);
			if (status != PJ_SUCCESS)
				return;
		}

		m_tx_power = m_rx_power = 0;
		m_tx_preprocessor = new pjs_agc("tx", tx_gain_max, tx_gain_min,
				tx_gain_target / 2, tx_gain_target);
		m_rx_preprocessor = new pjs_agc("rx", rx_gain_max, rx_gain_min,
				rx_gain_target / 2, rx_gain_target);
		m_ok = PJ_TRUE;
	}
}

#define echo_process_p(f,s) {pj_int16_t *b=(pj_int16_t *)f;pj_uint16_t size=s; for(;size>=m_echo_samples;size-=m_echo_samples,b+=m_echo_samples) m_echo->playback((pj_int16_t*) b, m_echo_samples);}
#define echo_process_c(f,s) {pj_int16_t *b=(pj_int16_t *)f;pj_uint16_t size=s; for(;size>=m_echo_samples;size-=m_echo_samples,b+=m_echo_samples) m_echo->capture((pj_int16_t*) b, m_echo_samples);}

pj_status_t pjs_master_port::get_frame(pjmedia_frame *frame)
{
#ifdef DEBUG_MASTER_PORT
	PJ_LOG_(ERROR_LEVEL,(__FILE__,"get_frame"));
#endif
	if (m_ok)
	{
		pj_status_t status = PJ_SUCCESS;
		pjmedia_frame up_frame = *frame;
		up_frame.timestamp = m_rx_timestamp;
		m_rx_timestamp.u64 += m_rx_timestamp_inc;

		if (m_rx_resample)
		{
			if (m_rx_buffer && (frame->size == m_rx_aud_param.samples_per_frame * 2))
			{
				up_frame.buf = m_rx_buffer;
				up_frame.size = m_rx_up_port->info.samples_per_frame * 2;
				status = pjmedia_port_get_frame(m_rx_up_port, &up_frame);
				if (status == PJ_SUCCESS)
				{
					frame->type = up_frame.type;
					if (m_rx_mute || (frame->type != PJMEDIA_FRAME_TYPE_AUDIO))
					{
						pj_bzero(up_frame.buf, up_frame.size);
						if (m_echo)
							echo_process_p(up_frame.buf, up_frame.size >> 1);
						pj_bzero(frame->buf, frame->size);
					}
					else if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO)
					{
						if (up_frame.size == m_rx_up_port->info.samples_per_frame * 2)
						{
							if (m_rx_preprocessor)
							{
								m_rx_power = m_rx_preprocessor->process(
										(pj_int16_t*) m_rx_buffer,
										m_rx_up_port->info.samples_per_frame, m_tx_power, NULL);
							}
							if (m_echo)
								echo_process_p(up_frame.buf, up_frame.size >> 1);
							pjmedia_resample_run(m_rx_resample, m_rx_buffer,
									(pj_int16_t*) frame->buf);

						}
						else
						{
							status = PJ_EUNKNOWN;
						}
					}
					else
					{
						if (m_echo)
							echo_process_p((pj_int16_t*) up_frame.buf, up_frame.size >> 1);
					}

				}
			}
			else
				status = PJ_EUNKNOWN;

		}
		else
		{
			status = pjmedia_port_get_frame(m_rx_up_port, &up_frame);
			if (status == PJ_SUCCESS)
			{
				frame->type = up_frame.type;
				if (m_rx_mute || (frame->type != PJMEDIA_FRAME_TYPE_AUDIO))
				{
					pj_bzero(up_frame.buf, up_frame.size);
				}
				else if (m_rx_preprocessor)
				{
					m_rx_power = m_rx_preprocessor->process((pj_int16_t*) up_frame.buf,
							m_rx_up_port->info.samples_per_frame, m_tx_power, NULL);
				}
				if (m_echo)
					echo_process_p((pj_int16_t*) up_frame.buf, up_frame.size>>1);
			}
		}

#ifdef DEBUG_MASTER_PORT
		PJ_LOG_(ERROR_LEVEL,(__FILE__,"end get_frame"));
#endif
		if ((status != PJ_SUCCESS) || (frame->type == PJMEDIA_FRAME_TYPE_NONE))
		{
			pj_bzero(frame->buf, frame->size);
		}
		return status;
	}
	else
	{
		pj_bzero(frame->buf, frame->size);
		return PJ_EUNKNOWN;
	}
}

pj_status_t pjs_master_port::put_frame(const pjmedia_frame *frame)
{
#ifdef DEBUG_MASTER_PORT
	PJ_LOG_(ERROR_LEVEL,(__FILE__,"put_frame"));
#endif
	pj_status_t result = PJ_SUCCESS;

	if (m_tx_mute)
	{
		m_tx_power = 0;
		return PJ_SUCCESS;
	}
	if (m_tx_resample)
	{
		if (m_tx_buffer && (frame->size == m_tx_aud_param.samples_per_frame * 2))
		{
			pjmedia_frame up_frame = *frame;
			up_frame.buf = m_tx_buffer;
			up_frame.size = m_tx_up_port->info.samples_per_frame * 2;
			up_frame.timestamp.u64 = m_tx_timestamp.u64;
			m_tx_timestamp.u64 += m_tx_timestamp_inc;
			pjmedia_resample_run(m_tx_resample, (const pj_int16_t*) frame->buf,
					m_tx_buffer);

			if (m_tx_preprocessor)
			{
				m_tx_power = m_tx_preprocessor->process((pj_int16_t*) up_frame.buf,
						m_tx_up_port->info.samples_per_frame, m_rx_power, NULL, PJ_TRUE);
			}
			if (m_put_thread)
			{
				m_lock->acquire();
				pjmedia_circ_buf_write(m_put_buffer, (pj_int16_t*) up_frame.buf,
						up_frame.size >> 1);
				m_put_event.signal();
				m_lock->release();
				result = PJ_SUCCESS;
			}
			else
			{
				if (m_echo)
					echo_process_c((pj_int16_t*)up_frame.buf, up_frame.size >> 1);

				result = pjmedia_port_put_frame(m_tx_up_port, &up_frame);
			}
			goto fexit;
		}
		else
		{
			result = PJ_EUNKNOWN;
			goto fexit;
		}
	}
	else
	{
		if (m_tx_preprocessor)
		{
			if (frame->size == m_tx_up_port->info.samples_per_frame * 2)
			{
				m_tx_power = m_tx_preprocessor->process((pj_int16_t*) frame->buf,
						m_tx_up_port->info.samples_per_frame, m_rx_power, NULL, PJ_TRUE);
			}
		}
		if (m_put_thread)
		{
			m_lock->acquire();
			pjmedia_circ_buf_write(m_put_buffer, (pj_int16_t*) frame->buf,
					frame->size >> 1);
			m_put_event.signal();
			m_lock->release();
			result = PJ_SUCCESS;
		}
		else
		{
			if (m_echo)
				echo_process_c((pj_int16_t*)frame->buf, frame->size >> 1);
			result = pjmedia_port_put_frame(m_tx_up_port, frame);
		}
	}
	fexit:
#ifdef DEBUG_MASTER_PORT
	PJ_LOG_(ERROR_LEVEL,(__FILE__,"exit put_frame"));
#endif
	return result;
}

void* pjs_master_port::put_thread_proc()
{
	pj_status_t status;
	const pj_uint16_t frame_size = m_tx_up_port->info.samples_per_frame;
	pj_int16_t buffer[frame_size];
	pj_timestamp last_frame_ts;

	last_frame_ts.u64 = 0;

	set_max_relative_thread_priority(-5);
	while (!m_exit)
	{
		m_lock->acquire();
		unsigned size = pjmedia_circ_buf_get_len(m_put_buffer);
		bool b = size >= frame_size;
		if (b)
		{
			status = pjmedia_circ_buf_read(m_put_buffer, buffer, frame_size);
			m_lock->release();
			if (status == PJ_SUCCESS)
			{
				pjmedia_frame up_frame;
				up_frame.buf = buffer;
				up_frame.size = frame_size << 1;
				up_frame.timestamp.u64 = last_frame_ts.u64;
#ifdef DEBUG_MASTER_PORT
				PJ_LOG_(ERROR_LEVEL,(__FILE__,"real put_frame"));
#endif

				if (m_echo)
					echo_process_c((pj_int16_t*)buffer, frame_size);
				pjmedia_port_put_frame(m_tx_up_port, &up_frame);
				last_frame_ts.u64 += frame_size;
#ifdef DEBUG_MASTER_PORT
				PJ_LOG_(ERROR_LEVEL,(__FILE__,"exit real put_frame"));
#endif
			}
		}
		else
			m_lock->release();
		if (!b)
		{
			m_put_event.wait(1000);
		}
	}
	return 0;
}

pjs_master_port::~pjs_master_port()
{
	m_exit = true;
	m_put_event.signal();
	if (m_put_thread)
	{
		pj_thread_join(m_put_thread);
		pj_thread_destroy(m_put_thread);
		m_put_thread = NULL;
	}

	if (m_tx_preprocessor)
		delete m_tx_preprocessor;
	if (m_rx_preprocessor)
		delete m_rx_preprocessor;
	if (m_tx_resample)
		pjmedia_resample_destroy(m_tx_resample);
	if (m_rx_resample)
		pjmedia_resample_destroy(m_rx_resample);
	if (m_echo)
		delete m_echo;
	if (m_lock)
		delete m_lock;
	if (m_pool)
		pj_pool_release(m_pool);
}

void pjs_intercom_call::i_before_terminate()
{
	m_lua_call->set_call_port(0);
}
void pjs_intercom_call::i_after_terminate()
{
	m_call_is_active = false;
}
void pjs_intercom_call::i_on_stop()
{
	if (m_master_vr)
		m_master_vr->stop();
}
void pjs_intercom_call::i_on_thread_end()
{
	if (m_call_is_active)
		pjsua_call_hangup(m_call_info.callid, 503, NULL, NULL);
}

//========================================================
pjs_intercom_call::pjs_intercom_call(pjs_script_id_t sid,pjs_intercom_script_interface *intercom_script_interface,pjs_global &global,
		pjs_lua_global *lua_global, pjs_script_data_t &script_data,pjs_call_data_t &call_info,
		const char *script_name) :
		pjs_intercom_script(sid, intercom_script_interface,global, lua_global, script_data,
				script_name, PJS_LUA_CALL_PROC), m_call_info(call_info)
{
	m_call_is_active = true;
	m_master_vr = NULL;
	m_master_vr_id = 0;
	m_lua_call = NULL;

	m_master_vr = new pjs_vr(m_global, m_global.get_config().system.sounds_path,
			m_global.get_config().sound_subsystem.clock_rate,
			m_global.get_config().sound_subsystem.ptime
					* m_global.get_config().sound_subsystem.clock_rate / 1000,
			m_script_exit, m_script_event, this, NULL, -1);
	if (pjsua_conf_add_port(m_pool, m_master_vr->get_mediaport(),
			&m_master_vr_id)!=PJ_SUCCESS)
		m_master_vr_id = 0;
	m_lua_call = new pjs_call_script_interface(m_script_data, m_call_info, m_call_is_active);
	m_lua_call->set_vr_port(m_master_vr_id);
	pj_status_t status = start_thread();
	if (status != PJ_SUCCESS)
	{
		m_call_is_active = false;
	}
	if (!m_call_is_active)
	{
		clean();
	}
}

void pjs_intercom_call::on_media_connected()
{
	if (m_call_is_active && m_lua_call)
	{
		pjsua_conf_port_id cport = pjsua_call_get_conf_port(m_call_info.callid);
		if (cport != PJSUA_INVALID_ID)
			m_lua_call->set_call_port(cport);
	}
}

pjs_intercom_call::~pjs_intercom_call()
{
	clean();
}

void pjs_intercom_call::i_on_clean()
{
	if (m_master_vr)
	{
		if (m_lua_call)
			m_lua_call->set_vr_port(0);
		if (m_master_vr_id)
			pjsua_conf_remove_port(m_master_vr_id);
		delete m_master_vr;
		m_master_vr_id = 0;
		m_master_vr = NULL;
	}

	if (m_lua_call)
	{
		delete m_lua_call;
		m_lua_call = NULL;
	}
}

int pjs_intercom_call::i_put_params_for_main_proc()
{
	if (m_script)
	{
		m_script->push_swig_object(m_lua_call, m_lua_call->class_name(), false);
		m_script->push_swig_object(m_master_vr, "pjs_vr *");
	}
	return 2;
}

void pjs_intercom_call::on_dtmf_digit(int digit)
{
	if (m_call_is_active)
	{
		if (m_master_vr)
			m_master_vr->on_input((char) digit);
	}
}

void pjs_intercom_call::on_async_completed(pjs_vr *sender, pj_uint8_t state,
		pjs_vr_async_param_t param1, pjs_vr_async_param_t param2)
{
	PJ_LOG_(INFO_LEVEL,
			(__FILE__,"Async VR operation completed %d,%d,%d",(int)state,(int)param1,(int)param2));
	pjs_intercom_message_t msg;
	clear_intercom_message(msg);
	switch (state)
	{
		case VRS_PLAY_FILE:
		{
			msg.message = IM_ASYNC_PLAY_COMPLETED;
			break;
		}
		default:
		{
			msg.message = IM_ASYNC_COMPLETED;
			break;
		}
	};
	msg.param1 = param1;
	msg.param2 = param2;
	m_messages_queue.push(msg, false);
}

//========================================================
pjs_intercom_script::pjs_intercom_script(pj_uint32_t sid, pjs_intercom_script_interface *intercom_script_interface,pjs_global &global,
		pjs_lua_global *lua_global, pjs_script_data_t &script_data,
		const char *script_name, const char *script_proc) :
		m_global(global), m_messages_queue(PJS_MAX_CONTROL_MESSAGES), m_script_name(
				script_name), m_script_proc(script_proc), m_lua_global(lua_global), m_script_data(
				script_data), m_sid(sid),m_intercom_script_interface(intercom_script_interface)
{
	m_script_thread_completed = false;
	m_script_exit = false;
	m_script_thread = NULL;
	m_script = NULL;
	m_pool = pj_pool_create(m_global.get_pool_factory(), "pjsis call", 128, 128,
			NULL);
}

pj_status_t pjs_intercom_script::start_thread()
{
	return pj_thread_create(m_pool, "script 0x%x",
			(pj_thread_proc*) &s_script_thread_proc, this,
			PJ_THREAD_DEFAULT_STACK_SIZE, 0, &m_script_thread);
}

pjs_intercom_script::~pjs_intercom_script()
{
	PJ_LOG_(INFO_LEVEL, (__FILE__,"Script %d destroyed",m_sid));
}

void pjs_intercom_script::post_script_quit_message()
{
	PJ_LOG_(INFO_LEVEL, (__FILE__,"Post quit message to script %d",m_sid));
	pjs_intercom_message_t m;
	clear_intercom_message(m);
	m.message = IM_QUIT;
	m_messages_queue.push(m, true);
}

void pjs_intercom_script::terminate_script()
{
	m_script_mutex.lock();
	if (m_script)
	{
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Terminate script %d",m_sid));
		m_script->terminate();
	}
	m_script_mutex.unlock();
}

void pjs_intercom_script::stop()
{
	if (m_script_thread && !m_script_thread_completed)
	{
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Stop script %d thread",m_sid));
		i_before_terminate();
		terminate_script();
		i_after_terminate();
		m_script_exit = true;
		m_script_event.signal();
		post_script_quit_message();
		i_on_stop();
	}
}

void pjs_intercom_script::clean()
{
	m_script_mutex.lock();
	m_script_mutex.unlock();
	if (m_script_thread)
	{
		stop();
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Join script %d thread",m_sid));
		pj_thread_join(m_script_thread);
		PJ_LOG_(INFO_LEVEL, (__FILE__,"Destroy script %d thread",m_sid));
		pj_thread_destroy(m_script_thread);
		m_script_thread = NULL;
	}

	i_on_clean();

	if (m_pool)
	{
		pj_pool_release(m_pool);
		m_pool = NULL;
	}
}

void* pjs_intercom_script::script_thread_proc()
{
	PJ_LOG_(DEBUG_LEVEL, (__FILE__,"Script thread proc %d starting",m_sid));
	m_script_mutex.lock();
	m_script = NULL;
	m_script_mutex.unlock();
	if(is_ready())
	{
		pjs_lua *script = NULL;

		m_script_mutex.lock();
		m_script = script = new pjs_lua();
		m_script_mutex.unlock();

		if (m_script)
		{
			char main_script[256];
			snprintf(main_script, sizeof(main_script), "%s/%s",
					m_global.get_config().script.scripts_path, m_script_name.c_str());
			m_script->set_path(m_global.get_config().script.scripts_path);
			if (m_script->load_script(main_script) == 0)
			{
				pjs_lua_utils *lua_utils = new pjs_lua_utils(m_script_exit,
						m_script_event);
				add_pjs_lua_global(m_lua_global, m_script->get_state(), PJS_LUA_GLOBAL);
				set_global_to_lua(m_script, &m_global);
				m_script->add_interface("utils", lua_utils, lua_utils, true);
				m_script->add_interface("msg_queue", &m_messages_queue,
						&m_messages_queue, false);
				m_script->add_interface("pjsis", m_intercom_script_interface,
						m_intercom_script_interface, false);
				int Error = m_script->run();
				if (!Error)
				{
					lua_getglobal(m_script->get_state(), m_script_proc.c_str());
					if (lua_isfunction(m_script->get_state(),-1))
					{
						int cnt = i_put_params_for_main_proc();

						Error = lua_pcall(m_script->get_state(),cnt,LUA_MULTRET,0);
						if (Error)
						{
							PJ_LOG_(ERROR_LEVEL,
									(__FILE__,"LUA script error: %s",lua_tostring(m_script->get_state(),-1)));
							lua_pop(m_script->get_state(), 1);
						}
					}
					else
					{
						PJ_LOG_(ERROR_LEVEL, (__FILE__,"Can't find %s function",m_script_proc.c_str()));
					}
				}
				else
				{
					PJ_LOG_(ERROR_LEVEL,
							(__FILE__,"LUA run script error: %s",lua_tostring(m_script->get_state(),-1)));
					lua_pop(m_script->get_state(), 1);
				}
			}
			else
			{
				PJ_LOG_(ERROR_LEVEL,
						(__FILE__,"Can't load script file: %s. Error %s",m_script_name.c_str(),lua_tostring(m_script->get_state(),-1)));
			}
		}
		else
		{
			PJ_LOG_(ERROR_LEVEL, (__FILE__,"Script is NULL"));
		}

		m_script_mutex.lock();
		m_script = NULL;
		m_script_mutex.unlock();
		delete script;
	}
	PJ_LOG_(DEBUG_LEVEL, (__FILE__,"Script thread proc %d ended",m_sid));
	i_on_thread_end();
	m_script_mutex.lock();
	m_script = NULL;
	m_script_mutex.unlock();
	m_script_thread_completed = true;
	return NULL;
}
