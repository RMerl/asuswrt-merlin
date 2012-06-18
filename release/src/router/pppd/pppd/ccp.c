/*
 * ccp.c - PPP Compression Control Protocol.
 *
 * Copyright (c) 1994-2002 Paul Mackerras. All rights reserved.
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

#define RCSID	"$Id: ccp.c,v 1.50 2005/06/26 19:34:41 carlsonj Exp $"

#include <stdlib.h>
#include <string.h>

#include "pppd.h"
#include "fsm.h"
#include "ccp.h"
#include <net/ppp-comp.h>

#ifdef MPPE
#include "chap_ms.h"	/* mppe_xxxx_key, mppe_keys_set */
#include "lcp.h"	/* lcp_close(), lcp_fsm */
#endif

static const char rcsid[] = RCSID;

/*
 * Unfortunately there is a bug in zlib which means that using a
 * size of 8 (window size = 256) for Deflate compression will cause
 * buffer overruns and kernel crashes in the deflate module.
 * Until this is fixed we only accept sizes in the range 9 .. 15.
 * Thanks to James Carlson for pointing this out.
 */
#define DEFLATE_MIN_WORKS	9

/*
 * Command-line options.
 */
static int setbsdcomp __P((char **));
static int setdeflate __P((char **));
static char bsd_value[8];
static char deflate_value[8];

static option_t ccp_option_list[] = {
    { "noccp", o_bool, &ccp_protent.enabled_flag,
      "Disable CCP negotiation" },
    { "-ccp", o_bool, &ccp_protent.enabled_flag,
      "Disable CCP negotiation", OPT_ALIAS },

    { "bsdcomp", o_special, (void *)setbsdcomp,
      "Request BSD-Compress packet compression",
      OPT_PRIO | OPT_A2STRVAL | OPT_STATIC, bsd_value },
    { "nobsdcomp", o_bool, &ccp_wantoptions[0].bsd_compress,
      "don't allow BSD-Compress", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].bsd_compress },
    { "-bsdcomp", o_bool, &ccp_wantoptions[0].bsd_compress,
      "don't allow BSD-Compress", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].bsd_compress },

    { "deflate", o_special, (void *)setdeflate,
      "request Deflate compression",
      OPT_PRIO | OPT_A2STRVAL | OPT_STATIC, deflate_value },
    { "nodeflate", o_bool, &ccp_wantoptions[0].deflate,
      "don't allow Deflate compression", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].deflate },
    { "-deflate", o_bool, &ccp_wantoptions[0].deflate,
      "don't allow Deflate compression", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].deflate },

    { "nodeflatedraft", o_bool, &ccp_wantoptions[0].deflate_draft,
      "don't use draft deflate #", OPT_A2COPY,
      &ccp_allowoptions[0].deflate_draft },

    { "predictor1", o_bool, &ccp_wantoptions[0].predictor_1,
      "request Predictor-1", OPT_PRIO | 1 },
    { "nopredictor1", o_bool, &ccp_wantoptions[0].predictor_1,
      "don't allow Predictor-1", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].predictor_1 },
    { "-predictor1", o_bool, &ccp_wantoptions[0].predictor_1,
      "don't allow Predictor-1", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].predictor_1 },

    { "lzs", o_bool, &ccp_wantoptions[0].lzs,
      "request Stac LZS", 1, &ccp_allowoptions[0].lzs, OPT_PRIO },
    { "+lzs", o_bool, &ccp_wantoptions[0].lzs,
      "request Stac LZS", 1, &ccp_allowoptions[0].lzs, OPT_ALIAS | OPT_PRIO },
    { "nolzs", o_bool, &ccp_wantoptions[0].lzs,
      "don't allow Stac LZS", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].lzs },
    { "-lzs", o_bool, &ccp_wantoptions[0].lzs,
      "don't allow Stac LZS", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].lzs },

#ifdef MPPE
    { "mppc", o_bool, &ccp_wantoptions[0].mppc,
      "request MPPC compression", 1, &ccp_allowoptions[0].mppc, OPT_PRIO },
    { "+mppc", o_bool, &ccp_wantoptions[0].mppc,
      "request MPPC compression", 1, &ccp_allowoptions[0].mppc,
      OPT_ALIAS | OPT_PRIO },
    { "nomppc", o_bool, &ccp_wantoptions[0].mppc,
      "don't allow MPPC compression", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppc },
    { "-mppc", o_bool, &ccp_wantoptions[0].mppc,
      "don't allow MPPC compression", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppc },

    { "require-mppe", o_bool, &ccp_wantoptions[0].mppe,
      "require MPPE encryption", 1, &ccp_allowoptions[0].mppe, OPT_PRIO },
    { "+mppe", o_bool, &ccp_wantoptions[0].mppe,
      "require MPPE encryption", 1, &ccp_allowoptions[0].mppe,
      OPT_ALIAS | OPT_PRIO },
    { "nomppe", o_bool, &ccp_wantoptions[0].mppe,
      "don't allow MPPE encryption", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe },
    { "-mppe", o_bool, &ccp_wantoptions[0].mppe,
      "don't allow MPPE encryption", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe },

    { "require-mppe-40", o_bool, &ccp_wantoptions[0].mppe_40,
      "require MPPE 40-bit encryption", 1, &ccp_allowoptions[0].mppe_40,
      OPT_PRIO },
    { "+mppe-40", o_bool, &ccp_wantoptions[0].mppe_40,
      "require MPPE 40-bit encryption", 1, &ccp_allowoptions[0].mppe_40,
      OPT_ALIAS | OPT_PRIO },
    { "nomppe-40", o_bool, &ccp_wantoptions[0].mppe_40,
      "don't allow MPPE 40-bit encryption", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe_40 },
    { "-mppe-40", o_bool, &ccp_wantoptions[0].mppe_40,
      "don't allow MPPE 40-bit encryption", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe_40 },

    { "require-mppe-56", o_bool, &ccp_wantoptions[0].mppe_56,
      "require MPPE 56-bit encryption", 1, &ccp_allowoptions[0].mppe_56,
      OPT_PRIO },
    { "+mppe-56", o_bool, &ccp_wantoptions[0].mppe_56,
      "require MPPE 56-bit encryption", 1, &ccp_allowoptions[0].mppe_56,
      OPT_ALIAS | OPT_PRIO },
    { "nomppe-56", o_bool, &ccp_wantoptions[0].mppe_56,
      "don't allow MPPE 56-bit encryption", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe_56 },
    { "-mppe-56", o_bool, &ccp_wantoptions[0].mppe_56,
      "don't allow MPPE 56-bit encryption", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe_56 },

    { "require-mppe-128", o_bool, &ccp_wantoptions[0].mppe_128,
      "require MPPE 128-bit encryption", 1, &ccp_allowoptions[0].mppe_128,
      OPT_PRIO },
    { "+mppe-128", o_bool, &ccp_wantoptions[0].mppe_128,
      "require MPPE 128-bit encryption", 1, &ccp_allowoptions[0].mppe_128,
      OPT_ALIAS | OPT_PRIO },
    { "nomppe-128", o_bool, &ccp_wantoptions[0].mppe_128,
      "don't allow MPPE 128-bit encryption", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe_128 },
    { "-mppe-128", o_bool, &ccp_wantoptions[0].mppe_128,
      "don't allow MPPE 128-bit encryption", OPT_ALIAS | OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe_128 },

    { "nomppe-stateful", o_bool, &ccp_wantoptions[0].mppe_stateless,
      "disallow MPPE stateful mode", 1, &ccp_allowoptions[0].mppe_stateless,
      OPT_PRIO },
    { "mppe-stateful", o_bool, &ccp_wantoptions[0].mppe_stateless,
      "allow MPPE stateful mode", OPT_PRIOSUB | OPT_A2CLR,
      &ccp_allowoptions[0].mppe_stateless },
#endif /* MPPE */

    { NULL }
};

/*
 * Protocol entry points from main code.
 */
static void ccp_init __P((int unit));
static void ccp_open __P((int unit));
static void ccp_close __P((int unit, char *));
static void ccp_lowerup __P((int unit));
static void ccp_lowerdown __P((int));
static void ccp_input __P((int unit, u_char *pkt, int len));
static void ccp_protrej __P((int unit));
static int  ccp_printpkt __P((u_char *pkt, int len,
			      void (*printer) __P((void *, char *, ...)),
			      void *arg));
static void ccp_datainput __P((int unit, u_char *pkt, int len));

struct protent ccp_protent = {
    PPP_CCP,
    ccp_init,
    ccp_input,
    ccp_protrej,
    ccp_lowerup,
    ccp_lowerdown,
    ccp_open,
    ccp_close,
    ccp_printpkt,
    ccp_datainput,
    1,
    "CCP",
    "Compressed",
    ccp_option_list,
    NULL,
    NULL,
    NULL
};

fsm ccp_fsm[NUM_PPP];
ccp_options ccp_wantoptions[NUM_PPP];	/* what to request the peer to use */
ccp_options ccp_gotoptions[NUM_PPP];	/* what the peer agreed to do */
ccp_options ccp_allowoptions[NUM_PPP];	/* what we'll agree to do */
ccp_options ccp_hisoptions[NUM_PPP];	/* what we agreed to do */

/*
 * Callbacks for fsm code.
 */
static void ccp_resetci __P((fsm *));
static int  ccp_cilen __P((fsm *));
static void ccp_addci __P((fsm *, u_char *, int *));
static int  ccp_ackci __P((fsm *, u_char *, int));
static int  ccp_nakci __P((fsm *, u_char *, int, int));
static int  ccp_rejci __P((fsm *, u_char *, int));
static int  ccp_reqci __P((fsm *, u_char *, int *, int));
static void ccp_up __P((fsm *));
static void ccp_down __P((fsm *));
static int  ccp_extcode __P((fsm *, int, int, u_char *, int));
static void ccp_rack_timeout __P((void *));
static char *method_name __P((ccp_options *, ccp_options *));

