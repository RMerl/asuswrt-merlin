/*
 * Implementation of the security services.
 *
 * Authors : Stephen Smalley, <sds@epoch.ncsc.mil>
 *	     James Morris <jmorris@redhat.com>
 *
 * Updated: Trusted Computer Solutions, Inc. <dgoeddel@trustedcs.com>
 *
 *	Support for enhanced MLS infrastructure.
 *	Support for context based audit filters.
 *
 * Updated: Frank Mayer <mayerf@tresys.com> and Karl MacMillan <kmacmillan@tresys.com>
 *
 *	Added conditional policy language extensions
 *
 * Updated: Hewlett-Packard <paul.moore@hp.com>
 *
 *      Added support for NetLabel
 *      Added support for the policy capability bitmap
 *
 * Updated: Chad Sellers <csellers@tresys.com>
 *
 *  Added validation of kernel classes and permissions
 *
 * Updated: KaiGai Kohei <kaigai@ak.jp.nec.com>
 *
 *  Added support for bounds domain and audit messaged on masked permissions
 *
 * Updated: Guido Trentalancia <guido@trentalancia.com>
 *
 *  Added support for runtime switching of the policy type
 *
 * Copyright (C) 2008, 2009 NEC Corporation
 * Copyright (C) 2006, 2007 Hewlett-Packard Development Company, L.P.
 * Copyright (C) 2004-2006 Trusted Computer Solutions, Inc.
 * Copyright (C) 2003 - 2004, 2006 Tresys Technology, LLC
 * Copyright (C) 2003 Red Hat, Inc., James Morris <jmorris@redhat.com>
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/sched.h>
#include <linux/audit.h>
#include <linux/mutex.h>
#include <linux/selinux.h>
#include <linux/flex_array.h>
#include <net/netlabel.h>

#include "flask.h"
#include "avc.h"
#include "avc_ss.h"
#include "security.h"
#include "context.h"
#include "policydb.h"
#include "sidtab.h"
#include "services.h"
#include "conditional.h"
#include "mls.h"
#include "objsec.h"
#include "netlabel.h"
#include "xfrm.h"
#include "ebitmap.h"
#include "audit.h"

extern void selnl_notify_policyload(u32 seqno);

int selinux_policycap_netpeer;
int selinux_policycap_openperm;

static DEFINE_RWLOCK(policy_rwlock);

static struct sidtab sidtab;
struct policydb policydb;
int ss_initialized;

/*
 * The largest sequence number that has been used when
 * providing an access decision to the access vector cache.
 * The sequence number only changes when a policy change
 * occurs.
 */
static u32 latest_granting;

/* Forward declaration. */
static int context_struct_to_string(struct context *context, char **scontext,
				    u32 *scontext_len);

static void context_struct_compute_av(struct context *scontext,
				      struct context *tcontext,
				      u16 tclass,
				      struct av_decision *avd);

struct selinux_mapping {
	u16 value; /* policy value */
	unsigned num_perms;
	u32 perms[sizeof(u32) * 8];
};

static struct selinux_mapping *current_mapping;
static u16 current_mapping_size;

static int selinux_set_mapping(struct policydb *pol,
			       struct security_class_mapping *map,
			       struct selinux_mapping **out_map_p,
			       u16 *out_map_size)
{
	struct selinux_mapping *out_map = NULL;
	size_t size = sizeof(struct selinux_mapping);
	u16 i, j;
	unsigned k;
	bool print_unknown_handle = false;

	/* Find number of classes in the input mapping */
	if (!map)
		return -EINVAL;
	i = 0;
	while (map[i].name)
		i++;

	/* Allocate space for the class records, plus one for class zero */
	out_map = kcalloc(++i, size, GFP_ATOMIC);
	if (!out_map)
		return -ENOMEM;

	/* Store the raw class and permission values */
	j = 0;
	while (map[j].name) {
		struct security_class_mapping *p_in = map + (j++);
		struct selinux_mapping *p_out = out_map + j;

		/* An empty class string skips ahead */
		if (!strcmp(p_in->name, "")) {
			p_out->num_perms = 0;
			continue;
		}

		p_out->value = string_to_security_class(pol, p_in->name);
		if (!p_out->value) {
			printk(KERN_INFO
			       "SELinux:  Class %s not defined in policy.\n",
			       p_in->name);
			if (pol->reject_unknown)
				goto err;
			p_out->num_perms = 0;
			print_unknown_handle = true;
			continue;
		}

		k = 0;
		while (p_in->perms && p_in->perms[k]) {
			/* An empty permission string skips ahead */
			if (!*p_in->perms[k]) {
				k++;
				continue;
			}
			p_out->perms[k] = string_to_av_perm(pol, p_out->value,
							    p_in->perms[k]);
			if (!p_out->perms[k]) {
				printk(KERN_INFO
				       "SELinux:  Permission %s in class %s not defined in policy.\n",
				       p_in->perms[k], p_in->name);
				if (pol->reject_unknown)
					goto err;
				print_unknown_handle = true;
			}

			k++;
		}
		p_out->num_perms = k;
	}

	if (print_unknown_handle)
		printk(KERN_INFO "SELinux: the above unknown classes and permissions will be %s\n",
		       pol->allow_unknown ? "allowed" : "denied");

	*out_map_p = out_map;
	*out_map_size = i;
	return 0;
err:
	kfree(out_map);
	return -EINVAL;
}

/*
 * Get real, policy values from mapped values
 */

static u16 unmap_class(u16 tclass)
{
	if (tclass < current_mapping_size)
		return current_mapping[tclass].value;

	return tclass;
}

static void map_decision(u16 tclass, struct av_decision *avd,
			 int allow_unknown)
{
	if (tclass < current_mapping_size) {
		unsigned i, n = current_mapping[tclass].num_perms;
		u32 result;

		for (i = 0, result = 0; i < n; i++) {
			if (avd->allowed & current_mapping[tclass].perms[i])
				result |= 1<<i;
			if (allow_unknown && !current_mapping[tclass].perms[i])
				result |= 1<<i;
		}
		avd->allowed = result;

		for (i = 0, result = 0; i < n; i++)
			if (avd->auditallow & current_mapping[tclass].perms[i])
				result |= 1<<i;
		avd->auditallow = result;

		for (i = 0, result = 0; i < n; i++) {
			if (avd->auditdeny & current_mapping[tclass].perms[i])
				result |= 1<<i;
			if (!allow_unknown && !current_mapping[tclass].perms[i])
				result |= 1<<i;
		}
		/*
		 * In case the kernel has a bug and requests a permission
		 * between num_perms and the maximum permission number, we
		 * should audit that denial
		 */
		for (; i < (sizeof(u32)*8); i++)
			result |= 1<<i;
		avd->auditdeny = result;
	}
}

int security_mls_enabled(void)
{
	return policydb.mls_enabled;
}

/*
 * Return the boolean value of a constraint expression
 * when it is applied to the specified source and target
 * security contexts.
 *
 * xcontext is a special beast...  It is used by the validatetrans rules
 * only.  For these rules, scontext is the context before the transition,
 * tcontext is the context after the transition, and xcontext is the context
 * of the process performing the transition.  All other callers of
 * constraint_expr_eval should pass in NULL for xcontext.
 */
static int constraint_expr_eval(struct context *scontext,
				struct context *tcontext,
				struct context *xcontext,
				struct constraint_expr *cexpr)
{
	u32 val1, val2;
	struct context *c;
	struct role_datum *r1, *r2;
	struct mls_level *l1, *l2;
	struct constraint_expr *e;
	int s[CEXPR_MAXDEPTH];
	int sp = -1;

	for (e = cexpr; e; e = e->next) {
		switch (e->expr_type) {
		case CEXPR_NOT:
			BUG_ON(sp < 0);
			s[sp] = !s[sp];
			break;
		case CEXPR_AND:
			BUG_ON(sp < 1);
			sp--;
			s[sp] &= s[sp + 1];
			break;
		case CEXPR_OR:
			BUG_ON(sp < 1);
			sp--;
			s[sp] |= s[sp + 1];
			break;
		case CEXPR_ATTR:
			if (sp == (CEXPR_MAXDEPTH - 1))
				return 0;
			switch (e->attr) {
			case CEXPR_USER:
				val1 = scontext->user;
				val2 = tcontext->user;
				break;
			case CEXPR_TYPE:
				val1 = scontext->type;
				val2 = tcontext->type;
				break;
			case CEXPR_ROLE:
				val1 = scontext->role;
				val2 = tcontext->role;
				r1 = policydb.role_val_to_struct[val1 - 1];
				r2 = policydb.role_val_to_struct[val2 - 1];
				switch (e->op) {
				case CEXPR_DOM:
					s[++sp] = ebitmap_get_bit(&r1->dominates,
								  val2 - 1);
					continue;
				case CEXPR_DOMBY:
					s[++sp] = ebitmap_get_bit(&r2->dominates,
								  val1 - 1);
					continue;
				case CEXPR_INCOMP:
					s[++sp] = (!ebitmap_get_bit(&r1->dominates,
								    val2 - 1) &&
						   !ebitmap_get_bit(&r2->dominates,
								    val1 - 1));
					continue;
				default:
					break;
				}
				break;
			case CEXPR_L1L2:
				l1 = &(scontext->range.level[0]);
				l2 = &(tcontext->range.level[0]);
				goto mls_ops;
			case CEXPR_L1H2:
				l1 = &(scontext->range.level[0]);
				l2 = &(tcontext->range.level[1]);
				goto mls_ops;
			case CEXPR_H1L2:
				l1 = &(scontext->range.level[1]);
				l2 = &(tcontext->range.level[0]);
				goto mls_ops;
			case CEXPR_H1H2:
				l1 = &(scontext->range.level[1]);
				l2 = &(tcontext->range.level[1]);
				goto mls_ops;
			case CEXPR_L1H1:
				l1 = &(scontext->range.level[0]);
				l2 = &(scontext->range.level[1]);
				goto mls_ops;
			case CEXPR_L2H2:
				l1 = &(tcontext->range.level[0]);
				l2 = &(tcontext->range.level[1]);
				goto mls_ops;
mls_ops:
			switch (e->op) {
			case CEXPR_EQ:
				s[++sp] = mls_level_eq(l1, l2);
				continue;
			case CEXPR_NEQ:
				s[++sp] = !mls_level_eq(l1, l2);
				continue;
			case CEXPR_DOM:
				s[++sp] = mls_level_dom(l1, l2);
				continue;
			case CEXPR_DOMBY:
				s[++sp] = mls_level_dom(l2, l1);
				continue;
			case CEXPR_INCOMP:
				s[++sp] = mls_level_incomp(l2, l1);
				continue;
			default:
				BUG();
				return 0;
			}
			break;
			default:
				BUG();
				return 0;
			}

			switch (e->op) {
			case CEXPR_EQ:
				s[++sp] = (val1 == val2);
				break;
			case CEXPR_NEQ:
				s[++sp] = (val1 != val2);
				break;
			default:
				BUG();
				return 0;
			}
			break;
		case CEXPR_NAMES:
			if (sp == (CEXPR_MAXDEPTH-1))
				return 0;
			c = scontext;
			if (e->attr & CEXPR_TARGET)
				c = tcontext;
			else if (e->attr & CEXPR_XTARGET) {
				c = xcontext;
				if (!c) {
					BUG();
					return 0;
				}
			}
			if (e->attr & CEXPR_USER)
				val1 = c->user;
			else if (e->attr & CEXPR_ROLE)
				val1 = c->role;
			else if (e->attr & CEXPR_TYPE)
				val1 = c->type;
			else {
				BUG();
				return 0;
			}

			switch (e->op) {
			case CEXPR_EQ:
				s[++sp] = ebitmap_get_bit(&e->names, val1 - 1);
				break;
			case CEXPR_NEQ:
				s[++sp] = !ebitmap_get_bit(&e->names, val1 - 1);
				break;
			default:
				BUG();
				return 0;
			}
			break;
		default:
			BUG();
			return 0;
		}
	}

