#ifndef PJS_LUA_H_
#define PJS_LUA_H_
#include "project_common.h"
#include <string>
#include <map>
#include "pjs_utils.h"
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "lua/luasocket/luasocket.h"
#include "lua/luasocket/mime.h"
#include "lua/luasocket/unix.h"
}
/* Lua flavors */
#define SWIG_LUA_FLAVOR_LUA 1
#define SWIG_LUA_FLAVOR_ELUA 2
#define SWIG_LUA_FLAVOR_ELUAC 3

#define SWIGLUA
#define SWIG_LUA_TARGET SWIG_LUA_FLAVOR_LUA
#define SWIG_LUA_MODULE_GLOBAL

#include "swig/nativesrc/swig_lua_runtime.h"
#include "pjs_lua_common.h"
#include <list>

extern "C"  int luaopen_pjs (lua_State* L);
extern "C"  int luaopen_lsqlite3(lua_State *L);
extern "C"  int luaopen_bit(lua_State *L);

//Lua global value type
#define LGVT_NIL	0
#define LGVT_NUM	1
#define LGVT_STR	2
#define LGVT_BOOL	3

class pjs_lua_global_value
{
public:
	virtual ~pjs_lua_global_value() {};
	virtual void get_value(lua_State *L)=0;
	virtual void set_value(lua_State *L,int idx)=0;
	virtual pj_uint8_t	 get_type()=0;
};

class pjs_lua_global_value_num:public pjs_lua_global_value
{
protected:
	lua_Number m_value;
public:
	pjs_lua_global_value_num()
	{
		m_value = 0;
	}
	virtual void set_value(lua_State *L,int idx)
	{
		m_value = lua_tonumber(L,idx);
	}
	virtual void get_value(lua_State *L)
	{
		lua_pushnumber(L,m_value);
	}
	virtual pj_uint8_t	 get_type() { return LGVT_NUM; }
};

class pjs_lua_global_value_bool:public pjs_lua_global_value
{
protected:
	bool m_value;
public:
	pjs_lua_global_value_bool()
	{
		m_value = false;
	}
	virtual void set_value(lua_State *L,int idx)
	{
		m_value = lua_toboolean(L,idx);
	}
	virtual void get_value(lua_State *L)
	{
		lua_pushboolean(L,m_value);
	}
	virtual pj_uint8_t	 get_type() { return LGVT_BOOL; }
};

class pjs_lua_global_value_str:public pjs_lua_global_value
{
protected:
	std::string m_value;
public:
	pjs_lua_global_value_str()
	{
	}
	virtual void set_value(lua_State *L,int idx)
	{
		m_value = lua_tostring(L,idx);
	}
	virtual void get_value(lua_State *L)
	{
		lua_pushstring(L,m_value.c_str());
	}
	virtual pj_uint8_t	 get_type() { return LGVT_STR; }
};

class pjs_lua_global
{
protected:
	typedef std::map<std::string,pjs_lua_global_value *> pjs_global_map_t;
	typedef std::map<std::string,pjs_lua_global_value *>::iterator pjs_global_map_itr_t;
	pjs_simple_mutex m_lock;
	pjs_global_map_t m_map;

	static pj_uint8_t lua_type_to_local(int lt)
	{
		switch(lt)
		{
		case LUA_TNUMBER:
		{
			return LGVT_NUM;
		}
		case LUA_TNIL:
		{
			return LGVT_NIL;
		}
		case LUA_TSTRING:
		{
			return LGVT_STR;
		}
		case LUA_TBOOLEAN:
		{
			return LGVT_BOOL;
		}
		default:
		{
			return LGVT_NIL;
		}
		}
	}
public:
	pjs_lua_global()
	{

	}

	~pjs_lua_global()
	{
		clear();
	}

	void clear()
	{
		m_lock.lock();
		for(pjs_global_map_itr_t itr=m_map.begin();itr!=m_map.end();itr++)
		{
			if(itr->second)
				delete itr->second;
		}
		m_map.clear();
		m_lock.unlock();
	}

	static int l_set(lua_State *L);
	static int l_get(lua_State *L);
};

const struct luaL_Reg pjs_lua_global_reg[] =
{
 { "__index", pjs_lua_global::l_get   },
 { "__newindex", pjs_lua_global::l_set   },
 { NULL, NULL }
};


void add_pjs_lua_global(pjs_lua_global *ptr, lua_State *L, const char *name);

class pjs_lua
{
protected:
	lua_State *m_state;
	static pjs_simple_mutex m_init_mutex;

	static void terminate_hook (lua_State *L, lua_Debug *ar)
	{
		luaL_error(L,"script terminated");
	}
	void set_loaded(const char * name,bool b=true)
	{
		lua_getfield(m_state,LUA_REGISTRYINDEX,"_LOADED");
		lua_pushboolean(m_state,b);
		lua_setfield(m_state,-2,name);
		lua_pop(m_state,1);
	}

	void load_lib(const char *name,lua_CFunction func)
	{
		luaL_requiref(m_state,name,func,1);
		lua_pop(m_state,1);
	}

public:
	pjs_lua()
	{
		m_state = luaL_newstate();
		m_init_mutex.lock();
		luaL_openlibs(m_state);
		load_lib("pjs",&luaopen_pjs);
		load_lib("sqlite3",&luaopen_lsqlite3);
		load_lib("bit",&luaopen_bit);
		load_lib("socketcore",&luaopen_socket_core);
		load_lib("mimecore",&luaopen_mime_core);
		load_lib("socketunix",&luaopen_socket_unix);
		m_init_mutex.unlock();
	}
	lua_State *get_state() { return m_state; }

	int load_script(const char * filename)
	{
		return luaL_loadfile(m_state,filename);
	}

	void set_global(const char * name, const char * str)
	{
		lua_pushstring( m_state, str );
		lua_setglobal( m_state, name );
	}

	int push_swig_object(void *ptr,const char * typename_,bool auto_delete=false)
	{
		swig_type_info *outputType = SWIG_TypeQuery(m_state,typename_);
		if(outputType)
		{
			SWIG_NewPointerObj(m_state, ptr, outputType, auto_delete?1:0);
			return 0;
		}
		else
			return -1;
	}

	int run() {return lua_pcall(m_state,0,0,0);}

	~pjs_lua()
	{
		lua_close(m_state);
	}

	void terminate()
	{
		lua_sethook(m_state,&terminate_hook,LUA_MASKCOUNT,1);
	}

	int set_path(const char* path )
	{
		lua_State* L = m_state;
	    lua_getglobal( L, "package" );
	    lua_getfield( L, -1, "path" ); // get field "path" from table at top of stack (-1)
	    std::string cur_path = path;
	    cur_path.append( "/?.lua;" ); // do your path magic here
	    cur_path.append( lua_tostring( L, -1 ) );
	    lua_pop( L, 1 ); // get rid of the string on the stack we just pushed on line 5
	    lua_pushstring( L, cur_path.c_str() ); // push the new one
	    lua_setfield( L, -2, "path" ); // set the field "path" in table at -2 with value at top of stack
	    lua_pop( L, 1 ); // get rid of package table from top of stack
	    return 0; // all done!
	}

	void add_interface(const char* name,void *ptr, pjs_lua_interface_object *intf, bool auto_delete)
	{
		if(intf&&ptr)
		{
			if(push_swig_object(ptr,intf->class_name(),auto_delete)>=0)
			{
				lua_setglobal(m_state,name);
			}
			else
				if(auto_delete)
					delete intf;
		}
	}
};



#endif /* PJS_LUA_H_ */
