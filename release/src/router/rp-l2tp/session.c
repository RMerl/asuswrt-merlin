/***********************************************************************
*
* session.c
*
* Code for managing L2TP sessions
*
* Copyright (C) 2001 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: session.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "l2tp.h"
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static uint16_t session_make_sid(l2tp_tunnel *tunnel);
static void session_set_state(l2tp_session *session, int state);
static void session_send_sli(l2tp_session *session);

/* Registered LNS incoming-call handlers */
static l2tp_lns_handler *lns_handlers = NULL;

/* Registered LAC handlers */
static l2tp_lac_handler *lac_handlers = NULL;

static uint32_t call_serial_number = 0;

static char *state_names[] = {
    "idle", "wait-tunnel", "wait-reply", "wait-connect", "established"
};

/**********************************************************************
* %FUNCTION: session_compute_hash
* %ARGUMENTS:
*  data -- a session
* %RETURNS:
*  The session ID
* %DESCRIPTION:
*  Returns a hash value for a session
***********************************************************************/
static unsigned int
session_compute_hash(void *data)
{
    return (unsigned int) ((l2tp_session *) data)->my_id;
}


/**********************************************************************
* %FUNCTION: session_compare
* %ARGUMENTS:
*  d1, d2 -- two sessions
* %RETURNS:
*  0 if sids are equal, non-zero otherwise
* %DESCRIPTION:
*  Compares two sessions
***********************************************************************/
static int
session_compare(void *d1, void *d2)
{
    return ((l2tp_session *) d1)->my_id != ((l2tp_session *) d2)->my_id;
}

/**********************************************************************
* %FUNCTION: session_hash_init
* %ARGUMENTS:
*  tab -- hash table
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Initializes hash table of sessions
***********************************************************************/
void
l2tp_session_hash_init(hash_table *tab)
{
    hash_init(tab, offsetof(l2tp_session, hash_by_my_id),
	      session_compute_hash, session_compare);
}

/**********************************************************************
* %FUNCTION: session_free
* %ARGUMENTS:
*  ses -- a session to free
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Frees a session, closing down all resources associated with it.
***********************************************************************/
void
l2tp_session_free(l2tp_session *ses, char const *reason, int may_reestablish)
{
    session_set_state(ses, SESSION_IDLE);
    DBG(l2tp_db(DBG_SESSION, "session_free(%s) %s\n",
	   l2tp_debug_session_to_str(ses), reason));
    if (ses->call_ops && ses->call_ops->close) {
	ses->call_ops->close(ses, reason, may_reestablish);
    }
    memset(ses, 0, sizeof(l2tp_session));
    free(ses);
}

/**********************************************************************
* %FUNCTION: session_call_lns
* %ARGUMENTS:
*  peer -- L2TP peer
*  calling_number -- calling phone number (or MAC address or whatever...)
*  private -- private data to be stored in session structure
* %RETURNS:
*  A newly-allocated session, or NULL if session could not be created
* %DESCRIPTION:
*  Initiates session setup.  Once call is active, established() will be
*  called.
***********************************************************************/
l2tp_session *
l2tp_session_call_lns(l2tp_peer *peer,
		      char const *calling_number,
		      EventSelector *es,
		      void *private)
{
    l2tp_session *ses;
    l2tp_tunnel *tunnel;

    /* Find a tunnel to the peer */
    tunnel = l2tp_tunnel_find_for_peer(peer, es);
    if (!tunnel) return NULL;

    /* Do we have call ops? */
    if (!peer->lac_ops) {
	l2tp_set_errmsg("Cannot act as LAC for peer");
	return NULL;
    }

    /* Allocate session */
    ses = malloc(sizeof(l2tp_session));
    if (!ses) {
	l2tp_set_errmsg("session_call_lns: out of memory");
	return NULL;
    }

    /* Init fields */
    memset(ses, 0, sizeof(l2tp_session));
    ses->we_are_lac = 1;
    ses->tunnel = tunnel;
    ses->my_id = session_make_sid(tunnel);
    ses->call_ops = peer->lac_ops;
    ses->state = SESSION_WAIT_TUNNEL;
    strncpy(ses->calling_number, calling_number, MAX_HOSTNAME);
    ses->calling_number[MAX_HOSTNAME-1] = 0;
    ses->private = private;
    ses->snooping = 1;
    ses->send_accm = 0xFFFFFFFF;
    ses->recv_accm = 0xFFFFFFFF;

    /* Add it to the tunnel */
    l2tp_tunnel_add_session(ses);

    return ses;
}

