/* $Id: regc_test.c 4093 2012-04-26 09:26:07Z bennylp $ */
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
#include "test.h"
#include <pjsip_ua.h>
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "regc_test.c"


/************************************************************************/
/* A module to inject error into outgoing sending operation */
static pj_status_t mod_send_on_tx_request(pjsip_tx_data *tdata);

static struct 
{
    pjsip_module mod;
    unsigned	 count;
    unsigned	 count_before_reject;
} send_mod = 
{
    {
	NULL, NULL,			    /* prev, next.		*/
	{ "mod-send", 8 },		    /* Name.			*/
	-1,				    /* Id			*/
	PJSIP_MOD_PRIORITY_TRANSPORT_LAYER,	    /* Priority			*/
	NULL,				    /* load()			*/
	NULL,				    /* start()			*/
	NULL,				    /* stop()			*/
	NULL,				    /* unload()			*/
	NULL,				    /* on_rx_request()		*/
	NULL,				    /* on_rx_response()		*/
	&mod_send_on_tx_request,		    /* on_tx_request.		*/
	NULL,				    /* on_tx_response()		*/
	NULL,				    /* on_tsx_state()		*/
    },
    0,
    0xFFFF
};


static pj_status_t mod_send_on_tx_request(pjsip_tx_data *tdata)
{
    PJ_UNUSED_ARG(tdata);

    if (++send_mod.count > send_mod.count_before_reject)
	return PJ_ECANCELLED;
    else
	return PJ_SUCCESS;
};


/************************************************************************/
/* Registrar for testing */
static pj_bool_t regs_rx_request(pjsip_rx_data *rdata);

enum contact_op
{
    NONE,	/* don't put Contact header	    */
    EXACT,	/* return exact contact		    */
    MODIFIED,	/* return modified Contact header   */
};

struct registrar_cfg
{
    pj_bool_t	    respond;	    /* should it respond at all		*/
    unsigned	    status_code;    /* final response status code	*/
    pj_bool_t	    authenticate;   /* should we authenticate?		*/
    enum contact_op contact_op;	    /* What should we do with Contact   */
    unsigned	    expires_param;  /* non-zero to put in expires param	*/
    unsigned	    expires;	    /* non-zero to put in Expires header*/

    pj_str_t	    more_contacts;  /* Additional Contact headers to put*/
};

static struct registrar
{
    pjsip_module	    mod;
    struct registrar_cfg    cfg;
    unsigned		    response_cnt;
} registrar = 
{
    {
	NULL, NULL,			    /* prev, next.		*/
	{ "registrar", 9 },		    /* Name.			*/
	-1,				    /* Id			*/
	PJSIP_MOD_PRIORITY_APPLICATION,	    /* Priority			*/
	NULL,				    /* load()			*/
	NULL,				    /* start()			*/
	NULL,				    /* stop()			*/
	NULL,				    /* unload()			*/
	&regs_rx_request,		    /* on_rx_request()		*/
	NULL,				    /* on_rx_response()		*/
	NULL,				    /* on_tx_request.		*/
	NULL,				    /* on_tx_response()		*/
	NULL,				    /* on_tsx_state()		*/
    }
};

