/* $Id: stun_session.c 4407 2013-02-27 15:02:03Z riza $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjnath/stun_session.h>
#include <pjnath/errno.h>
#include <pjlib.h>

struct pj_stun_session
{
    pj_stun_config	*cfg;
    pj_pool_t		*pool;
    pj_lock_t		*lock;
    pj_bool_t		 delete_lock;
    pj_stun_session_cb	 cb;
    void		*user_data;

    pj_atomic_t		*busy;
    pj_bool_t		 destroy_request;

    pj_bool_t		 use_fingerprint;

    pj_pool_t		*rx_pool;

#if PJ_LOG_MAX_LEVEL >= 5
    char		 dump_buf[1000];
#endif
    unsigned		 log_flag;

    pj_stun_auth_type	 auth_type;
    pj_stun_auth_cred	 cred;
    int			 auth_retry;
    pj_str_t		 next_nonce;
    pj_str_t		 server_realm;

    pj_str_t		 srv_name;

    pj_stun_tx_data	 pending_request_list;
    pj_stun_tx_data	 cached_response_list;
};

#define SNAME(s_)		    ((s_)->pool->obj_name)

#if PJ_LOG_MAX_LEVEL >= 5
#   define TRACE_(expr)		    PJ_LOG(5,expr)
#else
#   define TRACE_(expr)
#endif

#define LOG_ERR_(sess,title,rc) pjnath_perror(sess->pool->obj_name,title,rc)

#define TDATA_POOL_SIZE		    PJNATH_POOL_LEN_STUN_TDATA
#define TDATA_POOL_INC		    PJNATH_POOL_INC_STUN_TDATA


static void stun_tsx_on_complete(pj_stun_client_tsx *tsx,
				 pj_status_t status, 
				 const pj_stun_msg *response,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len);
static pj_status_t stun_tsx_on_send_msg(pj_stun_client_tsx *tsx,
					const void *stun_pkt,
					pj_size_t pkt_size);
static void stun_tsx_on_destroy(pj_stun_client_tsx *tsx);

static pj_stun_tsx_cb tsx_cb = 
{
    &stun_tsx_on_complete,
    &stun_tsx_on_send_msg,
    &stun_tsx_on_destroy
};


static pj_status_t tsx_add(pj_stun_session *sess,
			   pj_stun_tx_data *tdata)
{
    pj_list_push_front(&sess->pending_request_list, tdata);
    return PJ_SUCCESS;
}

static pj_status_t tsx_erase(pj_stun_session *sess,
			     pj_stun_tx_data *tdata)
{
    PJ_UNUSED_ARG(sess);
    pj_list_erase(tdata);
    return PJ_SUCCESS;
}

static pj_stun_tx_data* tsx_lookup(pj_stun_session *sess,
				   const pj_stun_msg *msg)
{
    pj_stun_tx_data *tdata;

    tdata = sess->pending_request_list.next;
    while (tdata != &sess->pending_request_list) {
	pj_assert(sizeof(tdata->msg_key)==sizeof(msg->hdr.tsx_id));
	if (tdata->msg_magic == msg->hdr.magic &&
	    pj_memcmp(tdata->msg_key, msg->hdr.tsx_id, 
		      sizeof(msg->hdr.tsx_id))==0)
	{
	    return tdata;
	}
	tdata = tdata->next;
    }

    return NULL;
}

static pj_status_t create_tdata(pj_stun_session *sess,
			        pj_stun_tx_data **p_tdata)
{
    pj_pool_t *pool;
    pj_stun_tx_data *tdata;

    /* Create pool and initialize basic tdata attributes */
    pool = pj_pool_create(sess->cfg->pf, "tdata%p", 
			  TDATA_POOL_SIZE, TDATA_POOL_INC, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    tdata = PJ_POOL_ZALLOC_T(pool, pj_stun_tx_data);
    tdata->pool = pool;
    tdata->sess = sess;

    pj_list_init(tdata);

    *p_tdata = tdata;

    return PJ_SUCCESS;
}

static void stun_tsx_on_destroy(pj_stun_client_tsx *tsx)
{
    pj_stun_tx_data *tdata;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);
    tsx_erase(tdata->sess, tdata);

    pj_stun_client_tsx_destroy(tsx);
    pj_pool_release(tdata->pool);
}

static void destroy_tdata(pj_stun_tx_data *tdata, pj_bool_t force)
{
    if (tdata->res_timer.id != PJ_FALSE) {
	pj_timer_heap_cancel(tdata->sess->cfg->timer_heap, 
			     &tdata->res_timer);
	tdata->res_timer.id = PJ_FALSE;
	pj_list_erase(tdata);
    }

    if (force) {
	if (tdata->client_tsx) {
	    tsx_erase(tdata->sess, tdata);
	    pj_stun_client_tsx_destroy(tdata->client_tsx);
	}
	pj_pool_release(tdata->pool);

    } else {
	if (tdata->client_tsx) {
	    pj_time_val delay = {2, 0};
	    pj_stun_client_tsx_schedule_destroy(tdata->client_tsx, &delay);

	} else {
	    pj_pool_release(tdata->pool);
	}
    }
}

/*
 * Destroy the transmit data.
 */
PJ_DEF(void) pj_stun_msg_destroy_tdata( pj_stun_session *sess,
					pj_stun_tx_data *tdata)
{
    PJ_UNUSED_ARG(sess);
    destroy_tdata(tdata, PJ_FALSE);
}


/* Timer callback to be called when it's time to destroy response cache */
static void on_cache_timeout(pj_timer_heap_t *timer_heap,
			     struct pj_timer_entry *entry)
{
    pj_stun_tx_data *tdata;

    PJ_UNUSED_ARG(timer_heap);

    entry->id = PJ_FALSE;
    tdata = (pj_stun_tx_data*) entry->user_data;

