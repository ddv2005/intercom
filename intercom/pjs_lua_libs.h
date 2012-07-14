#ifndef PJS_LUA_LIBS_H_
#define PJS_LUA_LIBS_H_
#include "project_common.h"
#include "pjs_lua_common.h"
#include "pjs_utils.h"
#include <string>
#include "crypto/sha.h"

static const unsigned char base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

unsigned char * base64_encode(const unsigned char *src, size_t len,
			      size_t *out_len);
unsigned char * base64_decode(const unsigned char *src, size_t len,
			      size_t *out_len);
SWIG_BEGIN_DECL


class pjs_lua_utils:public pjs_lua_interface_object
{
protected:
	pjs_exit_t &m_exit;
	pjs_shared_event &m_exit_event;
public:
#ifdef SWIG
protected:
#endif
	DEFINE_LUA_INTERFACE_CLASS_NAME("pjs_lua_utils")
	pjs_lua_utils(pjs_exit_t &exit,pjs_shared_event &exit_event):m_exit(exit),m_exit_event(exit_event)
	{

	}
#ifdef SWIG
public:
#endif


	virtual ~pjs_lua_utils()
	{
	}

	void log(const int level, const char *message)
	{
		if (level <= pj_log_get_level())
			PJ_LOG_(INFO_LEVEL,("lua",message));
	}

	void wait_for_end()
	{
		while(!m_exit)
		{
			m_exit_event.wait(100);
		}
	}

	void sleep(pj_uint32_t msec)
	{
		if(msec&&!m_exit)
		{
			pj_timestamp st,now;
			pj_uint32_t diff;
			pj_get_timestamp(&st);
			now = st;
			diff = 0;
			while(!m_exit&&(diff<msec))
			{
				pj_int32_t delay = msec-diff;
				if(delay>100)
					delay = 100;
				m_exit_event.wait(delay);
				pj_get_timestamp(&now);
				diff = pj_elapsed_msec(&st,&now);
			}
		}
	}

	std::string base64_encode(const unsigned char *src, size_t len)
	{
		std::string result;
		unsigned char *buf = ::base64_encode(src,len,NULL);
		if(buf)
		{
			result = (char *)buf;
			free(buf);
		}
		return result;
	}

	std::string sha256(const char *str)
	{
		unsigned char md[32];
		if(SHA256((unsigned char *)str,strlen(str),md))
		{

			return base64_encode(md,sizeof(md));
		}
		else
			return std::string();
	}

	pj_timestamp get_time()
	{
		pj_timestamp result;
		pj_get_timestamp(&result);
		return result;
	}

	pj_int32_t elapsed_msec(pj_timestamp *start,pj_timestamp *stop)
	{
		return pj_elapsed_msec(start,stop);
	}
};

#ifdef SWIG
%native(encode) int encode(lua_State*L);
%native(decode) int decode(lua_State*L);
%{
int encode(lua_State*L);
int decode(lua_State*L);
%}
#endif


SWIG_END_DECL

#endif /* PJS_LUA_LIBS_H_ */
