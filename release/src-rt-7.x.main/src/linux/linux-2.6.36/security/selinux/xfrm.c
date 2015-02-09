/*
 *  NSA Security-Enhanced Linux (SELinux) security module
 *
 *  This file contains the SELinux XFRM hook function implementations.
 *
 *  Authors:  Serge Hallyn <sergeh@us.ibm.com>
 *	      Trent Jaeger <jaegert@us.ibm.com>
 *
 *  Updated: Venkat Yekkirala <vyekkirala@TrustedCS.com>
 *
 *           Granular IPSec Associations for use in MLS environments.
 *
 *  Copyright (C) 2005 International Business Machines Corporation
 *  Copyright (C) 2006 Trusted Computer Solutions, Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2,
 *	as published by the Free Software Foundation.
 */

/*
 * USAGE:
 * NOTES:
 *   1. Make sure to enable the following options in your kernel config:
 *	CONFIG_SECURITY=y
 *	CONFIG_SECURITY_NETWORK=y
 *	CONFIG_SECURITY_NETWORK_XFRM=y
 *	CONFIG_SECURITY_SELINUX=m/y
 * ISSUES:
 *   1. Caching packets, so they are not dropped during negotiation
 *   2. Emulating a reasonable SO_PEERSEC across machines
 *   3. Testing addition of sk_policy's with security context via setsockopt
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/security.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/slab.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/xfrm.h>
#include <net/xfrm.h>
#include <net/checksum.h>
#include <net/udp.h>
#include <asm/atomic.h>

#include "avc.h"
#include "objsec.h"
#include "xfrm.h"

/* Labeled XFRM instance counter */
atomic_t selinux_xfrm_refcount = ATOMIC_INIT(0);

/*
 * Returns true if an LSM/SELinux context
 */
static inline int selinux_authorizable_ctx(struct xfrm_sec_ctx *ctx)
{
	return (ctx &&
		(ctx->ctx_doi == XFRM_SC_DOI_LSM) &&
		(ctx->ctx_alg == XFRM_SC_ALG_SELINUX));
}

/*
 * Returns true if the xfrm contains a security blob for SELinux
 */
static inline int selinux_authorizable_xfrm(struct xfrm_state *x)
{
	return selinux_authorizable_ctx(x->security);
}

/*
 * LSM hook implementation that authorizes that a flow can use
 * a xfrm policy rule.
 */
int selinux_xfrm_policy_lookup(struct xfrm_sec_ctx *ctx, u32 fl_secid, u8 dir)
{
	int rc;
	u32 sel_sid;

	/* Context sid is either set to label or ANY_ASSOC */
	if (ctx) {
		if (!selinux_authorizable_ctx(ctx))
			return -EINVAL;

		sel_sid = ctx->ctx_sid;
	} else
		/*
		 * All flows should be treated as polmatch'ing an
		 * otherwise applicable "non-labeled" policy. This
		 * would prevent inadvertent "leaks".
		 */
		return 0;

	rc = avc_has_perm(fl_secid, sel_sid, SECCLASS_ASSOCIATION,
			  ASSOCIATION__POLMATCH,
			  NULL);

	if (rc == -EACCES)
		return -ESRCH;

	return rc;
}

/*
 * LSM hook implementation that authorizes that a state matches
 * the given policy, flow combo.
 */

int selinux_xfrm_state_pol_flow_match(struct xfrm_state *x, struct xfrm_policy *xp,
			struct flowi *fl)
{
	u32 state_sid;
	int rc;

	if (!xp->security)
		if (x->security)
			/* unlabeled policy and labeled SA can't match */
			return 0;
		else
			/* unlabeled policy and unlabeled SA match all flows */
			return 1;
	else
		if (!x->security)
			/* unlabeled SA and labeled policy can't match */
			return 0;
		else
			if (!selinux_authorizable_xfrm(x))
				/* Not a SELinux-labeled SA */
				return 0;

	state_sid = x->security->ctx_sid;

	if (fl->secid != state_sid)
		return 0;

	rc = avc_has_perm(fl->secid, state_sid, SECCLASS_ASSOCIATION,
			  ASSOCIATION__SENDTO,
			  NULL)? 0:1;

	/*
	 * We don't need a separate SA Vs. policy polmatch check
	 * since the SA is now of the same label as the flow and
	 * a flow Vs. policy polmatch check had already happened
	 * in selinux_xfrm_policy_lookup() above.
	 */

	return rc;
}

/*
 * LSM hook implementation that checks and/or returns the xfrm sid for the
 * incoming packet.
 */