	BUG_ON(sp != 0);
	return s[0];
}

/*
 * security_dump_masked_av - dumps masked permissions during
 * security_compute_av due to RBAC, MLS/Constraint and Type bounds.
 */
static int dump_masked_av_helper(void *k, void *d, void *args)
{
	struct perm_datum *pdatum = d;
	char **permission_names = args;

	BUG_ON(pdatum->value < 1 || pdatum->value > 32);

	permission_names[pdatum->value - 1] = (char *)k;

	return 0;
}

static void security_dump_masked_av(struct context *scontext,
				    struct context *tcontext,
				    u16 tclass,
				    u32 permissions,
				    const char *reason)
{
	struct common_datum *common_dat;
	struct class_datum *tclass_dat;
	struct audit_buffer *ab;
	char *tclass_name;
	char *scontext_name = NULL;
	char *tcontext_name = NULL;
	char *permission_names[32];
	int index;
	u32 length;
	bool need_comma = false;

	if (!permissions)
		return;

	tclass_name = policydb.p_class_val_to_name[tclass - 1];
	tclass_dat = policydb.class_val_to_struct[tclass - 1];
	common_dat = tclass_dat->comdatum;

	/* init permission_names */
	if (common_dat &&
	    hashtab_map(common_dat->permissions.table,
			dump_masked_av_helper, permission_names) < 0)
		goto out;

	if (hashtab_map(tclass_dat->permissions.table,
			dump_masked_av_helper, permission_names) < 0)
		goto out;

	/* get scontext/tcontext in text form */
	if (context_struct_to_string(scontext,
				     &scontext_name, &length) < 0)
		goto out;

	if (context_struct_to_string(tcontext,
				     &tcontext_name, &length) < 0)
		goto out;

	/* audit a message */
	ab = audit_log_start(current->audit_context,
			     GFP_ATOMIC, AUDIT_SELINUX_ERR);
	if (!ab)
		goto out;

	audit_log_format(ab, "op=security_compute_av reason=%s "
			 "scontext=%s tcontext=%s tclass=%s perms=",
			 reason, scontext_name, tcontext_name, tclass_name);

	for (index = 0; index < 32; index++) {
		u32 mask = (1 << index);

		if ((mask & permissions) == 0)
			continue;

		audit_log_format(ab, "%s%s",
				 need_comma ? "," : "",
				 permission_names[index]
				 ? permission_names[index] : "????");
		need_comma = true;
	}
	audit_log_end(ab);
out:
	/* release scontext/tcontext */
	kfree(tcontext_name);
	kfree(scontext_name);

	return;
}

/*
 * security_boundary_permission - drops violated permissions
 * on boundary constraint.
 */
static void type_attribute_bounds_av(struct context *scontext,
				     struct context *tcontext,
				     u16 tclass,
				     struct av_decision *avd)
{
	struct context lo_scontext;
	struct context lo_tcontext;
	struct av_decision lo_avd;
	struct type_datum *source
		= policydb.type_val_to_struct[scontext->type - 1];
	struct type_datum *target
		= policydb.type_val_to_struct[tcontext->type - 1];
	u32 masked = 0;

	if (source->bounds) {
		memset(&lo_avd, 0, sizeof(lo_avd));

		memcpy(&lo_scontext, scontext, sizeof(lo_scontext));
		lo_scontext.type = source->bounds;

		context_struct_compute_av(&lo_scontext,
					  tcontext,
					  tclass,
					  &lo_avd);
		if ((lo_avd.allowed & avd->allowed) == avd->allowed)
			return;		/* no masked permission */
		masked = ~lo_avd.allowed & avd->allowed;
	}

	if (target->bounds) {
		memset(&lo_avd, 0, sizeof(lo_avd));

		memcpy(&lo_tcontext, tcontext, sizeof(lo_tcontext));
		lo_tcontext.type = target->bounds;

		context_struct_compute_av(scontext,
					  &lo_tcontext,
					  tclass,
					  &lo_avd);
		if ((lo_avd.allowed & avd->allowed) == avd->allowed)
			return;		/* no masked permission */
		masked = ~lo_avd.allowed & avd->allowed;
	}

	if (source->bounds && target->bounds) {
		memset(&lo_avd, 0, sizeof(lo_avd));
		/*
		 * lo_scontext and lo_tcontext are already
		 * set up.
		 */

		context_struct_compute_av(&lo_scontext,
					  &lo_tcontext,
					  tclass,
					  &lo_avd);
		if ((lo_avd.allowed & avd->allowed) == avd->allowed)
			return;		/* no masked permission */
		masked = ~lo_avd.allowed & avd->allowed;
	}

	if (masked) {
		/* mask violated permissions */
		avd->allowed &= ~masked;

		/* audit masked permissions */
		security_dump_masked_av(scontext, tcontext,
					tclass, masked, "bounds");
	}
}

/*
 * Compute access vectors based on a context structure pair for
 * the permissions in a particular class.
 */
static void context_struct_compute_av(struct context *scontext,
				      struct context *tcontext,
				      u16 tclass,
				      struct av_decision *avd)
{
	struct constraint_node *constraint;
	struct role_allow *ra;
	struct avtab_key avkey;
	struct avtab_node *node;
	struct class_datum *tclass_datum;
	struct ebitmap *sattr, *tattr;
	struct ebitmap_node *snode, *tnode;
	unsigned int i, j;

	avd->allowed = 0;
	avd->auditallow = 0;
	avd->auditdeny = 0xffffffff;

	if (unlikely(!tclass || tclass > policydb.p_classes.nprim)) {
		if (printk_ratelimit())
			printk(KERN_WARNING "SELinux:  Invalid class %hu\n", tclass);
		return;
	}

	tclass_datum = policydb.class_val_to_struct[tclass - 1];

	/*
	 * If a specific type enforcement rule was defined for
	 * this permission check, then use it.
	 */
	avkey.target_class = tclass;
	avkey.specified = AVTAB_AV;
	sattr = flex_array_get(policydb.type_attr_map_array, scontext->type - 1);
	BUG_ON(!sattr);
	tattr = flex_array_get(policydb.type_attr_map_array, tcontext->type - 1);
	BUG_ON(!tattr);
	ebitmap_for_each_positive_bit(sattr, snode, i) {
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			avkey.source_type = i + 1;
			avkey.target_type = j + 1;
			for (node = avtab_search_node(&policydb.te_avtab, &avkey);
			     node;
			     node = avtab_search_node_next(node, avkey.specified)) {
				if (node->key.specified == AVTAB_ALLOWED)
					avd->allowed |= node->datum.data;
				else if (node->key.specified == AVTAB_AUDITALLOW)
					avd->auditallow |= node->datum.data;
				else if (node->key.specified == AVTAB_AUDITDENY)
					avd->auditdeny &= node->datum.data;
			}

			/* Check conditional av table for additional permissions */
			cond_compute_av(&policydb.te_cond_avtab, &avkey, avd);

		}
	}

	/*
	 * Remove any permissions prohibited by a constraint (this includes
	 * the MLS policy).
	 */
	constraint = tclass_datum->constraints;
	while (constraint) {
		if ((constraint->permissions & (avd->allowed)) &&
		    !constraint_expr_eval(scontext, tcontext, NULL,
					  constraint->expr)) {
			avd->allowed &= ~(constraint->permissions);
		}
		constraint = constraint->next;
	}

	/*
	 * If checking process transition permission and the
	 * role is changing, then check the (current_role, new_role)
	 * pair.
	 */
	if (tclass == policydb.process_class &&
	    (avd->allowed & policydb.process_trans_perms) &&
	    scontext->role != tcontext->role) {
		for (ra = policydb.role_allow; ra; ra = ra->next) {
			if (scontext->role == ra->role &&
			    tcontext->role == ra->new_role)
				break;
		}
		if (!ra)
			avd->allowed &= ~policydb.process_trans_perms;
	}

	/*
	 * If the given source and target types have boundary
	 * constraint, lazy checks have to mask any violated
	 * permission and notice it to userspace via audit.
	 */
	type_attribute_bounds_av(scontext, tcontext,
				 tclass, avd);
}

static int security_validtrans_handle_fail(struct context *ocontext,
					   struct context *ncontext,
					   struct context *tcontext,
					   u16 tclass)
{
	char *o = NULL, *n = NULL, *t = NULL;
	u32 olen, nlen, tlen;

	if (context_struct_to_string(ocontext, &o, &olen) < 0)
		goto out;
	if (context_struct_to_string(ncontext, &n, &nlen) < 0)
		goto out;
	if (context_struct_to_string(tcontext, &t, &tlen) < 0)
		goto out;
	audit_log(current->audit_context, GFP_ATOMIC, AUDIT_SELINUX_ERR,
		  "security_validate_transition:  denied for"
		  " oldcontext=%s newcontext=%s taskcontext=%s tclass=%s",
		  o, n, t, policydb.p_class_val_to_name[tclass-1]);
out:
	kfree(o);
	kfree(n);
	kfree(t);

	if (!selinux_enforcing)
		return 0;
	return -EPERM;
}

int security_validate_transition(u32 oldsid, u32 newsid, u32 tasksid,
				 u16 orig_tclass)
{
	struct context *ocontext;
	struct context *ncontext;
	struct context *tcontext;
	struct class_datum *tclass_datum;
	struct constraint_node *constraint;
	u16 tclass;
	int rc = 0;

	if (!ss_initialized)
		return 0;

	read_lock(&policy_rwlock);

	tclass = unmap_class(orig_tclass);

	if (!tclass || tclass > policydb.p_classes.nprim) {
		printk(KERN_ERR "SELinux: %s:  unrecognized class %d\n",
			__func__, tclass);
		rc = -EINVAL;
		goto out;
	}
	tclass_datum = policydb.class_val_to_struct[tclass - 1];

	ocontext = sidtab_search(&sidtab, oldsid);
	if (!ocontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
			__func__, oldsid);
		rc = -EINVAL;
		goto out;
	}

	ncontext = sidtab_search(&sidtab, newsid);
	if (!ncontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
			__func__, newsid);
		rc = -EINVAL;
		goto out;
	}

	tcontext = sidtab_search(&sidtab, tasksid);
	if (!tcontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
			__func__, tasksid);
		rc = -EINVAL;
		goto out;
	}

	constraint = tclass_datum->validatetrans;
	while (constraint) {
		if (!constraint_expr_eval(ocontext, ncontext, tcontext,
					  constraint->expr)) {
			rc = security_validtrans_handle_fail(ocontext, ncontext,
							     tcontext, tclass);
			goto out;
		}
		constraint = constraint->next;
	}