/**********************************************************************
* %FUNCTION: session_make_sid
* %ARGUMENTS:
*  tunnel -- an L2TP tunnel
* %RETURNS:
*  An unused random session ID
***********************************************************************/
static uint16_t
session_make_sid(l2tp_tunnel *tunnel)
{
    uint16_t sid;
    while(1) {
	L2TP_RANDOM_FILL(sid);
	if (!sid) continue;
	if (!l2tp_tunnel_find_session(tunnel, sid)) return sid;
    }
}

/**********************************************************************
* %FUNCTION: session_notify_open
* %ARGUMENTS:
*  ses -- an L2TP session
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Called when tunnel has been established
***********************************************************************/
void
l2tp_session_notify_tunnel_open(l2tp_session *ses)
{
    uint16_t u16;
    uint32_t u32;
    l2tp_dgram *dgram;
    l2tp_tunnel *tunnel = ses->tunnel;

    if (ses->state != SESSION_WAIT_TUNNEL) return;

    /* Send ICRQ */
    DBG(l2tp_db(DBG_SESSION, "Session %s tunnel open\n",
	   l2tp_debug_session_to_str(ses)));

    dgram = l2tp_dgram_new_control(MESSAGE_ICRQ, ses->tunnel->assigned_id,
			      0);
    if (!dgram) {
	l2tp_set_errmsg("Could not establish session - out of memory");
	l2tp_tunnel_delete_session(ses, "Out of memory", 0);
	return;
    }

    /* assigned session ID */
    u16 = htons(ses->my_id);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_ASSIGNED_SESSION_ID, &u16);

    /* Call serial number */
    u32 = htonl(call_serial_number);
    call_serial_number++;
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u32), VENDOR_IETF, AVP_CALL_SERIAL_NUMBER, &u32);

    /* Ship it out */
    l2tp_tunnel_xmit_control_message(tunnel, dgram);

    session_set_state(ses, SESSION_WAIT_REPLY);
}

/**********************************************************************
* %FUNCTION: session_set_state
* %ARGUMENTS:
*  session -- the session
*  state -- new state
* %RETURNS:
*  Nothing
***********************************************************************/
static void
session_set_state(l2tp_session *session, int state)
{
    if (state == session->state) return;
    DBG(l2tp_db(DBG_SESSION, "session(%s) state %s -> %s\n",
	   l2tp_debug_session_to_str(session),
	   state_names[session->state],
	   state_names[state]));
    session->state = state;
}

/**********************************************************************
* %FUNCTION: session_register_lns_handler
* %ARGUMENTS:
*  handler -- LNS handler
* %RETURNS:
*  -1 on error, 0 if all is OK
* %DESCRIPTION:
*  Registers an LNS handler for incoming call requests
***********************************************************************/
int
l2tp_session_register_lns_handler(l2tp_lns_handler *handler)
{
    l2tp_lns_handler *prev = lns_handlers;

    if (l2tp_session_find_lns_handler(handler->handler_name)) {
	l2tp_set_errmsg("LNS Handler named %s already exists",
		   handler->handler_name);
	return -1;
    }
    /* Add to end of handler list */
    handler->next = NULL;
    if (!prev) {
	lns_handlers = handler;
	return 0;
    }
    while (prev->next) {
	prev = prev->next;
    }
    prev->next = handler;
    return 0;
}

/**********************************************************************
* %FUNCTION: session_register_lac_handler
* %ARGUMENTS:
*  handler -- LAC handler
* %RETURNS:
*  -1 on error, 0 if all is OK
* %DESCRIPTION:
*  Registers an LAC handler for incoming call requests
***********************************************************************/
int
l2tp_session_register_lac_handler(l2tp_lac_handler *handler)
{
    l2tp_lac_handler *prev = lac_handlers;

    if (l2tp_session_find_lac_handler(handler->handler_name)) {
	l2tp_set_errmsg("LAC Handler named %s already exists",
		   handler->handler_name);
	return -1;
    }
    /* Add to end of handler list */
    handler->next = NULL;
    if (!prev) {
	lac_handlers = handler;
	return 0;
    }
    while (prev->next) {
	prev = prev->next;
    }
    prev->next = handler;
    return 0;
}