int selinux_xfrm_decode_session(struct sk_buff *skb, u32 *sid, int ckall)
{
	struct sec_path *sp;

	*sid = SECSID_NULL;

	if (skb == NULL)
		return 0;

	sp = skb->sp;
	if (sp) {
		int i, sid_set = 0;

		for (i = sp->len-1; i >= 0; i--) {
			struct xfrm_state *x = sp->xvec[i];
			if (selinux_authorizable_xfrm(x)) {
				struct xfrm_sec_ctx *ctx = x->security;

				if (!sid_set) {
					*sid = ctx->ctx_sid;
					sid_set = 1;

					if (!ckall)
						break;
				} else if (*sid != ctx->ctx_sid)
					return -EINVAL;
			}
		}
	}

	return 0;
}

/*
 * Security blob allocation for xfrm_policy and xfrm_state
 * CTX does not have a meaningful value on input
 */
static int selinux_xfrm_sec_ctx_alloc(struct xfrm_sec_ctx **ctxp,
	struct xfrm_user_sec_ctx *uctx, u32 sid)
{
	int rc = 0;
	const struct task_security_struct *tsec = current_security();
	struct xfrm_sec_ctx *ctx = NULL;
	char *ctx_str = NULL;
	u32 str_len;

	BUG_ON(uctx && sid);

	if (!uctx)
		goto not_from_user;

	if (uctx->ctx_doi != XFRM_SC_ALG_SELINUX)
		return -EINVAL;

	str_len = uctx->ctx_len;
	if (str_len >= PAGE_SIZE)
		return -ENOMEM;

	*ctxp = ctx = kmalloc(sizeof(*ctx) +
			      str_len + 1,
			      GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;

	ctx->ctx_doi = uctx->ctx_doi;
	ctx->ctx_len = str_len;
	ctx->ctx_alg = uctx->ctx_alg;

	memcpy(ctx->ctx_str,
	       uctx+1,
	       str_len);
	ctx->ctx_str[str_len] = 0;
	rc = security_context_to_sid(ctx->ctx_str,
				     str_len,
				     &ctx->ctx_sid);

	if (rc)
		goto out;

	/*
	 * Does the subject have permission to set security context?
	 */
	rc = avc_has_perm(tsec->sid, ctx->ctx_sid,
			  SECCLASS_ASSOCIATION,
			  ASSOCIATION__SETCONTEXT, NULL);
	if (rc)
		goto out;

	return rc;

not_from_user:
	rc = security_sid_to_context(sid, &ctx_str, &str_len);
	if (rc)
		goto out;

	*ctxp = ctx = kmalloc(sizeof(*ctx) +
			      str_len,
			      GFP_ATOMIC);

	if (!ctx) {
		rc = -ENOMEM;
		goto out;
	}

	ctx->ctx_doi = XFRM_SC_DOI_LSM;
	ctx->ctx_alg = XFRM_SC_ALG_SELINUX;
	ctx->ctx_sid = sid;
	ctx->ctx_len = str_len;
	memcpy(ctx->ctx_str,
	       ctx_str,
	       str_len);

	goto out2;

out:
	*ctxp = NULL;
	kfree(ctx);
out2:
	kfree(ctx_str);
	return rc;
}

/*
 * LSM hook implementation that allocs and transfers uctx spec to
 * xfrm_policy.
 */
int selinux_xfrm_policy_alloc(struct xfrm_sec_ctx **ctxp,
			      struct xfrm_user_sec_ctx *uctx)
{
	int err;

	BUG_ON(!uctx);

	err = selinux_xfrm_sec_ctx_alloc(ctxp, uctx, 0);
	if (err == 0)
		atomic_inc(&selinux_xfrm_refcount);

	return err;
}


/*
 * LSM hook implementation that copies security data structure from old to
 * new for policy cloning.
 */
int selinux_xfrm_policy_clone(struct xfrm_sec_ctx *old_ctx,
			      struct xfrm_sec_ctx **new_ctxp)
{
	struct xfrm_sec_ctx *new_ctx;