out:
	read_unlock(&policy_rwlock);
	return rc;
}

/*
 * security_bounded_transition - check whether the given
 * transition is directed to bounded, or not.
 * It returns 0, if @newsid is bounded by @oldsid.
 * Otherwise, it returns error code.
 *
 * @oldsid : current security identifier
 * @newsid : destinated security identifier
 */
int security_bounded_transition(u32 old_sid, u32 new_sid)
{
	struct context *old_context, *new_context;
	struct type_datum *type;
	int index;
	int rc = -EINVAL;

	read_lock(&policy_rwlock);

	old_context = sidtab_search(&sidtab, old_sid);
	if (!old_context) {
		printk(KERN_ERR "SELinux: %s: unrecognized SID %u\n",
		       __func__, old_sid);
		goto out;
	}

	new_context = sidtab_search(&sidtab, new_sid);
	if (!new_context) {
		printk(KERN_ERR "SELinux: %s: unrecognized SID %u\n",
		       __func__, new_sid);
		goto out;
	}

	/* type/domain unchanged */
	if (old_context->type == new_context->type) {
		rc = 0;
		goto out;
	}

	index = new_context->type;
	while (true) {
		type = policydb.type_val_to_struct[index - 1];
		BUG_ON(!type);

		/* not bounded anymore */
		if (!type->bounds) {
			rc = -EPERM;
			break;
		}

		/* @newsid is bounded by @oldsid */
		if (type->bounds == old_context->type) {
			rc = 0;
			break;
		}
		index = type->bounds;
	}

	if (rc) {
		char *old_name = NULL;
		char *new_name = NULL;
		u32 length;

		if (!context_struct_to_string(old_context,
					      &old_name, &length) &&
		    !context_struct_to_string(new_context,
					      &new_name, &length)) {
			audit_log(current->audit_context,
				  GFP_ATOMIC, AUDIT_SELINUX_ERR,
				  "op=security_bounded_transition "
				  "result=denied "
				  "oldcontext=%s newcontext=%s",
				  old_name, new_name);
		}
		kfree(new_name);
		kfree(old_name);
	}
out:
	read_unlock(&policy_rwlock);

	return rc;
}

static void avd_init(struct av_decision *avd)
{
	avd->allowed = 0;
	avd->auditallow = 0;
	avd->auditdeny = 0xffffffff;
	avd->seqno = latest_granting;
	avd->flags = 0;
}


/**
 * security_compute_av - Compute access vector decisions.
 * @ssid: source security identifier
 * @tsid: target security identifier
 * @tclass: target security class
 * @avd: access vector decisions
 *
 * Compute a set of access vector decisions based on the
 * SID pair (@ssid, @tsid) for the permissions in @tclass.
 */
void security_compute_av(u32 ssid,
			 u32 tsid,
			 u16 orig_tclass,
			 struct av_decision *avd)
{
	u16 tclass;
	struct context *scontext = NULL, *tcontext = NULL;

	read_lock(&policy_rwlock);
	avd_init(avd);
	if (!ss_initialized)
		goto allow;

	scontext = sidtab_search(&sidtab, ssid);
	if (!scontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
		       __func__, ssid);
		goto out;
	}

	/* permissive domain? */
	if (ebitmap_get_bit(&policydb.permissive_map, scontext->type))
		avd->flags |= AVD_FLAGS_PERMISSIVE;

	tcontext = sidtab_search(&sidtab, tsid);
	if (!tcontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
		       __func__, tsid);
		goto out;
	}

	tclass = unmap_class(orig_tclass);
	if (unlikely(orig_tclass && !tclass)) {
		if (policydb.allow_unknown)
			goto allow;
		goto out;
	}
	context_struct_compute_av(scontext, tcontext, tclass, avd);
	map_decision(orig_tclass, avd, policydb.allow_unknown);
out:
	read_unlock(&policy_rwlock);
	return;
allow:
	avd->allowed = 0xffffffff;
	goto out;
}

void security_compute_av_user(u32 ssid,
			      u32 tsid,
			      u16 tclass,
			      struct av_decision *avd)
{
	struct context *scontext = NULL, *tcontext = NULL;

	read_lock(&policy_rwlock);
	avd_init(avd);
	if (!ss_initialized)
		goto allow;

	scontext = sidtab_search(&sidtab, ssid);
	if (!scontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
		       __func__, ssid);
		goto out;
	}

	/* permissive domain? */
	if (ebitmap_get_bit(&policydb.permissive_map, scontext->type))
		avd->flags |= AVD_FLAGS_PERMISSIVE;

	tcontext = sidtab_search(&sidtab, tsid);
	if (!tcontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
		       __func__, tsid);
		goto out;
	}

	if (unlikely(!tclass)) {
		if (policydb.allow_unknown)
			goto allow;
		goto out;
	}

	context_struct_compute_av(scontext, tcontext, tclass, avd);
 out:
	read_unlock(&policy_rwlock);
	return;
allow:
	avd->allowed = 0xffffffff;
	goto out;
}

/*
 * Write the security context string representation of
 * the context structure `context' into a dynamically
 * allocated string of the correct size.  Set `*scontext'
 * to point to this string and set `*scontext_len' to
 * the length of the string.
 */
static int context_struct_to_string(struct context *context, char **scontext, u32 *scontext_len)
{
	char *scontextp;

	*scontext = NULL;
	*scontext_len = 0;

	if (context->len) {
		*scontext_len = context->len;
		*scontext = kstrdup(context->str, GFP_ATOMIC);
		if (!(*scontext))
			return -ENOMEM;
		return 0;
	}

	/* Compute the size of the context. */
	*scontext_len += strlen(policydb.p_user_val_to_name[context->user - 1]) + 1;
	*scontext_len += strlen(policydb.p_role_val_to_name[context->role - 1]) + 1;
	*scontext_len += strlen(policydb.p_type_val_to_name[context->type - 1]) + 1;
	*scontext_len += mls_compute_context_len(context);

	/* Allocate space for the context; caller must free this space. */
	scontextp = kmalloc(*scontext_len, GFP_ATOMIC);
	if (!scontextp)
		return -ENOMEM;
	*scontext = scontextp;

	/*
	 * Copy the user name, role name and type name into the context.
	 */
	sprintf(scontextp, "%s:%s:%s",
		policydb.p_user_val_to_name[context->user - 1],
		policydb.p_role_val_to_name[context->role - 1],
		policydb.p_type_val_to_name[context->type - 1]);
	scontextp += strlen(policydb.p_user_val_to_name[context->user - 1]) +
		     1 + strlen(policydb.p_role_val_to_name[context->role - 1]) +
		     1 + strlen(policydb.p_type_val_to_name[context->type - 1]);

	mls_sid_to_context(context, &scontextp);

	*scontextp = 0;

	return 0;
}

#include "initial_sid_to_string.h"

const char *security_get_initial_sid_context(u32 sid)
{
	if (unlikely(sid > SECINITSID_NUM))
		return NULL;
	return initial_sid_to_string[sid];
}

static int security_sid_to_context_core(u32 sid, char **scontext,
					u32 *scontext_len, int force)
{
	struct context *context;
	int rc = 0;

	*scontext = NULL;
	*scontext_len  = 0;

	if (!ss_initialized) {
		if (sid <= SECINITSID_NUM) {
			char *scontextp;

			*scontext_len = strlen(initial_sid_to_string[sid]) + 1;
			scontextp = kmalloc(*scontext_len, GFP_ATOMIC);
			if (!scontextp) {
				rc = -ENOMEM;
				goto out;
			}
			strcpy(scontextp, initial_sid_to_string[sid]);
			*scontext = scontextp;
			goto out;
		}
		printk(KERN_ERR "SELinux: %s:  called before initial "
		       "load_policy on unknown SID %d\n", __func__, sid);
		rc = -EINVAL;
		goto out;
	}
	read_lock(&policy_rwlock);
	if (force)
		context = sidtab_search_force(&sidtab, sid);
	else
		context = sidtab_search(&sidtab, sid);
	if (!context) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
			__func__, sid);
		rc = -EINVAL;
		goto out_unlock;
	}
	rc = context_struct_to_string(context, scontext, scontext_len);
out_unlock:
	read_unlock(&policy_rwlock);
out:
	return rc;

}

/**
 * security_sid_to_context - Obtain a context for a given SID.
 * @sid: security identifier, SID
 * @scontext: security context
 * @scontext_len: length in bytes
 *
 * Write the string representation of the context associated with @sid
 * into a dynamically allocated string of the correct size.  Set @scontext
 * to point to this string and set @scontext_len to the length of the string.
 */
int security_sid_to_context(u32 sid, char **scontext, u32 *scontext_len)
{
	return security_sid_to_context_core(sid, scontext, scontext_len, 0);
}

int security_sid_to_context_force(u32 sid, char **scontext, u32 *scontext_len)
{
	return security_sid_to_context_core(sid, scontext, scontext_len, 1);
}

/*
 * Caveat:  Mutates scontext.
 */
static int string_to_context_struct(struct policydb *pol,
				    struct sidtab *sidtabp,
				    char *scontext,
				    u32 scontext_len,
				    struct context *ctx,
				    u32 def_sid)
{
	struct role_datum *role;
	struct type_datum *typdatum;
	struct user_datum *usrdatum;
	char *scontextp, *p, oldc;
	int rc = 0;

	context_init(ctx);

	/* Parse the security context. */

	rc = -EINVAL;
	scontextp = (char *) scontext;

	/* Extract the user. */
	p = scontextp;
	while (*p && *p != ':')
		p++;

	if (*p == 0)
		goto out;

	*p++ = 0;

	usrdatum = hashtab_search(pol->p_users.table, scontextp);
	if (!usrdatum)
		goto out;

	ctx->user = usrdatum->value;

	/* Extract role. */
	scontextp = p;
	while (*p && *p != ':')
		p++;

	if (*p == 0)
		goto out;

	*p++ = 0;

	role = hashtab_search(pol->p_roles.table, scontextp);
	if (!role)
		goto out;
	ctx->role = role->value;

	/* Extract type. */
	scontextp = p;
	while (*p && *p != ':')
		p++;
	oldc = *p;
	*p++ = 0;

	typdatum = hashtab_search(pol->p_types.table, scontextp);
	if (!typdatum || typdatum->attribute)
		goto out;

	ctx->type = typdatum->value;

	rc = mls_context_to_sid(pol, oldc, &p, ctx, sidtabp, def_sid);
	if (rc)
		goto out;

	if ((p - scontext) < scontext_len) {
		rc = -EINVAL;
		goto out;
	}

	/* Check the validity of the new context. */
	if (!policydb_context_isvalid(pol, ctx)) {
		rc = -EINVAL;
		goto out;
	}
	rc = 0;
out:
	if (rc)
		context_destroy(ctx);
	return rc;
}