static fsm_callbacks ccp_callbacks = {
    ccp_resetci,
    ccp_cilen,
    ccp_addci,
    ccp_ackci,
    ccp_nakci,
    ccp_rejci,
    ccp_reqci,
    ccp_up,
    ccp_down,
    NULL,
    NULL,
    NULL,
    NULL,
    ccp_extcode,
    "CCP"
};

/*
 * Do we want / did we get any compression?
 */
#define ANY_COMPRESS(opt)	((opt).deflate || (opt).bsd_compress \
				 || (opt).predictor_1 || (opt).predictor_2 \
				 || (opt).lzs || (opt).mppc || (opt).mppe)

/*
 * Local state (mainly for handling reset-reqs and reset-acks).
 */
static int ccp_localstate[NUM_PPP];
#define RACK_PENDING	1	/* waiting for reset-ack */
#define RREQ_REPEAT	2	/* send another reset-req if no reset-ack */

#define RACKTIMEOUT	1	/* second */

static int all_rejected[NUM_PPP];	/* we rejected all peer's options */

/*
 * Option parsing.
 */
static int
setbsdcomp(argv)
    char **argv;
{
    int rbits, abits;
    char *str, *endp;

    str = *argv;
    abits = rbits = strtol(str, &endp, 0);
    if (endp != str && *endp == ',') {
	str = endp + 1;
	abits = strtol(str, &endp, 0);
    }
    if (*endp != 0 || endp == str) {
	option_error("invalid parameter '%s' for bsdcomp option", *argv);
	return 0;
    }
    if ((rbits != 0 && (rbits < BSD_MIN_BITS || rbits > BSD_MAX_BITS))
	|| (abits != 0 && (abits < BSD_MIN_BITS || abits > BSD_MAX_BITS))) {
	option_error("bsdcomp option values must be 0 or %d .. %d",
		     BSD_MIN_BITS, BSD_MAX_BITS);
	return 0;
    }
    if (rbits > 0) {
	ccp_wantoptions[0].bsd_compress = 1;
	ccp_wantoptions[0].bsd_bits = rbits;
    } else
	ccp_wantoptions[0].bsd_compress = 0;
    if (abits > 0) {
	ccp_allowoptions[0].bsd_compress = 1;
	ccp_allowoptions[0].bsd_bits = abits;
    } else
	ccp_allowoptions[0].bsd_compress = 0;
    slprintf(bsd_value, sizeof(bsd_value),
	     rbits == abits? "%d": "%d,%d", rbits, abits);

    return 1;
}

static int
setdeflate(argv)
    char **argv;
{
    int rbits, abits;
    char *str, *endp;

    str = *argv;
    abits = rbits = strtol(str, &endp, 0);
    if (endp != str && *endp == ',') {
	str = endp + 1;
	abits = strtol(str, &endp, 0);
    }
    if (*endp != 0 || endp == str) {
	option_error("invalid parameter '%s' for deflate option", *argv);
	return 0;
    }
    if ((rbits != 0 && (rbits < DEFLATE_MIN_SIZE || rbits > DEFLATE_MAX_SIZE))
	|| (abits != 0 && (abits < DEFLATE_MIN_SIZE
			  || abits > DEFLATE_MAX_SIZE))) {
	option_error("deflate option values must be 0 or %d .. %d",
		     DEFLATE_MIN_SIZE, DEFLATE_MAX_SIZE);
	return 0;
    }
    if (rbits == DEFLATE_MIN_SIZE || abits == DEFLATE_MIN_SIZE) {
	if (rbits == DEFLATE_MIN_SIZE)
	    rbits = DEFLATE_MIN_WORKS;
	if (abits == DEFLATE_MIN_SIZE)
	    abits = DEFLATE_MIN_WORKS;
	warn("deflate option value of %d changed to %d to avoid zlib bug",
	     DEFLATE_MIN_SIZE, DEFLATE_MIN_WORKS);
    }
    if (rbits > 0) {
	ccp_wantoptions[0].deflate = 1;
	ccp_wantoptions[0].deflate_size = rbits;
    } else
	ccp_wantoptions[0].deflate = 0;
    if (abits > 0) {
	ccp_allowoptions[0].deflate = 1;
	ccp_allowoptions[0].deflate_size = abits;
    } else
	ccp_allowoptions[0].deflate = 0;
    slprintf(deflate_value, sizeof(deflate_value),
	     rbits == abits? "%d": "%d,%d", rbits, abits);

    return 1;
}

/*
 * ccp_init - initialize CCP.
 */
static void
ccp_init(unit)
    int unit;
{
    fsm *f = &ccp_fsm[unit];
    ccp_options *wo = &ccp_wantoptions[unit];
    ccp_options *ao = &ccp_allowoptions[unit];

    f->unit = unit;
    f->protocol = PPP_CCP;
    f->callbacks = &ccp_callbacks;
    fsm_init(f);

    memset(&ccp_wantoptions[unit],  0, sizeof(ccp_options));
    memset(&ccp_gotoptions[unit],   0, sizeof(ccp_options));
    memset(&ccp_allowoptions[unit], 0, sizeof(ccp_options));
    memset(&ccp_hisoptions[unit],   0, sizeof(ccp_options));

    wo->deflate = 1;
    wo->deflate_size = DEFLATE_MAX_SIZE;
    wo->deflate_correct = 1;
    wo->deflate_draft = 1;
    ao->deflate = 1;
    ao->deflate_size = DEFLATE_MAX_SIZE;
    ao->deflate_correct = 1;
    ao->deflate_draft = 1;

    wo->bsd_compress = 1;
    wo->bsd_bits = BSD_MAX_BITS;
    ao->bsd_compress = 1;
    ao->bsd_bits = BSD_MAX_BITS;

    ao->predictor_1 = 1;

    wo->lzs = 0; /* Stac LZS  - will be enabled in the future */
    wo->lzs_mode = LZS_MODE_SEQ;
    wo->lzs_hists = 1;
    ao->lzs = 0; /* Stac LZS  - will be enabled in the future */
    ao->lzs_mode = LZS_MODE_SEQ;
    ao->lzs_hists = 1;

#ifdef MPPE
    /* by default allow and request MPPC... */
    wo->mppc = ao->mppc = 1;

    /* ... and allow but don't request MPPE */
    ao->mppe = 1;
    ao->mppe_40 = 1;
    ao->mppe_56 = 1;
    ao->mppe_128 = 1;
    ao->mppe_stateless = 1;
    wo->mppe = 0;
    wo->mppe_40 = 0;
    wo->mppe_56 = 0;
    wo->mppe_128 = 0;
    wo->mppe_stateless = 0;
#endif /* MPPE */
}

/*
 * ccp_open - CCP is allowed to come up.
 */
static void
ccp_open(unit)
    int unit;
{
    fsm *f = &ccp_fsm[unit];

    if (f->state != OPENED)
	ccp_flags_set(unit, 1, 0);

    /*
     * Find out which compressors the kernel supports before
     * deciding whether to open in silent mode.
     */
    ccp_resetci(f);
    if (!ANY_COMPRESS(ccp_gotoptions[unit]))
	f->flags |= OPT_SILENT;

    fsm_open(f);
}

/*
 * ccp_close - Terminate CCP.
 */
static void
ccp_close(unit, reason)
    int unit;
    char *reason;
{
    ccp_flags_set(unit, 0, 0);
    fsm_close(&ccp_fsm[unit], reason);
}

/*
 * ccp_lowerup - we may now transmit CCP packets.
 */
static void
ccp_lowerup(unit)
    int unit;
{
    fsm_lowerup(&ccp_fsm[unit]);
}

/*
 * ccp_lowerdown - we may not transmit CCP packets.
 */
static void
ccp_lowerdown(unit)
    int unit;
{
    fsm_lowerdown(&ccp_fsm[unit]);
}

/*
 * ccp_input - process a received CCP packet.
 */
static void
ccp_input(unit, p, len)
    int unit;
    u_char *p;
    int len;
{
    fsm *f = &ccp_fsm[unit];
    int oldstate;

    /*
     * Check for a terminate-request so we can print a message.
     */
    oldstate = f->state;
    fsm_input(f, p, len);
    if (oldstate == OPENED && p[0] == TERMREQ && f->state != OPENED) {
	notice("Compression disabled by peer.");
#ifdef MPPE
	if (ccp_wantoptions[unit].mppe) {
	    error("MPPE disabled, closing LCP");
	    lcp_close(unit, "MPPE disabled by peer");
	}
#endif /* MPPE */
    }

    /*
     * If we get a terminate-ack and we're not asking for compression,
     * close CCP.
     */
    if (oldstate == REQSENT && p[0] == TERMACK
	&& !ANY_COMPRESS(ccp_gotoptions[unit]))
	ccp_close(unit, "No compression negotiated");
}

/*
 * Handle a CCP-specific code.
 */
static int
ccp_extcode(f, code, id, p, len)
    fsm *f;
    int code, id;
    u_char *p;
    int len;
{
    switch (code) {
    case CCP_RESETREQ:
	if (f->state != OPENED)
	    break;
	/* send a reset-ack, which the transmitter will see and
	   reset its compression state. */

	/* In case of MPPE/MPPC or LZS we shouldn't send CCP_RESETACK,
	   but we do it in order to reset compressor; CCP_RESETACK is
	   then silently discarded. See functions ppp_send_frame and
	   ppp_ccp_peek in ppp_generic.c (Linux only !!!). All the
	   confusion is caused by the fact that CCP code is splited
	   into two parts - one part is handled by pppd, the other one
	   is handled by kernel. */

	fsm_sdata(f, CCP_RESETACK, id, NULL, 0);
	break;

    case CCP_RESETACK:
	if (ccp_localstate[f->unit] & RACK_PENDING && id == f->reqid) {
	    ccp_localstate[f->unit] &= ~(RACK_PENDING | RREQ_REPEAT);
	    UNTIMEOUT(ccp_rack_timeout, f);
	}
	break;

    default:
	return 0;
    }

    return 1;
}