	if (old_ctx) {
		new_ctx = kmalloc(sizeof(*old_ctx) + old_ctx->ctx_len,
				  GFP_KERNEL);
		if (!new_ctx)
			return -ENOMEM;

		memcpy(new_ctx, old_ctx, sizeof(*new_ctx));
		memcpy(new_ctx->ctx_str, old_ctx->ctx_str, new_ctx->ctx_len);
		*new_ctxp = new_ctx;
	}
	return 0;
}

/*
 * LSM hook implementation that frees xfrm_sec_ctx security information.
 */
void selinux_xfrm_policy_free(struct xfrm_sec_ctx *ctx)
{
	kfree(ctx);
}

/*
 * LSM hook implementation that authorizes deletion of labeled policies.
 */
int selinux_xfrm_policy_delete(struct xfrm_sec_ctx *ctx)
{
	const struct task_security_struct *tsec = current_security();
	int rc = 0;

	if (ctx) {
		rc = avc_has_perm(tsec->sid, ctx->ctx_sid,
				  SECCLASS_ASSOCIATION,
				  ASSOCIATION__SETCONTEXT, NULL);
		if (rc == 0)
			atomic_dec(&selinux_xfrm_refcount);
	}

	return rc;
}

/*
 * LSM hook implementation that allocs and transfers sec_ctx spec to
 * xfrm_state.
 */
int selinux_xfrm_state_alloc(struct xfrm_state *x, struct xfrm_user_sec_ctx *uctx,
		u32 secid)
{
	int err;

	BUG_ON(!x);

	err = selinux_xfrm_sec_ctx_alloc(&x->security, uctx, secid);
	if (err == 0)
		atomic_inc(&selinux_xfrm_refcount);
	return err;
}

/*
 * LSM hook implementation that frees xfrm_state security information.
 */
void selinux_xfrm_state_free(struct xfrm_state *x)
{
	struct xfrm_sec_ctx *ctx = x->security;
	kfree(ctx);
}

 /*
  * LSM hook implementation that authorizes deletion of labeled SAs.
  */
int selinux_xfrm_state_delete(struct xfrm_state *x)
{
	const struct task_security_struct *tsec = current_security();
	struct xfrm_sec_ctx *ctx = x->security;
	int rc = 0;

	if (ctx) {
		rc = avc_has_perm(tsec->sid, ctx->ctx_sid,
				  SECCLASS_ASSOCIATION,
				  ASSOCIATION__SETCONTEXT, NULL);
		if (rc == 0)
			atomic_dec(&selinux_xfrm_refcount);
	}

	return rc;
}

/*
 * LSM hook that controls access to unlabelled packets.  If
 * a xfrm_state is authorizable (defined by macro) then it was
 * already authorized by the IPSec process.  If not, then
 * we need to check for unlabelled access since this may not have
 * gone thru the IPSec process.
 */
int selinux_xfrm_sock_rcv_skb(u32 isec_sid, struct sk_buff *skb,
				struct common_audit_data *ad)
{
	int i, rc = 0;
	struct sec_path *sp;
	u32 sel_sid = SECINITSID_UNLABELED;

	sp = skb->sp;

	if (sp) {
		for (i = 0; i < sp->len; i++) {
			struct xfrm_state *x = sp->xvec[i];

			if (x && selinux_authorizable_xfrm(x)) {
				struct xfrm_sec_ctx *ctx = x->security;
				sel_sid = ctx->ctx_sid;
				break;
			}
		}
	}

	/*
	 * This check even when there's no association involved is
	 * intended, according to Trent Jaeger, to make sure a
	 * process can't engage in non-ipsec communication unless
	 * explicitly allowed by policy.
	 */

	rc = avc_has_perm(isec_sid, sel_sid, SECCLASS_ASSOCIATION,
			  ASSOCIATION__RECVFROM, ad);

	return rc;
}

/*
 * POSTROUTE_LAST hook's XFRM processing:
 * If we have no security association, then we need to determine
 * whether the socket is allowed to send to an unlabelled destination.
 * If we do have a authorizable security association, then it has already been
 * checked in the selinux_xfrm_state_pol_flow_match hook above.
 */
int selinux_xfrm_postroute_last(u32 isec_sid, struct sk_buff *skb,
					struct common_audit_data *ad, u8 proto)
{
	struct dst_entry *dst;
	int rc = 0;

	dst = skb_dst(skb);

	if (dst) {
		struct dst_entry *dst_test;

		for (dst_test = dst; dst_test != NULL;
		     dst_test = dst_test->child) {
			struct xfrm_state *x = dst_test->xfrm;

			if (x && selinux_authorizable_xfrm(x))
				goto out;
		}
	}

	switch (proto) {
	case IPPROTO_AH:
	case IPPROTO_ESP:
	case IPPROTO_COMP:
		/*
		 * We should have already seen this packet once before
		 * it underwent xfrm(s). No need to subject it to the
		 * unlabeled check.
		 */
		goto out;
	default:
		break;
	}

	/*
	 * This check even when there's no association involved is
	 * intended, according to Trent Jaeger, to make sure a
	 * process can't engage in non-ipsec communication unless
	 * explicitly allowed by policy.
	 */

	rc = avc_has_perm(isec_sid, SECINITSID_UNLABELED, SECCLASS_ASSOCIATION,
			  ASSOCIATION__SENDTO, ad);
out:
	return rc;
}