    PJ_LOG(5,(SNAME(tdata->sess), "Response cache deleted"));

    pj_list_erase(tdata);
    pj_stun_msg_destroy_tdata(tdata->sess, tdata);
}

static pj_status_t apply_msg_options(pj_stun_session *sess,
				     pj_pool_t *pool,
				     const pj_stun_req_cred_info *auth_info,
				     pj_stun_msg *msg)
{
    pj_status_t status = 0;
    pj_str_t realm, username, nonce, auth_key;

    /* If the agent is sending a request, it SHOULD add a SOFTWARE attribute
     * to the request. The server SHOULD include a SOFTWARE attribute in all 
     * responses.
     *
     * If magic value is not PJ_STUN_MAGIC, only apply the attribute for
     * responses.
     */
    if (sess->srv_name.slen && 
	pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_SOFTWARE, 0)==NULL &&
	(PJ_STUN_IS_RESPONSE(msg->hdr.type) ||
	 (PJ_STUN_IS_REQUEST(msg->hdr.type) && msg->hdr.magic==PJ_STUN_MAGIC))) 
    {
	pj_stun_msg_add_string_attr(pool, msg, PJ_STUN_ATTR_SOFTWARE,
				    &sess->srv_name);
    }

    if (pj_stun_auth_valid_for_msg(msg) && auth_info) {
	realm = auth_info->realm;
	username = auth_info->username;
	nonce = auth_info->nonce;
	auth_key = auth_info->auth_key;
    } else {
	realm.slen = username.slen = nonce.slen = auth_key.slen = 0;
    }

    /* Create and add USERNAME attribute if needed */
    if (username.slen && PJ_STUN_IS_REQUEST(msg->hdr.type)) {
	status = pj_stun_msg_add_string_attr(pool, msg,
					     PJ_STUN_ATTR_USERNAME,
					     &username);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }

    /* Add REALM only when long term credential is used */
    if (realm.slen &&  PJ_STUN_IS_REQUEST(msg->hdr.type)) {
	status = pj_stun_msg_add_string_attr(pool, msg,
					    PJ_STUN_ATTR_REALM,
					    &realm);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }

    /* Add NONCE when desired */
    if (nonce.slen && 
	(PJ_STUN_IS_REQUEST(msg->hdr.type) ||
	 PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))) 
    {
	status = pj_stun_msg_add_string_attr(pool, msg,
					    PJ_STUN_ATTR_NONCE,
					    &nonce);
    }

    /* Add MESSAGE-INTEGRITY attribute */
    if (username.slen && auth_key.slen) {
	status = pj_stun_msg_add_msgint_attr(pool, msg);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }


    /* Add FINGERPRINT attribute if necessary */
    if (sess->use_fingerprint) {
	status = pj_stun_msg_add_uint_attr(pool, msg, 
					  PJ_STUN_ATTR_FINGERPRINT, 0);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }

    return PJ_SUCCESS;
}

static pj_status_t handle_auth_challenge(pj_stun_session *sess,
					 const pj_stun_tx_data *request,
					 const pj_stun_msg *response,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len,
					 pj_bool_t *notify_user)
{
    const pj_stun_errcode_attr *ea;

    *notify_user = PJ_TRUE;

    if (response==NULL)
	return PJ_SUCCESS;

    if (sess->auth_type != PJ_STUN_AUTH_LONG_TERM)
	return PJ_SUCCESS;
    
    if (!PJ_STUN_IS_ERROR_RESPONSE(response->hdr.type)) {
	sess->auth_retry = 0;
	return PJ_SUCCESS;
    }

    ea = (const pj_stun_errcode_attr*)
	 pj_stun_msg_find_attr(response, PJ_STUN_ATTR_ERROR_CODE, 0);
    if (!ea) {
	PJ_LOG(4,(SNAME(sess), "Invalid error response: no ERROR-CODE"
		  " attribute"));
	*notify_user = PJ_FALSE;
	return PJNATH_EINSTUNMSG;
    }

    if (ea->err_code == PJ_STUN_SC_UNAUTHORIZED || 
	ea->err_code == PJ_STUN_SC_STALE_NONCE)
    {
	const pj_stun_nonce_attr *anonce;
	const pj_stun_realm_attr *arealm;
	pj_stun_tx_data *tdata;
	unsigned i;
	pj_status_t status;

	anonce = (const pj_stun_nonce_attr*)
		 pj_stun_msg_find_attr(response, PJ_STUN_ATTR_NONCE, 0);
	if (!anonce) {
	    PJ_LOG(4,(SNAME(sess), "Invalid response: missing NONCE"));
	    *notify_user = PJ_FALSE;
	    return PJNATH_EINSTUNMSG;
	}

	/* Bail out if we've supplied the correct nonce */
	if (pj_strcmp(&anonce->value, &sess->next_nonce)==0) {
	    return PJ_SUCCESS;
	}

	/* Bail out if we've tried too many */
	if (++sess->auth_retry > 3) {
	    PJ_LOG(4,(SNAME(sess), "Error: authentication failed (too "
		      "many retries)"));
	    return PJ_STATUS_FROM_STUN_CODE(401);
	}

	/* Save next_nonce */
	pj_strdup(sess->pool, &sess->next_nonce, &anonce->value);

	/* Copy the realm from the response */
	arealm = (pj_stun_realm_attr*)
		 pj_stun_msg_find_attr(response, PJ_STUN_ATTR_REALM, 0);
	if (arealm) {
	    pj_strdup(sess->pool, &sess->server_realm, &arealm->value);
	    while (sess->server_realm.slen &&
		    !sess->server_realm.ptr[sess->server_realm.slen-1])
	    {
		--sess->server_realm.slen;
	    }
	}

	/* Create new request */
	status = pj_stun_session_create_req(sess, request->msg->hdr.type,
					    request->msg->hdr.magic,
					    NULL, &tdata);
	if (status != PJ_SUCCESS)
	    return status;

	/* Duplicate all the attributes in the old request, except
	 * USERNAME, REALM, M-I, and NONCE, which will be filled in
	 * later.
	 */
	for (i=0; i<request->msg->attr_count; ++i) {
	    const pj_stun_attr_hdr *asrc = request->msg->attr[i];

	    if (asrc->type == PJ_STUN_ATTR_USERNAME ||
		asrc->type == PJ_STUN_ATTR_REALM ||
		asrc->type == PJ_STUN_ATTR_MESSAGE_INTEGRITY ||
		asrc->type == PJ_STUN_ATTR_NONCE)
	    {
		continue;
	    }

	    tdata->msg->attr[tdata->msg->attr_count++] = 
		pj_stun_attr_clone(tdata->pool, asrc);
	}

	/* Will retry the request with authentication, no need to
	 * notify user.
	 */
	*notify_user = PJ_FALSE;

	PJ_LOG(4,(SNAME(sess), "Retrying request with new authentication"));

	/* Retry the request */
	status = pj_stun_session_send_msg(sess, request->token, PJ_TRUE, 
					  request->retransmit, src_addr, 
					  src_addr_len, tdata);

    } else {
	sess->auth_retry = 0;
    }

    return PJ_SUCCESS;
}