static pj_bool_t regs_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_hdr hdr_list;
    int code;
    pj_status_t status;

    if (msg->line.req.method.id != PJSIP_REGISTER_METHOD)
	return PJ_FALSE;

    if (!registrar.cfg.respond)
	return PJ_TRUE;

    pj_list_init(&hdr_list);

    if (registrar.cfg.authenticate && 
	pjsip_msg_find_hdr(msg, PJSIP_H_AUTHORIZATION, NULL)==NULL) 
    {
	pjsip_generic_string_hdr *hwww;
	const pj_str_t hname = pj_str("WWW-Authenticate");
	const pj_str_t hvalue = pj_str("Digest realm=\"test\"");

	hwww = pjsip_generic_string_hdr_create(rdata->tp_info.pool, &hname, 
					       &hvalue);
	pj_list_push_back(&hdr_list, hwww);

	code = 401;

    } else {
	if (registrar.cfg.contact_op == EXACT ||
	    registrar.cfg.contact_op == MODIFIED) 
	{
	    pjsip_hdr *hsrc;

	    for (hsrc=msg->hdr.next; hsrc!=&msg->hdr; hsrc=hsrc->next) {
		pjsip_contact_hdr *hdst;

		if (hsrc->type != PJSIP_H_CONTACT)
		    continue;

		hdst = (pjsip_contact_hdr*)
		       pjsip_hdr_clone(rdata->tp_info.pool, hsrc);

		if (hdst->expires==0)
		    continue;

		if (registrar.cfg.contact_op == MODIFIED) {
		    if (PJSIP_URI_SCHEME_IS_SIP(hdst->uri) ||
			PJSIP_URI_SCHEME_IS_SIPS(hdst->uri))
		    {
			pjsip_sip_uri *sip_uri = (pjsip_sip_uri*)
						 pjsip_uri_get_uri(hdst->uri);
			sip_uri->host = pj_str("x-modified-host");
			sip_uri->port = 1;
		    }
		}

		if (registrar.cfg.expires_param)
		    hdst->expires = registrar.cfg.expires_param;

		pj_list_push_back(&hdr_list, hdst);
	    }
	}

	if (registrar.cfg.more_contacts.slen) {
	    pjsip_generic_string_hdr *hcontact;
	    const pj_str_t hname = pj_str("Contact");

	    hcontact = pjsip_generic_string_hdr_create(rdata->tp_info.pool, &hname, 
						       &registrar.cfg.more_contacts);
	    pj_list_push_back(&hdr_list, hcontact);
	}

	if (registrar.cfg.expires) {
	    pjsip_expires_hdr *hexp;

	    hexp = pjsip_expires_hdr_create(rdata->tp_info.pool, 
					    registrar.cfg.expires);
	    pj_list_push_back(&hdr_list, hexp);
	}

	registrar.response_cnt++;

	code = registrar.cfg.status_code;
    }

    status = pjsip_endpt_respond(endpt, NULL, rdata, code, NULL,
				 &hdr_list, NULL, NULL);
    pj_assert(status == PJ_SUCCESS);

    return PJ_TRUE;
}


/************************************************************************/
/* Client registration test session */
struct client
{
    /* Result/expected result */
    int		error;
    int		code;
    pj_bool_t	have_reg;
    int		expiration;
    unsigned	contact_cnt;
    pj_bool_t	auth;

    /* Commands */
    pj_bool_t	destroy_on_cb;

    /* Status */
    pj_bool_t	done;

    /* Additional results */
    int		interval;
    int		next_reg;
};

/* regc callback */
static void client_cb(struct pjsip_regc_cbparam *param)
{
    struct client *client = (struct client*) param->token;
    pjsip_regc_info info;
    pj_status_t status;

    client->done = PJ_TRUE;

    status = pjsip_regc_get_info(param->regc, &info);
    pj_assert(status == PJ_SUCCESS);

    client->error = (param->status != PJ_SUCCESS);
    client->code = param->code;

    if (client->error)
	return;

    client->have_reg = info.auto_reg && info.interval>0 &&
		       param->expiration>0;
    client->expiration = param->expiration;
    client->contact_cnt = param->contact_cnt;
    client->interval = info.interval;
    client->next_reg = info.next_reg;

    if (client->destroy_on_cb)
	pjsip_regc_destroy(param->regc);
}


