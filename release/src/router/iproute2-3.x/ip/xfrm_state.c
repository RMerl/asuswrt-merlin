/* $USAGI: $ */

/*
 * Copyright (C)2004 USAGI/WIDE Project
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
/*
 * based on iproute.c
 */
/*
 * Authors:
 *	Masahide NAKAMURA @USAGI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <linux/xfrm.h>
#include "utils.h"
#include "xfrm.h"
#include "ip_common.h"

//#define NLMSG_DELETEALL_BUF_SIZE (4096-512)
#define NLMSG_DELETEALL_BUF_SIZE 8192

/*
 * Receiving buffer defines:
 * nlmsg
 *   data = struct xfrm_usersa_info
 *   rtattr
 *   rtattr
 *   ... (max count of rtattr is XFRM_MAX+1
 *
 *  each rtattr data = struct xfrm_algo(dynamic size) or xfrm_address_t
 */
#define NLMSG_BUF_SIZE 4096
#define RTA_BUF_SIZE 2048
#define XFRM_ALGO_KEY_BUF_SIZE 512
#define CTX_BUF_SIZE 256

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip xfrm state { add | update } ID [ ALGO-LIST ] [ mode MODE ]\n");
	fprintf(stderr, "        [ mark MARK [ mask MASK ] ] [ reqid REQID ] [ seq SEQ ]\n");
	fprintf(stderr, "        [ replay-window SIZE ] [ replay-seq SEQ ] [ replay-oseq SEQ ]\n");
	fprintf(stderr, "        [ flag FLAG-LIST ] [ sel SELECTOR ] [ LIMIT-LIST ] [ encap ENCAP ]\n");
	fprintf(stderr, "        [ coa ADDR[/PLEN] ] [ ctx CTX ]\n");
	fprintf(stderr, "Usage: ip xfrm state allocspi ID [ mode MODE ] [ mark MARK [ mask MASK ] ]\n");
	fprintf(stderr, "        [ reqid REQID ] [ seq SEQ ] [ min SPI max SPI ]\n");
	fprintf(stderr, "Usage: ip xfrm state { delete | get } ID [ mark MARK [ mask MASK ] ]\n");
	fprintf(stderr, "Usage: ip xfrm state { deleteall | list } [ ID ] [ mode MODE ] [ reqid REQID ]\n");
	fprintf(stderr, "        [ flag FLAG-LIST ]\n");
	fprintf(stderr, "Usage: ip xfrm state flush [ proto XFRM-PROTO ]\n");
	fprintf(stderr, "Usage: ip xfrm state count\n");
	fprintf(stderr, "ID := [ src ADDR ] [ dst ADDR ] [ proto XFRM-PROTO ] [ spi SPI ]\n");
	fprintf(stderr, "XFRM-PROTO := ");
	fprintf(stderr, "%s | ", strxf_xfrmproto(IPPROTO_ESP));
	fprintf(stderr, "%s | ", strxf_xfrmproto(IPPROTO_AH));
	fprintf(stderr, "%s | ", strxf_xfrmproto(IPPROTO_COMP));
	fprintf(stderr, "%s | ", strxf_xfrmproto(IPPROTO_ROUTING));
	fprintf(stderr, "%s\n", strxf_xfrmproto(IPPROTO_DSTOPTS));
	fprintf(stderr, "ALGO-LIST := [ ALGO-LIST ] ALGO\n");
	fprintf(stderr, "ALGO := { ");
	fprintf(stderr, "%s | ", strxf_algotype(XFRMA_ALG_CRYPT));
	fprintf(stderr, "%s | ", strxf_algotype(XFRMA_ALG_AUTH));
	fprintf(stderr, "%s", strxf_algotype(XFRMA_ALG_COMP));
	fprintf(stderr, " } ALGO-NAME ALGO-KEY |\n");
	fprintf(stderr, "        %s", strxf_algotype(XFRMA_ALG_AEAD));
	fprintf(stderr, " ALGO-NAME ALGO-KEY ALGO-ICV-LEN |\n");
	fprintf(stderr, "        %s", strxf_algotype(XFRMA_ALG_AUTH_TRUNC));
	fprintf(stderr, " ALGO-NAME ALGO-KEY ALGO-TRUNC-LEN\n");
 	fprintf(stderr, "MODE := transport | tunnel | ro | in_trigger | beet\n");
	fprintf(stderr, "FLAG-LIST := [ FLAG-LIST ] FLAG\n");
	fprintf(stderr, "FLAG := noecn | decap-dscp | nopmtudisc | wildrecv | icmp | af-unspec | align4\n");
	fprintf(stderr, "SELECTOR := [ src ADDR[/PLEN] ] [ dst ADDR[/PLEN] ] [ dev DEV ] [ UPSPEC ]\n");
	fprintf(stderr, "UPSPEC := proto { { ");
	fprintf(stderr, "%s | ", strxf_proto(IPPROTO_TCP));
	fprintf(stderr, "%s | ", strxf_proto(IPPROTO_UDP));
	fprintf(stderr, "%s | ", strxf_proto(IPPROTO_SCTP));
	fprintf(stderr, "%s", strxf_proto(IPPROTO_DCCP));
	fprintf(stderr, " } [ sport PORT ] [ dport PORT ] |\n");
	fprintf(stderr, "                  { ");
	fprintf(stderr, "%s | ", strxf_proto(IPPROTO_ICMP));
	fprintf(stderr, "%s | ", strxf_proto(IPPROTO_ICMPV6));
	fprintf(stderr, "%s", strxf_proto(IPPROTO_MH));
	fprintf(stderr, " } [ type NUMBER ] [ code NUMBER ] |\n");
	fprintf(stderr, "                  %s", strxf_proto(IPPROTO_GRE));
	fprintf(stderr, " [ key { DOTTED-QUAD | NUMBER } ] | PROTO }\n");
	fprintf(stderr, "LIMIT-LIST := [ LIMIT-LIST ] limit LIMIT\n");
	fprintf(stderr, "LIMIT := { time-soft | time-hard | time-use-soft | time-use-hard } SECONDS |\n");
	fprintf(stderr, "         { byte-soft | byte-hard } SIZE | { packet-soft | packet-hard } COUNT\n");
        fprintf(stderr, "ENCAP := { espinudp | espinudp-nonike } SPORT DPORT OADDR\n");

	exit(-1);
}

