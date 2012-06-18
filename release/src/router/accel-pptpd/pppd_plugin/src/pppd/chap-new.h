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

/*
 * CHAP packets begin with a standard header with code, id, len (2 bytes).
 */
#define CHAP_HDRLEN	4

/*
 * Values for the code field.
 */
#define CHAP_CHALLENGE	1
#define CHAP_RESPONSE	2
#define CHAP_SUCCESS	3
#define CHAP_FAILURE	4

/*
 * CHAP digest codes.
 */
#define CHAP_MD5		5
#define CHAP_MICROSOFT		0x80
#define CHAP_MICROSOFT_V2	0x81

/*
 * Semi-arbitrary limits on challenge and response fields.
 */
#define MAX_CHALLENGE_LEN	64
#define MAX_RESPONSE_LEN	64

/* bitmask of supported algorithms */
#define MDTYPE_MICROSOFT_V2	0x1
#define MDTYPE_MICROSOFT	0x2
#define MDTYPE_MD5		0x4
#define MDTYPE_NONE		0

/* hashes supported by this instance of pppd */
extern int chap_mdtype_all;

/* Return the digest alg. ID for the most preferred digest type. */
#define CHAP_DIGEST(mdtype) \
    ((mdtype) & MDTYPE_MD5)? CHAP_MD5: \
    ((mdtype) & MDTYPE_MICROSOFT_V2)? CHAP_MICROSOFT_V2: \
    ((mdtype) & MDTYPE_MICROSOFT)? CHAP_MICROSOFT: \
    0

/* Return the bit flag (lsb set) for our most preferred digest type. */
#define CHAP_MDTYPE(mdtype) ((mdtype) ^ ((mdtype) - 1)) & (mdtype)

/* Return the bit flag for a given digest algorithm ID. */
#define CHAP_MDTYPE_D(digest) \
    ((digest) == CHAP_MICROSOFT_V2)? MDTYPE_MICROSOFT_V2: \
    ((digest) == CHAP_MICROSOFT)? MDTYPE_MICROSOFT: \
    ((digest) == CHAP_MD5)? MDTYPE_MD5: \
    0

/* Can we do the requested digest? */
#define CHAP_CANDIGEST(mdtype, digest) \
    ((digest) == CHAP_MICROSOFT_V2)? (mdtype) & MDTYPE_MICROSOFT_V2: \
    ((digest) == CHAP_MICROSOFT)? (mdtype) & MDTYPE_MICROSOFT: \
    ((digest) == CHAP_MD5)? (mdtype) & MDTYPE_MD5: \
    0

/*
 * The code for each digest type has to supply one of these.
 */
struct chap_digest_type {
	int code;

	/*
	 * Note: challenge and response arguments below are formatted as
	 * a length byte followed by the actual challenge/response data.
	 */
	void (*generate_challenge)(unsigned char *challenge);
	int (*verify_response)(int id, char *name,
		unsigned char *secret, int secret_len,
		unsigned char *challenge, unsigned char *response,
		char *message, int message_space);
	void (*make_response)(unsigned char *response, int id, char *our_name,
		unsigned char *challenge, char *secret, int secret_len,
		unsigned char *priv);
	int (*check_success)(unsigned char *pkt, int len, unsigned char *priv);
	void (*handle_failure)(unsigned char *pkt, int len);

	struct chap_digest_type *next;
};

/* Hook for a plugin to validate CHAP challenge */
extern int (*chap_verify_hook)(char *name, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			char *message, int message_space);

/* Called by digest code to register a digest type */
extern void chap_register_digest(struct chap_digest_type *);

/* Called by authentication code to start authenticating the peer. */
extern void chap_auth_peer(int unit, char *our_name, int digest_code);

/* Called by auth. code to start authenticating us to the peer. */
extern void chap_auth_with_peer(int unit, char *our_name, int digest_code);

/* Represents the CHAP protocol to the main pppd code */
extern struct protent chap_protent;