/*
 * ccp_protrej - peer doesn't talk CCP.
 */
static void
ccp_protrej(unit)
    int unit;
{
    ccp_flags_set(unit, 0, 0);
    fsm_lowerdown(&ccp_fsm[unit]);

#ifdef MPPE
    if (ccp_wantoptions[unit].mppe) {
	error("MPPE required but peer negotiation failed");
	lcp_close(unit, "MPPE required but peer negotiation failed");
    }
#endif /* MPPE */
}

/*
 * ccp_resetci - initialize at start of negotiation.
 */
static void
ccp_resetci(f)
    fsm *f;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    u_char opt_buf[CCP_MAX_OPTION_LENGTH];

    *go = ccp_wantoptions[f->unit];
    all_rejected[f->unit] = 0;

#ifdef MPPE
    if (go->mppe || go->mppc) {
	ccp_options *ao = &ccp_allowoptions[f->unit];
	int auth_mschap_bits = auth_done[f->unit];
	int numbits;

	/*
	 * Start with a basic sanity check: mschap[v2] auth must be in
	 * exactly one direction.  RFC 3079 says that the keys are
	 * 'derived from the credentials of the peer that initiated the call',
	 * however the PPP protocol doesn't have such a concept, and pppd
	 * cannot get this info externally.  Instead we do the best we can.
	 * NB: If MPPE is required, all other compression opts are invalid.
	 *     So, we return right away if we can't do it.
	 */
	if (ccp_wantoptions[f->unit].mppe) {
	    /* Leave only the mschap auth bits set */
	    auth_mschap_bits &= (CHAP_MS_WITHPEER  | CHAP_MS_PEER |
				 CHAP_MS2_WITHPEER | CHAP_MS2_PEER);
	    /* Count the mschap auths */
	    auth_mschap_bits >>= CHAP_MS_SHIFT;
	    numbits = 0;
	    do {
		numbits += auth_mschap_bits & 1;
		auth_mschap_bits >>= 1;
	    } while (auth_mschap_bits);
	    if (numbits > 1) {
		error("MPPE required, but auth done in both directions.");
		lcp_close(f->unit, "MPPE required but not available");
		return;
	    }
	    if (!numbits) {
		error("MPPE required, but MS-CHAP[v2] auth not performed.");
		lcp_close(f->unit, "MPPE required but not available");
		return;
	    }

	    /* A plugin (eg radius) may not have obtained key material. */
	    if (!mppe_keys_set) {
		error("MPPE required, but keys are not available.  "
		      "Possible plugin problem?");
		lcp_close(f->unit, "MPPE required but not available");
		return;
	    }
	}

	/*
	 * Check whether the kernel knows about the various
	 * compression methods we might request. Key material
	 * unimportant here.
	 */
	if (go->mppc) {
	    opt_buf[0] = CI_MPPE;
	    opt_buf[1] = CILEN_MPPE;
	    opt_buf[2] = 0;
	    opt_buf[3] = 0;
	    opt_buf[4] = 0;
	    opt_buf[5] = MPPE_MPPC;
	    if (ccp_test(f->unit, opt_buf, CILEN_MPPE, 0) <= 0)
		go->mppc = 0;
	}
	if (go->mppe_40) {
	    opt_buf[0] = CI_MPPE;
	    opt_buf[1] = CILEN_MPPE;
	    opt_buf[2] = MPPE_STATELESS;
	    opt_buf[3] = 0;
	    opt_buf[4] = 0;
	    opt_buf[5] = MPPE_40BIT;
	    if (ccp_test(f->unit, opt_buf, CILEN_MPPE + MPPE_MAX_KEY_LEN, 0) <= 0)
		go->mppe_40 = 0;
	}
	if (go->mppe_56) {
	    opt_buf[0] = CI_MPPE;
	    opt_buf[1] = CILEN_MPPE;
	    opt_buf[2] = MPPE_STATELESS;
	    opt_buf[3] = 0;
	    opt_buf[4] = 0;
	    opt_buf[5] = MPPE_56BIT;
	    if (ccp_test(f->unit, opt_buf, CILEN_MPPE + MPPE_MAX_KEY_LEN, 0) <= 0)
		go->mppe_56 = 0;
	}
	if (go->mppe_128) {
	    opt_buf[0] = CI_MPPE;
	    opt_buf[1] = CILEN_MPPE;
	    opt_buf[2] = MPPE_STATELESS;
	    opt_buf[3] = 0;
	    opt_buf[4] = 0;
	    opt_buf[5] = MPPE_128BIT;
	    if (ccp_test(f->unit, opt_buf, CILEN_MPPE + MPPE_MAX_KEY_LEN, 0) <= 0)
		go->mppe_128 = 0;
	}
	if (!go->mppe_40 && !go->mppe_56 && !go->mppe_128) {
	    if (ccp_wantoptions[f->unit].mppe) {
		error("MPPE required, but kernel has no support.");
		lcp_close(f->unit, "MPPE required but not available");
	    }
	    go->mppe = go->mppe_stateless = 0;
	} else {
	    /* MPPE is not compatible with other compression types */
	    if (ccp_wantoptions[f->unit].mppe) {
		ao->bsd_compress = go->bsd_compress = 0;
		ao->predictor_1  = go->predictor_1  = 0;
		ao->predictor_2  = go->predictor_2  = 0;
		ao->deflate	 = go->deflate	    = 0;
		ao->lzs		 = go->lzs	    = 0;
	    }
	}
    }
#endif /* MPPE */
    if (go->lzs) {
	opt_buf[0] = CI_LZS;
	opt_buf[1] = CILEN_LZS;
	opt_buf[2] = go->lzs_hists >> 8;
	opt_buf[3] = go->lzs_hists & 0xff;
	opt_buf[4] = LZS_MODE_SEQ;
	if (ccp_test(f->unit, opt_buf, CILEN_LZS, 0) <= 0)
	    go->lzs = 0;
    }
    if (go->bsd_compress) {
	opt_buf[0] = CI_BSD_COMPRESS;
	opt_buf[1] = CILEN_BSD_COMPRESS;
	opt_buf[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION, BSD_MIN_BITS);
	if (ccp_test(f->unit, opt_buf, CILEN_BSD_COMPRESS, 0) <= 0)
	    go->bsd_compress = 0;
    }
    if (go->deflate) {
	if (go->deflate_correct) {
	    opt_buf[0] = CI_DEFLATE;
	    opt_buf[1] = CILEN_DEFLATE;
	    opt_buf[2] = DEFLATE_MAKE_OPT(DEFLATE_MIN_WORKS);
	    opt_buf[3] = DEFLATE_CHK_SEQUENCE;
	    if (ccp_test(f->unit, opt_buf, CILEN_DEFLATE, 0) <= 0)
		go->deflate_correct = 0;
	}
	if (go->deflate_draft) {
	    opt_buf[0] = CI_DEFLATE_DRAFT;
	    opt_buf[1] = CILEN_DEFLATE;
	    opt_buf[2] = DEFLATE_MAKE_OPT(DEFLATE_MIN_WORKS);
	    opt_buf[3] = DEFLATE_CHK_SEQUENCE;
	    if (ccp_test(f->unit, opt_buf, CILEN_DEFLATE, 0) <= 0)
		go->deflate_draft = 0;
	}
	if (!go->deflate_correct && !go->deflate_draft)
	    go->deflate = 0;
    }
    if (go->predictor_1) {
	opt_buf[0] = CI_PREDICTOR_1;
	opt_buf[1] = CILEN_PREDICTOR_1;
	if (ccp_test(f->unit, opt_buf, CILEN_PREDICTOR_1, 0) <= 0)
	    go->predictor_1 = 0;
    }
    if (go->predictor_2) {
	opt_buf[0] = CI_PREDICTOR_2;
	opt_buf[1] = CILEN_PREDICTOR_2;
	if (ccp_test(f->unit, opt_buf, CILEN_PREDICTOR_2, 0) <= 0)
	    go->predictor_2 = 0;
    }
}

/*
 * ccp_cilen - Return total length of our configuration info.
 */
static int
ccp_cilen(f)
    fsm *f;
{
    ccp_options *go = &ccp_gotoptions[f->unit];

    return (go->bsd_compress? CILEN_BSD_COMPRESS: 0)
	+ (go->deflate? CILEN_DEFLATE: 0)
	+ (go->predictor_1? CILEN_PREDICTOR_1: 0)
	+ (go->predictor_2? CILEN_PREDICTOR_2: 0)
	+ (go->lzs? CILEN_LZS: 0)
	+ ((go->mppe || go->mppc)? CILEN_MPPE: 0);
}

/*
 * ccp_addci - put our requests in a packet.
 */
