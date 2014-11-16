/*
 * chap-new.c - New CHAP implementation.
 *
 * Copyright (c) 2003 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define RCSID	"$Id: chap-new.c,v 1.9 2007/06/19 02:08:35 carlsonj Exp $"

#include <stdlib.h>
#include <string.h>
#include "pppd.h"
#include "session.h"
#include "chap-new.h"
#include "chap-md5.h"

#ifdef CHAPMS
#include "chap_ms.h"
#define MDTYPE_ALL (MDTYPE_MICROSOFT_V2 | MDTYPE_MICROSOFT | MDTYPE_MD5)
#else
#define MDTYPE_ALL (MDTYPE_MD5)
#endif

int chap_mdtype_all = MDTYPE_ALL;

/* Hook for a plugin to validate CHAP challenge */
int (*chap_verify_hook)(char *name, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			char *message, int message_space) = NULL;

/*
 * Option variables.
 */
int chap_timeout_time = 3;
int chap_max_transmits = 10;
int chap_rechallenge_time = 0;
int chapms_strip_domain = 0;

/*
 * Command-line options.
 */
static option_t chap_option_list[] = {
	{ "chap-restart", o_int, &chap_timeout_time,
	  "Set timeout for CHAP", OPT_PRIO },
	{ "chap-max-challenge", o_int, &chap_max_transmits,
	  "Set max #xmits for challenge", OPT_PRIO },
	{ "chap-interval", o_int, &chap_rechallenge_time,
	  "Set interval for rechallenge", OPT_PRIO },
	{ "chapms-strip-domain", o_bool, &chapms_strip_domain,
	  "Strip the domain prefix before the Username", 1 },
	{ NULL }
};

/*
 * Internal state.
 */
static struct chap_client_state {
	int flags;
	char *name;
	struct chap_digest_type *digest;
} client;

/*
 * These limits apply to challenge and response packets we send.
 * The +4 is the +1 that we actually need rounded up.
 */
#define CHAL_MAX_PKTLEN	(PPP_HDRLEN + CHAP_HDRLEN + 4 + MAX_CHALLENGE_LEN + MAXNAMELEN)
#define RESP_MAX_PKTLEN	(PPP_HDRLEN + CHAP_HDRLEN + 4 + MAX_RESPONSE_LEN + MAXNAMELEN)

static struct chap_server_state {
	int flags;
	int id;
	char *name;
	struct chap_digest_type *digest;
	int challenge_xmits;
	int challenge_pktlen;
	unsigned char challenge[CHAL_MAX_PKTLEN];
	char message[256];
} server;

/* Values for flags in chap_client_state and chap_server_state */
#define LOWERUP			1
#define AUTH_STARTED		2
#define AUTH_DONE		4
#define AUTH_FAILED		8
#define TIMEOUT_PENDING		0x10
#define CHALLENGE_VALID		0x20

/*
 * Prototypes.
 */
static void chap_init(int unit);
static void chap_lowerup(int unit);
static void chap_lowerdown(int unit);
static void chap_timeout(void *arg);
static void chap_generate_challenge(struct chap_server_state *ss);
static void chap_handle_response(struct chap_server_state *ss, int code,
		unsigned char *pkt, int len);
static int chap_verify_response(char *name, char *ourname, int id,
		struct chap_digest_type *digest,
		unsigned char *challenge, unsigned char *response,
		char *message, int message_space);
static void chap_respond(struct chap_client_state *cs, int id,
		unsigned char *pkt, int len);
static void chap_handle_status(struct chap_client_state *cs, int code, int id,
		unsigned char *pkt, int len);
static void chap_protrej(int unit);
static void chap_input(int unit, unsigned char *pkt, int pktlen);
static int chap_print_pkt(unsigned char *p, int plen,
		void (*printer) __P((void *, char *, ...)), void *arg);

/* List of digest types that we know about */
static struct chap_digest_type *chap_digests;

/*
 * chap_init - reset to initial state.
 */
static void
chap_init(int unit)
{
	memset(&client, 0, sizeof(client));
	memset(&server, 0, sizeof(server));

	chap_md5_init();
#ifdef CHAPMS
	chapms_init();
#endif
}

