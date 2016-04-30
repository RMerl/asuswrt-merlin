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
 * along with this program; if not, see <http://www.gnu.org/licenses>.
 */
/*
 * Authors:
 *	Masahide NAKAMURA @USAGI
 */

#ifndef __XFRM_H__
#define __XFRM_H__ 1

#include <stdio.h>
#include <sys/socket.h>
#include <linux/xfrm.h>

#ifndef IPPROTO_SCTP
# define IPPROTO_SCTP	132
#endif
#ifndef IPPROTO_DCCP
# define IPPROTO_DCCP	33
#endif
#ifndef IPPROTO_MH
# define IPPROTO_MH	135
#endif

#define XFRMS_RTA(x)  ((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(struct xfrm_usersa_info))))
#define XFRMS_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct xfrm_usersa_info))

#define XFRMP_RTA(x)  ((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(struct xfrm_userpolicy_info))))
#define XFRMP_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct xfrm_userpoilcy_info))

#define XFRMSID_RTA(x)  ((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(struct xfrm_usersa_id))))
#define XFRMSID_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct xfrm_usersa_id))

#define XFRMPID_RTA(x)  ((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(struct xfrm_userpolicy_id))))
#define XFRMPID_PAYLOAD(n) NLMSG_PAYLOAD(n,sizeof(struct xfrm_userpoilcy_id))

#define XFRMACQ_RTA(x)	((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(struct xfrm_user_acquire))))
#define XFRMEXP_RTA(x)	((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(struct xfrm_user_expire))))
#define XFRMPEXP_RTA(x)	((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(struct xfrm_user_polexpire))))

#define XFRMREP_RTA(x)	((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(struct xfrm_user_report))))

#define XFRMSAPD_RTA(x)	((struct rtattr*)(((char*)(x)) + NLMSG_ALIGN(sizeof(__u32))))
#define XFRM_FLAG_PRINT(fp, flags, f, s) \
	do { \
		if (flags & f) { \
			flags &= ~f; \
			fprintf(fp, s "%s", (flags ? " " : "")); \
		} \
	} while(0)

struct xfrm_buffer {
	char *buf;
	int size;
	int offset;

	int nlmsg_count;
	struct rtnl_handle *rth;
};

struct xfrm_filter {
	int use;

	struct xfrm_usersa_info xsinfo;
	__u8 id_src_mask;
	__u8 id_dst_mask;
	__u8 id_proto_mask;
	__u32 id_spi_mask;
	__u8 mode_mask;
	__u32 reqid_mask;
	__u8 state_flags_mask;

	struct xfrm_userpolicy_info xpinfo;
	__u8 dir_mask;
	__u8 sel_src_mask;
	__u8 sel_dst_mask;
	__u32 sel_dev_mask;
	__u8 upspec_proto_mask;
	__u16 upspec_sport_mask;
	__u16 upspec_dport_mask;
	__u32 index_mask;
	__u8 action_mask;
	__u32 priority_mask;
	__u8 policy_flags_mask;

	__u8 ptype;
	__u8 ptype_mask;

};
#define XFRM_FILTER_MASK_FULL (~0)

extern struct xfrm_filter filter;

int xfrm_state_print(const struct sockaddr_nl *who, struct nlmsghdr *n,
		     void *arg);
int xfrm_policy_print(const struct sockaddr_nl *who, struct nlmsghdr *n,
		      void *arg);
int do_xfrm_state(int argc, char **argv);
int do_xfrm_policy(int argc, char **argv);
int do_xfrm_monitor(int argc, char **argv);

int xfrm_addr_match(xfrm_address_t *x1, xfrm_address_t *x2, int bits);
int xfrm_xfrmproto_is_ipsec(__u8 proto);
int xfrm_xfrmproto_is_ro(__u8 proto);
int xfrm_xfrmproto_getbyname(char *name);
int xfrm_algotype_getbyname(char *name);
int xfrm_parse_mark(struct xfrm_mark *mark, int *argcp, char ***argvp);
const char *strxf_xfrmproto(__u8 proto);
const char *strxf_algotype(int type);
const char *strxf_mask8(__u8 mask);
const char *strxf_mask32(__u32 mask);
const char *strxf_share(__u8 share);
const char *strxf_proto(__u8 proto);
const char *strxf_ptype(__u8 ptype);
void xfrm_id_info_print(xfrm_address_t *saddr, struct xfrm_id *id,
			__u8 mode, __u32 reqid, __u16 family, int force_spi,
			FILE *fp, const char *prefix, const char *title);
void xfrm_stats_print(struct xfrm_stats *s, FILE *fp, const char *prefix);
void xfrm_lifetime_print(struct xfrm_lifetime_cfg *cfg,
			 struct xfrm_lifetime_cur *cur,
			 FILE *fp, const char *prefix);
void xfrm_selector_print(struct xfrm_selector *sel, __u16 family,
			 FILE *fp, const char *prefix);
void xfrm_xfrma_print(struct rtattr *tb[], __u16 family,
		      FILE *fp, const char *prefix);
void xfrm_state_info_print(struct xfrm_usersa_info *xsinfo,
			    struct rtattr *tb[], FILE *fp, const char *prefix,
			   const char *title);
void xfrm_policy_info_print(struct xfrm_userpolicy_info *xpinfo,
			    struct rtattr *tb[], FILE *fp, const char *prefix,
			    const char *title);
int xfrm_id_parse(xfrm_address_t *saddr, struct xfrm_id *id, __u16 *family,
		  int loose, int *argcp, char ***argvp);
int xfrm_mode_parse(__u8 *mode, int *argcp, char ***argvp);
int xfrm_encap_type_parse(__u16 *type, int *argcp, char ***argvp);
int xfrm_reqid_parse(__u32 *reqid, int *argcp, char ***argvp);
int xfrm_selector_parse(struct xfrm_selector *sel, int *argcp, char ***argvp);
int xfrm_lifetime_cfg_parse(struct xfrm_lifetime_cfg *lft,
			    int *argcp, char ***argvp);
int xfrm_sctx_parse(char *ctxstr, char *context,
		    struct xfrm_user_sec_ctx *sctx);
#endif
