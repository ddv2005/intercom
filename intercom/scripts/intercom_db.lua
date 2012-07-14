

intercom_db_file = db_path.."/intercom.db3"

function idb_open()
	return sqlite3.open(intercom_db_file)
end

function idb_close(idb)
	if(idb~=nil) then
		idb:close()
	end
end

function idb_check_source_number(idb,source_number)
  if(idb~=nil) then
    if(idb:isopen()) then
      local sql=idb:prepare("SELECT u.id,u.name,u.options FROM tblUserANI ua,tblUsers u WHERE u.id=ua.user and phone='"..source_number.."'")
      if(sql~=nil) then
        if(sql:step()==sqlite3.ROW) then
	        local row=sql:get_named_values()
	        sql:finalize()
	        return row
	    end
      end
      sql:finalize()
    end
  end
  return nil
end

function idb_check_pin(idb,pin)
  if(idb~=nil) then
    if(idb:isopen()) then
      local hash = utils:sha256(pin)
      local sql=idb:prepare("SELECT id,name,options FROM tblUsers WHERE pin='"..hash.."'")
      if(sql~=nil) then
        if(sql:step()==sqlite3.ROW) then
	        local row=sql:get_named_values()
	        sql:finalize()
	        return row
	    end
      end
      sql:finalize()
    end
  end
  return nil
end

function idb_get_option(idb,key)
  if(idb~=nil) then
    if(idb:isopen()) then
      local sql=idb:prepare("SELECT value FROM tblOptions WHERE name='"..key.."'")
      if(sql~=nil) then
        if(sql:step()==sqlite3.ROW) then
	        local row=sql:get_named_values()
	        sql:finalize()
	        return row.value
	    end
      end
      sql:finalize()
    end
  end
  return nil
end

function idb_get_num_option(idb,key,def)
  local res=tonumber(idb_get_option(idb,key) or def)
  utils:log(1,"Option "..key.."="..res)
  return res
end

function idb_phones_by_group(idb,group)
  if(idb~=nil) then
    if(idb:isopen()) then
      local sql=idb:prepare("SELECT account_idx,number,options,delay FROM tblPhones WHERE group_id="..group.." ORDER BY delay,id")
      if(sql~=nil) then
        local result = {}
        result.count = 0
        while(sql:step()==sqlite3.ROW) do
        	result.count = result.count+1
	        result[result.count] = sql:get_named_values()
	    end
	    sql:finalize()
	    return result
      end
    end
  end
  return nil
end