static void stun_tsx_on_complete(pj_stun_client_tsx *tsx,
				 pj_status_t status, 
				 const pj_stun_msg *response,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len)
{
    pj_stun_session *sess;
    pj_bool_t notify_user = PJ_TRUE;
    pj_stun_tx_data *tdata;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);
    sess = tdata->sess;

    /* Lock the session and prevent user from destroying us in the callback */
    pj_atomic_inc(sess->busy);
    pj_lock_acquire(sess->lock);

    /* Handle authentication challenge */
    handle_auth_challenge(sess, tdata, response, src_addr,
		          src_addr_len, &notify_user);

    if (notify_user && sess->cb.on_request_complete) {
	(*sess->cb.on_request_complete)(sess, status, tdata->token, tdata, 
					response, src_addr, src_addr_len);
    }

    /* Destroy the transmit data. This will remove the transaction
     * from the pending list too. 
     */
    pj_stun_msg_destroy_tdata(sess, tdata);
    tdata = NULL;

    pj_lock_release(sess->lock);

    if (pj_atomic_dec_and_get(sess->busy)==0 && sess->destroy_request) {
	pj_stun_session_destroy(sess);
	return;
    }
}

static pj_status_t stun_tsx_on_send_msg(pj_stun_client_tsx *tsx,
					const void *stun_pkt,
					pj_size_t pkt_size)
{
    pj_stun_tx_data *tdata;
    pj_stun_session *sess;
    pj_status_t status;

    tdata = (pj_stun_tx_data*) pj_stun_client_tsx_get_data(tsx);
    sess = tdata->sess;

    /* Lock the session and prevent user from destroying us in the callback */
    pj_atomic_inc(sess->busy);
    pj_lock_acquire(sess->lock);
    
    status = sess->cb.on_send_msg(tdata->sess, tdata->token, stun_pkt, 
				  pkt_size, tdata->dst_addr, 
				  tdata->addr_len);
    pj_lock_release(sess->lock);

    if (pj_atomic_dec_and_get(sess->busy)==0 && sess->destroy_request) {
	pj_stun_session_destroy(sess);
	return PJNATH_ESTUNDESTROYED;
    } else {
	return status;
    }
}

/* **************************************************************************/

PJ_DEF(pj_status_t) pj_stun_session_create( pj_stun_config *cfg,
					    const char *name,
					    const pj_stun_session_cb *cb,
					    pj_bool_t fingerprint,
					    pj_stun_session **p_sess)
{
    pj_pool_t	*pool;
    pj_stun_session *sess;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && cb && p_sess, PJ_EINVAL);

    if (name==NULL)
	name = "stuse%p";

    pool = pj_pool_create(cfg->pf, name, PJNATH_POOL_LEN_STUN_SESS, 
			  PJNATH_POOL_INC_STUN_SESS, NULL);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    sess = PJ_POOL_ZALLOC_T(pool, pj_stun_session);
    sess->cfg = cfg;
    sess->pool = pool;
    pj_memcpy(&sess->cb, cb, sizeof(*cb));
    sess->use_fingerprint = fingerprint;
    sess->log_flag = 0xFFFF;
    
    sess->srv_name.ptr = (char*) pj_pool_alloc(pool, 32);
    sess->srv_name.slen = pj_ansi_snprintf(sess->srv_name.ptr, 32,
					   "pjnath-%s", pj_get_version());

    sess->rx_pool = pj_pool_create(sess->cfg->pf, name, 
				   PJNATH_POOL_LEN_STUN_TDATA, 
				   PJNATH_POOL_INC_STUN_TDATA, NULL);

    pj_list_init(&sess->pending_request_list);
    pj_list_init(&sess->cached_response_list);

    status = pj_lock_create_recursive_mutex(pool, name, &sess->lock);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }
    sess->delete_lock = PJ_TRUE;

    status = pj_atomic_create(pool, 0, &sess->busy);
    if (status != PJ_SUCCESS) {
	pj_lock_destroy(sess->lock);
	pj_pool_release(pool);
	return status;
    }

    *p_sess = sess;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_destroy(pj_stun_session *sess)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    pj_lock_acquire(sess->lock);

    /* Can't destroy if we're in a callback */
    sess->destroy_request = PJ_TRUE;
    if (pj_atomic_get(sess->busy)) {
	pj_lock_release(sess->lock);
	return PJ_EPENDING;
    }

    while (!pj_list_empty(&sess->pending_request_list)) {
	pj_stun_tx_data *tdata = sess->pending_request_list.next;
	destroy_tdata(tdata, PJ_TRUE);
    }

    while (!pj_list_empty(&sess->cached_response_list)) {
	pj_stun_tx_data *tdata = sess->cached_response_list.next;
	destroy_tdata(tdata, PJ_TRUE);
    }
    pj_lock_release(sess->lock);

    if (sess->delete_lock) {
	pj_lock_destroy(sess->lock);
    }

    if (sess->rx_pool) {
	pj_pool_release(sess->rx_pool);
	sess->rx_pool = NULL;
    }

    pj_pool_release(sess->pool);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_session_set_user_data( pj_stun_session *sess,
						   void *user_data)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);
    pj_lock_acquire(sess->lock);
    sess->user_data = user_data;
    pj_lock_release(sess->lock);
    return PJ_SUCCESS;
}