static int xfrm_algo_parse(struct xfrm_algo *alg, enum xfrm_attr_type_t type,
			   char *name, char *key, char *buf, int max)
{
	int len;
	int slen = strlen(key);

#if 0
	/* XXX: verifying both name and key is required! */
	fprintf(stderr, "warning: ALGO-NAME/ALGO-KEY will send to kernel promiscuously! (verifying them isn't implemented yet)\n");
#endif

	strncpy(alg->alg_name, name, sizeof(alg->alg_name));

	if (slen > 2 && strncmp(key, "0x", 2) == 0) {
		/* split two chars "0x" from the top */
		char *p = key + 2;
		int plen = slen - 2;
		int i;
		int j;

		/* Converting hexadecimal numbered string into real key;
		 * Convert each two chars into one char(value). If number
		 * of the length is odd, add zero on the top for rounding.
		 */

		/* calculate length of the converted values(real key) */
		len = (plen + 1) / 2;
		if (len > max)
			invarg("\"ALGO-KEY\" makes buffer overflow\n", key);

		for (i = - (plen % 2), j = 0; j < len; i += 2, j++) {
			char vbuf[3];
			__u8 val;

			vbuf[0] = i >= 0 ? p[i] : '0';
			vbuf[1] = p[i + 1];
			vbuf[2] = '\0';

			if (get_u8(&val, vbuf, 16))
				invarg("\"ALGO-KEY\" is invalid", key);

			buf[j] = val;
		}
	} else {
		len = slen;
		if (len > 0) {
			if (len > max)
				invarg("\"ALGO-KEY\" makes buffer overflow\n", key);

			strncpy(buf, key, len);
		}
	}

	alg->alg_key_len = len * 8;

	return 0;
}

static int xfrm_seq_parse(__u32 *seq, int *argcp, char ***argvp)
{
	int argc = *argcp;
	char **argv = *argvp;

	if (get_u32(seq, *argv, 0))
		invarg("\"SEQ\" is invalid", *argv);

	*seq = htonl(*seq);

	*argcp = argc;
	*argvp = argv;

	return 0;
}

static int xfrm_state_flag_parse(__u8 *flags, int *argcp, char ***argvp)
{
	int argc = *argcp;
	char **argv = *argvp;
	int len = strlen(*argv);

	if (len > 2 && strncmp(*argv, "0x", 2) == 0) {
		__u8 val = 0;

		if (get_u8(&val, *argv, 16))
			invarg("\"FLAG\" is invalid", *argv);
		*flags = val;
	} else {
		while (1) {
			if (strcmp(*argv, "noecn") == 0)
				*flags |= XFRM_STATE_NOECN;
			else if (strcmp(*argv, "decap-dscp") == 0)
				*flags |= XFRM_STATE_DECAP_DSCP;
			else if (strcmp(*argv, "nopmtudisc") == 0)
				*flags |= XFRM_STATE_NOPMTUDISC;
			else if (strcmp(*argv, "wildrecv") == 0)
				*flags |= XFRM_STATE_WILDRECV;
			else if (strcmp(*argv, "icmp") == 0)
				*flags |= XFRM_STATE_ICMP;
			else if (strcmp(*argv, "af-unspec") == 0)
				*flags |= XFRM_STATE_AF_UNSPEC;
			else if (strcmp(*argv, "align4") == 0)
				*flags |= XFRM_STATE_ALIGN4;
			else {
				PREV_ARG(); /* back track */
				break;
			}

			if (!NEXT_ARG_OK())
				break;
			NEXT_ARG();
		}
	}

	*argcp = argc;
	*argvp = argv;

	return 0;
}