/*
 * Add a new digest type to the list.
 */
void
chap_register_digest(struct chap_digest_type *dp)
{
	dp->next = chap_digests;
	chap_digests = dp;
}

/*
 * chap_lowerup - we can start doing stuff now.
 */
static void
chap_lowerup(int unit)
{
	struct chap_client_state *cs = &client;
	struct chap_server_state *ss = &server;

	cs->flags |= LOWERUP;
	ss->flags |= LOWERUP;
	if (ss->flags & AUTH_STARTED)
		chap_timeout(ss);
}

static void
chap_lowerdown(int unit)
{
	struct chap_client_state *cs = &client;
	struct chap_server_state *ss = &server;

	cs->flags = 0;
	if (ss->flags & TIMEOUT_PENDING)
		UNTIMEOUT(chap_timeout, ss);
	ss->flags = 0;
}

/*
 * chap_auth_peer - Start authenticating the peer.
 * If the lower layer is already up, we start sending challenges,
 * otherwise we wait for the lower layer to come up.
 */
void
chap_auth_peer(int unit, char *our_name, int digest_code)
{
	struct chap_server_state *ss = &server;
	struct chap_digest_type *dp;

	if (ss->flags & AUTH_STARTED) {
		error("CHAP: peer authentication already started!");
		return;
	}
	for (dp = chap_digests; dp != NULL; dp = dp->next)
		if (dp->code == digest_code)
			break;
	if (dp == NULL)
		fatal("CHAP digest 0x%x requested but not available",
		      digest_code);

	ss->digest = dp;
	ss->name = our_name;
	/* Start with a random ID value */
	ss->id = (unsigned char)(drand48() * 256);
	ss->flags |= AUTH_STARTED;
	if (ss->flags & LOWERUP)
		chap_timeout(ss);
}

/*
 * chap_auth_with_peer - Prepare to authenticate ourselves to the peer.
 * There isn't much to do until we receive a challenge.
 */
void
chap_auth_with_peer(int unit, char *our_name, int digest_code)
{
	struct chap_client_state *cs = &client;
	struct chap_digest_type *dp;

	if (cs->flags & AUTH_STARTED) {
		error("CHAP: authentication with peer already started!");
		return;
	}
	for (dp = chap_digests; dp != NULL; dp = dp->next)
		if (dp->code == digest_code)
			break;
	if (dp == NULL)
		fatal("CHAP digest 0x%x requested but not available",
		      digest_code);

	cs->digest = dp;
	cs->name = our_name;
	cs->flags |= AUTH_STARTED;
}

/*
 * chap_timeout - It's time to send another challenge to the peer.
 * This could be either a retransmission of a previous challenge,
 * or a new challenge to start re-authentication.
 */
static void
chap_timeout(void *arg)
{
	struct chap_server_state *ss = arg;

	ss->flags &= ~TIMEOUT_PENDING;
	if ((ss->flags & CHALLENGE_VALID) == 0) {
		ss->challenge_xmits = 0;
		chap_generate_challenge(ss);
		ss->flags |= CHALLENGE_VALID;
	} else if (ss->challenge_xmits >= chap_max_transmits) {
		ss->flags &= ~CHALLENGE_VALID;
		ss->flags |= AUTH_DONE | AUTH_FAILED;
		auth_peer_fail(0, PPP_CHAP);
		return;
	}

	output(0, ss->challenge, ss->challenge_pktlen);
	++ss->challenge_xmits;
	ss->flags |= TIMEOUT_PENDING;
	TIMEOUT(chap_timeout, arg, chap_timeout_time);
}

/*
 * chap_generate_challenge - generate a challenge string and format
 * the challenge packet in ss->challenge_pkt.
 */