PJ_DEF(void*) pj_stun_session_get_user_data(pj_stun_session *sess)
{
    PJ_ASSERT_RETURN(sess, NULL);
    return sess->user_data;
}

PJ_DEF(pj_status_t) pj_stun_session_set_lock( pj_stun_session *sess,
					      pj_lock_t *lock,
					      pj_bool_t auto_del)
{
    pj_lock_t *old_lock = sess->lock;
    pj_bool_t old_del;

    PJ_ASSERT_RETURN(sess && lock, PJ_EINVAL);

    pj_lock_acquire(old_lock);
    sess->lock = lock;
    old_del = sess->delete_lock;
    sess->delete_lock = auto_del;
    pj_lock_release(old_lock);

    if (old_lock)
	pj_lock_destroy(old_lock);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_set_software_name(pj_stun_session *sess,
						      const pj_str_t *sw)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);
    if (sw && sw->slen)
	pj_strdup(sess->pool, &sess->srv_name, sw);
    else
	sess->srv_name.slen = 0;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_set_credential(pj_stun_session *sess,
						 pj_stun_auth_type auth_type,
						 const pj_stun_auth_cred *cred)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    sess->auth_type = auth_type;
    if (cred) {
	pj_stun_auth_cred_dup(sess->pool, &sess->cred, cred);
    } else {
	sess->auth_type = PJ_STUN_AUTH_NONE;
	pj_bzero(&sess->cred, sizeof(sess->cred));
    }

    return PJ_SUCCESS;
}

PJ_DEF(void) pj_stun_session_set_log( pj_stun_session *sess,
				      unsigned flags)
{
    PJ_ASSERT_ON_FAIL(sess, return);
    sess->log_flag = flags;
}

PJ_DEF(pj_bool_t) pj_stun_session_use_fingerprint(pj_stun_session *sess,
						  pj_bool_t use)
{
    pj_bool_t old_use;

    PJ_ASSERT_RETURN(sess, PJ_FALSE);

    old_use = sess->use_fingerprint;
    sess->use_fingerprint = use;
    return old_use;
}

