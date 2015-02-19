#include "pjs_lua.h"

pjs_simple_mutex pjs_lua::m_init_mutex;
typedef struct pjs_lua_global_data_t
{
	pjs_lua_global *self;
}pjs_lua_global_data_t;
void add_pjs_lua_global(pjs_lua_global *ptr, lua_State *L, const char *name)
{
	pjs_lua_global_data_t *v = (pjs_lua_global_data_t *) lua_newuserdata(L, sizeof(pjs_lua_global_data_t));
	v->self = ptr;

	luaL_newmetatable(L, "pjs_lua_global");
	luaL_setfuncs(L, pjs_lua_global_reg, 0);
	lua_setmetatable(L, -2);

	lua_setglobal(L, name);
}

#define lua_check_num_args(func_name,a,b) \
  if (lua_gettop(L)<a || lua_gettop(L)>b) \
  {lua_pushfstring(L,"Error in %s expected %d..%d args, got %d",func_name,a,b,lua_gettop(L));\
  goto fail;}

#define lua_fail_arg(func_name,argnum,type) \
  {lua_pushfstring(L,"Error in %s (arg %d), expected '%s' got '%s'",\
  func_name,argnum,type,lua_typename(L,lua_type(L,argnum)));\
  goto fail;}

int pjs_lua_global::l_set(lua_State *L)
{
	pj_uint8_t lt;
	std::string key;
	pjs_lua_global_value *value;
	pjs_lua_global_data_t *data;
	lua_check_num_args(__func__,3,3);
	if(!lua_isuserdata(L,1)) lua_fail_arg(__func__,1,"userdata");
	if(!lua_isstring(L,2)) lua_fail_arg(__func__,2,"string");
	data = (pjs_lua_global_data_t *)lua_touserdata(L,1);
	key = lua_tostring(L,2);
	if(data->self)
	{
		data->self->m_lock.lock();
		value = data->self->m_map[key];
		lt = lua_type_to_local(lua_type(L,3));
		if(value)
		{
			if(value->get_type()!=lt)
			{
				delete value;
				value = NULL;
			}
		}
		if(!value)
		{
			switch(lt)
			{
			case LGVT_BOOL:
			{
				value = new pjs_lua_global_value_bool();
				break;
			}
			case LGVT_NUM:
			{
				value = new pjs_lua_global_value_num();
				break;
			}
			case LGVT_STR:
			{
				value = new pjs_lua_global_value_str();
				break;
			}
			}
		}
		if(value)
		{
			data->self->m_map[key] = value;
			value->set_value(L,3);
		}
		else
			data->self->m_map.erase(key);
		data->self->m_lock.unlock();
	}
	return 0;

	fail:
	  lua_error(L);
	  return 0;
}

int pjs_lua_global::l_get(lua_State *L)
{
	std::string key;
	pjs_lua_global_value *value;
	pjs_lua_global_data_t *data;
	lua_check_num_args(__func__,2,2);
	if(!lua_isuserdata(L,1)) lua_fail_arg(__func__,1,"userdata");
	if(!lua_isstring(L,2)) lua_fail_arg(__func__,2,"string");
	data = (pjs_lua_global_data_t *)lua_touserdata(L,1);
	key = lua_tostring(L,2);

	if(data->self)
	{
		data->self->m_lock.lock();
		value = data->self->m_map[key];
		if(value)
		{
			value->get_value(L);
		}
		else
			lua_pushnil(L);
		data->self->m_lock.unlock();
		return 1;
	}
	else
		return 0;

	fail:
	  lua_error(L);
	  return 0;
}