static void
ccp_addci(f, p, lenp)
    fsm *f;
    u_char *p;
    int *lenp;
{
    int res;
    ccp_options *go = &ccp_gotoptions[f->unit];
    ccp_options *ao = &ccp_allowoptions[f->unit];
    ccp_options *wo = &ccp_wantoptions[f->unit];
    u_char *p0 = p;

    /*
     * Add the compression types that we can receive, in decreasing
     * preference order.  Get the kernel to allocate the first one
     * in case it gets Acked.
     */
#ifdef MPPE
    if (go->mppe || go->mppc || (!wo->mppe && ao->mppe)) {
	u_char opt_buf[CILEN_MPPE + MPPE_MAX_KEY_LEN];

	p[0] = CI_MPPE;
	p[1] = CILEN_MPPE;
	p[2] = (go->mppe_stateless ? MPPE_STATELESS : 0);
	p[3] = 0;
	p[4] = 0;
	p[5] = (go->mppe_40 ? MPPE_40BIT : 0) | (go->mppe_56 ? MPPE_56BIT : 0) |
	    (go->mppe_128 ? MPPE_128BIT : 0) | (go->mppc ? MPPE_MPPC : 0);

	BCOPY(p, opt_buf, CILEN_MPPE);
	BCOPY(mppe_recv_key, &opt_buf[CILEN_MPPE], MPPE_MAX_KEY_LEN);
	res = ccp_test(f->unit, opt_buf, CILEN_MPPE + MPPE_MAX_KEY_LEN, 0);
	if (res > 0) {
	    p += CILEN_MPPE;
	} else {
	    /* This shouldn't happen, we've already tested it! */
	    go->mppe = go->mppe_40 = go->mppe_56 = go->mppe_128 =
		go->mppe_stateless = go->mppc = 0;
	    if (ccp_wantoptions[f->unit].mppe)
		lcp_close(f->unit, "MPPE required but not available in kernel");
	}
    }
#endif /* MPPE */
    if (go->lzs) {
	p[0] = CI_LZS;
	p[1] = CILEN_LZS;
	p[2] = go->lzs_hists >> 8;
	p[3] = go->lzs_hists & 0xff;
	p[4] = LZS_MODE_SEQ;
	res = ccp_test(f->unit, p, CILEN_LZS, 0);
	if (res > 0) {
	    p += CILEN_LZS;
	} else
	    go->lzs = 0;
    }
    if (go->deflate) {
	p[0] = go->deflate_correct? CI_DEFLATE: CI_DEFLATE_DRAFT;
	p[1] = CILEN_DEFLATE;
	p[2] = DEFLATE_MAKE_OPT(go->deflate_size);
	p[3] = DEFLATE_CHK_SEQUENCE;
	if (p != p0) {
	    p += CILEN_DEFLATE;
	} else {
	    for (;;) {
		if (go->deflate_size < DEFLATE_MIN_WORKS) {
		    go->deflate = 0;
		    break;
		}
		res = ccp_test(f->unit, p, CILEN_DEFLATE, 0);
		if (res > 0) {
		    p += CILEN_DEFLATE;
		    break;
		} else if (res < 0) {
		    go->deflate = 0;
		    break;
		}
		--go->deflate_size;
		p[2] = DEFLATE_MAKE_OPT(go->deflate_size);
	    }
	}
	if (p != p0 && go->deflate_correct && go->deflate_draft) {
	    p[0] = CI_DEFLATE_DRAFT;
	    p[1] = CILEN_DEFLATE;
	    p[2] = p[2 - CILEN_DEFLATE];
	    p[3] = DEFLATE_CHK_SEQUENCE;
	    p += CILEN_DEFLATE;
	}
    }
    if (go->bsd_compress) {
	p[0] = CI_BSD_COMPRESS;
	p[1] = CILEN_BSD_COMPRESS;
	p[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION, go->bsd_bits);
	if (p != p0) {
	    p += CILEN_BSD_COMPRESS;	/* not the first option */
	} else {
	    for (;;) {
		if (go->bsd_bits < BSD_MIN_BITS) {
		    go->bsd_compress = 0;
		    break;
		}
		res = ccp_test(f->unit, p, CILEN_BSD_COMPRESS, 0);
		if (res > 0) {
		    p += CILEN_BSD_COMPRESS;
		    break;
		} else if (res < 0) {
		    go->bsd_compress = 0;
		    break;
		}
		--go->bsd_bits;
		p[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION, go->bsd_bits);
	    }
	}
    }
    /* XXX Should Predictor 2 be preferable to Predictor 1? */
    if (go->predictor_1) {
	p[0] = CI_PREDICTOR_1;
	p[1] = CILEN_PREDICTOR_1;
	if (p == p0 && ccp_test(f->unit, p, CILEN_PREDICTOR_1, 0) <= 0) {
	    go->predictor_1 = 0;
	} else {
	    p += CILEN_PREDICTOR_1;
	}
    }
    if (go->predictor_2) {
	p[0] = CI_PREDICTOR_2;
	p[1] = CILEN_PREDICTOR_2;
	if (p == p0 && ccp_test(f->unit, p, CILEN_PREDICTOR_2, 0) <= 0) {
	    go->predictor_2 = 0;
	} else {
	    p += CILEN_PREDICTOR_2;
	}
    }

    go->method = (p > p0)? p0[0]: -1;

    *lenp = p - p0;
}

/*
 * ccp_ackci - process a received configure-ack, and return
 * 1 if the packet was OK.
 */
static int
ccp_ackci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    ccp_options *ao = &ccp_allowoptions[f->unit];
    ccp_options *wo = &ccp_wantoptions[f->unit];
    u_char *p0 = p;

#ifdef MPPE
    if (go->mppe || go->mppc || (!wo->mppe && ao->mppe)) {
	if (len < CILEN_MPPE
	    || p[1] != CILEN_MPPE || p[0] != CI_MPPE
	    || p[2] != (go->mppe_stateless ? MPPE_STATELESS : 0)
	    || p[3] != 0
	    || p[4] != 0
	    || (p[5] != ((go->mppe_40 ? MPPE_40BIT : 0) |
			 (go->mppc ? MPPE_MPPC : 0))
		&& p[5] != ((go->mppe_56 ? MPPE_56BIT : 0) |
			    (go->mppc ? MPPE_MPPC : 0))
		&& p[5] != ((go->mppe_128 ? MPPE_128BIT : 0) |
			    (go->mppc ? MPPE_MPPC : 0))))
	    return 0;
	if (go->mppe_40 || go->mppe_56 || go->mppe_128)
	    go->mppe = 1;
	p += CILEN_MPPE;
	len -= CILEN_MPPE;
	/* Cope with first/fast ack */
	if (p == p0 && len == 0)
	    return 1;
    }
#endif /* MPPE */
    if (go->lzs) {
	if (len < CILEN_LZS || p[0] != CI_LZS || p[1] != CILEN_LZS
	    || p[2] != go->lzs_hists>>8 || p[3] != (go->lzs_hists&0xff)
	    || p[4] != LZS_MODE_SEQ)
	    return 0;
	p += CILEN_LZS;
	len -= CILEN_LZS;
	/* XXX Cope with first/fast ack */
	if (p == p0 && len == 0)
	    return 1;
    }
    if (go->deflate) {
	if (len < CILEN_DEFLATE
	    || p[0] != (go->deflate_correct? CI_DEFLATE: CI_DEFLATE_DRAFT)
	    || p[1] != CILEN_DEFLATE
	    || p[2] != DEFLATE_MAKE_OPT(go->deflate_size)
	    || p[3] != DEFLATE_CHK_SEQUENCE)
	    return 0;
	p += CILEN_DEFLATE;
	len -= CILEN_DEFLATE;
	/* XXX Cope with first/fast ack */
	if (len == 0)
	    return 1;
	if (go->deflate_correct && go->deflate_draft) {
	    if (len < CILEN_DEFLATE
		|| p[0] != CI_DEFLATE_DRAFT
		|| p[1] != CILEN_DEFLATE
		|| p[2] != DEFLATE_MAKE_OPT(go->deflate_size)
		|| p[3] != DEFLATE_CHK_SEQUENCE)
		return 0;
	    p += CILEN_DEFLATE;
	    len -= CILEN_DEFLATE;
	}
    }
    if (go->bsd_compress) {
	if (len < CILEN_BSD_COMPRESS
	    || p[0] != CI_BSD_COMPRESS || p[1] != CILEN_BSD_COMPRESS
	    || p[2] != BSD_MAKE_OPT(BSD_CURRENT_VERSION, go->bsd_bits))
	    return 0;
	p += CILEN_BSD_COMPRESS;
	len -= CILEN_BSD_COMPRESS;
	/* XXX Cope with first/fast ack */
	if (p == p0 && len == 0)
	    return 1;
    }
    if (go->predictor_1) {
	if (len < CILEN_PREDICTOR_1
	    || p[0] != CI_PREDICTOR_1 || p[1] != CILEN_PREDICTOR_1)
	    return 0;
	p += CILEN_PREDICTOR_1;
	len -= CILEN_PREDICTOR_1;
	/* XXX Cope with first/fast ack */
	if (p == p0 && len == 0)
	    return 1;
    }
    if (go->predictor_2) {
	if (len < CILEN_PREDICTOR_2
	    || p[0] != CI_PREDICTOR_2 || p[1] != CILEN_PREDICTOR_2)
	    return 0;
	p += CILEN_PREDICTOR_2;
	len -= CILEN_PREDICTOR_2;
	/* XXX Cope with first/fast ack */
	if (p == p0 && len == 0)
	    return 1;
    }

    if (len != 0)
	return 0;
    return 1;
}

/*
 * ccp_nakci - process received configure-nak.
 * Returns 1 if the nak was OK.
 */
static int
ccp_nakci(f, p, len, treat_as_reject)
    fsm *f;
    u_char *p;
    int len;
    int treat_as_reject;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    ccp_options *ao = &ccp_allowoptions[f->unit];
    ccp_options *wo = &ccp_wantoptions[f->unit];
    ccp_options no;		/* options we've seen already */
    ccp_options try;		/* options to ask for next time */

    memset(&no, 0, sizeof(no));
    try = *go;

