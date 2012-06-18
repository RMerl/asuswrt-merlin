/*
 * lcp.h - Link Control Protocol definitions.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: lcp.h,v 1.19 2002/12/04 23:03:32 paulus Exp $
 */

/*
 * Options.
 */
#define CI_MRU		1	/* Maximum Receive Unit */
#define CI_ASYNCMAP	2	/* Async Control Character Map */
#define CI_AUTHTYPE	3	/* Authentication Type */
#define CI_QUALITY	4	/* Quality Protocol */
#define CI_MAGICNUMBER	5	/* Magic Number */
#define CI_PCOMPRESSION	7	/* Protocol Field Compression */
#define CI_ACCOMPRESSION 8	/* Address/Control Field Compression */
#define CI_CALLBACK	13	/* callback */
#define CI_MRRU		17	/* max reconstructed receive unit; multilink */
#define CI_SSNHF	18	/* short sequence numbers for multilink */
#define CI_EPDISC	19	/* endpoint discriminator */

/*
 * LCP-specific packet types.
 */
#define PROTREJ		8	/* Protocol Reject */
#define ECHOREQ		9	/* Echo Request */
#define ECHOREP		10	/* Echo Reply */
#define DISCREQ		11	/* Discard Request */
#define CBCP_OPT	6	/* Use callback control protocol */

/*
 * The state of options is described by an lcp_options structure.
 */
typedef struct lcp_options {
    bool passive;		/* Don't die if we don't get a response */
    bool silent;		/* Wait for the other end to start first */
    bool restart;		/* Restart vs. exit after close */
    bool neg_mru;		/* Negotiate the MRU? */
    bool neg_asyncmap;		/* Negotiate the async map? */
    bool neg_upap;		/* Ask for UPAP authentication? */
    bool neg_chap;		/* Ask for CHAP authentication? */
    bool neg_eap;		/* Ask for EAP authentication? */
    bool neg_magicnumber;	/* Ask for magic number? */
    bool neg_pcompression;	/* HDLC Protocol Field Compression? */
    bool neg_accompression;	/* HDLC Address/Control Field Compression? */
    bool neg_lqr;		/* Negotiate use of Link Quality Reports */
    bool neg_cbcp;		/* Negotiate use of CBCP */
    bool neg_mrru;		/* negotiate multilink MRRU */
    bool neg_ssnhf;		/* negotiate short sequence numbers */
    bool neg_endpoint;		/* negotiate endpoint discriminator */
    int  mru;			/* Value of MRU */
    int	 mrru;			/* Value of MRRU, and multilink enable */
    u_char chap_mdtype;		/* which MD types (hashing algorithm) */
    u_int32_t asyncmap;		/* Value of async map */
    u_int32_t magicnumber;
    int  numloops;		/* Number of loops during magic number neg. */
    u_int32_t lqr_period;	/* Reporting period for LQR 1/100ths second */
    struct epdisc endpoint;	/* endpoint discriminator */
} lcp_options;

extern fsm lcp_fsm[];
extern lcp_options lcp_wantoptions[];
extern lcp_options lcp_gotoptions[];
extern lcp_options lcp_allowoptions[];
extern lcp_options lcp_hisoptions[];

#define DEFMRU	1500		/* Try for this */
#define MINMRU	128		/* No MRUs below this */
#define MAXMRU	16384		/* Normally limit MRU to this */

void lcp_open __P((int));
void lcp_close __P((int, char *));
void lcp_lowerup __P((int));
void lcp_lowerdown __P((int));
void lcp_sprotrej __P((int, u_char *, int));	/* send protocol reject */

extern struct protent lcp_protent;

/* Default number of times we receive our magic number from the peer
   before deciding the link is looped-back. */
#define DEFLOOPBACKFAIL	10