static pj_status_t get_auth(pj_stun_session *sess,
			    pj_stun_tx_data *tdata)
{
    if (sess->cred.type == PJ_STUN_AUTH_CRED_STATIC) {
	//tdata->auth_info.realm = sess->cred.data.static_cred.realm;
	tdata->auth_info.realm = sess->server_realm;
	tdata->auth_info.username = sess->cred.data.static_cred.username;
	tdata->auth_info.nonce = sess->cred.data.static_cred.nonce;

	pj_stun_create_key(tdata->pool, &tdata->auth_info.auth_key, 
			   &tdata->auth_info.realm,
			   &tdata->auth_info.username,
			   sess->cred.data.static_cred.data_type,
			   &sess->cred.data.static_cred.data);

    } else if (sess->cred.type == PJ_STUN_AUTH_CRED_DYNAMIC) {
	pj_str_t password;
	void *user_data = sess->cred.data.dyn_cred.user_data;
	pj_stun_passwd_type data_type = PJ_STUN_PASSWD_PLAIN;
	pj_status_t rc;

	rc = (*sess->cred.data.dyn_cred.get_cred)(tdata->msg, user_data, 
						  tdata->pool,
						  &tdata->auth_info.realm, 
						  &tdata->auth_info.username,
						  &tdata->auth_info.nonce, 
						  &data_type, &password);
	if (rc != PJ_SUCCESS)
	    return rc;

	pj_stun_create_key(tdata->pool, &tdata->auth_info.auth_key, 
			   &tdata->auth_info.realm, &tdata->auth_info.username,
			   data_type, &password);

    } else {
	pj_assert(!"Unknown credential type");
	return PJ_EBUG;
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_create_req(pj_stun_session *sess,
					       int method,
					       pj_uint32_t magic,
					       const pj_uint8_t tsx_id[12],
					       pj_stun_tx_data **p_tdata)
{
    pj_stun_tx_data *tdata = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && p_tdata, PJ_EINVAL);

    status = create_tdata(sess, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create STUN message */
    status = pj_stun_msg_create(tdata->pool, method,  magic, 
				tsx_id, &tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_pool_release(tdata->pool);
	return status;
    }

    /* copy the request's transaction ID as the transaction key. */
    pj_assert(sizeof(tdata->msg_key)==sizeof(tdata->msg->hdr.tsx_id));
    tdata->msg_magic = tdata->msg->hdr.magic;
    pj_memcpy(tdata->msg_key, tdata->msg->hdr.tsx_id,
	      sizeof(tdata->msg->hdr.tsx_id));

    
    /* Get authentication information for the request */
    if (sess->auth_type == PJ_STUN_AUTH_NONE) {
	/* No authentication */

    } else if (sess->auth_type == PJ_STUN_AUTH_SHORT_TERM) {
	/* MUST put authentication in request */
	status = get_auth(sess, tdata);
	if (status != PJ_SUCCESS) {
	    pj_pool_release(tdata->pool);
	    return status;
	}

    } else if (sess->auth_type == PJ_STUN_AUTH_LONG_TERM) {
	/* Only put authentication information if we've received
	 * response from server.
	 */
	if (sess->next_nonce.slen != 0) {
	    status = get_auth(sess, tdata);
	    if (status != PJ_SUCCESS) {
		pj_pool_release(tdata->pool);
		return status;
	    }
	    tdata->auth_info.nonce = sess->next_nonce;
	    tdata->auth_info.realm = sess->server_realm;
	}

    } else {
	pj_assert(!"Invalid authentication type");
	pj_pool_release(tdata->pool);
	return PJ_EBUG;
    }

    *p_tdata = tdata;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_stun_session_create_ind(pj_stun_session *sess,
					       int msg_type,
					       pj_stun_tx_data **p_tdata)
{
    pj_stun_tx_data *tdata = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && p_tdata, PJ_EINVAL);

    status = create_tdata(sess, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create STUN message */
    msg_type |= PJ_STUN_INDICATION_BIT;
    status = pj_stun_msg_create(tdata->pool, msg_type,  PJ_STUN_MAGIC, 
				NULL, &tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_pool_release(tdata->pool);
	return status;
    }

    *p_tdata = tdata;
    return PJ_SUCCESS;
}

/*
 * Create a STUN response message.
 */
PJ_DEF(pj_status_t) pj_stun_session_create_res( pj_stun_session *sess,
						const pj_stun_rx_data *rdata,
						unsigned err_code,
						const pj_str_t *err_msg,
						pj_stun_tx_data **p_tdata)
{
    pj_status_t status;
    pj_stun_tx_data *tdata = NULL;

    status = create_tdata(sess, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create STUN response message */
    status = pj_stun_msg_create_response(tdata->pool, rdata->msg, 
					 err_code, err_msg, &tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_pool_release(tdata->pool);
	return status;
    }

    /* copy the request's transaction ID as the transaction key. */
    pj_assert(sizeof(tdata->msg_key)==sizeof(rdata->msg->hdr.tsx_id));
    tdata->msg_magic = rdata->msg->hdr.magic;
    pj_memcpy(tdata->msg_key, rdata->msg->hdr.tsx_id, 
	      sizeof(rdata->msg->hdr.tsx_id));

    /* copy the credential found in the request */
    pj_stun_req_cred_info_dup(tdata->pool, &tdata->auth_info, &rdata->info);

    *p_tdata = tdata;

    return PJ_SUCCESS;
}


/* Print outgoing message to log */
static void dump_tx_msg(pj_stun_session *sess, const pj_stun_msg *msg,
			unsigned pkt_size, const pj_sockaddr_t *addr)
{
    char dst_name[PJ_INET6_ADDRSTRLEN+10];
    
    if ((PJ_STUN_IS_REQUEST(msg->hdr.type) && 
	 (sess->log_flag & PJ_STUN_SESS_LOG_TX_REQ)==0) ||
	(PJ_STUN_IS_RESPONSE(msg->hdr.type) &&
	 (sess->log_flag & PJ_STUN_SESS_LOG_TX_RES)==0) ||
	(PJ_STUN_IS_INDICATION(msg->hdr.type) &&
	 (sess->log_flag & PJ_STUN_SESS_LOG_TX_IND)==0))
    {
	return;
    }

    pj_sockaddr_print(addr, dst_name, sizeof(dst_name), 3);

    PJ_LOG(5,(SNAME(sess), 
	      "TX %d bytes STUN message to %s:\n"
	      "--- begin STUN message ---\n"
	      "%s"
	      "--- end of STUN message ---\n",
	      pkt_size, dst_name, 
	      pj_stun_msg_dump(msg, sess->dump_buf, sizeof(sess->dump_buf), 
			       NULL)));

}


PJ_DEF(pj_status_t) pj_stun_session_send_msg( pj_stun_session *sess,
					      void *token,
					      pj_bool_t cache_res,
					      pj_bool_t retransmit,
					      const pj_sockaddr_t *server,
					      unsigned addr_len,
					      pj_stun_tx_data *tdata)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && addr_len && server && tdata, PJ_EINVAL);

    /* Allocate packet */
    tdata->max_len = PJ_STUN_MAX_PKT_LEN;
    tdata->pkt = pj_pool_alloc(tdata->pool, tdata->max_len);

    tdata->token = token;
    tdata->retransmit = retransmit;

    /* Lock the session and prevent user from destroying us in the callback */
    pj_atomic_inc(sess->busy);
    pj_lock_acquire(sess->lock);

    /* Apply options */
    status = apply_msg_options(sess, tdata->pool, &tdata->auth_info, 
			       tdata->msg);
    if (status != PJ_SUCCESS) {
	pj_stun_msg_destroy_tdata(sess, tdata);
	LOG_ERR_(sess, "Error applying options", status);
	goto on_return;
    }

    /* Encode message */
    status = pj_stun_msg_encode(tdata->msg, (pj_uint8_t*)tdata->pkt, 
    				tdata->max_len, 0, 
    				&tdata->auth_info.auth_key,
				&tdata->pkt_size);
    if (status != PJ_SUCCESS) {
	pj_stun_msg_destroy_tdata(sess, tdata);
	LOG_ERR_(sess, "STUN encode() error", status);
	goto on_return;
    }

    /* Dump packet */
    dump_tx_msg(sess, tdata->msg, tdata->pkt_size, server);

    /* If this is a STUN request message, then send the request with
     * a new STUN client transaction.
     */
    if (PJ_STUN_IS_REQUEST(tdata->msg->hdr.type)) {

	/* Create STUN client transaction */
	status = pj_stun_client_tsx_create(sess->cfg, tdata->pool, 
					   &tsx_cb, &tdata->client_tsx);
	PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
	pj_stun_client_tsx_set_data(tdata->client_tsx, (void*)tdata);

	/* Save the remote address */
	tdata->addr_len = addr_len;
	tdata->dst_addr = server;

	/* Send the request! */
	status = pj_stun_client_tsx_send_msg(tdata->client_tsx, retransmit,
					     tdata->pkt, tdata->pkt_size);
	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    pj_stun_msg_destroy_tdata(sess, tdata);
	    LOG_ERR_(sess, "Error sending STUN request", status);
	    goto on_return;
	}

	/* Add to pending request list */
	tsx_add(sess, tdata);

    } else {
	if (cache_res && 
	    (PJ_STUN_IS_SUCCESS_RESPONSE(tdata->msg->hdr.type) ||
	     PJ_STUN_IS_ERROR_RESPONSE(tdata->msg->hdr.type))) 
	{
	    /* Requested to keep the response in the cache */
	    pj_time_val timeout;
	    
	    pj_memset(&tdata->res_timer, 0, sizeof(tdata->res_timer));
	    pj_timer_entry_init(&tdata->res_timer, PJ_TRUE, tdata, 
				&on_cache_timeout);

	    timeout.sec = sess->cfg->res_cache_msec / 1000;
	    timeout.msec = sess->cfg->res_cache_msec % 1000;

	    status = pj_timer_heap_schedule(sess->cfg->timer_heap, 
					    &tdata->res_timer,
					    &timeout);
	    if (status != PJ_SUCCESS) {
		tdata->res_timer.id = PJ_FALSE;
		pj_stun_msg_destroy_tdata(sess, tdata);
		LOG_ERR_(sess, "Error scheduling response timer", status);
		goto on_return;
	    }

	    pj_list_push_back(&sess->cached_response_list, tdata);
	}
    
	/* Otherwise for non-request message, send directly to transport. */
	status = sess->cb.on_send_msg(sess, token, tdata->pkt, 
				      tdata->pkt_size, server, addr_len);

	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	    pj_stun_msg_destroy_tdata(sess, tdata);
	    LOG_ERR_(sess, "Error sending STUN request", status);
	    goto on_return;
	}

	/* Destroy only when response is not cached*/
	if (tdata->res_timer.id == 0) {
	    pj_stun_msg_destroy_tdata(sess, tdata);
	}
    }

on_return:
    pj_lock_release(sess->lock);

    /* Check if application has called destroy() in the callback */
    if (pj_atomic_dec_and_get(sess->busy)==0 && sess->destroy_request) {
	pj_stun_session_destroy(sess);
	return PJNATH_ESTUNDESTROYED;
    }

    return status;
}


