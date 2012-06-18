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

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip xfrm state { add | update } ID [ ALGO-LIST ] [ mode MODE ]\n");
	fprintf(stderr, "        [ reqid REQID ] [ seq SEQ ] [ replay-window SIZE ] [ flag FLAG-LIST ]\n");
	fprintf(stderr, "        [ encap ENCAP ] [ sel SELECTOR ] [ LIMIT-LIST ]\n");
	fprintf(stderr, "Usage: ip xfrm state allocspi ID [ mode MODE ] [ reqid REQID ] [ seq SEQ ]\n");
	fprintf(stderr, "        [ min SPI max SPI ]\n");
	fprintf(stderr, "Usage: ip xfrm state { delete | get } ID\n");
	fprintf(stderr, "Usage: ip xfrm state { deleteall | list } [ ID ] [ mode MODE ] [ reqid REQID ]\n");
	fprintf(stderr, "        [ flag FLAG_LIST ]\n");
	fprintf(stderr, "Usage: ip xfrm state flush [ proto XFRM_PROTO ]\n");

	fprintf(stderr, "ID := [ src ADDR ] [ dst ADDR ] [ proto XFRM_PROTO ] [ spi SPI ]\n");
	//fprintf(stderr, "XFRM_PROTO := [ esp | ah | comp ]\n");
	fprintf(stderr, "XFRM_PROTO := [ ");
	fprintf(stderr, "%s | ", strxf_xfrmproto(IPPROTO_ESP));
	fprintf(stderr, "%s | ", strxf_xfrmproto(IPPROTO_AH));
	fprintf(stderr, "%s ", strxf_xfrmproto(IPPROTO_COMP));
	fprintf(stderr, "]\n");

	//fprintf(stderr, "SPI - security parameter index(default=0)\n");

 	fprintf(stderr, "MODE := [ transport | tunnel ](default=transport)\n");
 	//fprintf(stderr, "REQID - number(default=0)\n");

	fprintf(stderr, "FLAG-LIST := [ FLAG-LIST ] FLAG\n");
	fprintf(stderr, "FLAG := [ noecn | decap-dscp ]\n");
 
        fprintf(stderr, "ENCAP := ENCAP-TYPE SPORT DPORT OADDR\n");
        fprintf(stderr, "ENCAP-TYPE := espinudp | espinudp-nonike\n");

	fprintf(stderr, "ALGO-LIST := [ ALGO-LIST ] | [ ALGO ]\n");
	fprintf(stderr, "ALGO := ALGO_TYPE ALGO_NAME ALGO_KEY\n");
	fprintf(stderr, "ALGO_TYPE := [ ");
	fprintf(stderr, "%s | ", strxf_algotype(XFRMA_ALG_CRYPT));
	fprintf(stderr, "%s | ", strxf_algotype(XFRMA_ALG_AUTH));
	fprintf(stderr, "%s ", strxf_algotype(XFRMA_ALG_COMP));
	fprintf(stderr, "]\n");

	//fprintf(stderr, "ALGO_NAME - algorithm name\n");
	//fprintf(stderr, "ALGO_KEY - algorithm key\n");

	fprintf(stderr, "SELECTOR := src ADDR[/PLEN] dst ADDR[/PLEN] [ UPSPEC ] [ dev DEV ]\n");

	fprintf(stderr, "UPSPEC := proto PROTO [ [ sport PORT ] [ dport PORT ] |\n");
	fprintf(stderr, "                        [ type NUMBER ] [ code NUMBER ] ]\n");


	//fprintf(stderr, "DEV - device name(default=none)\n");
	fprintf(stderr, "LIMIT-LIST := [ LIMIT-LIST ] | [ limit LIMIT ]\n");
	fprintf(stderr, "LIMIT := [ [time-soft|time-hard|time-use-soft|time-use-hard] SECONDS ] |\n");
	fprintf(stderr, "         [ [byte-soft|byte-hard] SIZE ] | [ [packet-soft|packet-hard] COUNT ]\n");
	exit(-1);
}