static int xfrm_state_modify(int cmd, unsigned flags, int argc, char **argv)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr 	n;
		struct xfrm_usersa_info xsinfo;
		char   			buf[RTA_BUF_SIZE];
	} req;
	struct xfrm_replay_state replay;
	char *idp = NULL;
	char *aeadop = NULL;
	char *ealgop = NULL;
	char *aalgop = NULL;
	char *calgop = NULL;
	char *coap = NULL;
	char *sctxp = NULL;
	struct xfrm_mark mark = {0, 0};
	struct {
		struct xfrm_user_sec_ctx sctx;
		char    str[CTX_BUF_SIZE];
	} ctx;

	memset(&req, 0, sizeof(req));
	memset(&replay, 0, sizeof(replay));
	memset(&ctx, 0, sizeof(ctx));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.xsinfo));
	req.n.nlmsg_flags = NLM_F_REQUEST|flags;
	req.n.nlmsg_type = cmd;
	req.xsinfo.family = preferred_family;

	req.xsinfo.lft.soft_byte_limit = XFRM_INF;
	req.xsinfo.lft.hard_byte_limit = XFRM_INF;
	req.xsinfo.lft.soft_packet_limit = XFRM_INF;
	req.xsinfo.lft.hard_packet_limit = XFRM_INF;

	while (argc > 0) {
		if (strcmp(*argv, "mode") == 0) {
			NEXT_ARG();
			xfrm_mode_parse(&req.xsinfo.mode, &argc, &argv);
		} else if (strcmp(*argv, "mark") == 0) {
			xfrm_parse_mark(&mark, &argc, &argv);
		} else if (strcmp(*argv, "reqid") == 0) {
			NEXT_ARG();
			xfrm_reqid_parse(&req.xsinfo.reqid, &argc, &argv);
		} else if (strcmp(*argv, "seq") == 0) {
			NEXT_ARG();
			xfrm_seq_parse(&req.xsinfo.seq, &argc, &argv);
		} else if (strcmp(*argv, "replay-window") == 0) {
			NEXT_ARG();
			if (get_u8(&req.xsinfo.replay_window, *argv, 0))
				invarg("\"replay-window\" value is invalid", *argv);
		} else if (strcmp(*argv, "replay-seq") == 0) {
			NEXT_ARG();
			if (get_u32(&replay.seq, *argv, 0))
				invarg("\"replay-seq\" value is invalid", *argv);
		} else if (strcmp(*argv, "replay-oseq") == 0) {
			NEXT_ARG();
			if (get_u32(&replay.oseq, *argv, 0))
				invarg("\"replay-oseq\" value is invalid", *argv);
		} else if (strcmp(*argv, "flag") == 0) {
			NEXT_ARG();
			xfrm_state_flag_parse(&req.xsinfo.flags, &argc, &argv);
		} else if (strcmp(*argv, "sel") == 0) {
			NEXT_ARG();
			xfrm_selector_parse(&req.xsinfo.sel, &argc, &argv);
		} else if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			xfrm_lifetime_cfg_parse(&req.xsinfo.lft, &argc, &argv);
		} else if (strcmp(*argv, "encap") == 0) {
			struct xfrm_encap_tmpl encap;
			inet_prefix oa;
		        NEXT_ARG();
			xfrm_encap_type_parse(&encap.encap_type, &argc, &argv);
			NEXT_ARG();
			if (get_u16(&encap.encap_sport, *argv, 0))
				invarg("\"encap\" sport value is invalid", *argv);
			encap.encap_sport = htons(encap.encap_sport);
			NEXT_ARG();
			if (get_u16(&encap.encap_dport, *argv, 0))
				invarg("\"encap\" dport value is invalid", *argv);
			encap.encap_dport = htons(encap.encap_dport);
			NEXT_ARG();
			get_addr(&oa, *argv, AF_UNSPEC);
			memcpy(&encap.encap_oa, &oa.data, sizeof(encap.encap_oa));
			addattr_l(&req.n, sizeof(req.buf), XFRMA_ENCAP,
				  (void *)&encap, sizeof(encap));
		} else if (strcmp(*argv, "coa") == 0) {
			inet_prefix coa;
			xfrm_address_t xcoa;

			if (coap)
				duparg("coa", *argv);
			coap = *argv;

			NEXT_ARG();

			get_prefix(&coa, *argv, preferred_family);
			if (coa.family == AF_UNSPEC)
				invarg("\"coa\" address family is AF_UNSPEC", *argv);
			if (coa.bytelen > sizeof(xcoa))
				invarg("\"coa\" address length is too large", *argv);

			memset(&xcoa, 0, sizeof(xcoa));
			memcpy(&xcoa, &coa.data, coa.bytelen);

			addattr_l(&req.n, sizeof(req.buf), XFRMA_COADDR,
				  (void *)&xcoa, sizeof(xcoa));
		} else if (strcmp(*argv, "ctx") == 0) {
			char *context;

			if (sctxp)
				duparg("ctx", *argv);
			sctxp = *argv;

			NEXT_ARG();
			context = *argv;

			xfrm_sctx_parse((char *)&ctx.str, context, &ctx.sctx);
			addattr_l(&req.n, sizeof(req.buf), XFRMA_SEC_CTX,
				  (void *)&ctx, ctx.sctx.len);
		} else {
			/* try to assume ALGO */
			int type = xfrm_algotype_getbyname(*argv);
			switch (type) {
			case XFRMA_ALG_AEAD:
			case XFRMA_ALG_CRYPT:
			case XFRMA_ALG_AUTH:
			case XFRMA_ALG_AUTH_TRUNC:
			case XFRMA_ALG_COMP:
			{
				/* ALGO */
				struct {
					union {
						struct xfrm_algo alg;
						struct xfrm_algo_aead aead;
						struct xfrm_algo_auth auth;
					} u;
					char buf[XFRM_ALGO_KEY_BUF_SIZE];
				} alg = {};
				int len;
				__u32 icvlen, trunclen;
				char *name;
				char *key;
				char *buf;

				switch (type) {
				case XFRMA_ALG_AEAD:
					if (aeadop)
						duparg("ALGO-TYPE", *argv);
					aeadop = *argv;
					break;
				case XFRMA_ALG_CRYPT:
					if (ealgop)
						duparg("ALGO-TYPE", *argv);
					ealgop = *argv;
					break;
				case XFRMA_ALG_AUTH:
				case XFRMA_ALG_AUTH_TRUNC:
					if (aalgop)
						duparg("ALGO-TYPE", *argv);
					aalgop = *argv;
					break;
				case XFRMA_ALG_COMP:
					if (calgop)
						duparg("ALGO-TYPE", *argv);
					calgop = *argv;
					break;
				default:
					/* not reached */
					invarg("\"ALGO-TYPE\" is invalid\n", *argv);
				}

				if (!NEXT_ARG_OK())
					missarg("ALGO-NAME");
				NEXT_ARG();
				name = *argv;

				if (!NEXT_ARG_OK())
					missarg("ALGO-KEY");
				NEXT_ARG();
				key = *argv;

				buf = alg.u.alg.alg_key;
				len = sizeof(alg.u.alg);

				switch (type) {
				case XFRMA_ALG_AEAD:
					if (!NEXT_ARG_OK())
						missarg("ALGO-ICV-LEN");
					NEXT_ARG();
					if (get_u32(&icvlen, *argv, 0))
						invarg("\"aead\" ICV length is invalid",
						       *argv);
					alg.u.aead.alg_icv_len = icvlen;

					buf = alg.u.aead.alg_key;
					len = sizeof(alg.u.aead);
					break;
				case XFRMA_ALG_AUTH_TRUNC:
					if (!NEXT_ARG_OK())
						missarg("ALGO-TRUNC-LEN");
					NEXT_ARG();
					if (get_u32(&trunclen, *argv, 0))
						invarg("\"auth\" trunc length is invalid",
						       *argv);
					alg.u.auth.alg_trunc_len = trunclen;

					buf = alg.u.auth.alg_key;
					len = sizeof(alg.u.auth);
					break;
				}

				xfrm_algo_parse((void *)&alg, type, name, key,
						buf, sizeof(alg.buf));
				len += alg.u.alg.alg_key_len;

				addattr_l(&req.n, sizeof(req.buf), type,
					  (void *)&alg, len);
				break;
			}
			default:
				/* try to assume ID */
				if (idp)
					invarg("unknown", *argv);
				idp = *argv;

				/* ID */
				xfrm_id_parse(&req.xsinfo.saddr, &req.xsinfo.id,
					      &req.xsinfo.family, 0, &argc, &argv);
				if (preferred_family == AF_UNSPEC)
					preferred_family = req.xsinfo.family;
			}
		}
		argc--; argv++;
	}

	if (replay.seq || replay.oseq)
		addattr_l(&req.n, sizeof(req.buf), XFRMA_REPLAY_VAL,
			  (void *)&replay, sizeof(replay));

	if (!idp) {
		fprintf(stderr, "Not enough information: \"ID\" is required\n");
		exit(1);
	}

	if (mark.m & mark.v) {
		int r = addattr_l(&req.n, sizeof(req.buf), XFRMA_MARK,
				  (void *)&mark, sizeof(mark));
		if (r < 0) {
			fprintf(stderr, "XFRMA_MARK failed\n");
			exit(1);
		}
	}

	switch (req.xsinfo.mode) {
	case XFRM_MODE_TRANSPORT:
	case XFRM_MODE_TUNNEL:
		if (!xfrm_xfrmproto_is_ipsec(req.xsinfo.id.proto)) {
			fprintf(stderr, "\"mode\" is invalid with proto=%s\n",
				strxf_xfrmproto(req.xsinfo.id.proto));
			exit(1);
		}
		break;
	case XFRM_MODE_ROUTEOPTIMIZATION:
	case XFRM_MODE_IN_TRIGGER:
		if (!xfrm_xfrmproto_is_ro(req.xsinfo.id.proto)) {
			fprintf(stderr, "\"mode\" is invalid with proto=%s\n",
				strxf_xfrmproto(req.xsinfo.id.proto));
			exit(1);
		}
		if (req.xsinfo.id.spi != 0) {
			fprintf(stderr, "\"spi\" must be 0 with proto=%s\n",
				strxf_xfrmproto(req.xsinfo.id.proto));
			exit(1);
		}
		break;
	default:
		break;
	}

	if (aeadop || ealgop || aalgop || calgop) {
		if (!xfrm_xfrmproto_is_ipsec(req.xsinfo.id.proto)) {
			fprintf(stderr, "\"ALGO\" is invalid with proto=%s\n",
				strxf_xfrmproto(req.xsinfo.id.proto));
			exit(1);
		}
	} else {
		if (xfrm_xfrmproto_is_ipsec(req.xsinfo.id.proto)) {
			fprintf(stderr, "\"ALGO\" is required with proto=%s\n",
				strxf_xfrmproto(req.xsinfo.id.proto));
			exit (1);
		}
	}

	if (coap) {
		if (!xfrm_xfrmproto_is_ro(req.xsinfo.id.proto)) {
			fprintf(stderr, "\"coa\" is invalid with proto=%s\n",
				strxf_xfrmproto(req.xsinfo.id.proto));
			exit(1);
		}
	} else {
		if (xfrm_xfrmproto_is_ro(req.xsinfo.id.proto)) {
			fprintf(stderr, "\"coa\" is required with proto=%s\n",
				strxf_xfrmproto(req.xsinfo.id.proto));
			exit (1);
		}
	}

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (req.xsinfo.family == AF_UNSPEC)
		req.xsinfo.family = AF_INET;

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		exit(2);

	rtnl_close(&rth);

	return 0;
}