/*
 * Create and send STUN response message.
 */
PJ_DEF(pj_status_t) pj_stun_session_respond( pj_stun_session *sess, 
					     const pj_stun_rx_data *rdata,
					     unsigned code, 
					     const char *errmsg,
					     void *token,
					     pj_bool_t cache, 
					     const pj_sockaddr_t *dst_addr, 
					     unsigned addr_len)
{
    pj_status_t status;
    pj_str_t reason;
    pj_stun_tx_data *tdata;

    status = pj_stun_session_create_res(sess, rdata, code, 
					(errmsg?pj_cstr(&reason,errmsg):NULL), 
					&tdata);
    if (status != PJ_SUCCESS)
	return status;

    return pj_stun_session_send_msg(sess, token, cache, PJ_FALSE,
				    dst_addr,  addr_len, tdata);
}


/*
 * Cancel outgoing STUN transaction. 
 */
PJ_DEF(pj_status_t) pj_stun_session_cancel_req( pj_stun_session *sess,
						pj_stun_tx_data *tdata,
						pj_bool_t notify,
						pj_status_t notify_status)
{
    PJ_ASSERT_RETURN(sess && tdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(!notify || notify_status!=PJ_SUCCESS, PJ_EINVAL);
    PJ_ASSERT_RETURN(PJ_STUN_IS_REQUEST(tdata->msg->hdr.type), PJ_EINVAL);

    /* Lock the session and prevent user from destroying us in the callback */
    pj_atomic_inc(sess->busy);
    pj_lock_acquire(sess->lock);

    if (notify) {
	(sess->cb.on_request_complete)(sess, notify_status, tdata->token, 
				       tdata, NULL, NULL, 0);
    }

    /* Just destroy tdata. This will destroy the transaction as well */
    pj_stun_msg_destroy_tdata(sess, tdata);

    pj_lock_release(sess->lock);

    if (pj_atomic_dec_and_get(sess->busy)==0 && sess->destroy_request) {
	pj_stun_session_destroy(sess);
	return PJNATH_ESTUNDESTROYED;
    }

    return PJ_SUCCESS;
}

/*
 * Explicitly request retransmission of the request.
 */
PJ_DEF(pj_status_t) pj_stun_session_retransmit_req(pj_stun_session *sess,
						   pj_stun_tx_data *tdata,
                                                   pj_bool_t mod_count)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && tdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(PJ_STUN_IS_REQUEST(tdata->msg->hdr.type), PJ_EINVAL);

    /* Lock the session and prevent user from destroying us in the callback */
    pj_atomic_inc(sess->busy);
    pj_lock_acquire(sess->lock);

    status = pj_stun_client_tsx_retransmit(tdata->client_tsx, mod_count);

    pj_lock_release(sess->lock);

    if (pj_atomic_dec_and_get(sess->busy)==0 && sess->destroy_request) {
	pj_stun_session_destroy(sess);
	return PJNATH_ESTUNDESTROYED;
    }

    return status;
}