/**********************************************************************
* %FUNCTION: session_send_CDN
* %ARGUMENTS:
*  ses -- which session to terminate
*  result_code -- result code
*  error_code -- error code
*  fmt -- printf-style format string for error message
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sends CDN with specified result code and message.
***********************************************************************/
void
l2tp_session_send_CDN(l2tp_session *ses,
		      int result_code,
		      int error_code,
		      char const *fmt, ...)
{
    char buf[256];
    va_list ap;
    l2tp_tunnel *tunnel = ses->tunnel;
    uint16_t len;
    l2tp_dgram *dgram;
    uint16_t u16;

    /* Build the buffer for the result-code AVP */
    buf[0] = result_code / 256;
    buf[1] = result_code & 255;
    buf[2] = error_code / 256;
    buf[3] = error_code & 255;

    va_start(ap, fmt);
    vsnprintf(buf+4, 256-4, fmt, ap);
    buf[255] = 0;
    va_end(ap);

    DBG(l2tp_db(DBG_SESSION, "session_send_CDN(%s): %s\n",
	   l2tp_debug_session_to_str(ses), buf+4));

    len = 4 + strlen(buf+4);
    /* Build the datagram */
    dgram = l2tp_dgram_new_control(MESSAGE_CDN, tunnel->assigned_id,
			      ses->assigned_id);
    if (!dgram) return;

    /* Add assigned session ID */
    u16 = htons(ses->my_id);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_ASSIGNED_SESSION_ID, &u16);

    /* Add result code */
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  len, VENDOR_IETF, AVP_RESULT_CODE, buf);

    /* TODO: Clean up */
    session_set_state(ses, SESSION_IDLE);

    /* Ship it out */
    l2tp_tunnel_xmit_control_message(tunnel, dgram);

    /* Free session */
    l2tp_tunnel_delete_session(ses, buf+4, 0);
}

/**********************************************************************
* %FUNCTION: session_lns_handle_incoming_call
* %ARGUMENTS:
*  tunnel -- tunnel on which ICRQ arrived
*  sid -- assigned session ID
*  dgram -- the ICRQ datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles ICRQ.  If we find an LNS handler willing to take the call,
*  send ICRP.  Otherwise, send CDN.
***********************************************************************/
void
l2tp_session_lns_handle_incoming_call(l2tp_tunnel *tunnel,
				      uint16_t sid,
				      l2tp_dgram *dgram,
				      char const *calling_number)
{
    l2tp_call_ops *ops = tunnel->peer->lns_ops;
    l2tp_session *ses;
    uint16_t u16;

    /* Allocate a session */
    ses = malloc(sizeof(l2tp_session));
    if (!ses) {
	l2tp_set_errmsg("session_lns_handle_incoming_call: out of memory");
	return;
    }
    /* Init fields */
    memset(ses, 0, sizeof(l2tp_session));
    ses->we_are_lac = 0;
    ses->tunnel = tunnel;
    ses->my_id = session_make_sid(tunnel);
    ses->assigned_id = sid;
    ses->state = SESSION_IDLE;
    strncpy(ses->calling_number, calling_number, MAX_HOSTNAME);
    ses->calling_number[MAX_HOSTNAME-1] = 0;
    ses->private = NULL;
    ses->snooping = 1;
    ses->send_accm = 0xFFFFFFFF;
    ses->recv_accm = 0xFFFFFFFF;

    l2tp_tunnel_add_session(ses);

    if (!ops || !ops->establish) {
	l2tp_session_send_CDN(ses, RESULT_GENERAL_ERROR,
			 ERROR_OUT_OF_RESOURCES,
			 "No LNS handler willing to accept call");
	return;
    }

    ses->call_ops = ops;

    /* Send ICRP */
    dgram = l2tp_dgram_new_control(MESSAGE_ICRP, ses->tunnel->assigned_id,
			      ses->assigned_id);
    if (!dgram) {
	/* Ugh... not much chance of this working... */
	l2tp_session_send_CDN(ses, RESULT_GENERAL_ERROR, ERROR_OUT_OF_RESOURCES,
			 "Out of memory");
	return;
    }

    /* Add assigned session ID */
    u16 = htons(ses->my_id);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_ASSIGNED_SESSION_ID, &u16);

    /* Set session state */
    session_set_state(ses, SESSION_WAIT_CONNECT);

    /* Ship ICRP */
    l2tp_tunnel_xmit_control_message(tunnel, dgram);
}