static int xfrm_state_allocspi(int argc, char **argv)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr 	n;
		struct xfrm_userspi_info xspi;
		char   			buf[RTA_BUF_SIZE];
	} req;
	char *idp = NULL;
	char *minp = NULL;
	char *maxp = NULL;
	struct xfrm_mark mark = {0, 0};
	char res_buf[NLMSG_BUF_SIZE];
	struct nlmsghdr *res_n = (struct nlmsghdr *)res_buf;

	memset(res_buf, 0, sizeof(res_buf));

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.xspi));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = XFRM_MSG_ALLOCSPI;
	req.xspi.info.family = preferred_family;

#if 0
	req.xsinfo.lft.soft_byte_limit = XFRM_INF;
	req.xsinfo.lft.hard_byte_limit = XFRM_INF;
	req.xsinfo.lft.soft_packet_limit = XFRM_INF;
	req.xsinfo.lft.hard_packet_limit = XFRM_INF;
#endif

	while (argc > 0) {
		if (strcmp(*argv, "mode") == 0) {
			NEXT_ARG();
			xfrm_mode_parse(&req.xspi.info.mode, &argc, &argv);
		} else if (strcmp(*argv, "mark") == 0) {
			xfrm_parse_mark(&mark, &argc, &argv);
		} else if (strcmp(*argv, "reqid") == 0) {
			NEXT_ARG();
			xfrm_reqid_parse(&req.xspi.info.reqid, &argc, &argv);
		} else if (strcmp(*argv, "seq") == 0) {
			NEXT_ARG();
			xfrm_seq_parse(&req.xspi.info.seq, &argc, &argv);
		} else if (strcmp(*argv, "min") == 0) {
			if (minp)
				duparg("min", *argv);
			minp = *argv;

			NEXT_ARG();

			if (get_u32(&req.xspi.min, *argv, 0))
				invarg("\"min\" value is invalid", *argv);
		} else if (strcmp(*argv, "max") == 0) {
			if (maxp)
				duparg("max", *argv);
			maxp = *argv;

			NEXT_ARG();

			if (get_u32(&req.xspi.max, *argv, 0))
				invarg("\"max\" value is invalid", *argv);
		} else {
			/* try to assume ID */
			if (idp)
				invarg("unknown", *argv);
			idp = *argv;

			/* ID */
			xfrm_id_parse(&req.xspi.info.saddr, &req.xspi.info.id,
				      &req.xspi.info.family, 0, &argc, &argv);
			if (req.xspi.info.id.spi) {
				fprintf(stderr, "\"SPI\" must be zero\n");
				exit(1);
			}
			if (preferred_family == AF_UNSPEC)
				preferred_family = req.xspi.info.family;
		}
		argc--; argv++;
	}

	if (!idp) {
		fprintf(stderr, "Not enough information: \"ID\" is required\n");
		exit(1);
	}

	if (minp) {
		if (!maxp) {
			fprintf(stderr, "\"max\" is missing\n");
			exit(1);
		}
		if (req.xspi.min > req.xspi.max) {
			fprintf(stderr, "\"min\" value is larger than \"max\" value\n");
			exit(1);
		}
	} else {
		if (maxp) {
			fprintf(stderr, "\"min\" is missing\n");
			exit(1);
		}

		/* XXX: Default value defined in PF_KEY;
		 * See kernel's net/key/af_key.c(pfkey_getspi).
		 */
		req.xspi.min = 0x100;
		req.xspi.max = 0x0fffffff;

		/* XXX: IPCOMP spi is 16-bits;
		 * See kernel's net/xfrm/xfrm_user(verify_userspi_info).
		 */
		if (req.xspi.info.id.proto == IPPROTO_COMP)
			req.xspi.max = 0xffff;
	}

	if (mark.m & mark.v) {
		int r = addattr_l(&req.n, sizeof(req.buf), XFRMA_MARK,
				  (void *)&mark, sizeof(mark));
		if (r < 0) {
			fprintf(stderr, "XFRMA_MARK failed\n");
			exit(1);
		}
	}

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (req.xspi.info.family == AF_UNSPEC)
		req.xspi.info.family = AF_INET;


	if (rtnl_talk(&rth, &req.n, 0, 0, res_n, NULL, NULL) < 0)
		exit(2);

	if (xfrm_state_print(NULL, res_n, (void*)stdout) < 0) {
		fprintf(stderr, "An error :-)\n");
		exit(1);
	}

	rtnl_close(&rth);

	return 0;
}

