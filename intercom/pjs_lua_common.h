#ifndef PJS_LUA_COMMON_H_
#define PJS_LUA_COMMON_H_

class pjs_lua_interface_object
{
public:
	pjs_lua_interface_object(){}
	virtual ~pjs_lua_interface_object(){};
	virtual const char * class_name()=0;
};

#define DEFINE_LUA_INTERFACE_CLASS_NAME(A) virtual const char * class_name() { return A " *"; }

#endif /* PJS_LUA_COMMON_H_ */