#ifdef MPPE
    if ((go->mppe || go->mppc || (!wo->mppe && ao->mppe)) &&
	len >= CILEN_MPPE && p[0] == CI_MPPE && p[1] == CILEN_MPPE) {

	if (go->mppc) {
	    no.mppc = 1;
	    if (!(p[5] & MPPE_MPPC))
		try.mppc = 0;
	}

	if (go->mppe)
	    no.mppe = 1;
	if (go->mppe_40)
	    no.mppe_40 = 1;
	if (go->mppe_56)
	    no.mppe_56 = 1;
	if (go->mppe_128)
	    no.mppe_128 = 1;
	if (go->mppe_stateless)
	    no.mppe_stateless = 1;

	if (ao->mppe_40) {
	    if ((p[5] & MPPE_40BIT))
		try.mppe_40 = 1;
	    else
		try.mppe_40 = (p[5] == 0) ? 1 : 0;
	}
	if (ao->mppe_56) {
	    if ((p[5] & MPPE_56BIT))
		try.mppe_56 = 1;
	    else
		try.mppe_56 = (p[5] == 0) ? 1 : 0;
	}
	if (ao->mppe_128) {
	    if ((p[5] & MPPE_128BIT))
		try.mppe_128 = 1;
	    else
		try.mppe_128 = (p[5] == 0) ? 1 : 0;
	}

	if (ao->mppe_stateless) {
	    if ((p[2] & MPPE_STATELESS) || wo->mppe_stateless)
		try.mppe_stateless = 1;
	    else
		try.mppe_stateless = 0;
	}

	if (!try.mppe_56 && !try.mppe_40 && !try.mppe_128) {
	    try.mppe = try.mppe_stateless = 0;
	    if (wo->mppe) {
		/* we require encryption, but peer doesn't support it
		   so we close connection */
		//wo->mppc = wo->mppe = wo->mppe_stateless = wo->mppe_40 =
		//    wo->mppe_56 = wo->mppe_128 = 0;
		error("MPPE required but cannot negotiate MPPE key length");
		lcp_close(f->unit, "MPPE required but cannot negotiate MPPE "
			  "key length");
	    }
        }
	if (wo->mppe && (wo->mppe_40 != try.mppe_40) &&
	    (wo->mppe_56 != try.mppe_56) && (wo->mppe_128 != try.mppe_128)) {
	    /* cannot negotiate key length */
	    //wo->mppc = wo->mppe = wo->mppe_stateless = wo->mppe_40 =
	    //	wo->mppe_56 = wo->mppe_128 = 0;
	    error("Cannot negotiate MPPE key length");
	    lcp_close(f->unit, "Cannot negotiate MPPE key length");
	}
	if (try.mppe_40 && try.mppe_56 && try.mppe_128)
	    try.mppe_40 = try.mppe_56 = 0;
	else
	    if (try.mppe_56 && try.mppe_128)
		try.mppe_56 = 0;
	    else
		if (try.mppe_40 && try.mppe_128)
		    try.mppe_40 = 0;
		else
		    if (try.mppe_40 && try.mppe_56)
			try.mppe_40 = 0;

	p += CILEN_MPPE;
	len -= CILEN_MPPE;
    }
#endif /* MPPE */

    if (go->lzs && len >= CILEN_LZS && p[0] == CI_LZS && p[1] == CILEN_LZS) {
	no.lzs = 1;
	if (((p[2]<<8)|p[3]) > 1 || (p[4] != LZS_MODE_SEQ &&
				     p[4] != LZS_MODE_EXT))
	    try.lzs = 0;
	else {
	    try.lzs_mode = p[4];
	    try.lzs_hists = (p[2] << 8) | p[3];
	}
	p += CILEN_LZS;
	len -= CILEN_LZS;
    }

    if (go->deflate && len >= CILEN_DEFLATE
	&& p[0] == (go->deflate_correct? CI_DEFLATE: CI_DEFLATE_DRAFT)
	&& p[1] == CILEN_DEFLATE) {
	no.deflate = 1;
	/*
	 * Peer wants us to use a different code size or something.
	 * Stop asking for Deflate if we don't understand his suggestion.
	 */
	if (DEFLATE_METHOD(p[2]) != DEFLATE_METHOD_VAL
	    || DEFLATE_SIZE(p[2]) < DEFLATE_MIN_WORKS
	    || p[3] != DEFLATE_CHK_SEQUENCE)
	    try.deflate = 0;
	else if (DEFLATE_SIZE(p[2]) < go->deflate_size)
	    try.deflate_size = DEFLATE_SIZE(p[2]);
	p += CILEN_DEFLATE;
	len -= CILEN_DEFLATE;
	if (go->deflate_correct && go->deflate_draft
	    && len >= CILEN_DEFLATE && p[0] == CI_DEFLATE_DRAFT
	    && p[1] == CILEN_DEFLATE) {
	    p += CILEN_DEFLATE;
	    len -= CILEN_DEFLATE;
	}
    }

    if (go->bsd_compress && len >= CILEN_BSD_COMPRESS
	&& p[0] == CI_BSD_COMPRESS && p[1] == CILEN_BSD_COMPRESS) {
	no.bsd_compress = 1;
	/*
	 * Peer wants us to use a different number of bits
	 * or a different version.
	 */
	if (BSD_VERSION(p[2]) != BSD_CURRENT_VERSION)
	    try.bsd_compress = 0;
	else if (BSD_NBITS(p[2]) < go->bsd_bits)
	    try.bsd_bits = BSD_NBITS(p[2]);
	p += CILEN_BSD_COMPRESS;
	len -= CILEN_BSD_COMPRESS;
    }

    /*
     * Predictor-1 and 2 have no options, so they can't be Naked.
     *
     * There may be remaining options but we ignore them.
     */

    if (f->state != OPENED)
	*go = try;
    return 1;
}

/*
 * ccp_rejci - reject some of our suggested compression methods.
 */
static int
ccp_rejci(f, p, len)
    fsm *f;
    u_char *p;
    int len;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    ccp_options try;		/* options to request next time */

    try = *go;

    /*
     * Cope with empty configure-rejects by ceasing to send
     * configure-requests.
     */
    if (len == 0 && all_rejected[f->unit])
	return -1;

#ifdef MPPE
    if ((go->mppe || go->mppc) && len >= CILEN_MPPE
	&& p[0] == CI_MPPE && p[1] == CILEN_MPPE) {
	ccp_options *wo = &ccp_wantoptions[f->unit];
	if (p[2] != (go->mppe_stateless ? MPPE_STATELESS : 0) ||
	    p[3] != 0 ||
	    p[4] != 0 ||
	    p[5] != ((go->mppe_40 ? MPPE_40BIT : 0) |
		     (go->mppe_56 ? MPPE_56BIT : 0) |
		     (go->mppe_128 ? MPPE_128BIT : 0) |
		     (go->mppc ? MPPE_MPPC : 0)))
	    return 0;
	if (go->mppc)
	    try.mppc = 0;
	if (go->mppe) {
	    try.mppe = 0;
	    if (go->mppe_40)
		try.mppe_40 = 0;
	    if (go->mppe_56)
		try.mppe_56 = 0;
	    if (go->mppe_128)
		try.mppe_128 = 0;
	    if (go->mppe_stateless)
		try.mppe_stateless = 0;
	    if (!try.mppe_56 && !try.mppe_40 && !try.mppe_128)
		try.mppe = try.mppe_stateless = 0;
	    if (wo->mppe) { /* we want MPPE but cannot negotiate key length */
		//wo->mppc = wo->mppe = wo->mppe_stateless = wo->mppe_40 =
		//    wo->mppe_56 = wo->mppe_128 = 0;
		error("MPPE required but cannot negotiate MPPE key length");
		lcp_close(f->unit, "MPPE required but cannot negotiate MPPE "
			  "key length");
	    }
	}
	p += CILEN_MPPE;
	len -= CILEN_MPPE;
    }
#endif /* MPPE */
    if (go->lzs && len >= CILEN_LZS && p[0] == CI_LZS && p[1] == CILEN_LZS) {
	if (p[2] != go->lzs_hists>>8 || p[3] != (go->lzs_hists&0xff) 
	    || p[4] != go->lzs_mode)
	    return 0;
	try.lzs = 0;
	p += CILEN_LZS;
	len -= CILEN_LZS;
    }
    if (go->deflate_correct && len >= CILEN_DEFLATE
	&& p[0] == CI_DEFLATE && p[1] == CILEN_DEFLATE) {
	if (p[2] != DEFLATE_MAKE_OPT(go->deflate_size)
	    || p[3] != DEFLATE_CHK_SEQUENCE)
	    return 0;		/* Rej is bad */
	try.deflate_correct = 0;
	p += CILEN_DEFLATE;
	len -= CILEN_DEFLATE;
    }
    if (go->deflate_draft && len >= CILEN_DEFLATE
	&& p[0] == CI_DEFLATE_DRAFT && p[1] == CILEN_DEFLATE) {
	if (p[2] != DEFLATE_MAKE_OPT(go->deflate_size)
	    || p[3] != DEFLATE_CHK_SEQUENCE)
	    return 0;		/* Rej is bad */
	try.deflate_draft = 0;
	p += CILEN_DEFLATE;
	len -= CILEN_DEFLATE;
    }
    if (!try.deflate_correct && !try.deflate_draft)
	try.deflate = 0;
    if (go->bsd_compress && len >= CILEN_BSD_COMPRESS
	&& p[0] == CI_BSD_COMPRESS && p[1] == CILEN_BSD_COMPRESS) {
	if (p[2] != BSD_MAKE_OPT(BSD_CURRENT_VERSION, go->bsd_bits))
	    return 0;
	try.bsd_compress = 0;
	p += CILEN_BSD_COMPRESS;
	len -= CILEN_BSD_COMPRESS;
    }
    if (go->predictor_1 && len >= CILEN_PREDICTOR_1
	&& p[0] == CI_PREDICTOR_1 && p[1] == CILEN_PREDICTOR_1) {
	try.predictor_1 = 0;
	p += CILEN_PREDICTOR_1;
	len -= CILEN_PREDICTOR_1;
    }
    if (go->predictor_2 && len >= CILEN_PREDICTOR_2
	&& p[0] == CI_PREDICTOR_2 && p[1] == CILEN_PREDICTOR_2) {
	try.predictor_2 = 0;
	p += CILEN_PREDICTOR_2;
	len -= CILEN_PREDICTOR_2;
    }

    if (len != 0)
	return 0;

    if (f->state != OPENED)
	*go = try;

    return 1;
}

