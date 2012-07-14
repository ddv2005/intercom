require 'class'
require 'intercom_db'
require 'lua_lib'
local user = require('user')

function call_ivr(self,is_connected)
  local connected_to_main = is_connected
  local mqueue = self.mqueue
  local call = self.call
  local vr = self.vr
  while true do
    local msg = mqueue:get_message()
    if(msg~=nil) then
      if msg.message==pjs.IM_QUIT then
      	break
      else
        if (msg.message==pjs.IM_ASYNC_PLAY_COMPLETED) or (msg.message==pjs.IM_ASYNC_COMPLETED) then
		  	key = vr:get_input(1,"")
		  	if(key~="")then
		  	  utils:log(1,"Call got input "..key)
		  	  if(key=="1")then
		  	    vr:play_file_async("beep.wav","1234567890*#",0,0)
		  	    call:connect_main()
		  	    connected_to_main = true
		  	  elseif(key=="2")then
		  	    vr:play_file_async("double_beep.wav","1234567890*#",0,0)
		  	    call:set_sound_connection_type(bit.bor(call:get_sound_connection_type(),pjs.SCT_LISTEN_MAIN))
		  	    call:disconnect_main()
		  	    connected_to_main = false
		  	  elseif(key=="9")then
--		  	    if(connected_to_main==false) then
		  	    if(true) then
		  	      if (user.incoming_call_user_action~=nil) then
		  	        user.incoming_call_user_action(self,tonumber(key))
		  	      else
		  	        utils:log(1,"No user action")
		  	      end
		  	    else
		  	      utils:log(1,"No action because connected to main")
		  	    end
		  	  end
		  	end
		  	if(vr:is_idle()) then
		  	  vr:wait_input_async(1,"",10000,5000,0,0);
		  	end
        end
      end
    else
      mqueue:wait(100)
    end
  end
end

pjs_incoming_call = class()

function pjs_incoming_call:init(call,vr,mqueue)
  self.call = call
  self.vr = vr
  self.mqueue = mqueue;
end

function pjs_incoming_call:main(is_local)
  local call = self.call
  local vr = self.vr
  local info = call:get_call_info()
  utils:log(1,"Incoming call from "..info.remote_name.."<"..info.remote_number..">")
  call:answer(200)
  utils:sleep(500)
 
  if(is_local~=true) then
    local idb = idb_open()
    local user=idb_check_source_number(idb,info.remote_number)
    if(user==nil) then
	  	vr:clear_input()  	
	  	vr:play_file("welcome.wav","1234567890#")
	  	local pin
	  	local i
	  	for i=1,3 do
	  	  vr:play_file("enter_pin.wav","1234567890#")
	  	  pin = vr:wait_input(6,"#",10000,5000)
	  	  pin = pin:gsub("#","")
	  	  if(pin~="") then
	  	      user=idb_check_pin(idb,pin)
		  	  if(user==nil) then
		  	    vr:clear_input()
		  	    vr:play_file("wrong_pin.wav","1234567890#")
		  	  else
		  	    break
		  	  end
		  end
	  	end
	  	utils:log(1,"Pin code is "..pin)
	end
	idb_close(idb)
	if(user~=nil) then
		utils:log(1,"User "..user.name)
	else
		return
	end
	utils:sleep(200)
  end
  vr:clear_input()
  self.mqueue:clear()  
  if(is_local~=true) then
    vr:play_file_async("beep.wav","1234567890*#",0,0)
    call:connect_main()
  else
    vr:play_file_async("double_beep.wav","1234567890*#",0,0)
    call:set_sound_connection_type(bit.bor(call:get_sound_connection_type(),pjs.SCT_LISTEN_MAIN))
  end
  call_ivr(self,not is_local)
end

pjs_outgoing_call = class()

function pjs_outgoing_call:init(call,vr,mqueue)
  self.call = call
  self.vr = vr
  self.mqueue = mqueue;
end

function pjs_outgoing_call:main(is_local)
  local call = self.call
  local vr = self.vr
  local options = pjs.decode(call:get_user_options())  
  local info = call:get_call_info()
  local confirm_call = false; 
  
  utils:log(1,"Outgoing call to "..info.remote_name.."<"..info.remote_number..">")
  if(options ~= nil) then
    utils:log(1,"Options: "..table.tostring(options))
    if(bit.band(options.opt,1)==1) then
      confirm_call = true
    end
  end
  while call:get_state()~=pjs.PJSIP_INV_STATE_CONFIRMED do
  	utils:sleep(50)
  end
  utils:log(1,"Call connected")
  if(confirm_call==true) then
    utils:log(1,"Confirming the call")
  	vr:clear_input()  	
	vr:play_file("welcome_out.wav","1")
  	local i
  	local digits
  	for i=1,3 do
  	  vr:play_file("press_one.wav","1")
  	  digits = vr:wait_input(1,"",10000,10000)
  	  utils:log(1,"Digit "..digits)
  	  if(digits=="1") then
  	    utils:log(1,"Call is confirmed")
  	    call:set_tag(0)
 	    break
      end
      vr:clear_input()
    end
    if(digits~="1") then
      utils:log(1,"Call is NOT confirmed")
      return
    end
  end
  call:set_tag(0)
  utils:sleep(200)
  vr:play_file_async("double_beep.wav","1234567890*#",0,0)
  call:connect_main()
  call_ivr(self,true)
end


function call_main(call,vr)
    utils:log(1,"Call script begin")
	local iobj
    if(call:is_inbound()) then
    	iobj = pjs_incoming_call(call,vr,msg_queue)
    else
		iobj = pjs_outgoing_call(call,vr,msg_queue)    
    end
    iobj:main(call:is_local())
    utils:sleep(200)
    vr:play_file("goodbye.wav","")
    
    utils:log(1,"Call script end")
end