static int xfrm_state_filter_match(struct xfrm_usersa_info *xsinfo)
{
	if (!filter.use)
		return 1;

	if (filter.id_src_mask)
		if (xfrm_addr_match(&xsinfo->saddr, &filter.xsinfo.saddr,
				    filter.id_src_mask))
			return 0;
	if (filter.id_dst_mask)
		if (xfrm_addr_match(&xsinfo->id.daddr, &filter.xsinfo.id.daddr,
				    filter.id_dst_mask))
			return 0;
	if ((xsinfo->id.proto^filter.xsinfo.id.proto)&filter.id_proto_mask)
		return 0;
	if ((xsinfo->id.spi^filter.xsinfo.id.spi)&filter.id_spi_mask)
		return 0;
	if ((xsinfo->mode^filter.xsinfo.mode)&filter.mode_mask)
		return 0;
	if ((xsinfo->reqid^filter.xsinfo.reqid)&filter.reqid_mask)
		return 0;
	if (filter.state_flags_mask)
		if ((xsinfo->flags & filter.xsinfo.flags) == 0)
			return 0;

	return 1;
}

int xfrm_state_print(const struct sockaddr_nl *who, struct nlmsghdr *n,
		     void *arg)
{
	FILE *fp = (FILE*)arg;
	struct rtattr * tb[XFRMA_MAX+1];
	struct rtattr * rta;
	struct xfrm_usersa_info *xsinfo = NULL;
	struct xfrm_user_expire *xexp = NULL;
	struct xfrm_usersa_id	*xsid = NULL;
	int len = n->nlmsg_len;

	if (n->nlmsg_type != XFRM_MSG_NEWSA &&
	    n->nlmsg_type != XFRM_MSG_DELSA &&
	    n->nlmsg_type != XFRM_MSG_UPDSA &&
	    n->nlmsg_type != XFRM_MSG_EXPIRE) {
		fprintf(stderr, "Not a state: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}

	if (n->nlmsg_type == XFRM_MSG_DELSA) {
		/* Dont blame me for this .. Herbert made me do it */
		xsid = NLMSG_DATA(n);
		len -= NLMSG_SPACE(sizeof(*xsid));
	} else if (n->nlmsg_type == XFRM_MSG_EXPIRE) {
		xexp = NLMSG_DATA(n);
		xsinfo = &xexp->state;
		len -= NLMSG_SPACE(sizeof(*xexp));
	} else {
		xexp = NULL;
		xsinfo = NLMSG_DATA(n);
		len -= NLMSG_SPACE(sizeof(*xsinfo));
	}

	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (xsinfo && !xfrm_state_filter_match(xsinfo))
		return 0;

	if (n->nlmsg_type == XFRM_MSG_DELSA)
		fprintf(fp, "Deleted ");
	else if (n->nlmsg_type == XFRM_MSG_UPDSA)
		fprintf(fp, "Updated ");
	else if (n->nlmsg_type == XFRM_MSG_EXPIRE)
		fprintf(fp, "Expired ");

	if (n->nlmsg_type == XFRM_MSG_DELSA)
		rta = XFRMSID_RTA(xsid);
	else if (n->nlmsg_type == XFRM_MSG_EXPIRE)
		rta = XFRMEXP_RTA(xexp);
	else
		rta = XFRMS_RTA(xsinfo);

	parse_rtattr(tb, XFRMA_MAX, rta, len);

	if (n->nlmsg_type == XFRM_MSG_DELSA) {
		//xfrm_policy_id_print();

		if (!tb[XFRMA_SA]) {
			fprintf(stderr, "Buggy XFRM_MSG_DELSA: no XFRMA_SA\n");
			return -1;
		}
		if (RTA_PAYLOAD(tb[XFRMA_SA]) < sizeof(*xsinfo)) {
			fprintf(stderr, "Buggy XFRM_MSG_DELPOLICY: too short XFRMA_POLICY len\n");
			return -1;
		}
		xsinfo = RTA_DATA(tb[XFRMA_SA]);
	}

	xfrm_state_info_print(xsinfo, tb, fp, NULL, NULL);

	if (n->nlmsg_type == XFRM_MSG_EXPIRE) {
		fprintf(fp, "\t");
		fprintf(fp, "hard %u", xexp->hard);
		fprintf(fp, "%s", _SL_);
	}

	if (oneline)
		fprintf(fp, "\n");
	fflush(fp);

	return 0;
}

static int xfrm_state_get_or_delete(int argc, char **argv, int delete)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr 	n;
		struct xfrm_usersa_id	xsid;
		char   			buf[RTA_BUF_SIZE];
	} req;
	struct xfrm_id id;
	char *idp = NULL;
	struct xfrm_mark mark = {0, 0};

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.xsid));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = delete ? XFRM_MSG_DELSA : XFRM_MSG_GETSA;
	req.xsid.family = preferred_family;

	while (argc > 0) {
		xfrm_address_t saddr;

		if (strcmp(*argv, "mark") == 0) {
			xfrm_parse_mark(&mark, &argc, &argv);
		} else {
			if (idp)
				invarg("unknown", *argv);
			idp = *argv;

			/* ID */
			memset(&id, 0, sizeof(id));
			memset(&saddr, 0, sizeof(saddr));
			xfrm_id_parse(&saddr, &id, &req.xsid.family, 0,
				      &argc, &argv);

			memcpy(&req.xsid.daddr, &id.daddr, sizeof(req.xsid.daddr));
			req.xsid.spi = id.spi;
			req.xsid.proto = id.proto;

			addattr_l(&req.n, sizeof(req.buf), XFRMA_SRCADDR,
				  (void *)&saddr, sizeof(saddr));
		}

		argc--; argv++;
	}

	if (mark.m & mark.v) {
		int r = addattr_l(&req.n, sizeof(req.buf), XFRMA_MARK,
				  (void *)&mark, sizeof(mark));
		if (r < 0) {
			fprintf(stderr, "XFRMA_MARK failed\n");
			exit(1);
		}
	}

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (req.xsid.family == AF_UNSPEC)
		req.xsid.family = AF_INET;

	if (delete) {
		if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
			exit(2);
	} else {
		char buf[NLMSG_BUF_SIZE];
		struct nlmsghdr *res_n = (struct nlmsghdr *)buf;

		memset(buf, 0, sizeof(buf));

		if (rtnl_talk(&rth, &req.n, 0, 0, res_n, NULL, NULL) < 0)
			exit(2);

		if (xfrm_state_print(NULL, res_n, (void*)stdout) < 0) {
			fprintf(stderr, "An error :-)\n");
			exit(1);
		}
	}

	rtnl_close(&rth);

	return 0;
}

