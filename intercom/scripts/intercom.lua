require 'class'
require 'intercom_db'
require 'lua_lib'
local user = require('user')

OUTGOING_CALLS_TAG = 1

function messages_map(t)
	t.dispatch = function (self,obj,msg)
		local f=self[msg.message] or self.default
		if f then
			if type(f)=="function" then
				f(obj,msg)
			else
				error("dispatch "..tostring(msg.message).." not a function")
			end
		end
	end
	return t
end

pjs_intercom = class()

function pjs_intercom:load_options(idb)
	self.opt = {}
	self.opt.calling_timeout = idb_get_num_option(idb,"pjsis.calling_timeout",40000)
	self.opt.bell_time = idb_get_num_option(idb,"pjsis.bell.time",1000)
	self.opt.bell_guard_time = idb_get_num_option(idb,"pjsis.bell.guard_time",5000)
	self.opt.bell_repeat_time = idb_get_num_option(idb,"pjsis.bell.repeat_time",10000)
	self.opt.bell_repeat_count = idb_get_num_option(idb,"pjsis.bell.repeat_count",3)
	self.opt.call_guard_time = idb_get_num_option(idb,"pjsis.call.guard_time",5000)
	self.opt.ultrasonic_trigger = idb_get_num_option(idb,"pjsis.ultrasonic.trigger",300)
	self.opt.ultrasonic_door_close_timeout = idb_get_num_option(idb,"pjsis.ultrasonic.door_close_timeout",10000)
	self.opt.ultrasonic_door_open_delay = idb_get_num_option(idb,"pjsis.ultrasonic.door_open_delay",10000)
	self.opt.ultrasonic_timeout = idb_get_num_option(idb,"pjsis.ultrasonic.timeout",20000)
end;

function pjs_intercom:prepare_calls()
	self.pjsis:hangup_calls_by_tag(OUTGOING_CALLS_TAG,200)
	local now = utils:get_time()
	local idb = idb_open()
	self.outgoing_calls_start = now
	self.outgoing_calls = idb_phones_by_group(idb,idb_get_num_option(idb,"pjsis.current_phones_group",1))
	utils:log(1,"Calls list "..table.tostring(self.outgoing_calls))
	idb_close(idb)
	self:check_calls(now)
end;

function pjs_intercom:check_calls(now)
	if(self.calling==true)and(self.outgoing_calls~=nil) then
		local now = utils:get_time()
		local i
		for i=1,self.outgoing_calls.count do
			if(self.outgoing_calls[i].callid~=nil) then
				if(self.pjsis:call_is_active(self.outgoing_calls[i].callid)~=true) then
					self.outgoing_calls[i].callid = nil
					self.outgoing_calls[i].end_time = now
				end
			end
			if(self.outgoing_calls[i].callid==nil) then
				if(self.outgoing_calls[i].end_time~=nil) then
					if(utils:elapsed_msec(self.outgoing_calls[i].end_time,now)>=self.opt.call_guard_time) then
						self.outgoing_calls[i].end_time=nil
					end
				end
				if(utils:elapsed_msec(self.outgoing_calls_start,now)>=self.outgoing_calls[i].delay) then
					if(self.outgoing_calls[i].end_time==nil) then
						local timeout = 30000
						local options = {}
						options.opt = self.outgoing_calls[i].options
						self.outgoing_calls[i].callid = self.pjsis:make_call(self.outgoing_calls[i].account_idx,self.outgoing_calls[i].number,timeout,pjs.encode(options),OUTGOING_CALLS_TAG,false)
						if(self.outgoing_calls[i].callid<0) then
							self.outgoing_calls[i].callid = nil
							self.outgoing_calls[i].end_time = now
						end
					end
				end
			end
		end
	end
end;

function pjs_intercom:stop_ultrasonic()
	self.ultrasonic_action_time = nil;
end	

function pjs_intercom:start_calling()
	if(self.calling==false) then
		self.calling = true
		self.calling_start_time = utils:get_time()
		if(self.bell_start==nil) then
			self.bell_start = self.calling_start_time
		end
		self.bell_repeat_count = self.opt.bell_repeat_count-1
		self.bell_stop = nil
		self.controller:set_output(pjs.ECO_BELL,1)
		local ccnt = self.pjsis:get_connected_to_main_count()
		if(self:check_connected(ccnt)==false) then
			self:prepare_calls()
		end
		user.on_door_bell()
		return true
	else
		if(self.bell_stop~=nil) then
			local now = utils:get_time()
			if(utils:elapsed_msec(self.bell_stop,now)>=self.opt.bell_guard_time) then
				self.bell_stop = nil
				self.bell_start = now;
				self.bell_repeat_count = self.bell_repeat_count+1
				self.controller:set_output(pjs.ECO_BELL,1)
			end;
		end
	end
	return false
