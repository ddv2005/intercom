#include "project_config.h"
#include <pjmedia_audiodev.h>

void pjs_config_init(pjs_config_t &config)
{
  memset(&config,0,sizeof(pjs_config_t));
  config.logs.level = DEBUG_LEVEL;
  config.logs.max_file_size = 10;
  config.logs.max_files = 2;

  config.sound.rec_idx = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
  config.sound.rec_clock_rate = 16000;
  config.sound.play_idx = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;
  config.sound.play_clock_rate = 16000;

  config.sound.latency_ms = 20;
  config.sound.echo_tail_ms = 100;


  config.sound_subsystem.ptime = 20;
  config.sound_subsystem.clock_rate = 16000;
  config.sound_subsystem.play_gain_max = 3;
  config.sound_subsystem.play_gain_min = 2;
  config.sound_subsystem.play_gain_target = 30000;
  config.sound_subsystem.rec_gain_max = 2;
  config.sound_subsystem.rec_gain_min = 2;
  config.sound_subsystem.rec_gain_target = 30000;

  config.controller.speed = 9600;

  config.audio_monitor.gain_max = 5;

  strcpy(config.script.main_script,"intercom.lua");
  strcpy(config.script.call_script,"call.lua");
  strcpy(config.script.scripts_path,"./scripts");

#ifdef	__arm__
  strcpy(config.system.temp_sensor,"/sys/class/hwmon/hwmon0/device/temp1_input");
#else
  config.system.temp_sensor[0] = 0;
#endif

  strcpy(config.system.db_path,"./db");
  strcpy(config.system.sounds_path,"./sounds");
}