/*
 * With an existing state of nlmsg, make new nlmsg for deleting the state
 * and store it to buffer.
 */
static int xfrm_state_keep(const struct sockaddr_nl *who,
			   struct nlmsghdr *n,
			   void *arg)
{
	struct xfrm_buffer *xb = (struct xfrm_buffer *)arg;
	struct rtnl_handle *rth = xb->rth;
	struct xfrm_usersa_info *xsinfo = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct nlmsghdr *new_n;
	struct xfrm_usersa_id *xsid;

	if (n->nlmsg_type != XFRM_MSG_NEWSA) {
		fprintf(stderr, "Not a state: %08x %08x %08x\n",
			n->nlmsg_len, n->nlmsg_type, n->nlmsg_flags);
		return 0;
	}

	len -= NLMSG_LENGTH(sizeof(*xsinfo));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (!xfrm_state_filter_match(xsinfo))
		return 0;

	if (xb->offset > xb->size) {
		fprintf(stderr, "State buffer overflow\n");
		return -1;
	}

	new_n = (struct nlmsghdr *)(xb->buf + xb->offset);
	new_n->nlmsg_len = NLMSG_LENGTH(sizeof(*xsid));
	new_n->nlmsg_flags = NLM_F_REQUEST;
	new_n->nlmsg_type = XFRM_MSG_DELSA;
	new_n->nlmsg_seq = ++rth->seq;

	xsid = NLMSG_DATA(new_n);
	xsid->family = xsinfo->family;
	memcpy(&xsid->daddr, &xsinfo->id.daddr, sizeof(xsid->daddr));
	xsid->spi = xsinfo->id.spi;
	xsid->proto = xsinfo->id.proto;

	addattr_l(new_n, xb->size, XFRMA_SRCADDR, &xsinfo->saddr,
		  sizeof(xsid->daddr));

	xb->offset += new_n->nlmsg_len;
	xb->nlmsg_count ++;

	return 0;
}