static void
chap_generate_challenge(struct chap_server_state *ss)
{
	int clen = 1, nlen, len;
	unsigned char *p;

	p = ss->challenge;
	MAKEHEADER(p, PPP_CHAP);
	p += CHAP_HDRLEN;
	ss->digest->generate_challenge(p);
	clen = *p;
	nlen = strlen(ss->name);
	memcpy(p + 1 + clen, ss->name, nlen);

	len = CHAP_HDRLEN + 1 + clen + nlen;
	ss->challenge_pktlen = PPP_HDRLEN + len;

	p = ss->challenge + PPP_HDRLEN;
	p[0] = CHAP_CHALLENGE;
	p[1] = ++ss->id;
	p[2] = len >> 8;
	p[3] = len;
}

/*
 * chap_handle_response - check the response to our challenge.
 */
static void
chap_handle_response(struct chap_server_state *ss, int id,
		     unsigned char *pkt, int len)
{
	int response_len, ok, mlen;
	unsigned char *response, *p;
	char *name = NULL;	/* initialized to shut gcc up */
	int (*verifier)(char *, char *, int, struct chap_digest_type *,
		unsigned char *, unsigned char *, char *, int);
	char rname[MAXNAMELEN+1];

	if ((ss->flags & LOWERUP) == 0)
		return;
	if (id != ss->challenge[PPP_HDRLEN+1] || len < 2)
		return;
	if (ss->flags & CHALLENGE_VALID) {
		response = pkt;
		GETCHAR(response_len, pkt);
		len -= response_len + 1;	/* length of name */
		name = (char *)pkt + response_len;
		if (len < 0)
			return;

		if (ss->flags & TIMEOUT_PENDING) {
			ss->flags &= ~TIMEOUT_PENDING;
			UNTIMEOUT(chap_timeout, ss);
		}

		if (explicit_remote) {
			name = remote_name;
		} else {
			/* Null terminate and clean remote name. */
			slprintf(rname, sizeof(rname), "%.*v", len, name);
			name = rname;

			/* strip the MS domain name */
			if (chapms_strip_domain && strrchr(rname, '\\')) {
				char tmp[MAXNAMELEN+1];

				strcpy(tmp, strrchr(rname, '\\') + 1);
				strcpy(rname, tmp);
			}
		}

		if (chap_verify_hook)
			verifier = chap_verify_hook;
		else
			verifier = chap_verify_response;
		ok = (*verifier)(name, ss->name, id, ss->digest,
				 ss->challenge + PPP_HDRLEN + CHAP_HDRLEN,
				 response, ss->message, sizeof(ss->message));
		if (!ok || !auth_number()) {
			ss->flags |= AUTH_FAILED;
			warn("Peer %q failed CHAP authentication", name);
		}
	} else if ((ss->flags & AUTH_DONE) == 0)
		return;

	/* send the response */
	p = outpacket_buf;
	MAKEHEADER(p, PPP_CHAP);
	mlen = strlen(ss->message);
	len = CHAP_HDRLEN + mlen;
	p[0] = (ss->flags & AUTH_FAILED)? CHAP_FAILURE: CHAP_SUCCESS;
	p[1] = id;
	p[2] = len >> 8;
	p[3] = len;
	if (mlen > 0)
		memcpy(p + CHAP_HDRLEN, ss->message, mlen);
	output(0, outpacket_buf, PPP_HDRLEN + len);

	if (ss->flags & CHALLENGE_VALID) {
		ss->flags &= ~CHALLENGE_VALID;
		if (!(ss->flags & AUTH_DONE) && !(ss->flags & AUTH_FAILED)) {
		    /*
		     * Auth is OK, so now we need to check session restrictions
		     * to ensure everything is OK, but only if we used a
		     * plugin, and only if we're configured to check.  This
		     * allows us to do PAM checks on PPP servers that
		     * authenticate against ActiveDirectory, and use AD for
		     * account info (like when using Winbind integrated with
		     * PAM).
		     */
		    if (session_mgmt &&
			session_check(name, NULL, devnam, NULL) == 0) {
			ss->flags |= AUTH_FAILED;
			warn("Peer %q failed CHAP Session verification", name);
		    }
		}
		if (ss->flags & AUTH_FAILED) {
			auth_peer_fail(0, PPP_CHAP);
		} else {
			if ((ss->flags & AUTH_DONE) == 0)
				auth_peer_success(0, PPP_CHAP,
						  ss->digest->code,
						  name, strlen(name));
			if (chap_rechallenge_time) {
				ss->flags |= TIMEOUT_PENDING;
				TIMEOUT(chap_timeout, ss,
					chap_rechallenge_time);
			}
		}
		ss->flags |= AUTH_DONE;
	}
}