static int security_context_to_sid_core(const char *scontext, u32 scontext_len,
					u32 *sid, u32 def_sid, gfp_t gfp_flags,
					int force)
{
	char *scontext2, *str = NULL;
	struct context context;
	int rc = 0;

	if (!ss_initialized) {
		int i;

		for (i = 1; i < SECINITSID_NUM; i++) {
			if (!strcmp(initial_sid_to_string[i], scontext)) {
				*sid = i;
				return 0;
			}
		}
		*sid = SECINITSID_KERNEL;
		return 0;
	}
	*sid = SECSID_NULL;

	/* Copy the string so that we can modify the copy as we parse it. */
	scontext2 = kmalloc(scontext_len + 1, gfp_flags);
	if (!scontext2)
		return -ENOMEM;
	memcpy(scontext2, scontext, scontext_len);
	scontext2[scontext_len] = 0;

	if (force) {
		/* Save another copy for storing in uninterpreted form */
		str = kstrdup(scontext2, gfp_flags);
		if (!str) {
			kfree(scontext2);
			return -ENOMEM;
		}
	}

	read_lock(&policy_rwlock);
	rc = string_to_context_struct(&policydb, &sidtab,
				      scontext2, scontext_len,
				      &context, def_sid);
	if (rc == -EINVAL && force) {
		context.str = str;
		context.len = scontext_len;
		str = NULL;
	} else if (rc)
		goto out;
	rc = sidtab_context_to_sid(&sidtab, &context, sid);
	context_destroy(&context);
out:
	read_unlock(&policy_rwlock);
	kfree(scontext2);
	kfree(str);
	return rc;
}

/**
 * security_context_to_sid - Obtain a SID for a given security context.
 * @scontext: security context
 * @scontext_len: length in bytes
 * @sid: security identifier, SID
 *
 * Obtains a SID associated with the security context that
 * has the string representation specified by @scontext.
 * Returns -%EINVAL if the context is invalid, -%ENOMEM if insufficient
 * memory is available, or 0 on success.
 */
int security_context_to_sid(const char *scontext, u32 scontext_len, u32 *sid)
{
	return security_context_to_sid_core(scontext, scontext_len,
					    sid, SECSID_NULL, GFP_KERNEL, 0);
}

/**
 * security_context_to_sid_default - Obtain a SID for a given security context,
 * falling back to specified default if needed.
 *
 * @scontext: security context
 * @scontext_len: length in bytes
 * @sid: security identifier, SID
 * @def_sid: default SID to assign on error
 *
 * Obtains a SID associated with the security context that
 * has the string representation specified by @scontext.
 * The default SID is passed to the MLS layer to be used to allow
 * kernel labeling of the MLS field if the MLS field is not present
 * (for upgrading to MLS without full relabel).
 * Implicitly forces adding of the context even if it cannot be mapped yet.
 * Returns -%EINVAL if the context is invalid, -%ENOMEM if insufficient
 * memory is available, or 0 on success.
 */
int security_context_to_sid_default(const char *scontext, u32 scontext_len,
				    u32 *sid, u32 def_sid, gfp_t gfp_flags)
{
	return security_context_to_sid_core(scontext, scontext_len,
					    sid, def_sid, gfp_flags, 1);
}

int security_context_to_sid_force(const char *scontext, u32 scontext_len,
				  u32 *sid)
{
	return security_context_to_sid_core(scontext, scontext_len,
					    sid, SECSID_NULL, GFP_KERNEL, 1);
}

static int compute_sid_handle_invalid_context(
	struct context *scontext,
	struct context *tcontext,
	u16 tclass,
	struct context *newcontext)
{
	char *s = NULL, *t = NULL, *n = NULL;
	u32 slen, tlen, nlen;

	if (context_struct_to_string(scontext, &s, &slen) < 0)
		goto out;
	if (context_struct_to_string(tcontext, &t, &tlen) < 0)
		goto out;
	if (context_struct_to_string(newcontext, &n, &nlen) < 0)
		goto out;
	audit_log(current->audit_context, GFP_ATOMIC, AUDIT_SELINUX_ERR,
		  "security_compute_sid:  invalid context %s"
		  " for scontext=%s"
		  " tcontext=%s"
		  " tclass=%s",
		  n, s, t, policydb.p_class_val_to_name[tclass-1]);
out:
	kfree(s);
	kfree(t);
	kfree(n);
	if (!selinux_enforcing)
		return 0;
	return -EACCES;
}

static int security_compute_sid(u32 ssid,
				u32 tsid,
				u16 orig_tclass,
				u32 specified,
				u32 *out_sid,
				bool kern)
{
	struct context *scontext = NULL, *tcontext = NULL, newcontext;
	struct role_trans *roletr = NULL;
	struct avtab_key avkey;
	struct avtab_datum *avdatum;
	struct avtab_node *node;
	u16 tclass;
	int rc = 0;

	if (!ss_initialized) {
		switch (orig_tclass) {
		case SECCLASS_PROCESS: /* kernel value */
			*out_sid = ssid;
			break;
		default:
			*out_sid = tsid;
			break;
		}
		goto out;
	}

	context_init(&newcontext);

	read_lock(&policy_rwlock);

	if (kern)
		tclass = unmap_class(orig_tclass);
	else
		tclass = orig_tclass;

	scontext = sidtab_search(&sidtab, ssid);
	if (!scontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
		       __func__, ssid);
		rc = -EINVAL;
		goto out_unlock;
	}
	tcontext = sidtab_search(&sidtab, tsid);
	if (!tcontext) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
		       __func__, tsid);
		rc = -EINVAL;
		goto out_unlock;
	}

	/* Set the user identity. */
	switch (specified) {
	case AVTAB_TRANSITION:
	case AVTAB_CHANGE:
		/* Use the process user identity. */
		newcontext.user = scontext->user;
		break;
	case AVTAB_MEMBER:
		/* Use the related object owner. */
		newcontext.user = tcontext->user;
		break;
	}

	/* Set the role and type to default values. */
	if (tclass == policydb.process_class) {
		/* Use the current role and type of process. */
		newcontext.role = scontext->role;
		newcontext.type = scontext->type;
	} else {
		/* Use the well-defined object role. */
		newcontext.role = OBJECT_R_VAL;
		/* Use the type of the related object. */
		newcontext.type = tcontext->type;
	}

	/* Look for a type transition/member/change rule. */
	avkey.source_type = scontext->type;
	avkey.target_type = tcontext->type;
	avkey.target_class = tclass;
	avkey.specified = specified;
	avdatum = avtab_search(&policydb.te_avtab, &avkey);

	/* If no permanent rule, also check for enabled conditional rules */
	if (!avdatum) {
		node = avtab_search_node(&policydb.te_cond_avtab, &avkey);
		for (; node; node = avtab_search_node_next(node, specified)) {
			if (node->key.specified & AVTAB_ENABLED) {
				avdatum = &node->datum;
				break;
			}
		}
	}

	if (avdatum) {
		/* Use the type from the type transition/member/change rule. */
		newcontext.type = avdatum->data;
	}

	/* Check for class-specific changes. */
	if  (tclass == policydb.process_class) {
		if (specified & AVTAB_TRANSITION) {
			/* Look for a role transition rule. */
			for (roletr = policydb.role_tr; roletr;
			     roletr = roletr->next) {
				if (roletr->role == scontext->role &&
				    roletr->type == tcontext->type) {
					/* Use the role transition rule. */
					newcontext.role = roletr->new_role;
					break;
				}
			}
		}
	}

	/* Set the MLS attributes.
	   This is done last because it may allocate memory. */
	rc = mls_compute_sid(scontext, tcontext, tclass, specified, &newcontext);
	if (rc)
		goto out_unlock;

	/* Check the validity of the context. */
	if (!policydb_context_isvalid(&policydb, &newcontext)) {
		rc = compute_sid_handle_invalid_context(scontext,
							tcontext,
							tclass,
							&newcontext);
		if (rc)
			goto out_unlock;
	}
	/* Obtain the sid for the context. */
	rc = sidtab_context_to_sid(&sidtab, &newcontext, out_sid);
out_unlock:
	read_unlock(&policy_rwlock);
	context_destroy(&newcontext);
out:
	return rc;
}

/**
 * security_transition_sid - Compute the SID for a new subject/object.
 * @ssid: source security identifier
 * @tsid: target security identifier
 * @tclass: target security class
 * @out_sid: security identifier for new subject/object
 *
 * Compute a SID to use for labeling a new subject or object in the
 * class @tclass based on a SID pair (@ssid, @tsid).
 * Return -%EINVAL if any of the parameters are invalid, -%ENOMEM
 * if insufficient memory is available, or %0 if the new SID was
 * computed successfully.
 */
int security_transition_sid(u32 ssid,
			    u32 tsid,
			    u16 tclass,
			    u32 *out_sid)
{
	return security_compute_sid(ssid, tsid, tclass, AVTAB_TRANSITION,
				    out_sid, true);
}

int security_transition_sid_user(u32 ssid,
				 u32 tsid,
				 u16 tclass,
				 u32 *out_sid)
{
	return security_compute_sid(ssid, tsid, tclass, AVTAB_TRANSITION,
				    out_sid, false);
}

/**
 * security_member_sid - Compute the SID for member selection.
 * @ssid: source security identifier
 * @tsid: target security identifier
 * @tclass: target security class
 * @out_sid: security identifier for selected member
 *
 * Compute a SID to use when selecting a member of a polyinstantiated
 * object of class @tclass based on a SID pair (@ssid, @tsid).
 * Return -%EINVAL if any of the parameters are invalid, -%ENOMEM
 * if insufficient memory is available, or %0 if the SID was
 * computed successfully.
 */
int security_member_sid(u32 ssid,
			u32 tsid,
			u16 tclass,
			u32 *out_sid)
{
	return security_compute_sid(ssid, tsid, tclass, AVTAB_MEMBER, out_sid,
				    false);
}

/**
 * security_change_sid - Compute the SID for object relabeling.
 * @ssid: source security identifier
 * @tsid: target security identifier
 * @tclass: target security class
 * @out_sid: security identifier for selected member
 *
 * Compute a SID to use for relabeling an object of class @tclass
 * based on a SID pair (@ssid, @tsid).
 * Return -%EINVAL if any of the parameters are invalid, -%ENOMEM
 * if insufficient memory is available, or %0 if the SID was
 * computed successfully.
 */
int security_change_sid(u32 ssid,
			u32 tsid,
			u16 tclass,
			u32 *out_sid)
{
	return security_compute_sid(ssid, tsid, tclass, AVTAB_CHANGE, out_sid,
				    false);
}

/* Clone the SID into the new SID table. */
static int clone_sid(u32 sid,
		     struct context *context,
		     void *arg)
{
	struct sidtab *s = arg;

	if (sid > SECINITSID_NUM)
		return sidtab_insert(s, sid, context);
	else
		return 0;
}

static inline int convert_context_handle_invalid_context(struct context *context)
{
	int rc = 0;

	if (selinux_enforcing) {
		rc = -EINVAL;
	} else {
		char *s;
		u32 len;

		if (!context_struct_to_string(context, &s, &len)) {
			printk(KERN_WARNING
		       "SELinux:  Context %s would be invalid if enforcing\n",
			       s);
			kfree(s);
		}
	}
	return rc;
}