/**********************************************************************
* %FUNCTION: session_handle_CDN
* %ARGUMENTS:
*  ses -- the session
*  dgram -- the datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles a CDN by destroying session
***********************************************************************/
void
l2tp_session_handle_CDN(l2tp_session *ses,
			l2tp_dgram *dgram)
{
    char buf[1024];
    unsigned char *val;
    uint16_t len;
    val = l2tp_dgram_search_avp(dgram, ses->tunnel, NULL, NULL, &len,
				VENDOR_IETF, AVP_RESULT_CODE);
    if (!val || len < 4) {
	l2tp_tunnel_delete_session(ses, "Received CDN", 1);
    } else {
	uint16_t result_code, error_code;
	char *msg;
	result_code = ((uint16_t) val[0]) * 256 + (uint16_t) val[1];
	error_code = ((uint16_t) val[2]) * 256 + (uint16_t) val[3];
	if (len > 4) {
	    msg = (char *) &val[4];
	} else {
	    msg = "";
	}
	snprintf(buf, sizeof(buf), "Received CDN: result-code = %d, error-code = %d, message = '%.*s'", result_code, error_code, (int) len-4, msg);
	buf[1023] = 0;
	l2tp_tunnel_delete_session(ses, buf, 1);
    }
}

/**********************************************************************
* %FUNCTION: session_handle_ICRP
* %ARGUMENTS:
*  ses -- the session
*  dgram -- the datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles an ICRP
***********************************************************************/
void
l2tp_session_handle_ICRP(l2tp_session *ses,
			 l2tp_dgram *dgram)
{
    uint16_t u16;
    unsigned char *val;
    uint16_t len;
    uint32_t u32;

    int mandatory, hidden;
    l2tp_tunnel *tunnel = ses->tunnel;

    /* Get assigned session ID */
    val = l2tp_dgram_search_avp(dgram, tunnel, &mandatory, &hidden, &len,
			   VENDOR_IETF, AVP_ASSIGNED_SESSION_ID);
    if (!val) {
	l2tp_set_errmsg("No assigned session-ID in ICRP");
	return;
    }
    if (!l2tp_dgram_validate_avp(VENDOR_IETF, AVP_ASSIGNED_SESSION_ID,
			    len, mandatory)) {
	l2tp_set_errmsg("Invalid assigned session-ID in ICRP");
	return;
    }

    /* Set assigned session ID */
    u16 = ((uint16_t) val[0]) * 256 + (uint16_t) val[1];

    if (!u16) {
	l2tp_set_errmsg("Invalid assigned session-ID in ICRP");
	return;
    }

    ses->assigned_id = u16;

    /* If state is not WAIT_REPLY, fubar */
    if (ses->state != SESSION_WAIT_REPLY) {
	l2tp_session_send_CDN(ses, RESULT_FSM_ERROR, 0, "Received ICRP for session in state %s", state_names[ses->state]);
	return;
    }

    /* Tell PPP code that call has been established */
    if (ses->call_ops && ses->call_ops->establish) {
	if (ses->call_ops->establish(ses) < 0) {
	    l2tp_session_send_CDN(ses, RESULT_GENERAL_ERROR, ERROR_VENDOR_SPECIFIC,
			     "%s", l2tp_get_errmsg());
	    return;
	}
    }

    /* Send ICCN */
    dgram = l2tp_dgram_new_control(MESSAGE_ICCN, tunnel->assigned_id,
			      ses->assigned_id);
    if (!dgram) {
	/* Ugh... not much chance of this working... */
	l2tp_session_send_CDN(ses, RESULT_GENERAL_ERROR, ERROR_OUT_OF_RESOURCES,
			 "Out of memory");
	return;
    }

    /* TODO: Speed, etc. are faked for now. */

    /* Connect speed */
    u32 = htonl(100000000);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u32), VENDOR_IETF, AVP_TX_CONNECT_SPEED, &u32);

    /* Framing Type */
    u32 = htonl(1);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u32), VENDOR_IETF, AVP_FRAMING_TYPE, &u32);

    /* Ship it out */
    l2tp_tunnel_xmit_control_message(tunnel, dgram);

    /* Set session state */
    session_set_state(ses, SESSION_ESTABLISHED);
    ses->tunnel->peer->fail = 0;

}