/* Generic client test session */
static struct client client_result;
static int do_test(const char *title,
		   const struct registrar_cfg *srv_cfg, 
		   const struct client *client_cfg,
		   const pj_str_t *registrar_uri,
		   unsigned contact_cnt,
		   const pj_str_t contacts[],
		   unsigned expires,
		   pj_bool_t leave_session,
		   pjsip_regc **p_regc)
{
    pjsip_regc *regc;
    unsigned i;
    const pj_str_t aor = pj_str("<sip:regc-test@pjsip.org>");
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  %s", title));

    /* Modify registrar settings */
    pj_memcpy(&registrar.cfg, srv_cfg, sizeof(*srv_cfg));

    pj_bzero(&client_result, sizeof(client_result));
    client_result.destroy_on_cb = client_cfg->destroy_on_cb;

    status = pjsip_regc_create(endpt, &client_result, &client_cb, &regc);
    if (status != PJ_SUCCESS)
	return -100;

    status = pjsip_regc_init(0, regc, registrar_uri, &aor, &aor, contact_cnt,
			     contacts, expires ? expires : 60);
    if (status != PJ_SUCCESS) {
	pjsip_regc_destroy(regc);
	return -110;
    }

    if (client_cfg->auth) {
	pjsip_cred_info cred;

	pj_bzero(&cred, sizeof(cred));
	cred.realm = pj_str("*");
	cred.scheme = pj_str("digest");
	cred.username = pj_str("user");
	cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	cred.data = pj_str("password");

	status = pjsip_regc_set_credentials(regc, 1, &cred);
	if (status != PJ_SUCCESS) {
	    pjsip_regc_destroy(regc);
	    return -115;
	}
    }

    /* Register */
    status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (status != PJ_SUCCESS) {
	pjsip_regc_destroy(regc);
	return -120;
    }
    status = pjsip_regc_send(regc, tdata);

    /* That's it, wait until the callback is sent */
    for (i=0; i<600 && !client_result.done; ++i) {
	flush_events(100);
    }

    if (!client_result.done) {
	PJ_LOG(3,(THIS_FILE, "    error: test has timed out"));
	pjsip_regc_destroy(regc);
	return -200;
    }

    /* Destroy the regc, we're done with the test, unless we're
     * instructed to leave the session open.
     */
    if (!leave_session && !client_cfg->destroy_on_cb)
	pjsip_regc_destroy(regc);

    /* Compare results with expected results */

    if (client_result.error != client_cfg->error) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting err=%d, got err=%d",
		  client_cfg->error, client_result.error));
	return -210;
    }
    if (client_result.code != client_cfg->code) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting code=%d, got code=%d",
		  client_cfg->code, client_result.code));
	return -220;
    }
    if (client_result.expiration != client_cfg->expiration) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting expiration=%d, got expiration=%d",
		  client_cfg->expiration, client_result.expiration));
	return -240;
    }
    if (client_result.contact_cnt != client_cfg->contact_cnt) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting contact_cnt=%d, got contact_cnt=%d",
		  client_cfg->contact_cnt, client_result.contact_cnt));
	return -250;
    }
    if (client_result.have_reg != client_cfg->have_reg) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting have_reg=%d, got have_reg=%d",
		  client_cfg->have_reg, client_result.have_reg));
	return -260;
    }
    if (client_result.interval != client_result.expiration) {
	PJ_LOG(3,(THIS_FILE, "    error: interval (%d) is different than expiration (%d)",
		  client_result.interval, client_result.expiration));
	return -270;
    }
    if (client_result.expiration > 0 && client_result.next_reg < 1) {
	PJ_LOG(3,(THIS_FILE, "    error: next_reg=%d, expecting positive number because expiration is %d",
		  client_result.next_reg, client_result.expiration));
	return -280;
    }

    /* Looks like everything is okay. */
    if (leave_session) {
	*p_regc = regc;
    }

    return 0;
}


/************************************************************************/
/* Customized tests */

/* Check that client is sending register refresh */
static int keep_alive_test(const pj_str_t *registrar_uri)
{
    enum { TIMEOUT = 40 };
    struct registrar_cfg server_cfg = 
	/* respond	code	auth	  contact  exp_prm expires more_contacts */
	{ PJ_TRUE,	200,	PJ_FALSE, EXACT,   TIMEOUT, 0,	    {NULL, 0}};
    struct client client_cfg = 
	/* error	code	have_reg    expiration	contact_cnt auth?    destroy*/
	{ PJ_FALSE,	200,	PJ_TRUE,   TIMEOUT,	1,	    PJ_FALSE,PJ_FALSE};
    pj_str_t contact = pj_str("<sip:c@C>");


    pjsip_regc *regc;
    unsigned i;
    int ret;

    ret = do_test("register refresh (takes ~40 secs)", &server_cfg, &client_cfg, registrar_uri,
		  1, &contact, TIMEOUT, PJ_TRUE, &regc);
    if (ret != 0)
	return ret;

    /* Reset server response_cnt */
    registrar.response_cnt = 0;

    /* Wait until keep-alive/refresh is done */
    for (i=0; i<(TIMEOUT-1)*10 && registrar.response_cnt==0; ++i) {
	flush_events(100);
    }

    if (registrar.response_cnt==0) {
	PJ_LOG(3,(THIS_FILE, "    error: no refresh is received"));
	return -400;
    }

    if (client_result.error) {
	PJ_LOG(3,(THIS_FILE, "    error: got error"));
	return -410;
    }
    if (client_result.code != 200) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting code=%d, got code=%d",
		  200, client_result.code));
	return -420;
    }
    if (client_result.expiration != TIMEOUT) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting expiration=%d, got expiration=%d",
		  TIMEOUT, client_result.expiration));
	return -440;
    }
    if (client_result.contact_cnt != 1) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting contact_cnt=%d, got contact_cnt=%d",
		  TIMEOUT, client_result.contact_cnt));
	return -450;
    }
    if (client_result.have_reg == 0) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting have_reg=%d, got have_reg=%d",
		  1, client_result.have_reg));
	return -460;
    }
    if (client_result.interval != TIMEOUT) {
	PJ_LOG(3,(THIS_FILE, "    error: interval (%d) is different than expiration (%d)",
		  client_result.interval, TIMEOUT));
	return -470;
    }
    if (client_result.expiration > 0 && client_result.next_reg < 1) {
	PJ_LOG(3,(THIS_FILE, "    error: next_reg=%d, expecting positive number because expiration is %d",
		  client_result.next_reg, client_result.expiration));
	return -480;
    }

    /* Success */
    pjsip_regc_destroy(regc);
    return 0;
}