static int xfrm_algo_parse(struct xfrm_algo *alg, enum xfrm_attr_type_t type,
			   char *name, char *key, int max)
{
	int len;
	int slen = strlen(key);

#if 0
	/* XXX: verifying both name and key is required! */
	fprintf(stderr, "warning: ALGONAME/ALGOKEY will send to kernel promiscuously!(verifying them isn't implemented yet)\n");
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
			invarg("\"ALGOKEY\" makes buffer overflow\n", key);

		for (i = - (plen % 2), j = 0; j < len; i += 2, j++) {
			char vbuf[3];
			__u8 val;

			vbuf[0] = i >= 0 ? p[i] : '0';
			vbuf[1] = p[i + 1];
			vbuf[2] = '\0';

			if (get_u8(&val, vbuf, 16))
				invarg("\"ALGOKEY\" is invalid", key);

			alg->alg_key[j] = val;
		}
	} else {
		len = slen;
		if (len > 0) {
			if (len > max)
				invarg("\"ALGOKEY\" makes buffer overflow\n", key);

			strncpy(alg->alg_key, key, len);
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
			else {
				PREV_ARG(); /* back track */
				break;
			}

			if (!NEXT_ARG_OK())
				break;
			NEXT_ARG();
		}
	}

	filter.state_flags_mask = XFRM_FILTER_MASK_FULL;

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
	char *idp = NULL;
	char *ealgop = NULL;
	char *aalgop = NULL;
	char *calgop = NULL;

	memset(&req, 0, sizeof(req));

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
		} else {
			/* try to assume ALGO */
			int type = xfrm_algotype_getbyname(*argv);
			switch (type) {
			case XFRMA_ALG_CRYPT:
			case XFRMA_ALG_AUTH:
			case XFRMA_ALG_COMP:
			{
				/* ALGO */
				struct {
					struct xfrm_algo alg;
					char buf[XFRM_ALGO_KEY_BUF_SIZE];
				} alg;
				int len;
				char *name;
				char *key;

				switch (type) {
				case XFRMA_ALG_CRYPT:
					if (ealgop)
						duparg("ALGOTYPE", *argv);
					ealgop = *argv;
					break;
				case XFRMA_ALG_AUTH:
					if (aalgop)
						duparg("ALGOTYPE", *argv);
					aalgop = *argv;
					break;
				case XFRMA_ALG_COMP:
					if (calgop)
						duparg("ALGOTYPE", *argv);
					calgop = *argv;
					break;
				default:
					/* not reached */
					invarg("\"ALGOTYPE\" is invalid\n", *argv);
				}

				if (!NEXT_ARG_OK())
					missarg("ALGONAME");
				NEXT_ARG();
				name = *argv;

				if (!NEXT_ARG_OK())
					missarg("ALGOKEY");
				NEXT_ARG();
				key = *argv;

				memset(&alg, 0, sizeof(alg));

				xfrm_algo_parse((void *)&alg, type, name, key,
						sizeof(alg.buf));
				len = sizeof(struct xfrm_algo) + alg.alg.alg_key_len;

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

	if (!idp) {
		fprintf(stderr, "Not enough information: \"ID\" is required\n");
		exit(1);
	}

	if (ealgop || aalgop || calgop) {
		if (req.xsinfo.id.proto != IPPROTO_ESP &&
		    req.xsinfo.id.proto != IPPROTO_AH &&
		    req.xsinfo.id.proto != IPPROTO_COMP) {
			fprintf(stderr, "\"ALGO\" is invalid with proto=%s\n", strxf_xfrmproto(req.xsinfo.id.proto));
			exit(1);
		}
	} else {
		if (req.xsinfo.id.proto == IPPROTO_ESP ||
		    req.xsinfo.id.proto == IPPROTO_AH ||
		    req.xsinfo.id.proto == IPPROTO_COMP) {
			fprintf(stderr, "\"ALGO\" is required with proto=%s\n", strxf_xfrmproto(req.xsinfo.id.proto));
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
			fprintf(stderr, "\"min\" valie is larger than \"max\" one\n");
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
		len -= NLMSG_LENGTH(sizeof(*xsid));
	} else if (n->nlmsg_type == XFRM_MSG_EXPIRE) {
		xexp = NLMSG_DATA(n);
		xsinfo = &xexp->state;
		len -= NLMSG_LENGTH(sizeof(*xexp));
	} else {
		xexp = NULL;
		xsinfo = NLMSG_DATA(n);
		len -= NLMSG_LENGTH(sizeof(*xsinfo));
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
		xsinfo = (struct xfrm_usersa_info *)RTA_DATA(tb[XFRMA_SA]);
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
	} req;
	struct xfrm_id id;
	char *idp = NULL;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(req.xsid));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = delete ? XFRM_MSG_DELSA : XFRM_MSG_GETSA;
	req.xsid.family = preferred_family;

	while (argc > 0) {
		/*
		 * XXX: Source address is not used and ignore it to follow
		 * XXX: a manner of setkey e.g. in the case of deleting/getting
		 * XXX: message of IPsec SA.
		 */
		xfrm_address_t ignore_saddr;

		if (idp)
			invarg("unknown", *argv);
		idp = *argv;

		/* ID */
		memset(&id, 0, sizeof(id));
		xfrm_id_parse(&ignore_saddr, &id, &req.xsid.family, 0,
			      &argc, &argv);

		memcpy(&req.xsid.daddr, &id.daddr, sizeof(req.xsid.daddr));
		req.xsid.spi = id.spi;
		req.xsid.proto = id.proto;

		argc--; argv++;
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

			if (rtnl_send(&rth, xb.buf, xb.offset) < 0) {
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
	req.xsf.proto = IPSEC_PROTO_ANY;

	while (argc > 0) {
		if (strcmp(*argv, "proto") == 0) {
			int ret;

			if (protop)
				duparg("proto", *argv);
			protop = *argv;

			NEXT_ARG();

			ret = xfrm_xfrmproto_getbyname(*argv);
			if (ret < 0)
				invarg("\"XFRM_PROTO\" is invalid", *argv);

			req.xsf.proto = (__u8)ret;
		} else
			invarg("unknown", *argv);

		argc--; argv++;
	}

	if (rtnl_open_byproto(&rth, 0, NETLINK_XFRM) < 0)
		exit(1);

	if (show_stats > 1)
		fprintf(stderr, "Flush state proto=%s\n",
			(req.xsf.proto == IPSEC_PROTO_ANY) ? "any" :
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
	if (matches(*argv, "help") == 0)
		usage();
	fprintf(stderr, "Command \"%s\" is unknown, try \"ip xfrm state help\".\n", *argv);
	exit(-1);
}