struct convert_context_args {
	struct policydb *oldp;
	struct policydb *newp;
};

/*
 * Convert the values in the security context
 * structure `c' from the values specified
 * in the policy `p->oldp' to the values specified
 * in the policy `p->newp'.  Verify that the
 * context is valid under the new policy.
 */
static int convert_context(u32 key,
			   struct context *c,
			   void *p)
{
	struct convert_context_args *args;
	struct context oldc;
	struct ocontext *oc;
	struct mls_range *range;
	struct role_datum *role;
	struct type_datum *typdatum;
	struct user_datum *usrdatum;
	char *s;
	u32 len;
	int rc = 0;

	if (key <= SECINITSID_NUM)
		goto out;

	args = p;

	if (c->str) {
		struct context ctx;
		s = kstrdup(c->str, GFP_KERNEL);
		if (!s) {
			rc = -ENOMEM;
			goto out;
		}
		rc = string_to_context_struct(args->newp, NULL, s,
					      c->len, &ctx, SECSID_NULL);
		kfree(s);
		if (!rc) {
			printk(KERN_INFO
		       "SELinux:  Context %s became valid (mapped).\n",
			       c->str);
			/* Replace string with mapped representation. */
			kfree(c->str);
			memcpy(c, &ctx, sizeof(*c));
			goto out;
		} else if (rc == -EINVAL) {
			/* Retain string representation for later mapping. */
			rc = 0;
			goto out;
		} else {
			/* Other error condition, e.g. ENOMEM. */
			printk(KERN_ERR
		       "SELinux:   Unable to map context %s, rc = %d.\n",
			       c->str, -rc);
			goto out;
		}
	}

	rc = context_cpy(&oldc, c);
	if (rc)
		goto out;

	rc = -EINVAL;

	/* Convert the user. */
	usrdatum = hashtab_search(args->newp->p_users.table,
				  args->oldp->p_user_val_to_name[c->user - 1]);
	if (!usrdatum)
		goto bad;
	c->user = usrdatum->value;

	/* Convert the role. */
	role = hashtab_search(args->newp->p_roles.table,
			      args->oldp->p_role_val_to_name[c->role - 1]);
	if (!role)
		goto bad;
	c->role = role->value;

	/* Convert the type. */
	typdatum = hashtab_search(args->newp->p_types.table,
				  args->oldp->p_type_val_to_name[c->type - 1]);
	if (!typdatum)
		goto bad;
	c->type = typdatum->value;

	/* Convert the MLS fields if dealing with MLS policies */
	if (args->oldp->mls_enabled && args->newp->mls_enabled) {
		rc = mls_convert_context(args->oldp, args->newp, c);
		if (rc)
			goto bad;
	} else if (args->oldp->mls_enabled && !args->newp->mls_enabled) {
		/*
		 * Switching between MLS and non-MLS policy:
		 * free any storage used by the MLS fields in the
		 * context for all existing entries in the sidtab.
		 */
		mls_context_destroy(c);
	} else if (!args->oldp->mls_enabled && args->newp->mls_enabled) {
		/*
		 * Switching between non-MLS and MLS policy:
		 * ensure that the MLS fields of the context for all
		 * existing entries in the sidtab are filled in with a
		 * suitable default value, likely taken from one of the
		 * initial SIDs.
		 */
		oc = args->newp->ocontexts[OCON_ISID];
		while (oc && oc->sid[0] != SECINITSID_UNLABELED)
			oc = oc->next;
		if (!oc) {
			printk(KERN_ERR "SELinux:  unable to look up"
				" the initial SIDs list\n");
			goto bad;
		}
		range = &oc->context[0].range;
		rc = mls_range_set(c, range);
		if (rc)
			goto bad;
	}

	/* Check the validity of the new context. */
	if (!policydb_context_isvalid(args->newp, c)) {
		rc = convert_context_handle_invalid_context(&oldc);
		if (rc)
			goto bad;
	}

	context_destroy(&oldc);
	rc = 0;
out:
	return rc;
bad:
	/* Map old representation to string and save it. */
	if (context_struct_to_string(&oldc, &s, &len))
		return -ENOMEM;
	context_destroy(&oldc);
	context_destroy(c);
	c->str = s;
	c->len = len;
	printk(KERN_INFO
	       "SELinux:  Context %s became invalid (unmapped).\n",
	       c->str);
	rc = 0;
	goto out;
}

static void security_load_policycaps(void)
{
	selinux_policycap_netpeer = ebitmap_get_bit(&policydb.policycaps,
						  POLICYDB_CAPABILITY_NETPEER);
	selinux_policycap_openperm = ebitmap_get_bit(&policydb.policycaps,
						  POLICYDB_CAPABILITY_OPENPERM);
}

extern void selinux_complete_init(void);
static int security_preserve_bools(struct policydb *p);

/**
 * security_load_policy - Load a security policy configuration.
 * @data: binary policy data
 * @len: length of data in bytes
 *
 * Load a new set of security policy configuration data,
 * validate it and convert the SID table as necessary.
 * This function will flush the access vector cache after
 * loading the new policy.
 */
int security_load_policy(void *data, size_t len)
{
	struct policydb oldpolicydb, newpolicydb;
	struct sidtab oldsidtab, newsidtab;
	struct selinux_mapping *oldmap, *map = NULL;
	struct convert_context_args args;
	u32 seqno;
	u16 map_size;
	int rc = 0;
	struct policy_file file = { data, len }, *fp = &file;

	if (!ss_initialized) {
		avtab_cache_init();
		rc = policydb_read(&policydb, fp);
		if (rc) {
			avtab_cache_destroy();
			return rc;
		}

		rc = selinux_set_mapping(&policydb, secclass_map,
					 &current_mapping,
					 &current_mapping_size);
		if (rc) {
			policydb_destroy(&policydb);
			avtab_cache_destroy();
			return rc;
		}

		rc = policydb_load_isids(&policydb, &sidtab);
		if (rc) {
			policydb_destroy(&policydb);
			avtab_cache_destroy();
			return rc;
		}

		security_load_policycaps();
		ss_initialized = 1;
		seqno = ++latest_granting;
		selinux_complete_init();
		avc_ss_reset(seqno);
		selnl_notify_policyload(seqno);
		selinux_netlbl_cache_invalidate();
		selinux_xfrm_notify_policyload();
		return 0;
	}


	rc = policydb_read(&newpolicydb, fp);
	if (rc)
		return rc;

	/* If switching between different policy types, log MLS status */
	if (policydb.mls_enabled && !newpolicydb.mls_enabled)
		printk(KERN_INFO "SELinux: Disabling MLS support...\n");
	else if (!policydb.mls_enabled && newpolicydb.mls_enabled)
		printk(KERN_INFO "SELinux: Enabling MLS support...\n");

	rc = policydb_load_isids(&newpolicydb, &newsidtab);
	if (rc) {
		printk(KERN_ERR "SELinux:  unable to load the initial SIDs\n");
		policydb_destroy(&newpolicydb);
		return rc;
	}

	rc = selinux_set_mapping(&newpolicydb, secclass_map, &map, &map_size);
	if (rc)
		goto err;

	rc = security_preserve_bools(&newpolicydb);
	if (rc) {
		printk(KERN_ERR "SELinux:  unable to preserve booleans\n");
		goto err;
	}

	/* Clone the SID table. */
	sidtab_shutdown(&sidtab);

	rc = sidtab_map(&sidtab, clone_sid, &newsidtab);
	if (rc)
		goto err;

	/*
	 * Convert the internal representations of contexts
	 * in the new SID table.
	 */
	args.oldp = &policydb;
	args.newp = &newpolicydb;
	rc = sidtab_map(&newsidtab, convert_context, &args);
	if (rc) {
		printk(KERN_ERR "SELinux:  unable to convert the internal"
			" representation of contexts in the new SID"
			" table\n");
		goto err;
	}

	/* Save the old policydb and SID table to free later. */
	memcpy(&oldpolicydb, &policydb, sizeof policydb);
	sidtab_set(&oldsidtab, &sidtab);

	/* Install the new policydb and SID table. */
	write_lock_irq(&policy_rwlock);
	memcpy(&policydb, &newpolicydb, sizeof policydb);
	sidtab_set(&sidtab, &newsidtab);
	security_load_policycaps();
	oldmap = current_mapping;
	current_mapping = map;
	current_mapping_size = map_size;
	seqno = ++latest_granting;
	write_unlock_irq(&policy_rwlock);

	/* Free the old policydb and SID table. */
	policydb_destroy(&oldpolicydb);
	sidtab_destroy(&oldsidtab);
	kfree(oldmap);

	avc_ss_reset(seqno);
	selnl_notify_policyload(seqno);
	selinux_netlbl_cache_invalidate();
	selinux_xfrm_notify_policyload();

	return 0;

err:
	kfree(map);
	sidtab_destroy(&newsidtab);
	policydb_destroy(&newpolicydb);
	return rc;

}

/**
 * security_port_sid - Obtain the SID for a port.
 * @protocol: protocol number
 * @port: port number
 * @out_sid: security identifier
 */
int security_port_sid(u8 protocol, u16 port, u32 *out_sid)
{
	struct ocontext *c;
	int rc = 0;

	read_lock(&policy_rwlock);

	c = policydb.ocontexts[OCON_PORT];
	while (c) {
		if (c->u.port.protocol == protocol &&
		    c->u.port.low_port <= port &&
		    c->u.port.high_port >= port)
			break;
		c = c->next;
	}

	if (c) {
		if (!c->sid[0]) {
			rc = sidtab_context_to_sid(&sidtab,
						   &c->context[0],
						   &c->sid[0]);
			if (rc)
				goto out;
		}
		*out_sid = c->sid[0];
	} else {
		*out_sid = SECINITSID_PORT;
	}

out:
	read_unlock(&policy_rwlock);
	return rc;
}

/**
 * security_netif_sid - Obtain the SID for a network interface.
 * @name: interface name
 * @if_sid: interface SID
 */
int security_netif_sid(char *name, u32 *if_sid)
{
	int rc = 0;
	struct ocontext *c;

	read_lock(&policy_rwlock);

	c = policydb.ocontexts[OCON_NETIF];
	while (c) {
		if (strcmp(name, c->u.name) == 0)
			break;
		c = c->next;
	}

	if (c) {
		if (!c->sid[0] || !c->sid[1]) {
			rc = sidtab_context_to_sid(&sidtab,
						  &c->context[0],
						  &c->sid[0]);
			if (rc)
				goto out;
			rc = sidtab_context_to_sid(&sidtab,
						   &c->context[1],
						   &c->sid[1]);
			if (rc)
				goto out;
		}
		*if_sid = c->sid[0];
	} else
		*if_sid = SECINITSID_NETIF;

out:
	read_unlock(&policy_rwlock);
	return rc;
}

static int match_ipv6_addrmask(u32 *input, u32 *addr, u32 *mask)
{
	int i, fail = 0;

	for (i = 0; i < 4; i++)
		if (addr[i] != (input[i] & mask[i])) {
			fail = 1;
			break;
		}

	return !fail;
}

/**
 * security_node_sid - Obtain the SID for a node (host).
 * @domain: communication domain aka address family
 * @addrp: address
 * @addrlen: address length in bytes
 * @out_sid: security identifier
 */