end

function pjs_intercom:on_calling_end()
	self.calling = false
	self.pjsis:hangup_calls_by_tag(OUTGOING_CALLS_TAG,200)
	self:update_leds(self.pjsis:get_connected_to_main_count())
end;

function pjs_intercom:check_connected(ccnt)
	if(self.calling==true)then
		if(ccnt>0) then
			self.bvr:play_file_async("beep.wav",0,0)
			self:on_calling_end()
			return true
		end
		if(self.door_closed==false)then
			self:on_calling_end()
			return true
		end
	end
	return false
end

function pjs_intercom:update_leds(ccnt)
	if(ccnt>0) then
		self.controller:set_output(pjs.ECO_MAIN_LED,1)
		self.controller:set_output(pjs.ECO_SECOND_LED,1)
	else
		if(self.calling==true) then
			self.controller:set_output(pjs.ECO_MAIN_LED,2)
			self.controller:set_output(pjs.ECO_SECOND_LED,2)
		else
			self.controller:set_output(pjs.ECO_MAIN_LED,1)
			self.controller:set_output(pjs.ECO_SECOND_LED,1)
		end
	end
end

function pjs_intercom:on_main_button(msg)
	utils:log(1,"Main button state " .. msg.param1)
	self:stop_ultrasonic()
	if msg.param1~=0 then
		self:start_calling()
		if(self.calling) then
			if(self.vr:is_idle()) then
				self.vr:play_file_async("chime.wav",0,0)
			end
			if(self.bvr:is_idle()) then
				self.bvr:play_file_async("music.wav",0,0)
			end
		end
		self.controller:set_output(pjs.ECO_MAIN_LED,0)
	else
		self:update_leds(self.pjsis:get_connected_to_main_count())
	end
end

function pjs_intercom:stop_bell()
	self.bell_start = nil;
	self.bell_stop = now
	self.controller:set_output(pjs.ECO_BELL,0)
end

function pjs_intercom:on_indoor_button(msg)
	utils:log(1,"Indoor button state " .. msg.param1)
	self:stop_ultrasonic()
	if msg.param1~=0 then
		if self.calling == true then
			utils:log(1,"Stop calling on indoor button")
			self:on_calling_end()
			self.bvr:fade_out(1000)
			self:stop_bell()
		end
		self.controller:set_output(pjs.ECO_SECOND_LED,0)
	else
		self:update_leds(self.pjsis:get_connected_to_main_count())
	end
end

function pjs_intercom:on_door_button(msg)
	utils:log(1,"Door button state " .. msg.param1)
	self:stop_ultrasonic()
	if msg.param1~=0 then
		if self.calling == true then
			utils:log(1,"Stop calling on door open")
			self:on_calling_end()
			self.bvr:fade_out(1000)
			self:stop_bell()
		end
		self.door_closed = false
	else
		self.door_closed = true
		self.last_door_closed = utils:get_time()
	end
end


function pjs_intercom:on_connected_changed(msg)
	local ccnt = self.pjsis:get_connected_to_main_count()
	utils:log(1,"On connected changed to "..ccnt)
	self:check_connected(ccnt)
	self:update_leds(ccnt)
end

function pjs_intercom:on_unknown(msg)
	utils:log(1,"Unknown message " .. msg.message)
end

function pjs_intercom:on_ultrasonic_data(msg)
	local now = utils:get_time()
	local us = msg.param1
	utils:log(1,"Ultrasonic data " .. us)
	if(us>0) then
		local detected = (us<=self.opt.ultrasonic_trigger);
		if(detected~=self.ultrasonic_triggered) then
			self.ultrasonic_triggered = detected
			if(self.ultrasonic_triggered) then
				local canAction = not self.calling and self.door_closed and (utils:elapsed_msec(self.ultrasonic_last_triggered,now)>=self.opt.ultrasonic_timeout)
				and (utils:elapsed_msec(self.last_door_closed,now)>=self.opt.ultrasonic_door_close_timeout)
				and (self.ultrasonic_action_time==nil);

				utils:log(1,"Ultrasonic detected")
				if(canAction) then
					utils:log(1,"Ultrasonic starting action")
					self.ultrasonic_action_time = now;
					if(self.vr:is_idle()) then
						self.vr:play_file_async("double_beep.wav",0,0)
					end
				end
			else
				utils:log(1,"Ultrasonic empty")
			end
			self.ultrasonic_last_triggered = now
		end
	end
