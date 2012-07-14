function config(config)
    	config.sound.play_clock_rate = 16000;
	config.sound.play_idx = 0;
	config.sound.rec_clock_rate = 16000;
	config.sound.rec_idx = 0;

	config.sound.latency_ms = 20;
	config.sound.echo_tail_ms = 100;

	config.sound.play_idx = 0;
	config.sound.rec_idx = 0;
	config.sound.latency_ms = 120;
	config.sound.echo_tail_ms = 100;
	config.sound.disable_echo_canceller = false;

	config.sound_subsystem.clock_rate = 16000;
	config.sound_subsystem.ptime = 20;
	
	config.controller.port = "/dev/ttyUSB0";
	config.controller.type = pjs.PJS_ECT_EMULATOR;
	
	config.logs.level = 4;

	config.sip_accounts.count = 2;
	account = config.sip_accounts:get_account(0);
	account.server = "X.X.X.X";
	account.phone = "1";
	account.port = 5060;
	account.user = "1";
	account.password = "pwd";
	account.register_account = true;
	account.is_local = true;

	account = config.sip_accounts:get_account(1);
	account.server = "X.X.X.X";
	account.phone = "2";
	account.port = 5060;
	account.user = "2";
	account.password = "pwd";
	account.register_account = false;
	account.is_local = false;
	
end