/*
 * ccp_reqci - processed a received configure-request.
 * Returns CONFACK, CONFNAK or CONFREJ and the packet modified
 * appropriately.
 */
static int
ccp_reqci(f, p, lenp, dont_nak)
    fsm *f;
    u_char *p;
    int *lenp;
    int dont_nak;
{
    int ret, newret, res;
    u_char *p0, *retp, p2, p5;
    int len, clen, type, nb;
    ccp_options *ho = &ccp_hisoptions[f->unit];
    ccp_options *ao = &ccp_allowoptions[f->unit];
    ccp_options *wo = &ccp_wantoptions[f->unit];
#ifdef MPPE
    u_char opt_buf[CILEN_MPPE + MPPE_MAX_KEY_LEN];
/*     int mtu; */
#endif /* MPPE */

    ret = CONFACK;
    retp = p0 = p;
    len = *lenp;

    memset(ho, 0, sizeof(ccp_options));
    ho->method = (len > 0)? p[0]: -1;

    while (len > 0) {
	newret = CONFACK;
	if (len < 2 || p[1] < 2 || p[1] > len) {
	    /* length is bad */
	    clen = len;
	    newret = CONFREJ;

	} else {
	    type = p[0];
	    clen = p[1];

	    switch (type) {
#ifdef MPPE
	    case CI_MPPE:
		if ((!ao->mppc && !ao->mppe) || clen != CILEN_MPPE) {
		    newret = CONFREJ;
		    break;
		}
		p2 = p[2];
		p5 = p[5];
		/* not sure what they want, tell 'em what we got */
		if (((p[2] & ~MPPE_STATELESS) != 0 || p[3] != 0 || p[4] != 0 ||
		     (p[5] & ~(MPPE_40BIT | MPPE_56BIT | MPPE_128BIT |
			       MPPE_MPPC)) != 0 || p[5] == 0) ||
		    (p[2] == 0 && p[3] == 0 && p[4] == 0 &&  p[5] == 0)) {
		    newret = CONFNAK;
		}

		if ((p[5] & MPPE_MPPC)) {
		    if (ao->mppc) {
			ho->mppc = 1;
			BCOPY(p, opt_buf, CILEN_MPPE);
			opt_buf[2] = opt_buf[3] = opt_buf[4] = 0;
			opt_buf[5] = MPPE_MPPC;
			if (ccp_test(f->unit, opt_buf, CILEN_MPPE, 1) <= 0) {
			    ho->mppc = 0;
			    p[5] &= ~MPPE_MPPC;
			    newret = CONFNAK;
			}
		    } else {
			newret = CONFREJ;
			if (wo->mppe || ao->mppe) {
			    p[5] &= ~MPPE_MPPC;
			    newret = CONFNAK;
			}
		    }
		}

		if (ao->mppe)
		    ho->mppe = 1;

		if ((p[2] & MPPE_STATELESS)) {
		    if (ao->mppe_stateless) {
			if (wo->mppe_stateless)
			    ho->mppe_stateless = 1;
			else {
			    newret = CONFNAK;
			    if (!dont_nak)
				p[2] &= ~MPPE_STATELESS;
			}
		    } else {
			newret = CONFNAK;
			if (!dont_nak)
			    p[2] &= ~MPPE_STATELESS;
		    }
		} else {
		    if (wo->mppe_stateless && !dont_nak) {
			//wo->mppe_stateless = 0;
			newret = CONFNAK;
			p[2] |= MPPE_STATELESS;
		    }
		}

		if ((p[5] & ~MPPE_MPPC) == (MPPE_40BIT|MPPE_56BIT|MPPE_128BIT)) {
		    newret = CONFNAK;
		    if (ao->mppe_128) {
			ho->mppe_128 = 1;
			p[5] &= ~(MPPE_40BIT|MPPE_56BIT);
			BCOPY(p, opt_buf, CILEN_MPPE);
			BCOPY(mppe_send_key, &opt_buf[CILEN_MPPE],
			      MPPE_MAX_KEY_LEN);
			if (ccp_test(f->unit, opt_buf, CILEN_MPPE +
				     MPPE_MAX_KEY_LEN, 1) <= 0) {
			    ho->mppe_128 = 0;
			    p[5] |= (MPPE_40BIT|MPPE_56BIT);
			    p[5] &= ~MPPE_128BIT;
			    goto check_mppe_56_40;
			}
			goto check_mppe;
		    }
		    p[5] &= ~MPPE_128BIT;
		    goto check_mppe_56_40;
		}
		if ((p[5] & ~MPPE_MPPC) == (MPPE_56BIT|MPPE_128BIT)) {
		    newret = CONFNAK;
		    if (ao->mppe_128) {
			ho->mppe_128 = 1;
			p[5] &= ~MPPE_56BIT;
			BCOPY(p, opt_buf, CILEN_MPPE);
			BCOPY(mppe_send_key, &opt_buf[CILEN_MPPE],
			      MPPE_MAX_KEY_LEN);
			if (ccp_test(f->unit, opt_buf, CILEN_MPPE +
				     MPPE_MAX_KEY_LEN, 1) <= 0) {
			    ho->mppe_128 = 0;
			    p[5] |= MPPE_56BIT;
			    p[5] &= ~MPPE_128BIT;
			    goto check_mppe_56;
			}
			goto check_mppe;
		    }
		    p[5] &= ~MPPE_128BIT;
		    goto check_mppe_56;
		}
		if ((p[5] & ~MPPE_MPPC) == (MPPE_40BIT|MPPE_128BIT)) {
		    newret = CONFNAK;
		    if (ao->mppe_128) {
			ho->mppe_128 = 1;
			p[5] &= ~MPPE_40BIT;
			BCOPY(p, opt_buf, CILEN_MPPE);
			BCOPY(mppe_send_key, &opt_buf[CILEN_MPPE],
			      MPPE_MAX_KEY_LEN);
			if (ccp_test(f->unit, opt_buf, CILEN_MPPE +
				     MPPE_MAX_KEY_LEN, 1) <= 0) {
			    ho->mppe_128 = 0;
			    p[5] |= MPPE_40BIT;
			    p[5] &= ~MPPE_128BIT;
			    goto check_mppe_40;
			}
			goto check_mppe;
		    }
		    p[5] &= ~MPPE_128BIT;
		    goto check_mppe_40;
		}
		if ((p[5] & ~MPPE_MPPC) == MPPE_128BIT) {
		    if (ao->mppe_128) {
			ho->mppe_128 = 1;
			BCOPY(p, opt_buf, CILEN_MPPE);
			BCOPY(mppe_send_key, &opt_buf[CILEN_MPPE],
			      MPPE_MAX_KEY_LEN);
			if (ccp_test(f->unit, opt_buf, CILEN_MPPE +
				     MPPE_MAX_KEY_LEN, 1) <= 0) {
			    ho->mppe_128 = 0;
			    p[5] &= ~MPPE_128BIT;
			    newret = CONFNAK;
			}
			goto check_mppe;
		    }
		    p[5] &= ~MPPE_128BIT;
		    newret = CONFNAK;
		    goto check_mppe;
		}
	    check_mppe_56_40:
		if ((p[5] & ~MPPE_MPPC) == (MPPE_40BIT|MPPE_56BIT)) {
		    newret = CONFNAK;
		    if (ao->mppe_56) {
			ho->mppe_56 = 1;
			p[5] &= ~MPPE_40BIT;
			BCOPY(p, opt_buf, CILEN_MPPE);
			BCOPY(mppe_send_key, &opt_buf[CILEN_MPPE],
			      MPPE_MAX_KEY_LEN);
			if (ccp_test(f->unit, opt_buf, CILEN_MPPE +
				     MPPE_MAX_KEY_LEN, 1) <= 0) {
			    ho->mppe_56 = 0;
			    p[5] |= MPPE_40BIT;
			    p[5] &= ~MPPE_56BIT;
			    newret = CONFNAK;
			    goto check_mppe_40;
			}
			goto check_mppe;
		    }
		    p[5] &= ~MPPE_56BIT;
		    goto check_mppe_40;
		}
	    check_mppe_56:
		if ((p[5] & ~MPPE_MPPC) == MPPE_56BIT) {
		    if (ao->mppe_56) {
			ho->mppe_56 = 1;
			BCOPY(p, opt_buf, CILEN_MPPE);
			BCOPY(mppe_send_key, &opt_buf[CILEN_MPPE],
			      MPPE_MAX_KEY_LEN);
			if (ccp_test(f->unit, opt_buf, CILEN_MPPE +
				     MPPE_MAX_KEY_LEN, 1) <= 0) {
			    ho->mppe_56 = 0;
			    p[5] &= ~MPPE_56BIT;
			    newret = CONFNAK;
			}
			goto check_mppe;
		    }
		    p[5] &= ~MPPE_56BIT;
		    newret = CONFNAK;
		    goto check_mppe;
		}
	    check_mppe_40:
		if ((p[5] & ~MPPE_MPPC) == MPPE_40BIT) {
		    if (ao->mppe_40) {
			ho->mppe_40 = 1;
			BCOPY(p, opt_buf, CILEN_MPPE);
			BCOPY(mppe_send_key, &opt_buf[CILEN_MPPE],
			      MPPE_MAX_KEY_LEN);
			if (ccp_test(f->unit, opt_buf, CILEN_MPPE +
				     MPPE_MAX_KEY_LEN, 1) <= 0) {
			    ho->mppe_40 = 0;
			    p[5] &= ~MPPE_40BIT;
			    newret = CONFNAK;
			}
			goto check_mppe;
		    }
		    p[5] &= ~MPPE_40BIT;
		}

	    check_mppe:
		if (!ho->mppe_40 && !ho->mppe_56 && !ho->mppe_128) {
		    if (wo->mppe_40 || wo->mppe_56 || wo->mppe_128) {
			newret = CONFNAK;
			p[2] |= (wo->mppe_stateless ? MPPE_STATELESS : 0);
			p[5] |= (wo->mppe_40 ? MPPE_40BIT : 0) |
			    (wo->mppe_56 ? MPPE_56BIT : 0) |
			    (wo->mppe_128 ? MPPE_128BIT : 0) |
			    (wo->mppc ? MPPE_MPPC : 0);
		    } else {
			ho->mppe = ho->mppe_stateless = 0;
		    }
		} else {
		    /* MPPE is not compatible with other compression types */
		    if (wo->mppe) {
			ao->bsd_compress = 0;
			ao->predictor_1 = 0;
			ao->predictor_2 = 0;
			ao->deflate = 0;
			ao->lzs = 0;
		    }
		}
		if ((!ho->mppc || !ao->mppc) && !ho->mppe) {
		    p[2] = p2;
		    p[5] = p5;
		    /* We cannot accept this.  */
		    newret = CONFREJ;
		    /* Give the peer our idea of what can be used,
		       so it can choose and confirm */
		    ho->mppe = ao->mppe;
		}

		/*
		 * I have commented the code below because according to RFC1547
		 * MTU is only information for higher level protocols about
		 * "the maximum allowable length for a packet (q.v.) transmitted
		 * over a point-to-point link without incurring network layer
		 * fragmentation." Of course a PPP implementation should be able
		 * to handle overhead added by MPPE - in our case apropriate code
		 * is located in drivers/net/ppp_generic.c in the kernel sources.
		 *
		 * According to RFC1661:
		 * - when negotiated MRU is less than 1500 octets, a PPP
		 *   implementation must still be able to receive at least 1500
		 *   octets,
		 * - when PFC is negotiated, a PPP implementation is still
		 *   required to receive frames with uncompressed protocol field.
		 *
		 * So why not to handle MPPE overhead without changing MTU value?
		 * I am sure that RFC3078, unfortunately silently, assumes that.
		 */

		/*
		 * We need to decrease the interface MTU by MPPE_PAD
		 * because MPPE frames **grow**.  The kernel [must]
		 * allocate MPPE_PAD extra bytes in xmit buffers.
		 */
/*
		mtu = netif_get_mtu(f->unit);
		if (mtu) {
		    netif_set_mtu(f->unit, mtu - MPPE_PAD);
		} else {
		    newret = CONFREJ;
		    if (ccp_wantoptions[f->unit].mppe) {
			error("Cannot adjust MTU needed by MPPE.");
			lcp_close(f->unit, "Cannot adjust MTU needed by MPPE.");
		    }
		}
*/
		break;
#endif /* MPPE */

	    case CI_LZS:
		if (!ao->lzs || clen != CILEN_LZS) {
		    newret = CONFREJ;
		    break;
		}

		ho->lzs = 1;
		ho->lzs_hists = (p[2] << 8) | p[3];
		ho->lzs_mode = p[4];
		if ((ho->lzs_hists != ao->lzs_hists) ||
		    (ho->lzs_mode != ao->lzs_mode)) {
		    newret = CONFNAK;
		    if (!dont_nak) {
			p[2] = ao->lzs_hists >> 8;
			p[3] = ao->lzs_hists & 0xff;
			p[4] = ao->lzs_mode;
		    } else
			break;
		}

		if (p == p0 && ccp_test(f->unit, p, CILEN_LZS, 1) <= 0) {
		    newret = CONFREJ;
		}
		break;

	    case CI_DEFLATE:
	    case CI_DEFLATE_DRAFT:
		if (!ao->deflate || clen != CILEN_DEFLATE
		    || (!ao->deflate_correct && type == CI_DEFLATE)
		    || (!ao->deflate_draft && type == CI_DEFLATE_DRAFT)) {
		    newret = CONFREJ;
		    break;
		}

		ho->deflate = 1;
		ho->deflate_size = nb = DEFLATE_SIZE(p[2]);
		if (DEFLATE_METHOD(p[2]) != DEFLATE_METHOD_VAL
		    || p[3] != DEFLATE_CHK_SEQUENCE
		    || nb > ao->deflate_size || nb < DEFLATE_MIN_WORKS) {
		    newret = CONFNAK;
		    if (!dont_nak) {
			p[2] = DEFLATE_MAKE_OPT(ao->deflate_size);
			p[3] = DEFLATE_CHK_SEQUENCE;
			/* fall through to test this #bits below */
		    } else
			break;
		}

		/*
		 * Check whether we can do Deflate with the window
		 * size they want.  If the window is too big, reduce
		 * it until the kernel can cope and nak with that.
		 * We only check this for the first option.
		 */
		if (p == p0) {
		    for (;;) {
			res = ccp_test(f->unit, p, CILEN_DEFLATE, 1);
			if (res > 0)
			    break;		/* it's OK now */
			if (res < 0 || nb == DEFLATE_MIN_WORKS || dont_nak) {
			    newret = CONFREJ;
			    p[2] = DEFLATE_MAKE_OPT(ho->deflate_size);
			    break;
			}
			newret = CONFNAK;
			--nb;
			p[2] = DEFLATE_MAKE_OPT(nb);
		    }
		}
		break;

	    case CI_BSD_COMPRESS:
		if (!ao->bsd_compress || clen != CILEN_BSD_COMPRESS) {
		    newret = CONFREJ;
		    break;
		}

		ho->bsd_compress = 1;
		ho->bsd_bits = nb = BSD_NBITS(p[2]);
		if (BSD_VERSION(p[2]) != BSD_CURRENT_VERSION
		    || nb > ao->bsd_bits || nb < BSD_MIN_BITS) {
		    newret = CONFNAK;
		    if (!dont_nak) {
			p[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION, ao->bsd_bits);
			/* fall through to test this #bits below */
		    } else
			break;
		}

		/*
		 * Check whether we can do BSD-Compress with the code
		 * size they want.  If the code size is too big, reduce
		 * it until the kernel can cope and nak with that.
		 * We only check this for the first option.
		 */
		if (p == p0) {
		    for (;;) {
			res = ccp_test(f->unit, p, CILEN_BSD_COMPRESS, 1);
			if (res > 0)
			    break;
			if (res < 0 || nb == BSD_MIN_BITS || dont_nak) {
			    newret = CONFREJ;
			    p[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION,
						ho->bsd_bits);
			    break;
			}
			newret = CONFNAK;
			--nb;
			p[2] = BSD_MAKE_OPT(BSD_CURRENT_VERSION, nb);
		    }
		}
		break;

	    case CI_PREDICTOR_1:
		if (!ao->predictor_1 || clen != CILEN_PREDICTOR_1) {
		    newret = CONFREJ;
		    break;
		}

		ho->predictor_1 = 1;
		if (p == p0
		    && ccp_test(f->unit, p, CILEN_PREDICTOR_1, 1) <= 0) {
		    newret = CONFREJ;
		}
		break;

	    case CI_PREDICTOR_2:
		if (!ao->predictor_2 || clen != CILEN_PREDICTOR_2) {
		    newret = CONFREJ;
		    break;
		}

		ho->predictor_2 = 1;
		if (p == p0
		    && ccp_test(f->unit, p, CILEN_PREDICTOR_2, 1) <= 0) {
		    newret = CONFREJ;
		}
		break;

	    default:
		newret = CONFREJ;
	    }
	}

	if (newret == CONFNAK && dont_nak)
	    newret = CONFREJ;
	if (!(newret == CONFACK || (newret == CONFNAK && ret == CONFREJ))) {
	    /* we're returning this option */
	    if (newret == CONFREJ && ret == CONFNAK)
		retp = p0;
	    ret = newret;
	    if (p != retp)
		BCOPY(p, retp, clen);
	    retp += clen;
	}

	p += clen;
	len -= clen;
    }

    if (ret != CONFACK) {
	if (ret == CONFREJ && *lenp == retp - p0)
	    all_rejected[f->unit] = 1;
	else
	    *lenp = retp - p0;
    }
    return ret;
}