/* Send response */
static pj_status_t send_response(pj_stun_session *sess, void *token,
				 pj_pool_t *pool, pj_stun_msg *response,
				 const pj_stun_req_cred_info *auth_info,
				 pj_bool_t retransmission,
				 const pj_sockaddr_t *addr, unsigned addr_len)
{
    pj_uint8_t *out_pkt;
    pj_size_t out_max_len, out_len;
    pj_status_t status;

    /* Apply options */
    if (!retransmission) {
	status = apply_msg_options(sess, pool, auth_info, response);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Alloc packet buffer */
    out_max_len = PJ_STUN_MAX_PKT_LEN;
    out_pkt = (pj_uint8_t*) pj_pool_alloc(pool, out_max_len);

    /* Encode */
    status = pj_stun_msg_encode(response, out_pkt, out_max_len, 0, 
				&auth_info->auth_key, &out_len);
    if (status != PJ_SUCCESS) {
	LOG_ERR_(sess, "Error encoding message", status);
	return status;
    }

    /* Print log */
    dump_tx_msg(sess, response, out_len, addr);

    /* Send packet */
    status = sess->cb.on_send_msg(sess, token, out_pkt, out_len, 
				  addr, addr_len);

    return status;
}

/* Authenticate incoming message */
static pj_status_t authenticate_req(pj_stun_session *sess,
				    void *token,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    pj_stun_rx_data *rdata,
				    pj_pool_t *tmp_pool,
				    const pj_sockaddr_t *src_addr,
				    unsigned src_addr_len)
{
    pj_stun_msg *response;
    pj_status_t status;

    if (PJ_STUN_IS_ERROR_RESPONSE(rdata->msg->hdr.type) || 
	sess->auth_type == PJ_STUN_AUTH_NONE)
    {
	return PJ_SUCCESS;
    }

    status = pj_stun_authenticate_request(pkt, pkt_len, rdata->msg, 
					  &sess->cred, tmp_pool, &rdata->info,
					  &response);
    if (status != PJ_SUCCESS && response != NULL) {
	PJ_LOG(5,(SNAME(sess), "Message authentication failed"));
	send_response(sess, token, tmp_pool, response, &rdata->info, 
		      PJ_FALSE, src_addr, src_addr_len);
    }

    return status;
}


/* Handle incoming response */
static pj_status_t on_incoming_response(pj_stun_session *sess,
					unsigned options,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					pj_stun_msg *msg,
					const pj_sockaddr_t *src_addr,
					unsigned src_addr_len)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* Lookup pending client transaction */
    tdata = tsx_lookup(sess, msg);
    if (tdata == NULL) {
	PJ_LOG(5,(SNAME(sess), 
		  "Transaction not found, response silently discarded"));
	return PJ_SUCCESS;
    }

    if (sess->auth_type == PJ_STUN_AUTH_NONE)
	options |= PJ_STUN_NO_AUTHENTICATE;

    /* Authenticate the message, unless PJ_STUN_NO_AUTHENTICATE
     * is specified in the option.
     */
    if ((options & PJ_STUN_NO_AUTHENTICATE) == 0 && 
	tdata->auth_info.auth_key.slen != 0 && 
	pj_stun_auth_valid_for_msg(msg))
    {
	status = pj_stun_authenticate_response(pkt, pkt_len, msg, 
					       &tdata->auth_info.auth_key);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(5,(SNAME(sess), 
		      "Response authentication failed"));
	    return status;
	}
    }

    /* Pass the response to the transaction. 
     * If the message is accepted, transaction callback will be called,
     * and this will call the session callback too.
     */
    status = pj_stun_client_tsx_on_rx_msg(tdata->client_tsx, msg, 
					  src_addr, src_addr_len);
    if (status != PJ_SUCCESS) {
	return status;
    }

    return PJ_SUCCESS;
}


/* For requests, check if we cache the response */
static pj_status_t check_cached_response(pj_stun_session *sess,
					 pj_pool_t *tmp_pool,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    pj_stun_tx_data *t;

    /* First lookup response in response cache */
    t = sess->cached_response_list.next;
    while (t != &sess->cached_response_list) {
	if (t->msg_magic == msg->hdr.magic &&
	    t->msg->hdr.type == msg->hdr.type &&
	    pj_memcmp(t->msg_key, msg->hdr.tsx_id, 
		      sizeof(msg->hdr.tsx_id))==0)
	{
	    break;
	}
	t = t->next;
    }

    if (t != &sess->cached_response_list) {
	/* Found response in the cache */

	PJ_LOG(5,(SNAME(sess), 
		 "Request retransmission, sending cached response"));

	send_response(sess, t->token, tmp_pool, t->msg, &t->auth_info, 
		      PJ_TRUE, src_addr, src_addr_len);
	return PJ_SUCCESS;
    }

    return PJ_ENOTFOUND;
}