static int xfrm_state_list_or_deleteall(int argc, char **argv, int deleteall)
{
	char *idp = NULL;
	struct rtnl_handle rth;

	if(argc > 0)
		filter.use = 1;
	filter.xsinfo.family = preferred_family;

	while (argc > 0) {
		if (strcmp(*argv, "mode") == 0) {
			NEXT_ARG();
			xfrm_mode_parse(&filter.xsinfo.mode, &argc, &argv);

			filter.mode_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "reqid") == 0) {
			NEXT_ARG();
			xfrm_reqid_parse(&filter.xsinfo.reqid, &argc, &argv);

			filter.reqid_mask = XFRM_FILTER_MASK_FULL;

		} else if (strcmp(*argv, "flag") == 0) {
			NEXT_ARG();
			xfrm_state_flag_parse(&filter.xsinfo.flags, &argc, &argv);

			filter.state_flags_mask = XFRM_FILTER_MASK_FULL;

		} else {
			if (idp)
				invarg("unknown", *argv);
			idp = *argv;

			/* ID */
			xfrm_id_parse(&filter.xsinfo.saddr, &filter.xsinfo.id,
				      &filter.xsinfo.family, 1, &argc, &argv);
			if (preferred_family == AF_UNSPEC)
				preferred_family = filter.xsinfo.family;
		}
		argc--; argv++;
	}

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (deleteall) {
		struct xfrm_buffer xb;
		char buf[NLMSG_DELETEALL_BUF_SIZE];
		int i;

		xb.buf = buf;
		xb.size = sizeof(buf);
		xb.rth = &rth;

		for (i = 0; ; i++) {
			xb.offset = 0;
			xb.nlmsg_count = 0;

			if (show_stats > 1)
				fprintf(stderr, "Delete-all round = %d\n", i);

			if (rtnl_wilddump_request(&rth, preferred_family, XFRM_MSG_GETSA) < 0) {
				perror("Cannot send dump request");
				exit(1);
			}

			if (rtnl_dump_filter(&rth, xfrm_state_keep, &xb, NULL, NULL) < 0) {
				fprintf(stderr, "Delete-all terminated\n");
				exit(1);
			}
			if (xb.nlmsg_count == 0) {
				if (show_stats > 1)
					fprintf(stderr, "Delete-all completed\n");
				break;
			}

			if (rtnl_send_check(&rth, xb.buf, xb.offset) < 0) {
				perror("Failed to send delete-all request\n");
				exit(1);
			}
			if (show_stats > 1)
				fprintf(stderr, "Delete-all nlmsg count = %d\n", xb.nlmsg_count);

			xb.offset = 0;
			xb.nlmsg_count = 0;
		}

	} else {
		if (rtnl_wilddump_request(&rth, preferred_family, XFRM_MSG_GETSA) < 0) {
			perror("Cannot send dump request");
			exit(1);
		}

		if (rtnl_dump_filter(&rth, xfrm_state_print, stdout, NULL, NULL) < 0) {
			fprintf(stderr, "Dump terminated\n");
			exit(1);
		}
	}

	rtnl_close(&rth);

	exit(0);
}