int security_node_sid(u16 domain,
		      void *addrp,
		      u32 addrlen,
		      u32 *out_sid)
{
	int rc = 0;
	struct ocontext *c;

	read_lock(&policy_rwlock);

	switch (domain) {
	case AF_INET: {
		u32 addr;

		if (addrlen != sizeof(u32)) {
			rc = -EINVAL;
			goto out;
		}

		addr = *((u32 *)addrp);

		c = policydb.ocontexts[OCON_NODE];
		while (c) {
			if (c->u.node.addr == (addr & c->u.node.mask))
				break;
			c = c->next;
		}
		break;
	}

	case AF_INET6:
		if (addrlen != sizeof(u64) * 2) {
			rc = -EINVAL;
			goto out;
		}
		c = policydb.ocontexts[OCON_NODE6];
		while (c) {
			if (match_ipv6_addrmask(addrp, c->u.node6.addr,
						c->u.node6.mask))
				break;
			c = c->next;
		}
		break;

	default:
		*out_sid = SECINITSID_NODE;
		goto out;
	}

	if (c) {
		if (!c->sid[0]) {
			rc = sidtab_context_to_sid(&sidtab,
						   &c->context[0],
						   &c->sid[0]);
			if (rc)
				goto out;
		}
		*out_sid = c->sid[0];
	} else {
		*out_sid = SECINITSID_NODE;
	}

out:
	read_unlock(&policy_rwlock);
	return rc;
}

#define SIDS_NEL 25

/**
 * security_get_user_sids - Obtain reachable SIDs for a user.
 * @fromsid: starting SID
 * @username: username
 * @sids: array of reachable SIDs for user
 * @nel: number of elements in @sids
 *
 * Generate the set of SIDs for legal security contexts
 * for a given user that can be reached by @fromsid.
 * Set *@sids to point to a dynamically allocated
 * array containing the set of SIDs.  Set *@nel to the
 * number of elements in the array.
 */

int security_get_user_sids(u32 fromsid,
			   char *username,
			   u32 **sids,
			   u32 *nel)
{
	struct context *fromcon, usercon;
	u32 *mysids = NULL, *mysids2, sid;
	u32 mynel = 0, maxnel = SIDS_NEL;
	struct user_datum *user;
	struct role_datum *role;
	struct ebitmap_node *rnode, *tnode;
	int rc = 0, i, j;

	*sids = NULL;
	*nel = 0;

	if (!ss_initialized)
		goto out;

	read_lock(&policy_rwlock);

	context_init(&usercon);

	fromcon = sidtab_search(&sidtab, fromsid);
	if (!fromcon) {
		rc = -EINVAL;
		goto out_unlock;
	}

	user = hashtab_search(policydb.p_users.table, username);
	if (!user) {
		rc = -EINVAL;
		goto out_unlock;
	}
	usercon.user = user->value;

	mysids = kcalloc(maxnel, sizeof(*mysids), GFP_ATOMIC);
	if (!mysids) {
		rc = -ENOMEM;
		goto out_unlock;
	}

	ebitmap_for_each_positive_bit(&user->roles, rnode, i) {
		role = policydb.role_val_to_struct[i];
		usercon.role = i + 1;
		ebitmap_for_each_positive_bit(&role->types, tnode, j) {
			usercon.type = j + 1;

			if (mls_setup_user_range(fromcon, user, &usercon))
				continue;

			rc = sidtab_context_to_sid(&sidtab, &usercon, &sid);
			if (rc)
				goto out_unlock;
			if (mynel < maxnel) {
				mysids[mynel++] = sid;
			} else {
				maxnel += SIDS_NEL;
				mysids2 = kcalloc(maxnel, sizeof(*mysids2), GFP_ATOMIC);
				if (!mysids2) {
					rc = -ENOMEM;
					goto out_unlock;
				}
				memcpy(mysids2, mysids, mynel * sizeof(*mysids2));
				kfree(mysids);
				mysids = mysids2;
				mysids[mynel++] = sid;
			}
		}
	}

out_unlock:
	read_unlock(&policy_rwlock);
	if (rc || !mynel) {
		kfree(mysids);
		goto out;
	}

	mysids2 = kcalloc(mynel, sizeof(*mysids2), GFP_KERNEL);
	if (!mysids2) {
		rc = -ENOMEM;
		kfree(mysids);
		goto out;
	}
	for (i = 0, j = 0; i < mynel; i++) {
		rc = avc_has_perm_noaudit(fromsid, mysids[i],
					  SECCLASS_PROCESS, /* kernel value */
					  PROCESS__TRANSITION, AVC_STRICT,
					  NULL);
		if (!rc)
			mysids2[j++] = mysids[i];
		cond_resched();
	}
	rc = 0;
	kfree(mysids);
	*sids = mysids2;
	*nel = j;
out:
	return rc;
}

/**
 * security_genfs_sid - Obtain a SID for a file in a filesystem
 * @fstype: filesystem type
 * @path: path from root of mount
 * @sclass: file security class
 * @sid: SID for path
 *
 * Obtain a SID to use for a file in a filesystem that
 * cannot support xattr or use a fixed labeling behavior like
 * transition SIDs or task SIDs.
 */
int security_genfs_sid(const char *fstype,
		       char *path,
		       u16 orig_sclass,
		       u32 *sid)
{
	int len;
	u16 sclass;
	struct genfs *genfs;
	struct ocontext *c;
	int rc = 0, cmp = 0;

	while (path[0] == '/' && path[1] == '/')
		path++;

	read_lock(&policy_rwlock);

	sclass = unmap_class(orig_sclass);

	for (genfs = policydb.genfs; genfs; genfs = genfs->next) {
		cmp = strcmp(fstype, genfs->fstype);
		if (cmp <= 0)
			break;
	}

	if (!genfs || cmp) {
		*sid = SECINITSID_UNLABELED;
		rc = -ENOENT;
		goto out;
	}

	for (c = genfs->head; c; c = c->next) {
		len = strlen(c->u.name);
		if ((!c->v.sclass || sclass == c->v.sclass) &&
		    (strncmp(c->u.name, path, len) == 0))
			break;
	}

	if (!c) {
		*sid = SECINITSID_UNLABELED;
		rc = -ENOENT;
		goto out;
	}

	if (!c->sid[0]) {
		rc = sidtab_context_to_sid(&sidtab,
					   &c->context[0],
					   &c->sid[0]);
		if (rc)
			goto out;
	}

	*sid = c->sid[0];
out:
	read_unlock(&policy_rwlock);
	return rc;
}

/**
 * security_fs_use - Determine how to handle labeling for a filesystem.
 * @fstype: filesystem type
 * @behavior: labeling behavior
 * @sid: SID for filesystem (superblock)
 */
int security_fs_use(
	const char *fstype,
	unsigned int *behavior,
	u32 *sid)
{
	int rc = 0;
	struct ocontext *c;

	read_lock(&policy_rwlock);

	c = policydb.ocontexts[OCON_FSUSE];
	while (c) {
		if (strcmp(fstype, c->u.name) == 0)
			break;
		c = c->next;
	}

	if (c) {
		*behavior = c->v.behavior;
		if (!c->sid[0]) {
			rc = sidtab_context_to_sid(&sidtab,
						   &c->context[0],
						   &c->sid[0]);
			if (rc)
				goto out;
		}
		*sid = c->sid[0];
	} else {
		rc = security_genfs_sid(fstype, "/", SECCLASS_DIR, sid);
		if (rc) {
			*behavior = SECURITY_FS_USE_NONE;
			rc = 0;
		} else {
			*behavior = SECURITY_FS_USE_GENFS;
		}
	}

out:
	read_unlock(&policy_rwlock);
	return rc;
}

int security_get_bools(int *len, char ***names, int **values)
{
	int i, rc = -ENOMEM;

	read_lock(&policy_rwlock);
	*names = NULL;
	*values = NULL;

	*len = policydb.p_bools.nprim;
	if (!*len) {
		rc = 0;
		goto out;
	}

       *names = kcalloc(*len, sizeof(char *), GFP_ATOMIC);
	if (!*names)
		goto err;

       *values = kcalloc(*len, sizeof(int), GFP_ATOMIC);
	if (!*values)
		goto err;

	for (i = 0; i < *len; i++) {
		size_t name_len;
		(*values)[i] = policydb.bool_val_to_struct[i]->state;
		name_len = strlen(policydb.p_bool_val_to_name[i]) + 1;
	       (*names)[i] = kmalloc(sizeof(char) * name_len, GFP_ATOMIC);
		if (!(*names)[i])
			goto err;
		strncpy((*names)[i], policydb.p_bool_val_to_name[i], name_len);
		(*names)[i][name_len - 1] = 0;
	}
	rc = 0;
out:
	read_unlock(&policy_rwlock);
	return rc;
err:
	if (*names) {
		for (i = 0; i < *len; i++)
			kfree((*names)[i]);
	}
	kfree(*values);
	goto out;
}


int security_set_bools(int len, int *values)
{
	int i, rc = 0;
	int lenp, seqno = 0;
	struct cond_node *cur;

	write_lock_irq(&policy_rwlock);

	lenp = policydb.p_bools.nprim;
	if (len != lenp) {
		rc = -EFAULT;
		goto out;
	}

	for (i = 0; i < len; i++) {
		if (!!values[i] != policydb.bool_val_to_struct[i]->state) {
			audit_log(current->audit_context, GFP_ATOMIC,
				AUDIT_MAC_CONFIG_CHANGE,
				"bool=%s val=%d old_val=%d auid=%u ses=%u",
				policydb.p_bool_val_to_name[i],
				!!values[i],
				policydb.bool_val_to_struct[i]->state,
				audit_get_loginuid(current),
				audit_get_sessionid(current));
		}
		if (values[i])
			policydb.bool_val_to_struct[i]->state = 1;
		else
			policydb.bool_val_to_struct[i]->state = 0;
	}

	for (cur = policydb.cond_list; cur; cur = cur->next) {
		rc = evaluate_cond_node(&policydb, cur);
		if (rc)
			goto out;
	}

	seqno = ++latest_granting;

out:
	write_unlock_irq(&policy_rwlock);
	if (!rc) {
		avc_ss_reset(seqno);
		selnl_notify_policyload(seqno);
		selinux_xfrm_notify_policyload();
	}
	return rc;
}

int security_get_bool_value(int bool)
{
	int rc = 0;
	int len;

	read_lock(&policy_rwlock);

	len = policydb.p_bools.nprim;
	if (bool >= len) {
		rc = -EFAULT;
		goto out;
	}

	rc = policydb.bool_val_to_struct[bool]->state;
out:
	read_unlock(&policy_rwlock);
	return rc;
}