/* Send error on refresh */
static int refresh_error(const pj_str_t *registrar_uri,
			 pj_bool_t destroy_on_cb)
{
    enum { TIMEOUT = 40 };
    struct registrar_cfg server_cfg = 
	/* respond	code	auth	  contact  exp_prm expires more_contacts */
	{ PJ_TRUE,	200,	PJ_FALSE, EXACT,   TIMEOUT, 0,	    {NULL, 0}};
    struct client client_cfg = 
	/* error	code	have_reg    expiration	contact_cnt auth?    destroy*/
	{ PJ_FALSE,	200,	PJ_TRUE,   TIMEOUT,	1,	    PJ_FALSE,PJ_FALSE};
    pj_str_t contact = pj_str("<sip:c@C>");

    pjsip_regc *regc;
    unsigned i;
    int ret;

    ret = do_test("refresh error (takes ~40 secs)", &server_cfg, &client_cfg, registrar_uri,
		  1, &contact, TIMEOUT, PJ_TRUE, &regc);
    if (ret != 0)
	return ret;

    /* Reset server response_cnt */
    registrar.response_cnt = 0;

    /* inject error for transmission */
    send_mod.count = 0;
    send_mod.count_before_reject = 0;

    /* reconfigure client */
    client_result.done = PJ_FALSE;
    client_result.destroy_on_cb = destroy_on_cb;

    /* Wait until keep-alive/refresh is done */
    for (i=0; i<TIMEOUT*10 && !client_result.done; ++i) {
	flush_events(100);
    }

    send_mod.count_before_reject = 0xFFFF;

    if (!destroy_on_cb)
	pjsip_regc_destroy(regc);

    if (!client_result.done) {
	PJ_LOG(3,(THIS_FILE, "    error: test has timed out"));
	return -500;
    }

    /* Expecting error */
    if (client_result.error==PJ_FALSE && client_result.code/100==2) {
	PJ_LOG(3,(THIS_FILE, "    error: expecting error got successfull result"));
	return -510;
    }

    return PJ_SUCCESS;
};