/*
 * chap_verify_response - check whether the peer's response matches
 * what we think it should be.  Returns 1 if it does (authentication
 * succeeded), or 0 if it doesn't.
 */
static int
chap_verify_response(char *name, char *ourname, int id,
		     struct chap_digest_type *digest,
		     unsigned char *challenge, unsigned char *response,
		     char *message, int message_space)
{
	int ok;
	unsigned char secret[MAXSECRETLEN];
	int secret_len;

	/* Get the secret that the peer is supposed to know */
	if (!get_secret(0, name, ourname, (char *)secret, &secret_len, 1)) {
		error("No CHAP secret found for authenticating %q", name);
		return 0;
	}

	ok = digest->verify_response(id, name, secret, secret_len, challenge,
				     response, message, message_space);
	memset(secret, 0, sizeof(secret));

	return ok;
}

/*
 * chap_respond - Generate and send a response to a challenge.
 */
static void
chap_respond(struct chap_client_state *cs, int id,
	     unsigned char *pkt, int len)
{
	int clen, nlen;
	int secret_len;
	unsigned char *p;
	unsigned char response[RESP_MAX_PKTLEN];
	char rname[MAXNAMELEN+1];
	char secret[MAXSECRETLEN+1];

	if ((cs->flags & (LOWERUP | AUTH_STARTED)) != (LOWERUP | AUTH_STARTED))
		return;		/* not ready */
	if (len < 2 || len < pkt[0] + 1)
		return;		/* too short */
	clen = pkt[0];
	nlen = len - (clen + 1);

	/* Null terminate and clean remote name. */
	slprintf(rname, sizeof(rname), "%.*v", nlen, pkt + clen + 1);

	/* Microsoft doesn't send their name back in the PPP packet */
	if (explicit_remote || (remote_name[0] != 0 && rname[0] == 0))
		strlcpy(rname, remote_name, sizeof(rname));

	/* get secret for authenticating ourselves with the specified host */
	if (!get_secret(0, cs->name, rname, secret, &secret_len, 0)) {
		secret_len = 0;	/* assume null secret if can't find one */
		warn("No CHAP secret found for authenticating us to %q", rname);
	}

	p = response;
	MAKEHEADER(p, PPP_CHAP);
	p += CHAP_HDRLEN;

	cs->digest->make_response(p, id, cs->name, pkt,
				  secret, secret_len);
	memset(secret, 0, secret_len);

	clen = *p;
	nlen = strlen(cs->name);
	memcpy(p + clen + 1, cs->name, nlen);

	p = response + PPP_HDRLEN;
	len = CHAP_HDRLEN + clen + 1 + nlen;
	p[0] = CHAP_RESPONSE;
	p[1] = id;
	p[2] = len >> 8;
	p[3] = len;

	output(0, response, PPP_HDRLEN + len);
}

static void
chap_handle_status(struct chap_client_state *cs, int code, int id,
		   unsigned char *pkt, int len)
{
	const char *msg = NULL;

	if ((cs->flags & (AUTH_DONE|AUTH_STARTED|LOWERUP))
	    != (AUTH_STARTED|LOWERUP))
		return;
	cs->flags |= AUTH_DONE;

	if (code == CHAP_SUCCESS) {
		/* used for MS-CHAP v2 mutual auth, yuck */
		if (cs->digest->check_success != NULL) {
			if (!(*cs->digest->check_success)(id, pkt, len))
				code = CHAP_FAILURE;
		} else
			msg = "CHAP authentication succeeded";
	} else {
		if (cs->digest->handle_failure != NULL)
			(*cs->digest->handle_failure)(pkt, len);
		else
			msg = "CHAP authentication failed";
	}
	if (msg) {
		if (len > 0)
			info("%s: %.*v", msg, len, pkt);
		else
			info("%s", msg);
	}
	if (code == CHAP_SUCCESS)
		auth_withpeer_success(0, PPP_CHAP, cs->digest->code);
	else {
		cs->flags |= AUTH_FAILED;
		error("CHAP authentication failed");
		auth_withpeer_fail(0, PPP_CHAP);
	}
}

