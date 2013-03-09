function readSettingAsInt(configItem, settings, name)
  if settings:exists(name) then
    configItem[name] = settings:getValueAsInt(name,0)
--    utils:log(1,"Set "..name.." to "..configItem[name])    
  end
end

function readSettingAsBool(configItem, settings, name)
  if settings:exists(name) then
    configItem[name] = settings:getValueAsBool(name,false)
--    utils:log(1,"Set "..name.." to "..tostring(configItem[name]))
  end
end

function readSettingAsString(configItem, settings, name)
  if settings:exists(name) then
    configItem[name] = settings:getValueAsString(name,'')
--    utils:log(1,"Set "..name.." to "..configItem[name])
  end
end


function readASettingAsInt(account, settings, name)
  if settings:exists(name) then
    account[name] = settings:getValueAsInt(name,0)
--    utils:log(1,"Set "..name.." to "..account[name])    
  end
end

function readASettingAsBool(account, settings, name)
  if settings:exists(name) then
    account[name] = settings:getValueAsBool(name,false)
--    utils:log(1,"Set "..name.." to "..tostring(account[name]))
  end
end

function readASettingAsString(account, settings, name)
  if settings:exists(name) then
    account[name] = settings:getValueAsString(name,'')
--    utils:log(1,"Set "..name.." to "..account[name])
  end
end


function config(config)
	local cfg = pjs.Config();
	if cfg:readFile("intercom.cfg")==pjs.LCSR_OK then
	    local root = cfg:getRoot();
	    if root:exists("intercom") then
	        local intercom = root:settingsByKey("intercom");
	    
		    if intercom:exists("sound") then
		      local sound = intercom:settingsByKey("sound");
		      readSettingAsInt(config.sound,sound,"play_idx")
		      readSettingAsInt(config.sound,sound,"play_clock_rate")
		      readSettingAsInt(config.sound,sound,"rec_idx")
		      readSettingAsInt(config.sound,sound,"rec_clock_rate")
		      readSettingAsInt(config.sound,sound,"latency_ms")
		      readSettingAsInt(config.sound,sound,"echo_tail_ms")
		      readSettingAsBool(config.sound,sound,"disable_echo_canceller")
		    end
		    
		    if intercom:exists("sound_subsystem") then
		      local sound_subsystem = intercom:settingsByKey("sound_subsystem");
		      readSettingAsInt(config.sound_subsystem,sound_subsystem,"ptime")
		      readSettingAsInt(config.sound_subsystem,sound_subsystem,"clock_rate")
		      readSettingAsInt(config.sound_subsystem,sound_subsystem,"play_gain_max")
		      readSettingAsInt(config.sound_subsystem,sound_subsystem,"play_gain_min")
		      readSettingAsInt(config.sound_subsystem,sound_subsystem,"play_gain_target")
		      readSettingAsInt(config.sound_subsystem,sound_subsystem,"rec_gain_max")
		      readSettingAsInt(config.sound_subsystem,sound_subsystem,"rec_gain_min")
		      readSettingAsInt(config.sound_subsystem,sound_subsystem,"rec_gain_target")
		    end
		    
		    if intercom:exists("log") then
		      local log = intercom:settingsByKey("log");
		      readSettingAsInt(config.logs,log,"level")
		      readSettingAsInt(config.logs,log,"max_files")
		      readSettingAsInt(config.logs,log,"max_file_size")
		      readSettingAsString(config.logs,log,"file_name")
		    end

		    if intercom:exists("scripts") then
		      local scripts = intercom:settingsByKey("scripts");
		      readSettingAsString(config.script,scripts,"main_script")
		      readSettingAsString(config.script,scripts,"call_script")
		      readSettingAsString(config.script,scripts,"scripts_path")
		    end
		    
		    if intercom:exists("system") then
		      local system = intercom:settingsByKey("system");
		      readSettingAsString(config.system,system,"sounds_path")
		      readSettingAsString(config.system,system,"db_path")
		      readSettingAsString(config.system,system,"temp_sensor")
		    end

		    if intercom:exists("network") then
		      local network = intercom:settingsByKey("network");
		      readSettingAsString(config.network,network,"stun")
		    end

		    if intercom:exists("controller") then
		      local controller = intercom:settingsByKey("controller")
		      local stype = controller:getValueAsString("type","PJS_ECT_EMULATOR")
		      config.controller.type = pjs[stype]
		      readSettingAsInt(config.controller,controller,"speed")
		      readSettingAsString(config.controller,controller,"port")
		    end
		    
		    if intercom:exists("accounts") then
		      local accounts = intercom:settingsByKey("accounts")
		      if accounts:isGroup() then
		      	local count = accounts:getLength()
		      	local i
		      	config.sip_accounts.count = count
		      	for i=0,count-1 do
		      	  local account = config.sip_accounts:get_account(i)
		      	  local caccount = accounts:settingsByIndex(i)
		      	  readASettingAsString(account,caccount,"server")
		      	  readASettingAsString(account,caccount,"phone")
		      	  readASettingAsInt(account,caccount,"port")
		      	  readASettingAsString(account,caccount,"user")
		      	  readASettingAsString(account,caccount,"password")
		      	  readASettingAsBool(account,caccount,"register_account")
		      	  readASettingAsBool(account,caccount,"is_local")
		      	end
		      end
		    end		    
		end
	else
	    return -1;
	end
	return 0;
end 