/* Send error on refresh */
static int update_test(const pj_str_t *registrar_uri)
{
    enum { TIMEOUT = 40 };
    struct registrar_cfg server_cfg = 
	/* respond	code	auth	  contact  exp_prm expires more_contacts */
	{ PJ_TRUE,	200,	PJ_FALSE, EXACT,   TIMEOUT, 0,	    {NULL, 0}};
    struct client client_cfg = 
	/* error	code	have_reg    expiration	contact_cnt auth?    destroy*/
	{ PJ_FALSE,	200,	PJ_TRUE,   TIMEOUT,	1,	    PJ_FALSE,PJ_FALSE};
    pj_str_t contacts[] = {
	{ "<sip:a>", 7 },
	{ "<sip:b>", 7 },
	{ "<sip:c>", 7 }
    };

    pjsip_regc *regc;
    pjsip_contact_hdr *h1, *h2;
    pjsip_sip_uri *u1, *u2;
    unsigned i;
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;
    int ret = 0;

    /* initially only has 1 contact */
    ret = do_test("update test", &server_cfg, &client_cfg, registrar_uri,
		  1, &contacts[0], TIMEOUT, PJ_TRUE, &regc);
    if (ret != 0) {
	return -600;
    }

    /*****
     * replace the contact with new one 
     */
    PJ_LOG(3,(THIS_FILE, "   replacing contact"));
    status = pjsip_regc_update_contact(0, regc, 1, &contacts[1]);
    if (status != PJ_SUCCESS) {
	ret = -610;
	goto on_return;
    }

    status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
    if (status != PJ_SUCCESS) {
	ret = -620;
	goto on_return;
    }

    /* Check that the REGISTER contains two Contacts:
     *  - <sip:a>;expires=0,
     *  - <sip:b>
     */
    h1 = (pjsip_contact_hdr*) 
	  pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, NULL);
    if (!h1) {
	ret = -630;
	goto on_return;
    }
    if ((void*)h1->next == (void*)&tdata->msg->hdr)
	h2 = NULL;
    else
	h2 = (pjsip_contact_hdr*) 
	      pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, h1->next);
    if (!h2) {
	ret = -640;
	goto on_return;
    }
    /* must not have other Contact header */
    if ((void*)h2->next != (void*)&tdata->msg->hdr &&
	pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, h2->next) != NULL)
    {
	ret = -645;
	goto on_return;
    }

    u1 = (pjsip_sip_uri*) pjsip_uri_get_uri(h1->uri);
    u2 = (pjsip_sip_uri*) pjsip_uri_get_uri(h2->uri);

    if (*u1->host.ptr == 'a') {
	if (h1->expires != 0) {
	    ret = -650;
	    goto on_return;
	}
	if (h2->expires == 0) {
	    ret = -660;
	    goto on_return;
	}

    } else {
	pj_assert(*u1->host.ptr == 'b');
	if (h1->expires == 0) {
	    ret = -670;
	    goto on_return;
	}
	if (h2->expires != 0) {
	    ret = -680;
	    goto on_return;
	}
    }

    /* Destroy tdata */
    pjsip_tx_data_dec_ref(tdata);
    tdata = NULL;



    /** 
     * First loop, it will update with more contacts. Second loop
     * should do nothing.
     */
    for (i=0; i<2; ++i) {
	if (i==0)
	    PJ_LOG(3,(THIS_FILE, "   replacing with more contacts"));
	else
	    PJ_LOG(3,(THIS_FILE, "   updating contacts with same contacts"));

	status = pjsip_regc_update_contact(0, regc, 2, &contacts[1]);
	if (status != PJ_SUCCESS) {
	    ret = -710;
	    goto on_return;
	}

	status = pjsip_regc_register(regc, PJ_TRUE, &tdata);
	if (status != PJ_SUCCESS) {
	    ret = -720;
	    goto on_return;
	}

	/* Check that the REGISTER contains two Contacts:
	 *  - <sip:b>
	 *  - <sip:c>
	 */
	h1 = (pjsip_contact_hdr*) 
	      pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, NULL);
	if (!h1) {
	    ret = -730;
	    goto on_return;
	}
	if ((void*)h1->next == (void*)&tdata->msg->hdr)
	    h2 = NULL;
	else
	    h2 = (pjsip_contact_hdr*) 
		  pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, h1->next);
	if (!h2) {
	    ret = -740;
	    goto on_return;
	}
	/* must not have other Contact header */
	if ((void*)h2->next != (void*)&tdata->msg->hdr &&
	    pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, h2->next) != NULL)
	{
	    ret = -745;
	    goto on_return;
	}

	/* both contacts must not have expires=0 parameter */
	if (h1->expires == 0) {
	    ret = -750;
	    goto on_return;
	}
	if (h2->expires == 0) {
	    ret = -760;
	    goto on_return;
	}

	/* Destroy tdata */
	pjsip_tx_data_dec_ref(tdata);
	tdata = NULL;
    }

on_return:
    if (tdata) pjsip_tx_data_dec_ref(tdata);
    pjsip_regc_destroy(regc);
    return ret;
};