static int security_preserve_bools(struct policydb *p)
{
	int rc, nbools = 0, *bvalues = NULL, i;
	char **bnames = NULL;
	struct cond_bool_datum *booldatum;
	struct cond_node *cur;

	rc = security_get_bools(&nbools, &bnames, &bvalues);
	if (rc)
		goto out;
	for (i = 0; i < nbools; i++) {
		booldatum = hashtab_search(p->p_bools.table, bnames[i]);
		if (booldatum)
			booldatum->state = bvalues[i];
	}
	for (cur = p->cond_list; cur; cur = cur->next) {
		rc = evaluate_cond_node(p, cur);
		if (rc)
			goto out;
	}

out:
	if (bnames) {
		for (i = 0; i < nbools; i++)
			kfree(bnames[i]);
	}
	kfree(bnames);
	kfree(bvalues);
	return rc;
}

/*
 * security_sid_mls_copy() - computes a new sid based on the given
 * sid and the mls portion of mls_sid.
 */
int security_sid_mls_copy(u32 sid, u32 mls_sid, u32 *new_sid)
{
	struct context *context1;
	struct context *context2;
	struct context newcon;
	char *s;
	u32 len;
	int rc = 0;

	if (!ss_initialized || !policydb.mls_enabled) {
		*new_sid = sid;
		goto out;
	}

	context_init(&newcon);

	read_lock(&policy_rwlock);
	context1 = sidtab_search(&sidtab, sid);
	if (!context1) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
			__func__, sid);
		rc = -EINVAL;
		goto out_unlock;
	}

	context2 = sidtab_search(&sidtab, mls_sid);
	if (!context2) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
			__func__, mls_sid);
		rc = -EINVAL;
		goto out_unlock;
	}

	newcon.user = context1->user;
	newcon.role = context1->role;
	newcon.type = context1->type;
	rc = mls_context_cpy(&newcon, context2);
	if (rc)
		goto out_unlock;

	/* Check the validity of the new context. */
	if (!policydb_context_isvalid(&policydb, &newcon)) {
		rc = convert_context_handle_invalid_context(&newcon);
		if (rc)
			goto bad;
	}

	rc = sidtab_context_to_sid(&sidtab, &newcon, new_sid);
	goto out_unlock;

bad:
	if (!context_struct_to_string(&newcon, &s, &len)) {
		audit_log(current->audit_context, GFP_ATOMIC, AUDIT_SELINUX_ERR,
			  "security_sid_mls_copy: invalid context %s", s);
		kfree(s);
	}

out_unlock:
	read_unlock(&policy_rwlock);
	context_destroy(&newcon);
out:
	return rc;
}

/**
 * security_net_peersid_resolve - Compare and resolve two network peer SIDs
 * @nlbl_sid: NetLabel SID
 * @nlbl_type: NetLabel labeling protocol type
 * @xfrm_sid: XFRM SID
 *
 * Description:
 * Compare the @nlbl_sid and @xfrm_sid values and if the two SIDs can be
 * resolved into a single SID it is returned via @peer_sid and the function
 * returns zero.  Otherwise @peer_sid is set to SECSID_NULL and the function
 * returns a negative value.  A table summarizing the behavior is below:
 *
 *                                 | function return |      @sid
 *   ------------------------------+-----------------+-----------------
 *   no peer labels                |        0        |    SECSID_NULL
 *   single peer label             |        0        |    <peer_label>
 *   multiple, consistent labels   |        0        |    <peer_label>
 *   multiple, inconsistent labels |    -<errno>     |    SECSID_NULL
 *
 */
int security_net_peersid_resolve(u32 nlbl_sid, u32 nlbl_type,
				 u32 xfrm_sid,
				 u32 *peer_sid)
{
	int rc;
	struct context *nlbl_ctx;
	struct context *xfrm_ctx;

	/* handle the common (which also happens to be the set of easy) cases
	 * right away, these two if statements catch everything involving a
	 * single or absent peer SID/label */
	if (xfrm_sid == SECSID_NULL) {
		*peer_sid = nlbl_sid;
		return 0;
	}
	/* NOTE: an nlbl_type == NETLBL_NLTYPE_UNLABELED is a "fallback" label
	 * and is treated as if nlbl_sid == SECSID_NULL when a XFRM SID/label
	 * is present */
	if (nlbl_sid == SECSID_NULL || nlbl_type == NETLBL_NLTYPE_UNLABELED) {
		*peer_sid = xfrm_sid;
		return 0;
	}

	/* we don't need to check ss_initialized here since the only way both
	 * nlbl_sid and xfrm_sid are not equal to SECSID_NULL would be if the
	 * security server was initialized and ss_initialized was true */
	if (!policydb.mls_enabled) {
		*peer_sid = SECSID_NULL;
		return 0;
	}

	read_lock(&policy_rwlock);

	nlbl_ctx = sidtab_search(&sidtab, nlbl_sid);
	if (!nlbl_ctx) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
		       __func__, nlbl_sid);
		rc = -EINVAL;
		goto out_slowpath;
	}
	xfrm_ctx = sidtab_search(&sidtab, xfrm_sid);
	if (!xfrm_ctx) {
		printk(KERN_ERR "SELinux: %s:  unrecognized SID %d\n",
		       __func__, xfrm_sid);
		rc = -EINVAL;
		goto out_slowpath;
	}
	rc = (mls_context_cmp(nlbl_ctx, xfrm_ctx) ? 0 : -EACCES);

out_slowpath:
	read_unlock(&policy_rwlock);
	if (rc == 0)
		/* at present NetLabel SIDs/labels really only carry MLS
		 * information so if the MLS portion of the NetLabel SID
		 * matches the MLS portion of the labeled XFRM SID/label
		 * then pass along the XFRM SID as it is the most
		 * expressive */
		*peer_sid = xfrm_sid;
	else
		*peer_sid = SECSID_NULL;
	return rc;
}

static int get_classes_callback(void *k, void *d, void *args)
{
	struct class_datum *datum = d;
	char *name = k, **classes = args;
	int value = datum->value - 1;

	classes[value] = kstrdup(name, GFP_ATOMIC);
	if (!classes[value])
		return -ENOMEM;

	return 0;
}

int security_get_classes(char ***classes, int *nclasses)
{
	int rc = -ENOMEM;

	read_lock(&policy_rwlock);

	*nclasses = policydb.p_classes.nprim;
	*classes = kcalloc(*nclasses, sizeof(**classes), GFP_ATOMIC);
	if (!*classes)
		goto out;

	rc = hashtab_map(policydb.p_classes.table, get_classes_callback,
			*classes);
	if (rc < 0) {
		int i;
		for (i = 0; i < *nclasses; i++)
			kfree((*classes)[i]);
		kfree(*classes);
	}

out:
	read_unlock(&policy_rwlock);
	return rc;
}

static int get_permissions_callback(void *k, void *d, void *args)
{
	struct perm_datum *datum = d;
	char *name = k, **perms = args;
	int value = datum->value - 1;

	perms[value] = kstrdup(name, GFP_ATOMIC);
	if (!perms[value])
		return -ENOMEM;

	return 0;
}

int security_get_permissions(char *class, char ***perms, int *nperms)
{
	int rc = -ENOMEM, i;
	struct class_datum *match;

	read_lock(&policy_rwlock);

	match = hashtab_search(policydb.p_classes.table, class);
	if (!match) {
		printk(KERN_ERR "SELinux: %s:  unrecognized class %s\n",
			__func__, class);
		rc = -EINVAL;
		goto out;
	}

	*nperms = match->permissions.nprim;
	*perms = kcalloc(*nperms, sizeof(**perms), GFP_ATOMIC);
	if (!*perms)
		goto out;

	if (match->comdatum) {
		rc = hashtab_map(match->comdatum->permissions.table,
				get_permissions_callback, *perms);
		if (rc < 0)
			goto err;
	}

	rc = hashtab_map(match->permissions.table, get_permissions_callback,
			*perms);
	if (rc < 0)
		goto err;

out:
	read_unlock(&policy_rwlock);
	return rc;

err:
	read_unlock(&policy_rwlock);
	for (i = 0; i < *nperms; i++)
		kfree((*perms)[i]);
	kfree(*perms);
	return rc;
}

int security_get_reject_unknown(void)
{
	return policydb.reject_unknown;
}

int security_get_allow_unknown(void)
{
	return policydb.allow_unknown;
}

/**
 * security_policycap_supported - Check for a specific policy capability
 * @req_cap: capability
 *
 * Description:
 * This function queries the currently loaded policy to see if it supports the
 * capability specified by @req_cap.  Returns true (1) if the capability is
 * supported, false (0) if it isn't supported.
 *
 */
int security_policycap_supported(unsigned int req_cap)
{
	int rc;

	read_lock(&policy_rwlock);
	rc = ebitmap_get_bit(&policydb.policycaps, req_cap);
	read_unlock(&policy_rwlock);

	return rc;
}

struct selinux_audit_rule {
	u32 au_seqno;
	struct context au_ctxt;
};

void selinux_audit_rule_free(void *vrule)
{
	struct selinux_audit_rule *rule = vrule;

	if (rule) {
		context_destroy(&rule->au_ctxt);
		kfree(rule);
	}
}

int selinux_audit_rule_init(u32 field, u32 op, char *rulestr, void **vrule)
{
	struct selinux_audit_rule *tmprule;
	struct role_datum *roledatum;
	struct type_datum *typedatum;
	struct user_datum *userdatum;
	struct selinux_audit_rule **rule = (struct selinux_audit_rule **)vrule;
	int rc = 0;

	*rule = NULL;

	if (!ss_initialized)
		return -EOPNOTSUPP;

	switch (field) {
	case AUDIT_SUBJ_USER:
	case AUDIT_SUBJ_ROLE:
	case AUDIT_SUBJ_TYPE:
	case AUDIT_OBJ_USER:
	case AUDIT_OBJ_ROLE:
	case AUDIT_OBJ_TYPE:
		/* only 'equals' and 'not equals' fit user, role, and type */
		if (op != Audit_equal && op != Audit_not_equal)
			return -EINVAL;
		break;
	case AUDIT_SUBJ_SEN:
	case AUDIT_SUBJ_CLR:
	case AUDIT_OBJ_LEV_LOW:
	case AUDIT_OBJ_LEV_HIGH:
		/* we do not allow a range, indicated by the presense of '-' */
		if (strchr(rulestr, '-'))
			return -EINVAL;
		break;
	default:
		/* only the above fields are valid */
		return -EINVAL;
	}

	tmprule = kzalloc(sizeof(struct selinux_audit_rule), GFP_KERNEL);
	if (!tmprule)
		return -ENOMEM;

	context_init(&tmprule->au_ctxt);

	read_lock(&policy_rwlock);

	tmprule->au_seqno = latest_granting;

	switch (field) {
	case AUDIT_SUBJ_USER:
	case AUDIT_OBJ_USER:
		userdatum = hashtab_search(policydb.p_users.table, rulestr);
		if (!userdatum)
			rc = -EINVAL;
		else
			tmprule->au_ctxt.user = userdatum->value;
		break;
	case AUDIT_SUBJ_ROLE:
	case AUDIT_OBJ_ROLE:
		roledatum = hashtab_search(policydb.p_roles.table, rulestr);
		if (!roledatum)
			rc = -EINVAL;
		else
			tmprule->au_ctxt.role = roledatum->value;
		break;
	case AUDIT_SUBJ_TYPE:
	case AUDIT_OBJ_TYPE:
		typedatum = hashtab_search(policydb.p_types.table, rulestr);
		if (!typedatum)
			rc = -EINVAL;
		else
			tmprule->au_ctxt.type = typedatum->value;
		break;
	case AUDIT_SUBJ_SEN:
	case AUDIT_SUBJ_CLR:
	case AUDIT_OBJ_LEV_LOW:
	case AUDIT_OBJ_LEV_HIGH:
		rc = mls_from_string(rulestr, &tmprule->au_ctxt, GFP_ATOMIC);
		break;
	}

	read_unlock(&policy_rwlock);

	if (rc) {
		selinux_audit_rule_free(tmprule);
		tmprule = NULL;
	}

	*rule = tmprule;

	return rc;
}