/* Handle incoming request */
static pj_status_t on_incoming_request(pj_stun_session *sess,
				       unsigned options,
				       void *token,
				       pj_pool_t *tmp_pool,
				       const pj_uint8_t *in_pkt,
				       unsigned in_pkt_len,
				       pj_stun_msg *msg,
				       const pj_sockaddr_t *src_addr,
				       unsigned src_addr_len)
{
    pj_stun_rx_data rdata;
    pj_status_t status;

    /* Init rdata */
    rdata.msg = msg;
    pj_bzero(&rdata.info, sizeof(rdata.info));

    if (sess->auth_type == PJ_STUN_AUTH_NONE)
	options |= PJ_STUN_NO_AUTHENTICATE;

    /* Authenticate the message, unless PJ_STUN_NO_AUTHENTICATE
     * is specified in the option.
     */
    if ((options & PJ_STUN_NO_AUTHENTICATE) == 0) {
	status = authenticate_req(sess, token, (const pj_uint8_t*) in_pkt, 
				  in_pkt_len,&rdata, tmp_pool, src_addr, 
				  src_addr_len);
	if (status != PJ_SUCCESS) {
	    return status;
	}
    }

    /* Distribute to handler, or respond with Bad Request */
    if (sess->cb.on_rx_request) {
	status = (*sess->cb.on_rx_request)(sess, in_pkt, in_pkt_len, &rdata,
					   token, src_addr, src_addr_len);
    } else {
	pj_str_t err_text;
	pj_stun_msg *response;

	err_text = pj_str("Callback is not set to handle request");
	status = pj_stun_msg_create_response(tmp_pool, msg, 
					     PJ_STUN_SC_BAD_REQUEST, 
					     &err_text, &response);
	if (status == PJ_SUCCESS && response) {
	    status = send_response(sess, token, tmp_pool, response, 
				   NULL, PJ_FALSE, src_addr, src_addr_len);
	}
    }

    return status;
}


/* Handle incoming indication */
static pj_status_t on_incoming_indication(pj_stun_session *sess,
					  void *token,
					  pj_pool_t *tmp_pool,
					  const pj_uint8_t *in_pkt,
					  unsigned in_pkt_len,
					  const pj_stun_msg *msg,
					  const pj_sockaddr_t *src_addr,
					  unsigned src_addr_len)
{
    PJ_UNUSED_ARG(tmp_pool);

    /* Distribute to handler */
    if (sess->cb.on_rx_indication) {
	return (*sess->cb.on_rx_indication)(sess, in_pkt, in_pkt_len, msg,
					    token, src_addr, src_addr_len);
    } else {
	return PJ_SUCCESS;
    }
}


/* Print outgoing message to log */
static void dump_rx_msg(pj_stun_session *sess, const pj_stun_msg *msg,
			unsigned pkt_size, const pj_sockaddr_t *addr)
{
    char src_info[PJ_INET6_ADDRSTRLEN+10];
    
    if ((PJ_STUN_IS_REQUEST(msg->hdr.type) && 
	 (sess->log_flag & PJ_STUN_SESS_LOG_RX_REQ)==0) ||
	(PJ_STUN_IS_RESPONSE(msg->hdr.type) &&
	 (sess->log_flag & PJ_STUN_SESS_LOG_RX_RES)==0) ||
	(PJ_STUN_IS_INDICATION(msg->hdr.type) &&
	 (sess->log_flag & PJ_STUN_SESS_LOG_RX_IND)==0))
    {
	return;
    }

    pj_sockaddr_print(addr, src_info, sizeof(src_info), 3);

    PJ_LOG(5,(SNAME(sess),
	      "RX %d bytes STUN message from %s:\n"
	      "--- begin STUN message ---\n"
	      "%s"
	      "--- end of STUN message ---\n",
	      pkt_size, src_info,
	      pj_stun_msg_dump(msg, sess->dump_buf, sizeof(sess->dump_buf), 
			       NULL)));

}

/* Incoming packet */
PJ_DEF(pj_status_t) pj_stun_session_on_rx_pkt(pj_stun_session *sess,
					      const void *packet,
					      pj_size_t pkt_size,
					      unsigned options,
					      void *token,
					      pj_size_t *parsed_len,
					      const pj_sockaddr_t *src_addr,
					      unsigned src_addr_len)
{
    pj_stun_msg *msg, *response;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && packet && pkt_size, PJ_EINVAL);

    /* Lock the session and prevent user from destroying us in the callback */
    pj_atomic_inc(sess->busy);
    pj_lock_acquire(sess->lock);

    /* Reset pool */
    pj_pool_reset(sess->rx_pool);

    /* Try to parse the message */
    status = pj_stun_msg_decode(sess->rx_pool, (const pj_uint8_t*)packet,
			        pkt_size, options, 
				&msg, parsed_len, &response);
    if (status != PJ_SUCCESS) {
	LOG_ERR_(sess, "STUN msg_decode() error", status);
	if (response) {
	    send_response(sess, token, sess->rx_pool, response, NULL,
			  PJ_FALSE, src_addr, src_addr_len);
	}
	goto on_return;
    }

    dump_rx_msg(sess, msg, pkt_size, src_addr);

    /* For requests, check if we have cached response */
    status = check_cached_response(sess, sess->rx_pool, msg, 
				   src_addr, src_addr_len);
    if (status == PJ_SUCCESS) {
	goto on_return;
    }

    /* Handle message */
    if (PJ_STUN_IS_SUCCESS_RESPONSE(msg->hdr.type) ||
	PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))
    {
	status = on_incoming_response(sess, options, 
				      (const pj_uint8_t*) packet, pkt_size, 
				      msg, src_addr, src_addr_len);

    } else if (PJ_STUN_IS_REQUEST(msg->hdr.type)) {

	status = on_incoming_request(sess, options, token, sess->rx_pool, 
				     (const pj_uint8_t*) packet, pkt_size, 
				     msg, src_addr, src_addr_len);

    } else if (PJ_STUN_IS_INDICATION(msg->hdr.type)) {

	status = on_incoming_indication(sess, token, sess->rx_pool, 
					(const pj_uint8_t*) packet, pkt_size,
					msg, src_addr, src_addr_len);

    } else {
	pj_assert(!"Unexpected!");
	status = PJ_EBUG;
    }

on_return:
    pj_lock_release(sess->lock);

    /* If we've received destroy request while we're on the callback,
     * destroy the session now.
     */
    if (pj_atomic_dec_and_get(sess->busy)==0 && sess->destroy_request) {
	pj_stun_session_destroy(sess);
	return PJNATH_ESTUNDESTROYED;
    }

    return status;
}