end




function pjs_intercom:init(pjsis,vr,bvr)
	self.pjsis = pjsis
	self.vr = vr
	self.bvr = bvr
	self.controller = pjsis:get_controller()
	self.mqueue = msg_queue;
	self.messages_map = messages_map {
		[pjs.IM_MAIN_BUTTON] = pjs_intercom.on_main_button,
		[pjs.IM_CONNECTED_CHANGED] = pjs_intercom.on_connected_changed,
		[pjs.IM_INPUT1] = pjs_intercom.on_indoor_button,
		[pjs.IM_INPUT2] = pjs_intercom.on_door_button,
		[pjs.IM_ULTRASONIC_DATA] = pjs_intercom.on_ultrasonic_data,
		default = pjs_intercom.on_unknown
	}

	self.calling = false;
	self.calling_start_time = nil;
	self.bell_start = nil;
	self.bell_repeat_count = 0;
	self.bell_stop = nil;

	self.outgoing_calls_start = nil
	self.outgoing_calls = nil

	local idb = idb_open()
	self:load_options(idb)
	idb_close(idb)
end

function pjs_intercom:main()
	local now = utils:get_time()
	local mqueue = self.mqueue
	self.pjsis:hangup_calls_by_tag(OUTGOING_CALLS_TAG,200)
	self.door_closed = true
	self.last_door_closed = now
	self.ultrasonic_triggered = false
	self.ultrasonic_action_time = nil
	self.ultrasonic_last_triggered = now
	self.controller:update_inputs()
	self.controller:set_output(pjs.ECO_MAIN_LED,1)
	self.controller:set_output(pjs.ECO_SECOND_LED,1)
	self.controller:set_output(pjs.ECO_BELL,0)
	while true do
		local msg = mqueue:get_message()
		if(msg~=nil) then
			if msg.message==pjs.IM_QUIT then
				break
			else
				self.messages_map:dispatch(self,msg)
			end
		else
		
			if(self.ultrasonic_action_time~=nil) then
				local now = utils:get_time()
				if(utils:elapsed_msec(self.ultrasonic_action_time,now)>=self.opt.ultrasonic_door_open_delay) then
					utils:log(1,"Ultrasonic start action")
					self:stop_ultrasonic()
					if(self.bell_start==nil) then
						self.bell_start = now
					end
					self.bell_repeat_count = 0
					self.bell_stop = nil
					self.controller:set_output(pjs.ECO_BELL,1)
					self:start_calling()
					if(self.calling) then
						if(self.vr:is_idle()) then
							self.vr:play_file_async("chime.wav",0,0)
						end
						if(self.bvr:is_idle()) then
							self.bvr:play_file_async("music.wav",0,0)
						end
					end
					self.controller:set_output(pjs.ECO_MAIN_LED,0)
				end
			end
		
			if(self.bell_start~=nil) then
				local now = utils:get_time()
				if(utils:elapsed_msec(self.bell_start,now)>=self.opt.bell_time) then
					utils:log(1,"Stop bell")
					self:stop_bell()
				end
			end

			if(self.calling) then
				local now = utils:get_time()
				if(self.calling_start_time~=nil) then
					if(utils:elapsed_msec(self.calling_start_time,now)>=self.opt.calling_timeout) then
						utils:log(1,"Call timeout")
						self:on_calling_end()
						self.bvr:fade_out(1000)
						self:stop_bell()
					end
				end
				self:check_calls(now)
				if((self.bell_stop~=nil)and(self.bell_repeat_count>0)) then
					if(utils:elapsed_msec(self.bell_stop,now)>=self.opt.bell_repeat_time) then
						utils:log(1,"Repeat bell")
						self.bell_stop = nil
						self.bell_start = now;
						self.bell_repeat_count = self.bell_repeat_count-1
						self.controller:set_output(pjs.ECO_BELL,1)
					end
				end;
			end
			mqueue:wait(100)
		end
	end
end


function intercom(pjsis,vr,bvr)
	utils:log(1,"Intercom script begin")
	local iobj = pjs_intercom(pjsis,vr,bvr)
	iobj:main()
	utils:log(1,"Intercom script end")
end
