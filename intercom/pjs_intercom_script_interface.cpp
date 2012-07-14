#include "pjs_intercom_script_interface.h"

void clear_pjs_call_data(pjs_call_data_t &data)
{
	data.callid = 0;
	data.timeout = 0;
	data.is_local = data.is_inbound = data.is_connected_to_main = false;
	data.callback = NULL;
	data.sound_connection_type = SCT_CONFERENCE_ON_CONNECTED;
}
void extract_name_and_number(pj_pool_t *pool,string &uri, string &name, string &number)
{
	pjsip_name_addr *naddr = (pjsip_name_addr *)pjsip_parse_uri(pool,(char*)uri.c_str(),uri.length(),PJSIP_PARSE_URI_AS_NAMEADDR);
	if(naddr)
	{
		if(naddr->display.ptr)
			name.assign(naddr->display.ptr,naddr->display.slen);
		pjsip_uri *uri_ = naddr->uri;
		if(uri_)
		{
			if(PJSIP_URI_SCHEME_IS_SIP(uri_)||PJSIP_URI_SCHEME_IS_SIPS(uri_))
			{
				pjsip_sip_uri *suri = (pjsip_sip_uri *)uri_;
				if(suri->user.ptr)
					number.assign(suri->user.ptr,suri->user.slen);
			}
		}
	}
}

pjs_call_info_t pjs_call_script_interface::get_call_info()
{
	pjs_call_info_t result;
	if(m_call_is_active)
	{
		pj_pool_t *pool = pjsua_pool_create("get call info",512,512);
		if(pool)
		{
			pjsua_call_info info;
			pj_status_t status = pjsua_call_get_info(m_call_info.callid,&info);
			if(status==PJ_SUCCESS)
			{
				result.local_uri.assign(info.local_info.ptr,info.local_info.slen);
				result.remote_uri.assign(info.remote_info.ptr,info.remote_info.slen);
				extract_name_and_number(pool,result.local_uri,result.local_name,result.local_number);
				extract_name_and_number(pool,result.remote_uri,result.remote_name,result.remote_number);
			}
			pj_pool_release(pool);
		}
	}
	return result;
}