/* Check to see if the rule contains any selinux fields */
int selinux_audit_rule_known(struct audit_krule *rule)
{
	int i;

	for (i = 0; i < rule->field_count; i++) {
		struct audit_field *f = &rule->fields[i];
		switch (f->type) {
		case AUDIT_SUBJ_USER:
		case AUDIT_SUBJ_ROLE:
		case AUDIT_SUBJ_TYPE:
		case AUDIT_SUBJ_SEN:
		case AUDIT_SUBJ_CLR:
		case AUDIT_OBJ_USER:
		case AUDIT_OBJ_ROLE:
		case AUDIT_OBJ_TYPE:
		case AUDIT_OBJ_LEV_LOW:
		case AUDIT_OBJ_LEV_HIGH:
			return 1;
		}
	}

	return 0;
}

int selinux_audit_rule_match(u32 sid, u32 field, u32 op, void *vrule,
			     struct audit_context *actx)
{
	struct context *ctxt;
	struct mls_level *level;
	struct selinux_audit_rule *rule = vrule;
	int match = 0;

	if (!rule) {
		audit_log(actx, GFP_ATOMIC, AUDIT_SELINUX_ERR,
			  "selinux_audit_rule_match: missing rule\n");
		return -ENOENT;
	}

	read_lock(&policy_rwlock);

	if (rule->au_seqno < latest_granting) {
		audit_log(actx, GFP_ATOMIC, AUDIT_SELINUX_ERR,
			  "selinux_audit_rule_match: stale rule\n");
		match = -ESTALE;
		goto out;
	}

	ctxt = sidtab_search(&sidtab, sid);
	if (!ctxt) {
		audit_log(actx, GFP_ATOMIC, AUDIT_SELINUX_ERR,
			  "selinux_audit_rule_match: unrecognized SID %d\n",
			  sid);
		match = -ENOENT;
		goto out;
	}

	/* a field/op pair that is not caught here will simply fall through
	   without a match */
	switch (field) {
	case AUDIT_SUBJ_USER:
	case AUDIT_OBJ_USER:
		switch (op) {
		case Audit_equal:
			match = (ctxt->user == rule->au_ctxt.user);
			break;
		case Audit_not_equal:
			match = (ctxt->user != rule->au_ctxt.user);
			break;
		}
		break;
	case AUDIT_SUBJ_ROLE:
	case AUDIT_OBJ_ROLE:
		switch (op) {
		case Audit_equal:
			match = (ctxt->role == rule->au_ctxt.role);
			break;
		case Audit_not_equal:
			match = (ctxt->role != rule->au_ctxt.role);
			break;
		}
		break;
	case AUDIT_SUBJ_TYPE:
	case AUDIT_OBJ_TYPE:
		switch (op) {
		case Audit_equal:
			match = (ctxt->type == rule->au_ctxt.type);
			break;
		case Audit_not_equal:
			match = (ctxt->type != rule->au_ctxt.type);
			break;
		}
		break;
	case AUDIT_SUBJ_SEN:
	case AUDIT_SUBJ_CLR:
	case AUDIT_OBJ_LEV_LOW:
	case AUDIT_OBJ_LEV_HIGH:
		level = ((field == AUDIT_SUBJ_SEN ||
			  field == AUDIT_OBJ_LEV_LOW) ?
			 &ctxt->range.level[0] : &ctxt->range.level[1]);
		switch (op) {
		case Audit_equal:
			match = mls_level_eq(&rule->au_ctxt.range.level[0],
					     level);
			break;
		case Audit_not_equal:
			match = !mls_level_eq(&rule->au_ctxt.range.level[0],
					      level);
			break;
		case Audit_lt:
			match = (mls_level_dom(&rule->au_ctxt.range.level[0],
					       level) &&
				 !mls_level_eq(&rule->au_ctxt.range.level[0],
					       level));
			break;
		case Audit_le:
			match = mls_level_dom(&rule->au_ctxt.range.level[0],
					      level);
			break;
		case Audit_gt:
			match = (mls_level_dom(level,
					      &rule->au_ctxt.range.level[0]) &&
				 !mls_level_eq(level,
					       &rule->au_ctxt.range.level[0]));
			break;
		case Audit_ge:
			match = mls_level_dom(level,
					      &rule->au_ctxt.range.level[0]);
			break;
		}
	}

out:
	read_unlock(&policy_rwlock);
	return match;
}

static int (*aurule_callback)(void) = audit_update_lsm_rules;

static int aurule_avc_callback(u32 event, u32 ssid, u32 tsid,
			       u16 class, u32 perms, u32 *retained)
{
	int err = 0;

	if (event == AVC_CALLBACK_RESET && aurule_callback)
		err = aurule_callback();
	return err;
}

static int __init aurule_init(void)
{
	int err;

	err = avc_add_callback(aurule_avc_callback, AVC_CALLBACK_RESET,
			       SECSID_NULL, SECSID_NULL, SECCLASS_NULL, 0);
	if (err)
		panic("avc_add_callback() failed, error %d\n", err);

	return err;
}
__initcall(aurule_init);

#ifdef CONFIG_NETLABEL
/**
 * security_netlbl_cache_add - Add an entry to the NetLabel cache
 * @secattr: the NetLabel packet security attributes
 * @sid: the SELinux SID
 *
 * Description:
 * Attempt to cache the context in @ctx, which was derived from the packet in
 * @skb, in the NetLabel subsystem cache.  This function assumes @secattr has
 * already been initialized.
 *
 */
static void security_netlbl_cache_add(struct netlbl_lsm_secattr *secattr,
				      u32 sid)
{
	u32 *sid_cache;

	sid_cache = kmalloc(sizeof(*sid_cache), GFP_ATOMIC);
	if (sid_cache == NULL)
		return;
	secattr->cache = netlbl_secattr_cache_alloc(GFP_ATOMIC);
	if (secattr->cache == NULL) {
		kfree(sid_cache);
		return;
	}

	*sid_cache = sid;
	secattr->cache->free = kfree;
	secattr->cache->data = sid_cache;
	secattr->flags |= NETLBL_SECATTR_CACHE;
}

/**
 * security_netlbl_secattr_to_sid - Convert a NetLabel secattr to a SELinux SID
 * @secattr: the NetLabel packet security attributes
 * @sid: the SELinux SID
 *
 * Description:
 * Convert the given NetLabel security attributes in @secattr into a
 * SELinux SID.  If the @secattr field does not contain a full SELinux
 * SID/context then use SECINITSID_NETMSG as the foundation.  If possibile the
 * 'cache' field of @secattr is set and the CACHE flag is set; this is to
 * allow the @secattr to be used by NetLabel to cache the secattr to SID
 * conversion for future lookups.  Returns zero on success, negative values on
 * failure.
 *
 */
int security_netlbl_secattr_to_sid(struct netlbl_lsm_secattr *secattr,
				   u32 *sid)
{
	int rc = -EIDRM;
	struct context *ctx;
	struct context ctx_new;

	if (!ss_initialized) {
		*sid = SECSID_NULL;
		return 0;
	}

	read_lock(&policy_rwlock);

	if (secattr->flags & NETLBL_SECATTR_CACHE) {
		*sid = *(u32 *)secattr->cache->data;
		rc = 0;
	} else if (secattr->flags & NETLBL_SECATTR_SECID) {
		*sid = secattr->attr.secid;
		rc = 0;
	} else if (secattr->flags & NETLBL_SECATTR_MLS_LVL) {
		ctx = sidtab_search(&sidtab, SECINITSID_NETMSG);
		if (ctx == NULL)
			goto netlbl_secattr_to_sid_return;

		context_init(&ctx_new);
		ctx_new.user = ctx->user;
		ctx_new.role = ctx->role;
		ctx_new.type = ctx->type;
		mls_import_netlbl_lvl(&ctx_new, secattr);
		if (secattr->flags & NETLBL_SECATTR_MLS_CAT) {
			if (ebitmap_netlbl_import(&ctx_new.range.level[0].cat,
						  secattr->attr.mls.cat) != 0)
				goto netlbl_secattr_to_sid_return;
			memcpy(&ctx_new.range.level[1].cat,
			       &ctx_new.range.level[0].cat,
			       sizeof(ctx_new.range.level[0].cat));
		}
		if (mls_context_isvalid(&policydb, &ctx_new) != 1)
			goto netlbl_secattr_to_sid_return_cleanup;

		rc = sidtab_context_to_sid(&sidtab, &ctx_new, sid);
		if (rc != 0)
			goto netlbl_secattr_to_sid_return_cleanup;

		security_netlbl_cache_add(secattr, *sid);

		ebitmap_destroy(&ctx_new.range.level[0].cat);
	} else {
		*sid = SECSID_NULL;
		rc = 0;
	}

netlbl_secattr_to_sid_return:
	read_unlock(&policy_rwlock);
	return rc;
netlbl_secattr_to_sid_return_cleanup:
	ebitmap_destroy(&ctx_new.range.level[0].cat);
	goto netlbl_secattr_to_sid_return;
}

/**
 * security_netlbl_sid_to_secattr - Convert a SELinux SID to a NetLabel secattr
 * @sid: the SELinux SID
 * @secattr: the NetLabel packet security attributes
 *
 * Description:
 * Convert the given SELinux SID in @sid into a NetLabel security attribute.
 * Returns zero on success, negative values on failure.
 *
 */
int security_netlbl_sid_to_secattr(u32 sid, struct netlbl_lsm_secattr *secattr)
{
	int rc;
	struct context *ctx;

	if (!ss_initialized)
		return 0;

	read_lock(&policy_rwlock);
	ctx = sidtab_search(&sidtab, sid);
	if (ctx == NULL) {
		rc = -ENOENT;
		goto netlbl_sid_to_secattr_failure;
	}
	secattr->domain = kstrdup(policydb.p_type_val_to_name[ctx->type - 1],
				  GFP_ATOMIC);
	if (secattr->domain == NULL) {
		rc = -ENOMEM;
		goto netlbl_sid_to_secattr_failure;
	}
	secattr->attr.secid = sid;
	secattr->flags |= NETLBL_SECATTR_DOMAIN_CPY | NETLBL_SECATTR_SECID;
	mls_export_netlbl_lvl(ctx, secattr);
	rc = mls_export_netlbl_cat(ctx, secattr);
	if (rc != 0)
		goto netlbl_sid_to_secattr_failure;
	read_unlock(&policy_rwlock);

	return 0;

netlbl_sid_to_secattr_failure:
	read_unlock(&policy_rwlock);
	return rc;
}
#endif /* CONFIG_NETLABEL */