/* send error on authentication */
static int auth_send_error(const pj_str_t *registrar_uri,
			   pj_bool_t destroy_on_cb)
{
    enum { TIMEOUT = 40 };
    struct registrar_cfg server_cfg = 
	/* respond	code	auth	  contact  exp_prm expires more_contacts */
	{ PJ_TRUE,	200,	PJ_TRUE,  EXACT,   75,	    0,	    {NULL, 0}};
    struct client client_cfg = 
	/* error	code	have_reg    expiration	contact_cnt auth?    destroy*/
	{ PJ_TRUE,	401,	PJ_FALSE, -1,		0,	    PJ_TRUE, PJ_TRUE};
    pj_str_t contact = pj_str("<sip:c@C>");

    pjsip_regc *regc;
    int ret;

    client_cfg.destroy_on_cb = destroy_on_cb;

    /* inject error for second request retry */
    send_mod.count = 0;
    send_mod.count_before_reject = 1;

    ret = do_test("auth send error", &server_cfg, &client_cfg, registrar_uri,
		  1, &contact, TIMEOUT, PJ_TRUE, &regc);

    send_mod.count_before_reject = 0xFFFF;

    return ret;
};




/************************************************************************/
enum
{
    OFF	    = 1,
    ON	    = 2,
    ON_OFF  = 3,
};