/**********************************************************************
* %FUNCTION: session_handle_ICCN
* %ARGUMENTS:
*  ses -- the session
*  dgram -- the datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles an ICCN
***********************************************************************/
void
l2tp_session_handle_ICCN(l2tp_session *ses,
			 l2tp_dgram *dgram)
{
    unsigned char *val;
    int mandatory, hidden;
    uint16_t len, vendor, type;
    int err = 0;

    l2tp_tunnel *tunnel = ses->tunnel;

    /* If state is not WAIT_CONNECT, fubar */
    if (ses->state != SESSION_WAIT_CONNECT) {
	l2tp_session_send_CDN(ses, RESULT_FSM_ERROR, 0,
			 "Received ICCN for session in state %s",
			 state_names[ses->state]);
	return;
    }

    /* Set session state */
    session_set_state(ses, SESSION_ESTABLISHED);
    ses->tunnel->peer->fail = 0;

    /* Pull out and examine AVP's */
    while(1) {
	val = l2tp_dgram_pull_avp(dgram, tunnel, &mandatory, &hidden,
				  &len, &vendor, &type, &err);
	if (!val) {
	    if (err) {
		l2tp_session_send_CDN(ses, RESULT_GENERAL_ERROR,
				      ERROR_BAD_VALUE, "%s", l2tp_get_errmsg());
		return;
	    }
	    break;
	}
	if (vendor != VENDOR_IETF) {
	    if (!mandatory) continue;
	    l2tp_session_send_CDN(ses, RESULT_GENERAL_ERROR,
				  ERROR_UNKNOWN_AVP_WITH_M_BIT,
				  "Unknown mandatory attribute (vendor=%d, type=%d) in %s",
				  (int) vendor, (int) type,
				  l2tp_debug_avp_type_to_str(dgram->msg_type));
	    return;
	}
	switch(type) {
	case AVP_SEQUENCING_REQUIRED:
	    ses->sequencing_required = 1;
	    break;
	}

    }

    /* Start the call */
    if (ses->call_ops->establish(ses) < 0) {
	l2tp_session_send_CDN(ses, RESULT_GENERAL_ERROR,
			 ERROR_OUT_OF_RESOURCES,
			 "No LNS handler willing to accept call");
	return;
    }

}

/**********************************************************************
* %FUNCTION: session_find_lns_handler
* %ARGUMENTS:
*  name -- name of handler
* %RETURNS:
*  Pointer to the handler if found, NULL if not
* %DESCRIPTION:
*  Searches for an LNS handler by name
***********************************************************************/
l2tp_lns_handler *
l2tp_session_find_lns_handler(char const *name)
{
    l2tp_lns_handler *cur = lns_handlers;
    while(cur) {
	if (!strcmp(name, cur->handler_name)) return cur;
	cur = cur->next;
    }
    return NULL;
}

/**********************************************************************
* %FUNCTION: session_find_lac_handler
* %ARGUMENTS:
*  name -- name of handler
* %RETURNS:
*  Pointer to the handler if found, NULL if not
* %DESCRIPTION:
*  Searches for an LAC handler by name
***********************************************************************/
l2tp_lac_handler *
l2tp_session_find_lac_handler(char const *name)
{
    l2tp_lac_handler *cur = lac_handlers;
    while(cur) {
	if (!strcmp(name, cur->handler_name)) return cur;
	cur = cur->next;
    }
    return NULL;
}

/**********************************************************************
* %FUNCTION: l2tp_session_state_name
* %ARGUMENTS:
*  ses -- the session
* %RETURNS:
*  The name of the session's state
***********************************************************************/
char const *
l2tp_session_state_name(l2tp_session *ses)
{
    return state_names[ses->state];
}

