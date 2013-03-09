#ifndef PJS_CONFIG_H_
#define PJS_CONFIG_H_

#include "project_common.h"
#include <pjsua.h>


#pragma pack(push)
#pragma pack(1)

SWIG_BEGIN_DECL

#define MAX_FILENAME_PATH	255
#define MAX_CFG_STRING		64
#define	MAX_ACCOUNTS		PJSUA_MAX_ACC

typedef struct
{
	pj_char_t	bind[MAX_FILENAME_PATH+1];
	pj_uint32_t	clock_rate;
	pj_uint8_t	gain_max;
}pjs_audio_monitor_config_t;

typedef struct
{
	pj_uint8_t	level;
	pj_char_t	file_name[MAX_FILENAME_PATH+1];
	pj_uint8_t 	max_files;
	pj_uint32_t	max_file_size; // in Megabytes;
}pjs_log_config_t;

typedef struct
{
	pj_int16_t	play_idx;
	pj_int16_t	rec_idx;
	pj_uint32_t	play_clock_rate;
	pj_uint32_t	rec_clock_rate;
	pj_uint16_t	latency_ms;
	pj_uint16_t	echo_tail_ms;
	bool		disable_echo_canceller;
}pjs_sound_config_t;

typedef struct
{
	pj_uint16_t	ptime;
	pj_uint32_t	clock_rate;
	pj_uint16_t	play_gain_max;
	pj_uint16_t	play_gain_min;
	pj_uint16_t	play_gain_target;
	pj_uint16_t	rec_gain_target;
	pj_uint16_t	rec_gain_max;
	pj_uint16_t	rec_gain_min;
}pjs_sound_subsystem_config_t;


typedef struct
{
	pj_char_t	server[MAX_CFG_STRING];
	pj_uint16_t	port;
	pj_char_t	phone[MAX_CFG_STRING];
	pj_char_t	user[MAX_CFG_STRING];
	pj_char_t	password[MAX_CFG_STRING];
	bool		register_account;
	bool		is_local;
	bool		is_local_network;
}pjs_account_config_t;

typedef struct
{
	pj_uint8_t	count;
	pjs_account_config_t	accounts[MAX_ACCOUNTS];
	pjs_account_config_t 	*get_account(unsigned int idx)
	{
		return &accounts[idx];
	}
}pjs_accounts_config_t;


// External controller types
#define PJS_ECT_ARDUINO	0
#define PJS_ECT_EMULATOR	1

typedef struct
{
	pj_uint8_t	type;
	pj_char_t	port[MAX_CFG_STRING];
	pj_uint32_t	speed;
}pjs_controller_config_t;

typedef struct
{
	pj_char_t	main_script[MAX_CFG_STRING];
	pj_char_t	call_script[MAX_CFG_STRING];
	pj_char_t	scripts_path[MAX_FILENAME_PATH];
}pjs_script_config_t;


typedef struct
{
	pj_char_t	sounds_path[MAX_FILENAME_PATH];
	pj_char_t	db_path[MAX_FILENAME_PATH];
	pj_char_t	temp_sensor[MAX_FILENAME_PATH];
}pjs_system_config_t;

typedef struct
{
	pj_char_t	stun[MAX_CFG_STRING];
}pjs_network_config_t;

typedef struct
{
	pjs_log_config_t	logs;
	pjs_sound_config_t	sound;
	pjs_sound_subsystem_config_t	sound_subsystem;
	pjs_accounts_config_t	sip_accounts;
	pjs_controller_config_t controller;
	pjs_script_config_t		script;
	pjs_system_config_t		system;
	pjs_network_config_t	network;
	pjs_audio_monitor_config_t audio_monitor;
}pjs_config_t;

SWIG_END_DECL

#pragma pack(pop)

void pjs_config_init(pjs_config_t &config);

#endif /* PJS_CONFIG_H_ */