int regc_test(void)
{
    struct test_rec {
	unsigned		 check_contact;
	unsigned		 add_xuid_param;

	const char		*title;
	char			*alt_registrar;
	unsigned		 contact_cnt;
	char			*contacts[4];
	unsigned		 expires;
	struct registrar_cfg	 server_cfg;
	struct client		 client_cfg;
    } test_rec[] = 
    {
	/* immediate error */
	{
	    OFF,			    /* check_contact	*/
	    OFF,			    /* add_xuid_param	*/
	    "immediate error",		    /* title		*/
	    "sip:unresolved-host-xyy",	    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "sip:user@127.0.0.1:5060" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact   exp_prm expires more_contacts */
	    { PJ_FALSE,	200,	PJ_FALSE, NONE,	    0,	    0,	    {NULL, 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	502,	PJ_FALSE,   -1,		0,	    PJ_FALSE}
	},

	/* timeout test */
	{
	    OFF,			    /* check_contact	*/
	    OFF,			    /* add_xuid_param	*/
	    "timeout test (takes ~32 secs)",/* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "sip:user@127.0.0.1:5060" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact   exp_prm expires more_contacts */
	    { PJ_FALSE,	200,	PJ_FALSE, NONE,	    0,	    0,	    {NULL, 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth? */
	    { PJ_FALSE,	408,	PJ_FALSE,   -1,		0,	    PJ_FALSE}
	},

	/* Basic successful registration scenario:
	 * a good registrar returns the Contact header as is and
	 * add expires parameter. In this test no additional bindings
	 * are returned.
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON_OFF,			    /* add_xuid_param	*/
	    "basic",			    /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060;transport=udp;x-param=1234>" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact  exp_prm expires more_contacts */
	    { PJ_TRUE,	200,	PJ_FALSE, EXACT,    75,	    65,	    {NULL, 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	200,	PJ_TRUE,   75,		1,	    PJ_FALSE}
	},

	/* Basic successful registration scenario with authentication
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON_OFF,			    /* add_xuid_param	*/
	    "authentication",		    /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060;transport=udp;x-param=1234>" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact  exp_prm expires more_contacts */
	    { PJ_TRUE,	200,	PJ_TRUE,  EXACT,    75,	    65,	    {NULL, 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	200,	PJ_TRUE,   75,		1,	    PJ_TRUE}
	},

	/* a good registrar returns the Contact header as is and
	 * add expires parameter. Also it adds bindings from other
	 * clients in this test.
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON,				    /* add_xuid_param	*/
	    "more bindings in response",    /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060;transport=udp>" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact  exp_prm expires more_contacts */
	    { PJ_TRUE,	200,	PJ_FALSE, EXACT,    75,	    65,	    {"<sip:a@a>;expires=70", 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	200,	PJ_TRUE,   75,		2,	    PJ_FALSE}
	},


	/* a bad registrar returns modified Contact header, but it
	 * still returns all parameters intact. In this case
	 * the expiration is taken from the expires param because
	 * of matching xuid param or because the number of
	 * Contact header matches.
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON_OFF,			    /* add_xuid_param	*/
	    "registrar modifies Contact header",    /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060>" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact   exp_prm expires more_contacts */
	    { PJ_TRUE,	200,	PJ_FALSE, MODIFIED, 75,	    65,	    {NULL, 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	200,	PJ_TRUE,   75,		1,	    PJ_FALSE}
	},


	/* a bad registrar returns modified Contact header, but it
	 * still returns all parameters intact. In addition it returns
	 * bindings from other clients.
	 *
	 * In this case the expiration is taken from the expires param 
	 * because add_xuid_param is enabled.
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON,				    /* add_xuid_param	*/
	    "registrar modifies Contact header and add bindings",    /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060>" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact   exp_prm expires more_contacts */
	    { PJ_TRUE,	200,	PJ_FALSE, MODIFIED, 75,	    65,	    {"<sip:a@a>;expires=70", 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	200,	PJ_TRUE,   75,		2,	    PJ_FALSE}
	},


	/* a bad registrar returns completely different Contact and
	 * all parameters are gone. In this case the expiration is 
	 * also taken from the expires param since the number of 
	 * header matches.
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON_OFF,			    /* add_xuid_param	*/
	    "registrar replaces Contact header",    /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060>" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact   exp_prm expires more_contacts */
	    { PJ_TRUE,	202,	PJ_FALSE, NONE,	    0,	    65,	    {"<sip:a@A>;expires=75", 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	202,	PJ_TRUE,   75,		1,	    PJ_FALSE}
	},


	/* a bad registrar returns completely different Contact (and
	 * all parameters are gone) and it also includes bindings from
	 * other clients.
	 * In this case the expiration is taken from the Expires header.
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON_OFF,			    /* add_xuid_param	*/
	    " as above with additional bindings",    /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060>" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact   exp_prm expires more_contacts */
	    { PJ_TRUE,	200,	PJ_FALSE, NONE,	    0,	    65,	    {"<sip:a@A>;expires=75, <sip:b@B;expires=70>", 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	200,	PJ_TRUE,   65,		2,	    PJ_FALSE}
	},

	/* the registrar doesn't return any bindings, but for some
	 * reason it includes an Expires header.
	 * In this case the expiration is taken from the Expires header.
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON_OFF,			    /* add_xuid_param	*/
	    "no Contact but with Expires",  /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060>" },  /* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact   exp_prm expires more_contacts */
	    { PJ_TRUE,	200,	PJ_FALSE, NONE,	    0,	    65,	    {NULL, 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	200,	PJ_TRUE,   65,		0,	    PJ_FALSE}
	},

	/* Neither Contact header nor Expires header are present.
	 * In this case the expiration is taken from the request.
	 */
	{
	    ON_OFF,			    /* check_contact	*/
	    ON_OFF,			    /* add_xuid_param	*/
	    "no Contact and no Expires",    /* title		*/
	    NULL,			    /* alt_registrar	*/
	    1,				    /* contact cnt	*/
	    { "<sip:user@127.0.0.1:5060>" },/* contacts[]	*/
	    600,			    /* expires		*/

	    /* registrar config: */
	    /* respond	code	auth	  contact   exp_prm expires more_contacts */
	    { PJ_TRUE,	200,	PJ_FALSE, NONE,	    0,	    0,	    {NULL, 0}},

	    /* client expected results: */
	    /* error	code	have_reg    expiration	contact_cnt auth?*/
	    { PJ_FALSE,	200,	PJ_TRUE,   600,		0,	    PJ_FALSE}
	},
    };

    unsigned i;
    pj_sockaddr_in addr;
    pjsip_transport *udp = NULL;
    pj_uint16_t port; 
    char registrar_uri_buf[80];
    pj_str_t registrar_uri;
    int rc = 0;

    pj_sockaddr_in_init(&addr, 0, 0);

    /* Acquire existing transport, if any */
    rc = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_UDP, &addr, sizeof(addr), NULL, &udp);
    if (rc == PJ_SUCCESS) {
	port = pj_sockaddr_get_port(&udp->local_addr);
	pjsip_transport_dec_ref(udp);
	udp = NULL;
    } else {
	rc = pjsip_udp_transport_start(endpt, NULL, NULL, 1, &udp);
	if (rc != PJ_SUCCESS) {
	    app_perror("   error creating UDP transport", rc);
	    rc = -2;
	    goto on_return;
	}

	port = pj_sockaddr_get_port(&udp->local_addr);
    }

    /* Register registrar module */
    rc = pjsip_endpt_register_module(endpt, &registrar.mod);
    if (rc != PJ_SUCCESS) {
	app_perror("   error registering module", rc);
	rc = -3;
	goto on_return;
    }

    /* Register send module */
    rc = pjsip_endpt_register_module(endpt, &send_mod.mod);
    if (rc != PJ_SUCCESS) {
	app_perror("   error registering module", rc);
	rc = -3;
	goto on_return;
    }

    pj_ansi_snprintf(registrar_uri_buf, sizeof(registrar_uri_buf),
		    "sip:127.0.0.1:%d", (int)port);
    registrar_uri = pj_str(registrar_uri_buf);

    for (i=0; i<PJ_ARRAY_SIZE(test_rec); ++i) {
	struct test_rec *t = &test_rec[i];
	unsigned j, x;
	pj_str_t reg_uri;
	pj_str_t contacts[8];

	/* Fill in the registrar address if it's not specified */
	if (t->alt_registrar == NULL) {
	    reg_uri = registrar_uri;
	} else {
	    reg_uri = pj_str(t->alt_registrar);
	}

	/* Build contact pj_str_t's */
	for (j=0; j<t->contact_cnt; ++j) {
	    contacts[j] = pj_str(t->contacts[j]);
	}

	/* Normalize more_contacts field */
	if (t->server_cfg.more_contacts.ptr)
	    t->server_cfg.more_contacts.slen = strlen(t->server_cfg.more_contacts.ptr);

	/* Do tests with three combinations:
	 *  - check_contact on/off
	 *  - add_xuid_param on/off
	 *  - destroy_on_callback on/off
	 */
	for (x=1; x<=2; ++x) {
	    unsigned y;

	    if ((t->check_contact & x) == 0)
		continue;

	    pjsip_cfg()->regc.check_contact = (x-1);

	    for (y=1; y<=2; ++y) {
		unsigned z;

		if ((t->add_xuid_param & y) == 0)
		    continue;

		pjsip_cfg()->regc.add_xuid_param = (y-1);

		for (z=0; z<=1; ++z) {
		    char new_title[200];

		    t->client_cfg.destroy_on_cb = z;

		    sprintf(new_title, "%s [check=%d, xuid=%d, destroy=%d]", 
			    t->title, pjsip_cfg()->regc.check_contact,
			    pjsip_cfg()->regc.add_xuid_param, z);
		    rc = do_test(new_title, &t->server_cfg, &t->client_cfg, 
				 &reg_uri, t->contact_cnt, contacts, 
				 t->expires, PJ_FALSE, NULL);
		    if (rc != 0)
			goto on_return;
		}

	    }
	}

	/* Sleep between test groups to avoid using up too many
	 * active transactions.
	 */
	pj_thread_sleep(1000);
    }

    /* keep-alive test */
    rc = keep_alive_test(&registrar_uri);
    if (rc != 0)
	goto on_return;

    /* Send error on refresh without destroy on callback */
    rc = refresh_error(&registrar_uri, PJ_FALSE);
    if (rc != 0)
	goto on_return;

    /* Send error on refresh, destroy on callback */
    rc = refresh_error(&registrar_uri, PJ_TRUE);
    if (rc != 0)
	goto on_return;

    /* Updating contact */
    rc = update_test(&registrar_uri);
    if (rc != 0)
	goto on_return;

    /* Send error during auth, don't destroy on callback */
    rc = auth_send_error(&registrar_uri, PJ_FALSE);
    if (rc != 0)
	goto on_return;

    /* Send error during auth, destroy on callback */
    rc = auth_send_error(&registrar_uri, PJ_FALSE);
    if (rc != 0)
	goto on_return;

on_return:
    if (registrar.mod.id != -1) {
	pjsip_endpt_unregister_module(endpt, &registrar.mod);
    }
    if (send_mod.mod.id != -1) {
	pjsip_endpt_unregister_module(endpt, &send_mod.mod);
    }
    if (udp) {
	pjsip_transport_dec_ref(udp);
    }
    return rc;
}