/*
 * Make a string name for a compression method (or 2).
 */
static char *
method_name(opt, opt2)
    ccp_options *opt, *opt2;
{
    static char result[64];

    if (!ANY_COMPRESS(*opt))
	return "(none)";
    switch (opt->method) {
#ifdef MPPE
    case CI_MPPE:
    {
	char *p = result;
	char *q = result + sizeof(result); /* 1 past result */

	if (opt->mppe) {
	    if (opt->mppc) {
		slprintf(p, q - p, "MPPC/MPPE ");
		p += 10;
	    } else {
		slprintf(p, q - p, "MPPE ");
		p += 5;
	    }
	    if (opt->mppe_128) {
		slprintf(p, q - p, "128-bit ");
		p += 8;
	    } else if (opt->mppe_56) {
		slprintf(p, q - p, "56-bit ");
		p += 7;
	    } else if (opt->mppe_40) {
		slprintf(p, q - p, "40-bit ");
		p += 7;
	    }
	    if (opt->mppe_stateless)
		slprintf(p, q - p, "stateless");
	    else
		slprintf(p, q - p, "stateful");
	} else if (opt->mppc)
	    slprintf(p, q - p, "MPPC");
	break;
    }
#endif /* MPPE */
    case CI_LZS:
	return "Stac LZS";
    case CI_DEFLATE:
    case CI_DEFLATE_DRAFT:
	if (opt2 != NULL && opt2->deflate_size != opt->deflate_size)
	    slprintf(result, sizeof(result), "Deflate%s (%d/%d)",
		     (opt->method == CI_DEFLATE_DRAFT? "(old#)": ""),
		     opt->deflate_size, opt2->deflate_size);
	else
	    slprintf(result, sizeof(result), "Deflate%s (%d)",
		     (opt->method == CI_DEFLATE_DRAFT? "(old#)": ""),
		     opt->deflate_size);
	break;
    case CI_BSD_COMPRESS:
	if (opt2 != NULL && opt2->bsd_bits != opt->bsd_bits)
	    slprintf(result, sizeof(result), "BSD-Compress (%d/%d)",
		     opt->bsd_bits, opt2->bsd_bits);
	else
	    slprintf(result, sizeof(result), "BSD-Compress (%d)",
		     opt->bsd_bits);
	break;
    case CI_PREDICTOR_1:
	return "Predictor 1";
    case CI_PREDICTOR_2:
	return "Predictor 2";
    default:
	slprintf(result, sizeof(result), "Method %d", opt->method);
    }
    return result;
}