static void
chap_input(int unit, unsigned char *pkt, int pktlen)
{
	struct chap_client_state *cs = &client;
	struct chap_server_state *ss = &server;
	unsigned char code, id;
	int len;

	if (pktlen < CHAP_HDRLEN)
		return;
	GETCHAR(code, pkt);
	GETCHAR(id, pkt);
	GETSHORT(len, pkt);
	if (len < CHAP_HDRLEN || len > pktlen)
		return;
	len -= CHAP_HDRLEN;

	switch (code) {
	case CHAP_CHALLENGE:
		chap_respond(cs, id, pkt, len);
		break;
	case CHAP_RESPONSE:
		chap_handle_response(ss, id, pkt, len);
		break;
	case CHAP_FAILURE:
	case CHAP_SUCCESS:
		chap_handle_status(cs, code, id, pkt, len);
		break;
	}
}

static void
chap_protrej(int unit)
{
	struct chap_client_state *cs = &client;
	struct chap_server_state *ss = &server;

	if (ss->flags & TIMEOUT_PENDING) {
		ss->flags &= ~TIMEOUT_PENDING;
		UNTIMEOUT(chap_timeout, ss);
	}
	if (ss->flags & AUTH_STARTED) {
		ss->flags = 0;
		auth_peer_fail(0, PPP_CHAP);
	}
	if ((cs->flags & (AUTH_STARTED|AUTH_DONE)) == AUTH_STARTED) {
		cs->flags &= ~AUTH_STARTED;
		error("CHAP authentication failed due to protocol-reject");
		auth_withpeer_fail(0, PPP_CHAP);
	}
}

/*
 * chap_print_pkt - print the contents of a CHAP packet.
 */
static char *chap_code_names[] = {
	"Challenge", "Response", "Success", "Failure"
};

static int
chap_print_pkt(unsigned char *p, int plen,
	       void (*printer) __P((void *, char *, ...)), void *arg)
{
	int code, id, len;
	int clen, nlen;
	unsigned char x;

	if (plen < CHAP_HDRLEN)
		return 0;
	GETCHAR(code, p);
	GETCHAR(id, p);
	GETSHORT(len, p);
	if (len < CHAP_HDRLEN || len > plen)
		return 0;

	if (code >= 1 && code <= sizeof(chap_code_names) / sizeof(char *))
		printer(arg, " %s", chap_code_names[code-1]);
	else
		printer(arg, " code=0x%x", code);
	printer(arg, " id=0x%x", id);
	len -= CHAP_HDRLEN;
	switch (code) {
	case CHAP_CHALLENGE:
	case CHAP_RESPONSE:
		if (len < 1)
			break;
		clen = p[0];
		if (len < clen + 1)
			break;
		++p;
		nlen = len - clen - 1;
		printer(arg, " <");
		for (; clen > 0; --clen) {
			GETCHAR(x, p);
			printer(arg, "%.2x", x);
		}
		printer(arg, ">, name = ");
		print_string((char *)p, nlen, printer, arg);
		break;
	case CHAP_FAILURE:
	case CHAP_SUCCESS:
		printer(arg, " ");
		print_string((char *)p, len, printer, arg);
		break;
	default:
		for (clen = len; clen > 0; --clen) {
			GETCHAR(x, p);
			printer(arg, " %.2x", x);
		}
	}

	return len + CHAP_HDRLEN;
}

struct protent chap_protent = {
	PPP_CHAP,
	chap_init,
	chap_input,
	chap_protrej,
	chap_lowerup,
	chap_lowerdown,
	NULL,		/* open */
	NULL,		/* close */
	chap_print_pkt,
	NULL,		/* datainput */
	1,		/* enabled_flag */
	"CHAP",		/* name */
	NULL,		/* data_name */
	chap_option_list,
	NULL,		/* check_options */
};