int print_sadinfo(struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	__u32 *f = NLMSG_DATA(n);
	struct rtattr *tb[XFRMA_SAD_MAX+1];
	struct rtattr *rta;
	__u32 *cnt;

	int len = n->nlmsg_len;

	len -= NLMSG_LENGTH(sizeof(__u32));
	if (len < 0) {
		fprintf(stderr, "SADinfo: Wrong len %d\n", len);
		return -1;
	}

	rta = XFRMSAPD_RTA(f);
	parse_rtattr(tb, XFRMA_SAD_MAX, rta, len);

	if (tb[XFRMA_SAD_CNT]) {
		fprintf(fp,"\t SAD");
		cnt = (__u32 *)RTA_DATA(tb[XFRMA_SAD_CNT]);
		fprintf(fp," count %d", *cnt);
	} else {
		fprintf(fp,"BAD SAD info returned\n");
		return -1;
	}

	if (show_stats) {
		if (tb[XFRMA_SAD_HINFO]) {
			struct xfrmu_sadhinfo *si;

			if (RTA_PAYLOAD(tb[XFRMA_SAD_HINFO]) < sizeof(*si)) {
				fprintf(fp,"BAD SAD length returned\n");
				return -1;
			}
				
			si = RTA_DATA(tb[XFRMA_SAD_HINFO]);
			fprintf(fp," (buckets ");
			fprintf(fp,"count %d", si->sadhcnt);
			fprintf(fp," Max %d", si->sadhmcnt);
			fprintf(fp,")");
		}
	}
	fprintf(fp,"\n");

        return 0;
}

static int xfrm_sad_getinfo(int argc, char **argv)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr			n;
		__u32				flags;
		char				ans[64];
	} req;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.flags));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = XFRM_MSG_GETSADINFO;
	req.flags = 0XFFFFFFFF;

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (rtnl_talk(&rth, &req.n, 0, 0, &req.n, NULL, NULL) < 0)
		exit(2);

	print_sadinfo(&req.n, (void*)stdout);

	rtnl_close(&rth);

	return 0;
}

static int xfrm_state_flush(int argc, char **argv)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr			n;
		struct xfrm_usersa_flush	xsf;
	} req;
	char *protop = NULL;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.xsf));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = XFRM_MSG_FLUSHSA;
	req.xsf.proto = 0;

	while (argc > 0) {
		if (strcmp(*argv, "proto") == 0) {
			int ret;

			if (protop)
				duparg("proto", *argv);
			protop = *argv;

			NEXT_ARG();

			ret = xfrm_xfrmproto_getbyname(*argv);
			if (ret < 0)
				invarg("\"XFRM-PROTO\" is invalid", *argv);

			req.xsf.proto = (__u8)ret;
		} else
			invarg("unknown", *argv);

		argc--; argv++;
	}

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (show_stats > 1)
		fprintf(stderr, "Flush state proto=%s\n",
			strxf_xfrmproto(req.xsf.proto));

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		exit(2);

	rtnl_close(&rth);

	return 0;
}

int do_xfrm_state(int argc, char **argv)
{
	if (argc < 1)
		return xfrm_state_list_or_deleteall(0, NULL, 0);

	if (matches(*argv, "add") == 0)
		return xfrm_state_modify(XFRM_MSG_NEWSA, 0,
					 argc-1, argv+1);
	if (matches(*argv, "update") == 0)
		return xfrm_state_modify(XFRM_MSG_UPDSA, 0,
					 argc-1, argv+1);
	if (matches(*argv, "allocspi") == 0)
		return xfrm_state_allocspi(argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return xfrm_state_get_or_delete(argc-1, argv+1, 1);
	if (matches(*argv, "deleteall") == 0 || matches(*argv, "delall") == 0)
		return xfrm_state_list_or_deleteall(argc-1, argv+1, 1);
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return xfrm_state_list_or_deleteall(argc-1, argv+1, 0);
	if (matches(*argv, "get") == 0)
		return xfrm_state_get_or_delete(argc-1, argv+1, 0);
	if (matches(*argv, "flush") == 0)
		return xfrm_state_flush(argc-1, argv+1);
	if (matches(*argv, "count") == 0) {
		return xfrm_sad_getinfo(argc, argv);
	}
	if (matches(*argv, "help") == 0)
		usage();
	fprintf(stderr, "Command \"%s\" is unknown, try \"ip xfrm state help\".\n", *argv);
	exit(-1);
}
