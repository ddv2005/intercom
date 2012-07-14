require 'lua_lib'
local http = require('socket.http')

local base = _G
module('user')

local base_url = 'http://192.168.33.26:89'

function open_door(call)
  local vr = call.vr
  base.utils:log(1,"Send request on open door")
  local r,c = http.request(base_url..'/open_door.asp')
  base.utils:log(1,"Request result code "..c)
  vr:play_file_async("tri_beep_1.wav","1234567890*#",0,0)
end

function base.get_http(data)
  local params = base.pjs.decode(data:get_user_options());
  base.utils:log(1,"Send HTTP request to "..params.url)
  local r,c = http.request(params.url)
  base.utils:log(1,"Request result code "..c)
end

function on_door_bell()
  local params = {}
  params.url = base_url..'/door_bell.asp'
  base.pjsis:create_thread('user.lua','get_http',base.pjs.encode(params),0);
end


function incoming_call_user_action(call,action)
  if(action==9) then
    open_door(call)
  end
end