/**********************************************************************
* %FUNCTION: l2tp_session_lcp_snoop
* %ARGUMENTS:
*  ses -- L2TP session structure
*  buf -- PPP frame
*  len -- length of PPP frame
*  incoming -- if 1, frame is coming from L2TP tunnel.  If 0, frame is
*              going to L2TP tunnel.
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Snoops on LCP negotiation.  Used to send SLI to LAC if we're an LNS.
***********************************************************************/
void
l2tp_session_lcp_snoop(l2tp_session *ses,
		       unsigned char const *buf,
		       int len,
		       int incoming)
{
    unsigned int protocol;
    int stated_len;
    int opt, opt_len;
    int reject;
    unsigned char const *opt_data;
    uint32_t accm;

    /* If we are LAC, do not snoop */
    if (ses->we_are_lac) {
	DBG(l2tp_db(DBG_SNOOP, "Turning off snooping: We are LAC.\n"));
	ses->snooping = 0;
	return;
    }

    /* Get protocol */
    if (buf[0] & 0x01) {
	/* Compressed protcol field */
	protocol = buf[0];
    } else {
	protocol = ((unsigned int) buf[0]) * 256 + buf[1];
    }

    /* If it's a network protocol, stop snooping */
    if (protocol <= 0x3fff) {
	DBG(l2tp_db(DBG_SNOOP, "Turning off snooping: Network protocol %04x found.\n", protocol));
	ses->snooping = 0;
	return;
    }

    /* If it's not LCP, do not snoop */
    if (protocol != 0xc021) {
	return;
    }

    /* Skip protocol; go to packet data */
    buf += 2;
    len -= 2;

    /* Unreasonably short frame?? */
    if (len <= 0) return;

    /* Look for Configure-Ack or Configure-Reject code */
    if (buf[0] != 2 && buf[0] != 4) return;

    reject = (buf[0] == 4);

    stated_len = ((unsigned int) buf[2]) * 256 + buf[3];

    /* Something fishy with length field? */
    if (stated_len > len) return;

    /* Skip to options */
    len = stated_len - 4;
    buf += 4;

    while (len > 0) {
	/* Pull off an option */
	opt = buf[0];
	opt_len = buf[1];
	opt_data = &buf[2];
	if (opt_len > len || opt_len < 2) break;
	len -= opt_len;
	buf += opt_len;
	DBG(l2tp_db(DBG_SNOOP, "Found option type %02x; len %d\n",
		    opt, opt_len));
	/* We are specifically interested in ACCM */
	if (opt == 0x02 && opt_len == 0x06) {
	    if (reject) {
		/* ACCM negotiation REJECTED; use default */
		accm = 0xFFFFFFFF;
		DBG(l2tp_db(DBG_SNOOP, "Rejected ACCM negotiation; defaulting (%s)\n", incoming ? "incoming" : "outgoing"));
		/* ??? Is this right? */
		ses->recv_accm = accm;
		ses->send_accm = accm;
		ses->got_recv_accm = 1;
		ses->got_send_accm = 1;
	    } else {
		memcpy(&accm, opt_data, sizeof(accm));
		DBG(l2tp_db(DBG_SNOOP, "Found ACCM of %08x (%s)\n", accm, incoming ? "incoming" : "outgoing"));
		if (incoming) {
		    ses->recv_accm = accm;
		    ses->got_recv_accm = 1;
		} else {
		    ses->send_accm = accm;
		    ses->got_send_accm = 1;
		}
	    }

	    if (ses->got_recv_accm && ses->got_send_accm) {
		DBG(l2tp_db(DBG_SNOOP, "Sending SLI: Send ACCM = %08x; Receive ACCM = %08x\n", ses->send_accm, ses->recv_accm));
		session_send_sli(ses);
		ses->got_recv_accm = 0;
		ses->got_send_accm = 0;
	    }
	}
    }
}
/**********************************************************************
* %FUNCTION: session_send_sli
* %ARGUMENTS:
*  ses -- the session
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sends an SLI message with send/receive ACCM's.
***********************************************************************/
void
session_send_sli(l2tp_session *ses)
{
    l2tp_dgram *dgram;

    unsigned char buf[10];
    memset(buf, 0, 2);
    memcpy(buf+2, &ses->send_accm, 4);
    memcpy(buf+6, &ses->recv_accm, 4);

    dgram = l2tp_dgram_new_control(MESSAGE_SLI, ses->tunnel->assigned_id,
				   ses->assigned_id);
    if (!dgram) return;

    /* Add ACCM */
    l2tp_dgram_add_avp(dgram, ses->tunnel, MANDATORY,
		       sizeof(buf), VENDOR_IETF, AVP_ACCM, buf);

    /* Ship it out */
    l2tp_tunnel_xmit_control_message(ses->tunnel, dgram);
    ses->sent_sli = 1;
}