/*
 * CCP has come up - inform the kernel driver and log a message.
 */
static void
ccp_up(f)
    fsm *f;
{
    ccp_options *go = &ccp_gotoptions[f->unit];
    ccp_options *ho = &ccp_hisoptions[f->unit];
    char method1[64];

    ccp_flags_set(f->unit, 1, 1);
    if (ANY_COMPRESS(*go)) {
	if (ANY_COMPRESS(*ho)) {
	    if (go->method == ho->method) {
		notice("%s compression enabled", method_name(go, ho));
	    } else {
		strlcpy(method1, method_name(go, NULL), sizeof(method1));
		notice("%s / %s compression enabled",
		       method1, method_name(ho, NULL));
	    }
	} else
	    notice("%s receive compression enabled", method_name(go, NULL));
    } else if (ANY_COMPRESS(*ho))
	notice("%s transmit compression enabled", method_name(ho, NULL));
#ifdef MPPE
    if (go->mppe || go->mppc) {
	BZERO(mppe_recv_key, MPPE_MAX_KEY_LEN);
	BZERO(mppe_send_key, MPPE_MAX_KEY_LEN);
	continue_networks(f->unit);		/* Bring up IP et al */
    }
#endif /* MPPE */
}

/*
 * CCP has gone down - inform the kernel driver.
 */
static void
ccp_down(f)
    fsm *f;
{
    if (ccp_localstate[f->unit] & RACK_PENDING)
	UNTIMEOUT(ccp_rack_timeout, f);
    ccp_localstate[f->unit] = 0;
    ccp_flags_set(f->unit, 1, 0);
#ifdef MPPE
    if (ccp_gotoptions[f->unit].mppe) {
	ccp_gotoptions[f->unit].mppe = 0;
	if (lcp_fsm[f->unit].state == OPENED) {
	    /* If LCP is not already going down, make sure it does. */
	    error("MPPE disabled");
	    lcp_close(f->unit, "MPPE disabled");
	}
    }
#endif /* MPPE */
}

/*
 * Print the contents of a CCP packet.
 */
static char *ccp_codenames[] = {
    "ConfReq", "ConfAck", "ConfNak", "ConfRej",
    "TermReq", "TermAck", "CodeRej",
    NULL, NULL, NULL, NULL, NULL, NULL,
    "ResetReq", "ResetAck",
};

static int
ccp_printpkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    u_char *p0, *optend;
    int code, id, len;
    int optlen;

    p0 = p;
    if (plen < HEADERLEN)
	return 0;
    code = p[0];
    id = p[1];
    len = (p[2] << 8) + p[3];
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(ccp_codenames) / sizeof(char *)
	&& ccp_codenames[code-1] != NULL)
	printer(arg, " %s", ccp_codenames[code-1]);
    else
	printer(arg, " code=0x%x", code);
    printer(arg, " id=0x%x", id);
    len -= HEADERLEN;
    p += HEADERLEN;

    switch (code) {
    case CONFREQ:
    case CONFACK:
    case CONFNAK:
    case CONFREJ:
	/* print list of possible compression methods */
	while (len >= 2) {
	    code = p[0];
	    optlen = p[1];
	    if (optlen < 2 || optlen > len)
		break;
	    printer(arg, " <");
	    len -= optlen;
	    optend = p + optlen;
	    switch (code) {
#ifdef MPPE
	    case CI_MPPE:
		if (optlen >= CILEN_MPPE) {
		    printer(arg, "mppe %s %s %s %s %s %s",
			    (p[2] & MPPE_STATELESS)? "+H": "-H",
			    (p[5] & MPPE_56BIT)? "+M": "-M",
			    (p[5] & MPPE_128BIT)? "+S": "-S",
			    (p[5] & MPPE_40BIT)? "+L": "-L",
			    (p[5] & MPPE_D_BIT)? "+D": "-D",
			    (p[5] & MPPE_MPPC)? "+C": "-C");
		    if ((p[5] & ~(MPPE_56BIT | MPPE_128BIT | MPPE_40BIT |
				  MPPE_D_BIT | MPPE_MPPC)) ||
			(p[2] & ~MPPE_STATELESS))
			printer(arg, " (%.2x %.2x %.2x %.2x)",
				p[2], p[3], p[4], p[5]);
		    p += CILEN_MPPE;
		}
		break;
#endif /* MPPE */
	    case CI_LZS:
		if (optlen >= CILEN_LZS) {
		    printer(arg, "lzs %.2x %.2x %.2x", p[2], p[3], p[4]);
		    p += CILEN_LZS;
		}
		break;
	    case CI_DEFLATE:
	    case CI_DEFLATE_DRAFT:
		if (optlen >= CILEN_DEFLATE) {
		    printer(arg, "deflate%s %d",
			    (code == CI_DEFLATE_DRAFT? "(old#)": ""),
			    DEFLATE_SIZE(p[2]));
		    if (DEFLATE_METHOD(p[2]) != DEFLATE_METHOD_VAL)
			printer(arg, " method %d", DEFLATE_METHOD(p[2]));
		    if (p[3] != DEFLATE_CHK_SEQUENCE)
			printer(arg, " check %d", p[3]);
		    p += CILEN_DEFLATE;
		}
		break;
	    case CI_BSD_COMPRESS:
		if (optlen >= CILEN_BSD_COMPRESS) {
		    printer(arg, "bsd v%d %d", BSD_VERSION(p[2]),
			    BSD_NBITS(p[2]));
		    p += CILEN_BSD_COMPRESS;
		}
		break;
	    case CI_PREDICTOR_1:
		if (optlen >= CILEN_PREDICTOR_1) {
		    printer(arg, "predictor 1");
		    p += CILEN_PREDICTOR_1;
		}
		break;
	    case CI_PREDICTOR_2:
		if (optlen >= CILEN_PREDICTOR_2) {
		    printer(arg, "predictor 2");
		    p += CILEN_PREDICTOR_2;
		}
		break;
	    }
	    while (p < optend)
		printer(arg, " %.2x", *p++);
	    printer(arg, ">");
	}
	break;

    case TERMACK:
    case TERMREQ:
	if (len > 0 && *p >= ' ' && *p < 0x7f) {
	    print_string((char *)p, len, printer, arg);
	    p += len;
	    len = 0;
	}
	break;
    }

    /* dump out the rest of the packet in hex */
    while (--len >= 0)
	printer(arg, " %.2x", *p++);

    return p - p0;
}

/*
 * We have received a packet that the decompressor failed to
 * decompress.  Here we would expect to issue a reset-request, but
 * Motorola has a patent on resetting the compressor as a result of
 * detecting an error in the decompressed data after decompression.
 * (See US patent 5,130,993; international patent publication number
 * WO 91/10289; Australian patent 73296/91.)
 *
 * So we ask the kernel whether the error was detected after
 * decompression; if it was, we take CCP down, thus disabling
 * compression :-(, otherwise we issue the reset-request.
 */
static void
ccp_datainput(unit, pkt, len)
    int unit;
    u_char *pkt;
    int len;
{
    fsm *f;

    f = &ccp_fsm[unit];
    if (f->state == OPENED) {
	if (ccp_fatal_error(unit)) {
	    /*
	     * Disable compression by taking CCP down.
	     */
	    error("Lost compression sync: disabling compression");
	    ccp_close(unit, "Lost compression sync");
#ifdef MPPE
	    /* My module dosn't need this. J.D., 2003-07-06 */
	    /*
	     * If we were doing MPPE, we must also take the link down.
	     */
	    if (ccp_gotoptions[unit].mppe) {
		error("Too many MPPE errors, closing LCP");
		lcp_close(unit, "Too many MPPE errors");
	    }
#endif /* MPPE */
	} else {
	    /*
	     * When LZS or MPPE/MPPC is negotiated we just send CCP_RESETREQ
	     * and don't wait for CCP_RESETACK
	     */
	    if ((ccp_gotoptions[f->unit].method == CI_LZS) ||
		(ccp_gotoptions[f->unit].method == CI_MPPE)) {
		fsm_sdata(f, CCP_RESETREQ, f->reqid = ++f->id, NULL, 0);
		return;
	    }
	    /*
	     * Send a reset-request to reset the peer's compressor.
	     * We don't do that if we are still waiting for an
	     * acknowledgement to a previous reset-request.
	     */
	    if (!(ccp_localstate[f->unit] & RACK_PENDING)) {
		fsm_sdata(f, CCP_RESETREQ, f->reqid = ++f->id, NULL, 0);
		TIMEOUT(ccp_rack_timeout, f, RACKTIMEOUT);
		ccp_localstate[f->unit] |= RACK_PENDING;
	    } else
		ccp_localstate[f->unit] |= RREQ_REPEAT;
	}
    }
}

/*
 * Timeout waiting for reset-ack.
 */
static void
ccp_rack_timeout(arg)
    void *arg;
{
    fsm *f = arg;

    if (f->state == OPENED && ccp_localstate[f->unit] & RREQ_REPEAT) {
	fsm_sdata(f, CCP_RESETREQ, f->reqid, NULL, 0);
	TIMEOUT(ccp_rack_timeout, f, RACKTIMEOUT);
	ccp_localstate[f->unit] &= ~RREQ_REPEAT;
    } else
	ccp_localstate[f->unit] &= ~RACK_PENDING;
}

