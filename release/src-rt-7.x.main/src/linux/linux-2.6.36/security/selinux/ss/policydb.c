/*
 * Implementation of the policy database.
 *
 * Author : Stephen Smalley, <sds@epoch.ncsc.mil>
 */

/*
 * Updated: Trusted Computer Solutions, Inc. <dgoeddel@trustedcs.com>
 *
 *	Support for enhanced MLS infrastructure.
 *
 * Updated: Frank Mayer <mayerf@tresys.com> and Karl MacMillan <kmacmillan@tresys.com>
 *
 *	Added conditional policy language extensions
 *
 * Updated: Hewlett-Packard <paul.moore@hp.com>
 *
 *      Added support for the policy capability bitmap
 *
 * Copyright (C) 2007 Hewlett-Packard Development Company, L.P.
 * Copyright (C) 2004-2005 Trusted Computer Solutions, Inc.
 * Copyright (C) 2003 - 2004 Tresys Technology, LLC
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/audit.h>
#include <linux/flex_array.h>
#include "security.h"

#include "policydb.h"
#include "conditional.h"
#include "mls.h"

#define _DEBUG_HASHES

#ifdef DEBUG_HASHES
static const char *symtab_name[SYM_NUM] = {
	"common prefixes",
	"classes",
	"roles",
	"types",
	"users",
	"bools",
	"levels",
	"categories",
};
#endif

static unsigned int symtab_sizes[SYM_NUM] = {
	2,
	32,
	16,
	512,
	128,
	16,
	16,
	16,
};

struct policydb_compat_info {
	int version;
	int sym_num;
	int ocon_num;
};

/* These need to be updated if SYM_NUM or OCON_NUM changes */
static struct policydb_compat_info policydb_compat[] = {
	{
		.version	= POLICYDB_VERSION_BASE,
		.sym_num	= SYM_NUM - 3,
		.ocon_num	= OCON_NUM - 1,
	},
	{
		.version	= POLICYDB_VERSION_BOOL,
		.sym_num	= SYM_NUM - 2,
		.ocon_num	= OCON_NUM - 1,
	},
	{
		.version	= POLICYDB_VERSION_IPV6,
		.sym_num	= SYM_NUM - 2,
		.ocon_num	= OCON_NUM,
	},
	{
		.version	= POLICYDB_VERSION_NLCLASS,
		.sym_num	= SYM_NUM - 2,
		.ocon_num	= OCON_NUM,
	},
	{
		.version	= POLICYDB_VERSION_MLS,
		.sym_num	= SYM_NUM,
		.ocon_num	= OCON_NUM,
	},
	{
		.version	= POLICYDB_VERSION_AVTAB,
		.sym_num	= SYM_NUM,
		.ocon_num	= OCON_NUM,
	},
	{
		.version	= POLICYDB_VERSION_RANGETRANS,
		.sym_num	= SYM_NUM,
		.ocon_num	= OCON_NUM,
	},
	{
		.version	= POLICYDB_VERSION_POLCAP,
		.sym_num	= SYM_NUM,
		.ocon_num	= OCON_NUM,
	},
	{
		.version	= POLICYDB_VERSION_PERMISSIVE,
		.sym_num	= SYM_NUM,
		.ocon_num	= OCON_NUM,
	},
	{
		.version	= POLICYDB_VERSION_BOUNDARY,
		.sym_num	= SYM_NUM,
		.ocon_num	= OCON_NUM,
	},
};

static struct policydb_compat_info *policydb_lookup_compat(int version)
{
	int i;
	struct policydb_compat_info *info = NULL;

	for (i = 0; i < ARRAY_SIZE(policydb_compat); i++) {
		if (policydb_compat[i].version == version) {
			info = &policydb_compat[i];
			break;
		}
	}
	return info;
}

/*
 * Initialize the role table.
 */
static int roles_init(struct policydb *p)
{
	char *key = NULL;
	int rc;
	struct role_datum *role;

	role = kzalloc(sizeof(*role), GFP_KERNEL);
	if (!role) {
		rc = -ENOMEM;
		goto out;
	}
	role->value = ++p->p_roles.nprim;
	if (role->value != OBJECT_R_VAL) {
		rc = -EINVAL;
		goto out_free_role;
	}
	key = kstrdup(OBJECT_R, GFP_KERNEL);
	if (!key) {
		rc = -ENOMEM;
		goto out_free_role;
	}
	rc = hashtab_insert(p->p_roles.table, key, role);
	if (rc)
		goto out_free_key;
out:
	return rc;

out_free_key:
	kfree(key);
out_free_role:
	kfree(role);
	goto out;
}

static u32 rangetr_hash(struct hashtab *h, const void *k)
{
	const struct range_trans *key = k;
	return (key->source_type + (key->target_type << 3) +
		(key->target_class << 5)) & (h->size - 1);
}

static int rangetr_cmp(struct hashtab *h, const void *k1, const void *k2)
{
	const struct range_trans *key1 = k1, *key2 = k2;
	return (key1->source_type != key2->source_type ||
		key1->target_type != key2->target_type ||
		key1->target_class != key2->target_class);
}

/*
 * Initialize a policy database structure.
 */
static int policydb_init(struct policydb *p)
{
	int i, rc;

	memset(p, 0, sizeof(*p));

	for (i = 0; i < SYM_NUM; i++) {
		rc = symtab_init(&p->symtab[i], symtab_sizes[i]);
		if (rc)
			goto out_free_symtab;
	}

	rc = avtab_init(&p->te_avtab);
	if (rc)
		goto out_free_symtab;

	rc = roles_init(p);
	if (rc)
		goto out_free_symtab;

	rc = cond_policydb_init(p);
	if (rc)
		goto out_free_symtab;

	p->range_tr = hashtab_create(rangetr_hash, rangetr_cmp, 256);
	if (!p->range_tr)
		goto out_free_symtab;

	ebitmap_init(&p->policycaps);
	ebitmap_init(&p->permissive_map);

out:
	return rc;

out_free_symtab:
	for (i = 0; i < SYM_NUM; i++)
		hashtab_destroy(p->symtab[i].table);
	goto out;
}

/*
 * The following *_index functions are used to
 * define the val_to_name and val_to_struct arrays
 * in a policy database structure.  The val_to_name
 * arrays are used when converting security context
 * structures into string representations.  The
 * val_to_struct arrays are used when the attributes
 * of a class, role, or user are needed.
 */

static int common_index(void *key, void *datum, void *datap)
{
	struct policydb *p;
	struct common_datum *comdatum;

	comdatum = datum;
	p = datap;
	if (!comdatum->value || comdatum->value > p->p_commons.nprim)
		return -EINVAL;
	p->p_common_val_to_name[comdatum->value - 1] = key;
	return 0;
}

static int class_index(void *key, void *datum, void *datap)
{
	struct policydb *p;
	struct class_datum *cladatum;

	cladatum = datum;
	p = datap;
	if (!cladatum->value || cladatum->value > p->p_classes.nprim)
		return -EINVAL;
	p->p_class_val_to_name[cladatum->value - 1] = key;
	p->class_val_to_struct[cladatum->value - 1] = cladatum;
	return 0;
}

static int role_index(void *key, void *datum, void *datap)
{
	struct policydb *p;
	struct role_datum *role;

	role = datum;
	p = datap;
	if (!role->value
	    || role->value > p->p_roles.nprim
	    || role->bounds > p->p_roles.nprim)
		return -EINVAL;
	p->p_role_val_to_name[role->value - 1] = key;
	p->role_val_to_struct[role->value - 1] = role;
	return 0;
}

static int type_index(void *key, void *datum, void *datap)
{
	struct policydb *p;
	struct type_datum *typdatum;

	typdatum = datum;
	p = datap;

	if (typdatum->primary) {
		if (!typdatum->value
		    || typdatum->value > p->p_types.nprim
		    || typdatum->bounds > p->p_types.nprim)
			return -EINVAL;
		p->p_type_val_to_name[typdatum->value - 1] = key;
		p->type_val_to_struct[typdatum->value - 1] = typdatum;
	}

	return 0;
}

static int user_index(void *key, void *datum, void *datap)
{
	struct policydb *p;
	struct user_datum *usrdatum;

	usrdatum = datum;
	p = datap;
	if (!usrdatum->value
	    || usrdatum->value > p->p_users.nprim
	    || usrdatum->bounds > p->p_users.nprim)
		return -EINVAL;
	p->p_user_val_to_name[usrdatum->value - 1] = key;
	p->user_val_to_struct[usrdatum->value - 1] = usrdatum;
	return 0;
}

static int sens_index(void *key, void *datum, void *datap)
{
	struct policydb *p;
	struct level_datum *levdatum;

	levdatum = datum;
	p = datap;

	if (!levdatum->isalias) {
		if (!levdatum->level->sens ||
		    levdatum->level->sens > p->p_levels.nprim)
			return -EINVAL;
		p->p_sens_val_to_name[levdatum->level->sens - 1] = key;
	}

	return 0;
}

static int cat_index(void *key, void *datum, void *datap)
{
	struct policydb *p;
	struct cat_datum *catdatum;

	catdatum = datum;
	p = datap;

	if (!catdatum->isalias) {
		if (!catdatum->value || catdatum->value > p->p_cats.nprim)
			return -EINVAL;
		p->p_cat_val_to_name[catdatum->value - 1] = key;
	}

	return 0;
}

static int (*index_f[SYM_NUM]) (void *key, void *datum, void *datap) =
{
	common_index,
	class_index,
	role_index,
	type_index,
	user_index,
	cond_index_bool,
	sens_index,
	cat_index,
};

/*
 * Define the common val_to_name array and the class
 * val_to_name and val_to_struct arrays in a policy
 * database structure.
 *
 * Caller must clean up upon failure.
 */
static int policydb_index_classes(struct policydb *p)
{
	int rc;

	p->p_common_val_to_name =
		kmalloc(p->p_commons.nprim * sizeof(char *), GFP_KERNEL);
	if (!p->p_common_val_to_name) {
		rc = -ENOMEM;
		goto out;
	}

	rc = hashtab_map(p->p_commons.table, common_index, p);
	if (rc)
		goto out;

	p->class_val_to_struct =
		kmalloc(p->p_classes.nprim * sizeof(*(p->class_val_to_struct)), GFP_KERNEL);
	if (!p->class_val_to_struct) {
		rc = -ENOMEM;
		goto out;
	}

	p->p_class_val_to_name =
		kmalloc(p->p_classes.nprim * sizeof(char *), GFP_KERNEL);
	if (!p->p_class_val_to_name) {
		rc = -ENOMEM;
		goto out;
	}

	rc = hashtab_map(p->p_classes.table, class_index, p);
out:
	return rc;
}

#ifdef DEBUG_HASHES
static void symtab_hash_eval(struct symtab *s)
{
	int i;

	for (i = 0; i < SYM_NUM; i++) {
		struct hashtab *h = s[i].table;
		struct hashtab_info info;

		hashtab_stat(h, &info);
		printk(KERN_DEBUG "SELinux: %s:  %d entries and %d/%d buckets used, "
		       "longest chain length %d\n", symtab_name[i], h->nel,
		       info.slots_used, h->size, info.max_chain_len);
	}
}

static void rangetr_hash_eval(struct hashtab *h)
{
	struct hashtab_info info;

	hashtab_stat(h, &info);
	printk(KERN_DEBUG "SELinux: rangetr:  %d entries and %d/%d buckets used, "
	       "longest chain length %d\n", h->nel,
	       info.slots_used, h->size, info.max_chain_len);
}
#else
static inline void rangetr_hash_eval(struct hashtab *h)
{
}
#endif

/*
 * Define the other val_to_name and val_to_struct arrays
 * in a policy database structure.
 *
 * Caller must clean up on failure.
 */
static int policydb_index_others(struct policydb *p)
{
	int i, rc = 0;

	printk(KERN_DEBUG "SELinux:  %d users, %d roles, %d types, %d bools",
	       p->p_users.nprim, p->p_roles.nprim, p->p_types.nprim, p->p_bools.nprim);
	if (p->mls_enabled)
		printk(", %d sens, %d cats", p->p_levels.nprim,
		       p->p_cats.nprim);
	printk("\n");

	printk(KERN_DEBUG "SELinux:  %d classes, %d rules\n",
	       p->p_classes.nprim, p->te_avtab.nel);

#ifdef DEBUG_HASHES
	avtab_hash_eval(&p->te_avtab, "rules");
	symtab_hash_eval(p->symtab);
#endif

	p->role_val_to_struct =
		kmalloc(p->p_roles.nprim * sizeof(*(p->role_val_to_struct)),
			GFP_KERNEL);
	if (!p->role_val_to_struct) {
		rc = -ENOMEM;
		goto out;
	}

	p->user_val_to_struct =
		kmalloc(p->p_users.nprim * sizeof(*(p->user_val_to_struct)),
			GFP_KERNEL);
	if (!p->user_val_to_struct) {
		rc = -ENOMEM;
		goto out;
	}

	p->type_val_to_struct =
		kmalloc(p->p_types.nprim * sizeof(*(p->type_val_to_struct)),
			GFP_KERNEL);
	if (!p->type_val_to_struct) {
		rc = -ENOMEM;
		goto out;
	}

	if (cond_init_bool_indexes(p)) {
		rc = -ENOMEM;
		goto out;
	}

	for (i = SYM_ROLES; i < SYM_NUM; i++) {
		p->sym_val_to_name[i] =
			kmalloc(p->symtab[i].nprim * sizeof(char *), GFP_KERNEL);
		if (!p->sym_val_to_name[i]) {
			rc = -ENOMEM;
			goto out;
		}
		rc = hashtab_map(p->symtab[i].table, index_f[i], p);
		if (rc)
			goto out;
	}

out:
	return rc;
}

/*
 * The following *_destroy functions are used to
 * free any memory allocated for each kind of
 * symbol data in the policy database.
 */

static int perm_destroy(void *key, void *datum, void *p)
{
	kfree(key);
	kfree(datum);
	return 0;
}

static int common_destroy(void *key, void *datum, void *p)
{
	struct common_datum *comdatum;

	kfree(key);
	comdatum = datum;
	hashtab_map(comdatum->permissions.table, perm_destroy, NULL);
	hashtab_destroy(comdatum->permissions.table);
	kfree(datum);
	return 0;
}

static int cls_destroy(void *key, void *datum, void *p)
{
	struct class_datum *cladatum;
	struct constraint_node *constraint, *ctemp;
	struct constraint_expr *e, *etmp;

	kfree(key);
	cladatum = datum;
	hashtab_map(cladatum->permissions.table, perm_destroy, NULL);
	hashtab_destroy(cladatum->permissions.table);
	constraint = cladatum->constraints;
	while (constraint) {
		e = constraint->expr;
		while (e) {
			ebitmap_destroy(&e->names);
			etmp = e;
			e = e->next;
			kfree(etmp);
		}
		ctemp = constraint;
		constraint = constraint->next;
		kfree(ctemp);
	}

	constraint = cladatum->validatetrans;
	while (constraint) {
		e = constraint->expr;
		while (e) {
			ebitmap_destroy(&e->names);
			etmp = e;
			e = e->next;
			kfree(etmp);
		}
		ctemp = constraint;
		constraint = constraint->next;
		kfree(ctemp);
	}

	kfree(cladatum->comkey);
	kfree(datum);
	return 0;
}

static int role_destroy(void *key, void *datum, void *p)
{
	struct role_datum *role;

	kfree(key);
	role = datum;
	ebitmap_destroy(&role->dominates);
	ebitmap_destroy(&role->types);
	kfree(datum);
	return 0;
}

static int type_destroy(void *key, void *datum, void *p)
{
	kfree(key);
	kfree(datum);
	return 0;
}

static int user_destroy(void *key, void *datum, void *p)
{
	struct user_datum *usrdatum;

	kfree(key);
	usrdatum = datum;
	ebitmap_destroy(&usrdatum->roles);
	ebitmap_destroy(&usrdatum->range.level[0].cat);
	ebitmap_destroy(&usrdatum->range.level[1].cat);
	ebitmap_destroy(&usrdatum->dfltlevel.cat);
	kfree(datum);
	return 0;
}

static int sens_destroy(void *key, void *datum, void *p)
{
	struct level_datum *levdatum;

	kfree(key);
	levdatum = datum;
	ebitmap_destroy(&levdatum->level->cat);
	kfree(levdatum->level);
	kfree(datum);
	return 0;
}

static int cat_destroy(void *key, void *datum, void *p)
{
	kfree(key);
	kfree(datum);
	return 0;
}

static int (*destroy_f[SYM_NUM]) (void *key, void *datum, void *datap) =
{
	common_destroy,
	cls_destroy,
	role_destroy,
	type_destroy,
	user_destroy,
	cond_destroy_bool,
	sens_destroy,
	cat_destroy,
};

static int range_tr_destroy(void *key, void *datum, void *p)
{
	struct mls_range *rt = datum;
	kfree(key);
	ebitmap_destroy(&rt->level[0].cat);
	ebitmap_destroy(&rt->level[1].cat);
	kfree(datum);
	cond_resched();
	return 0;
}

static void ocontext_destroy(struct ocontext *c, int i)
{
	if (!c)
		return;

	context_destroy(&c->context[0]);
	context_destroy(&c->context[1]);
	if (i == OCON_ISID || i == OCON_FS ||
	    i == OCON_NETIF || i == OCON_FSUSE)
		kfree(c->u.name);
	kfree(c);
}

/*
 * Free any memory allocated by a policy database structure.
 */
void policydb_destroy(struct policydb *p)
{
	struct ocontext *c, *ctmp;
	struct genfs *g, *gtmp;
	int i;
	struct role_allow *ra, *lra = NULL;
	struct role_trans *tr, *ltr = NULL;

	for (i = 0; i < SYM_NUM; i++) {
		cond_resched();
		hashtab_map(p->symtab[i].table, destroy_f[i], NULL);
		hashtab_destroy(p->symtab[i].table);
	}

	for (i = 0; i < SYM_NUM; i++)
		kfree(p->sym_val_to_name[i]);

	kfree(p->class_val_to_struct);
	kfree(p->role_val_to_struct);
	kfree(p->user_val_to_struct);
	kfree(p->type_val_to_struct);

	avtab_destroy(&p->te_avtab);

	for (i = 0; i < OCON_NUM; i++) {
		cond_resched();
		c = p->ocontexts[i];
		while (c) {
			ctmp = c;
			c = c->next;
			ocontext_destroy(ctmp, i);
		}
		p->ocontexts[i] = NULL;
	}

	g = p->genfs;
	while (g) {
		cond_resched();
		kfree(g->fstype);
		c = g->head;
		while (c) {
			ctmp = c;
			c = c->next;
			ocontext_destroy(ctmp, OCON_FSUSE);
		}
		gtmp = g;
		g = g->next;
		kfree(gtmp);
	}
	p->genfs = NULL;

	cond_policydb_destroy(p);

	for (tr = p->role_tr; tr; tr = tr->next) {
		cond_resched();
		kfree(ltr);
		ltr = tr;
	}
	kfree(ltr);

	for (ra = p->role_allow; ra; ra = ra->next) {
		cond_resched();
		kfree(lra);
		lra = ra;
	}
	kfree(lra);

	hashtab_map(p->range_tr, range_tr_destroy, NULL);
	hashtab_destroy(p->range_tr);

	if (p->type_attr_map_array) {
		for (i = 0; i < p->p_types.nprim; i++) {
			struct ebitmap *e;

			e = flex_array_get(p->type_attr_map_array, i);
			if (!e)
				continue;
			ebitmap_destroy(e);
		}
		flex_array_free(p->type_attr_map_array);
	}
	ebitmap_destroy(&p->policycaps);
	ebitmap_destroy(&p->permissive_map);

	return;
}

/*
 * Load the initial SIDs specified in a policy database
 * structure into a SID table.
 */
int policydb_load_isids(struct policydb *p, struct sidtab *s)
{
	struct ocontext *head, *c;
	int rc;

	rc = sidtab_init(s);
	if (rc) {
		printk(KERN_ERR "SELinux:  out of memory on SID table init\n");
		goto out;
	}

	head = p->ocontexts[OCON_ISID];
	for (c = head; c; c = c->next) {
		if (!c->context[0].user) {
			printk(KERN_ERR "SELinux:  SID %s was never "
			       "defined.\n", c->u.name);
			rc = -EINVAL;
			goto out;
		}
		if (sidtab_insert(s, c->sid[0], &c->context[0])) {
			printk(KERN_ERR "SELinux:  unable to load initial "
			       "SID %s.\n", c->u.name);
			rc = -EINVAL;
			goto out;
		}
	}
out:
	return rc;
}

int policydb_class_isvalid(struct policydb *p, unsigned int class)
{
	if (!class || class > p->p_classes.nprim)
		return 0;
	return 1;
}

int policydb_role_isvalid(struct policydb *p, unsigned int role)
{
	if (!role || role > p->p_roles.nprim)
		return 0;
	return 1;
}

int policydb_type_isvalid(struct policydb *p, unsigned int type)
{
	if (!type || type > p->p_types.nprim)
		return 0;
	return 1;
}

/*
 * Return 1 if the fields in the security context
 * structure `c' are valid.  Return 0 otherwise.
 */
int policydb_context_isvalid(struct policydb *p, struct context *c)
{
	struct role_datum *role;
	struct user_datum *usrdatum;

	if (!c->role || c->role > p->p_roles.nprim)
		return 0;

	if (!c->user || c->user > p->p_users.nprim)
		return 0;

	if (!c->type || c->type > p->p_types.nprim)
		return 0;

	if (c->role != OBJECT_R_VAL) {
		/*
		 * Role must be authorized for the type.
		 */
		role = p->role_val_to_struct[c->role - 1];
		if (!ebitmap_get_bit(&role->types,
				     c->type - 1))
			/* role may not be associated with type */
			return 0;

		/*
		 * User must be authorized for the role.
		 */
		usrdatum = p->user_val_to_struct[c->user - 1];
		if (!usrdatum)
			return 0;

		if (!ebitmap_get_bit(&usrdatum->roles,
				     c->role - 1))
			/* user may not be associated with role */
			return 0;
	}

	if (!mls_context_isvalid(p, c))
		return 0;

	return 1;
}

/*
 * Read a MLS range structure from a policydb binary
 * representation file.
 */
static int mls_read_range_helper(struct mls_range *r, void *fp)
{
	__le32 buf[2];
	u32 items;
	int rc;

	rc = next_entry(buf, fp, sizeof(u32));
	if (rc < 0)
		goto out;

	items = le32_to_cpu(buf[0]);
	if (items > ARRAY_SIZE(buf)) {
		printk(KERN_ERR "SELinux: mls:  range overflow\n");
		rc = -EINVAL;
		goto out;
	}
	rc = next_entry(buf, fp, sizeof(u32) * items);
	if (rc < 0) {
		printk(KERN_ERR "SELinux: mls:  truncated range\n");
		goto out;
	}
	r->level[0].sens = le32_to_cpu(buf[0]);
	if (items > 1)
		r->level[1].sens = le32_to_cpu(buf[1]);
	else
		r->level[1].sens = r->level[0].sens;

	rc = ebitmap_read(&r->level[0].cat, fp);
	if (rc) {
		printk(KERN_ERR "SELinux: mls:  error reading low "
		       "categories\n");
		goto out;
	}
	if (items > 1) {
		rc = ebitmap_read(&r->level[1].cat, fp);
		if (rc) {
			printk(KERN_ERR "SELinux: mls:  error reading high "
			       "categories\n");
			goto bad_high;
		}
	} else {
		rc = ebitmap_cpy(&r->level[1].cat, &r->level[0].cat);
		if (rc) {
			printk(KERN_ERR "SELinux: mls:  out of memory\n");
			goto bad_high;
		}
	}

	rc = 0;
out:
	return rc;
bad_high:
	ebitmap_destroy(&r->level[0].cat);
	goto out;
}

/*
 * Read and validate a security context structure
 * from a policydb binary representation file.
 */
static int context_read_and_validate(struct context *c,
				     struct policydb *p,
				     void *fp)
{
	__le32 buf[3];
	int rc;

	rc = next_entry(buf, fp, sizeof buf);
	if (rc < 0) {
		printk(KERN_ERR "SELinux: context truncated\n");
		goto out;
	}
	c->user = le32_to_cpu(buf[0]);
	c->role = le32_to_cpu(buf[1]);
	c->type = le32_to_cpu(buf[2]);
	if (p->policyvers >= POLICYDB_VERSION_MLS) {
		if (mls_read_range_helper(&c->range, fp)) {
			printk(KERN_ERR "SELinux: error reading MLS range of "
			       "context\n");
			rc = -EINVAL;
			goto out;
		}
	}

	if (!policydb_context_isvalid(p, c)) {
		printk(KERN_ERR "SELinux:  invalid security context\n");
		context_destroy(c);
		rc = -EINVAL;
	}
out:
	return rc;
}

/*
 * The following *_read functions are used to
 * read the symbol data from a policy database
 * binary representation file.
 */

static int perm_read(struct policydb *p, struct hashtab *h, void *fp)
{
	char *key = NULL;
	struct perm_datum *perdatum;
	int rc;
	__le32 buf[2];
	u32 len;

	perdatum = kzalloc(sizeof(*perdatum), GFP_KERNEL);
	if (!perdatum) {
		rc = -ENOMEM;
		goto out;
	}

	rc = next_entry(buf, fp, sizeof buf);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	perdatum->value = le32_to_cpu(buf[1]);

	key = kmalloc(len + 1, GFP_KERNEL);
	if (!key) {
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = '\0';

	rc = hashtab_insert(h, key, perdatum);
	if (rc)
		goto bad;
out:
	return rc;
bad:
	perm_destroy(key, perdatum, NULL);
	goto out;
}

static int common_read(struct policydb *p, struct hashtab *h, void *fp)
{
	char *key = NULL;
	struct common_datum *comdatum;
	__le32 buf[4];
	u32 len, nel;
	int i, rc;

	comdatum = kzalloc(sizeof(*comdatum), GFP_KERNEL);
	if (!comdatum) {
		rc = -ENOMEM;
		goto out;
	}

	rc = next_entry(buf, fp, sizeof buf);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	comdatum->value = le32_to_cpu(buf[1]);

	rc = symtab_init(&comdatum->permissions, PERM_SYMTAB_SIZE);
	if (rc)
		goto bad;
	comdatum->permissions.nprim = le32_to_cpu(buf[2]);
	nel = le32_to_cpu(buf[3]);

	key = kmalloc(len + 1, GFP_KERNEL);
	if (!key) {
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = '\0';

	for (i = 0; i < nel; i++) {
		rc = perm_read(p, comdatum->permissions.table, fp);
		if (rc)
			goto bad;
	}

	rc = hashtab_insert(h, key, comdatum);
	if (rc)
		goto bad;
out:
	return rc;
bad:
	common_destroy(key, comdatum, NULL);
	goto out;
}

static int read_cons_helper(struct constraint_node **nodep, int ncons,
			    int allowxtarget, void *fp)
{
	struct constraint_node *c, *lc;
	struct constraint_expr *e, *le;
	__le32 buf[3];
	u32 nexpr;
	int rc, i, j, depth;

	lc = NULL;
	for (i = 0; i < ncons; i++) {
		c = kzalloc(sizeof(*c), GFP_KERNEL);
		if (!c)
			return -ENOMEM;

		if (lc)
			lc->next = c;
		else
			*nodep = c;

		rc = next_entry(buf, fp, (sizeof(u32) * 2));
		if (rc < 0)
			return rc;
		c->permissions = le32_to_cpu(buf[0]);
		nexpr = le32_to_cpu(buf[1]);
		le = NULL;
		depth = -1;
		for (j = 0; j < nexpr; j++) {
			e = kzalloc(sizeof(*e), GFP_KERNEL);
			if (!e)
				return -ENOMEM;

			if (le)
				le->next = e;
			else
				c->expr = e;

			rc = next_entry(buf, fp, (sizeof(u32) * 3));
			if (rc < 0)
				return rc;
			e->expr_type = le32_to_cpu(buf[0]);
			e->attr = le32_to_cpu(buf[1]);
			e->op = le32_to_cpu(buf[2]);

			switch (e->expr_type) {
			case CEXPR_NOT:
				if (depth < 0)
					return -EINVAL;
				break;
			case CEXPR_AND:
			case CEXPR_OR:
				if (depth < 1)
					return -EINVAL;
				depth--;
				break;
			case CEXPR_ATTR:
				if (depth == (CEXPR_MAXDEPTH - 1))
					return -EINVAL;
				depth++;
				break;
			case CEXPR_NAMES:
				if (!allowxtarget && (e->attr & CEXPR_XTARGET))
					return -EINVAL;
				if (depth == (CEXPR_MAXDEPTH - 1))
					return -EINVAL;
				depth++;
				if (ebitmap_read(&e->names, fp))
					return -EINVAL;
				break;
			default:
				return -EINVAL;
			}
			le = e;
		}
		if (depth != 0)
			return -EINVAL;
		lc = c;
	}

	return 0;
}

static int class_read(struct policydb *p, struct hashtab *h, void *fp)
{
	char *key = NULL;
	struct class_datum *cladatum;
	__le32 buf[6];
	u32 len, len2, ncons, nel;
	int i, rc;

	cladatum = kzalloc(sizeof(*cladatum), GFP_KERNEL);
	if (!cladatum) {
		rc = -ENOMEM;
		goto out;
	}

	rc = next_entry(buf, fp, sizeof(u32)*6);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	len2 = le32_to_cpu(buf[1]);
	cladatum->value = le32_to_cpu(buf[2]);

	rc = symtab_init(&cladatum->permissions, PERM_SYMTAB_SIZE);
	if (rc)
		goto bad;
	cladatum->permissions.nprim = le32_to_cpu(buf[3]);
	nel = le32_to_cpu(buf[4]);

	ncons = le32_to_cpu(buf[5]);

	key = kmalloc(len + 1, GFP_KERNEL);
	if (!key) {
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = '\0';

	if (len2) {
		cladatum->comkey = kmalloc(len2 + 1, GFP_KERNEL);
		if (!cladatum->comkey) {
			rc = -ENOMEM;
			goto bad;
		}
		rc = next_entry(cladatum->comkey, fp, len2);
		if (rc < 0)
			goto bad;
		cladatum->comkey[len2] = '\0';

		cladatum->comdatum = hashtab_search(p->p_commons.table,
						    cladatum->comkey);
		if (!cladatum->comdatum) {
			printk(KERN_ERR "SELinux:  unknown common %s\n",
			       cladatum->comkey);
			rc = -EINVAL;
			goto bad;
		}
	}
	for (i = 0; i < nel; i++) {
		rc = perm_read(p, cladatum->permissions.table, fp);
		if (rc)
			goto bad;
	}

	rc = read_cons_helper(&cladatum->constraints, ncons, 0, fp);
	if (rc)
		goto bad;

	if (p->policyvers >= POLICYDB_VERSION_VALIDATETRANS) {
		/* grab the validatetrans rules */
		rc = next_entry(buf, fp, sizeof(u32));
		if (rc < 0)
			goto bad;
		ncons = le32_to_cpu(buf[0]);
		rc = read_cons_helper(&cladatum->validatetrans, ncons, 1, fp);
		if (rc)
			goto bad;
	}

	rc = hashtab_insert(h, key, cladatum);
	if (rc)
		goto bad;

	rc = 0;
out:
	return rc;
bad:
	cls_destroy(key, cladatum, NULL);
	goto out;
}

static int role_read(struct policydb *p, struct hashtab *h, void *fp)
{
	char *key = NULL;
	struct role_datum *role;
	int rc, to_read = 2;
	__le32 buf[3];
	u32 len;

	role = kzalloc(sizeof(*role), GFP_KERNEL);
	if (!role) {
		rc = -ENOMEM;
		goto out;
	}

	if (p->policyvers >= POLICYDB_VERSION_BOUNDARY)
		to_read = 3;

	rc = next_entry(buf, fp, sizeof(buf[0]) * to_read);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	role->value = le32_to_cpu(buf[1]);
	if (p->policyvers >= POLICYDB_VERSION_BOUNDARY)
		role->bounds = le32_to_cpu(buf[2]);

	key = kmalloc(len + 1, GFP_KERNEL);
	if (!key) {
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = '\0';

	rc = ebitmap_read(&role->dominates, fp);
	if (rc)
		goto bad;

	rc = ebitmap_read(&role->types, fp);
	if (rc)
		goto bad;

	if (strcmp(key, OBJECT_R) == 0) {
		if (role->value != OBJECT_R_VAL) {
			printk(KERN_ERR "SELinux: Role %s has wrong value %d\n",
			       OBJECT_R, role->value);
			rc = -EINVAL;
			goto bad;
		}
		rc = 0;
		goto bad;
	}

	rc = hashtab_insert(h, key, role);
	if (rc)
		goto bad;
out:
	return rc;
bad:
	role_destroy(key, role, NULL);
	goto out;
}

static int type_read(struct policydb *p, struct hashtab *h, void *fp)
{
	char *key = NULL;
	struct type_datum *typdatum;
	int rc, to_read = 3;
	__le32 buf[4];
	u32 len;

	typdatum = kzalloc(sizeof(*typdatum), GFP_KERNEL);
	if (!typdatum) {
		rc = -ENOMEM;
		return rc;
	}

	if (p->policyvers >= POLICYDB_VERSION_BOUNDARY)
		to_read = 4;

	rc = next_entry(buf, fp, sizeof(buf[0]) * to_read);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	typdatum->value = le32_to_cpu(buf[1]);
	if (p->policyvers >= POLICYDB_VERSION_BOUNDARY) {
		u32 prop = le32_to_cpu(buf[2]);

		if (prop & TYPEDATUM_PROPERTY_PRIMARY)
			typdatum->primary = 1;
		if (prop & TYPEDATUM_PROPERTY_ATTRIBUTE)
			typdatum->attribute = 1;

		typdatum->bounds = le32_to_cpu(buf[3]);
	} else {
		typdatum->primary = le32_to_cpu(buf[2]);
	}

	key = kmalloc(len + 1, GFP_KERNEL);
	if (!key) {
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = '\0';

	rc = hashtab_insert(h, key, typdatum);
	if (rc)
		goto bad;
out:
	return rc;
bad:
	type_destroy(key, typdatum, NULL);
	goto out;
}


/*
 * Read a MLS level structure from a policydb binary
 * representation file.
 */
static int mls_read_level(struct mls_level *lp, void *fp)
{
	__le32 buf[1];
	int rc;

	memset(lp, 0, sizeof(*lp));

	rc = next_entry(buf, fp, sizeof buf);
	if (rc < 0) {
		printk(KERN_ERR "SELinux: mls: truncated level\n");
		goto bad;
	}
	lp->sens = le32_to_cpu(buf[0]);

	if (ebitmap_read(&lp->cat, fp)) {
		printk(KERN_ERR "SELinux: mls:  error reading level "
		       "categories\n");
		goto bad;
	}

	return 0;

bad:
	return -EINVAL;
}

static int user_read(struct policydb *p, struct hashtab *h, void *fp)
{
	char *key = NULL;
	struct user_datum *usrdatum;
	int rc, to_read = 2;
	__le32 buf[3];
	u32 len;

	usrdatum = kzalloc(sizeof(*usrdatum), GFP_KERNEL);
	if (!usrdatum) {
		rc = -ENOMEM;
		goto out;
	}

	if (p->policyvers >= POLICYDB_VERSION_BOUNDARY)
		to_read = 3;

	rc = next_entry(buf, fp, sizeof(buf[0]) * to_read);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	usrdatum->value = le32_to_cpu(buf[1]);
	if (p->policyvers >= POLICYDB_VERSION_BOUNDARY)
		usrdatum->bounds = le32_to_cpu(buf[2]);

	key = kmalloc(len + 1, GFP_KERNEL);
	if (!key) {
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = '\0';

	rc = ebitmap_read(&usrdatum->roles, fp);
	if (rc)
		goto bad;

	if (p->policyvers >= POLICYDB_VERSION_MLS) {
		rc = mls_read_range_helper(&usrdatum->range, fp);
		if (rc)
			goto bad;
		rc = mls_read_level(&usrdatum->dfltlevel, fp);
		if (rc)
			goto bad;
	}

	rc = hashtab_insert(h, key, usrdatum);
	if (rc)
		goto bad;
out:
	return rc;
bad:
	user_destroy(key, usrdatum, NULL);
	goto out;
}

static int sens_read(struct policydb *p, struct hashtab *h, void *fp)
{
	char *key = NULL;
	struct level_datum *levdatum;
	int rc;
	__le32 buf[2];
	u32 len;

	levdatum = kzalloc(sizeof(*levdatum), GFP_ATOMIC);
	if (!levdatum) {
		rc = -ENOMEM;
		goto out;
	}

	rc = next_entry(buf, fp, sizeof buf);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	levdatum->isalias = le32_to_cpu(buf[1]);

	key = kmalloc(len + 1, GFP_ATOMIC);
	if (!key) {
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = '\0';

	levdatum->level = kmalloc(sizeof(struct mls_level), GFP_ATOMIC);
	if (!levdatum->level) {
		rc = -ENOMEM;
		goto bad;
	}
	if (mls_read_level(levdatum->level, fp)) {
		rc = -EINVAL;
		goto bad;
	}

	rc = hashtab_insert(h, key, levdatum);
	if (rc)
		goto bad;
out:
	return rc;
bad:
	sens_destroy(key, levdatum, NULL);
	goto out;
}

static int cat_read(struct policydb *p, struct hashtab *h, void *fp)
{
	char *key = NULL;
	struct cat_datum *catdatum;
	int rc;
	__le32 buf[3];
	u32 len;

	catdatum = kzalloc(sizeof(*catdatum), GFP_ATOMIC);
	if (!catdatum) {
		rc = -ENOMEM;
		goto out;
	}

	rc = next_entry(buf, fp, sizeof buf);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	catdatum->value = le32_to_cpu(buf[1]);
	catdatum->isalias = le32_to_cpu(buf[2]);

	key = kmalloc(len + 1, GFP_ATOMIC);
	if (!key) {
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = '\0';

	rc = hashtab_insert(h, key, catdatum);
	if (rc)
		goto bad;
out:
	return rc;

bad:
	cat_destroy(key, catdatum, NULL);
	goto out;
}

static int (*read_f[SYM_NUM]) (struct policydb *p, struct hashtab *h, void *fp) =
{
	common_read,
	class_read,
	role_read,
	type_read,
	user_read,
	cond_read_bool,
	sens_read,
	cat_read,
};

static int user_bounds_sanity_check(void *key, void *datum, void *datap)
{
	struct user_datum *upper, *user;
	struct policydb *p = datap;
	int depth = 0;

	upper = user = datum;
	while (upper->bounds) {
		struct ebitmap_node *node;
		unsigned long bit;

		if (++depth == POLICYDB_BOUNDS_MAXDEPTH) {
			printk(KERN_ERR "SELinux: user %s: "
			       "too deep or looped boundary",
			       (char *) key);
			return -EINVAL;
		}

		upper = p->user_val_to_struct[upper->bounds - 1];
		ebitmap_for_each_positive_bit(&user->roles, node, bit) {
			if (ebitmap_get_bit(&upper->roles, bit))
				continue;

			printk(KERN_ERR
			       "SELinux: boundary violated policy: "
			       "user=%s role=%s bounds=%s\n",
			       p->p_user_val_to_name[user->value - 1],
			       p->p_role_val_to_name[bit],
			       p->p_user_val_to_name[upper->value - 1]);

			return -EINVAL;
		}
	}

	return 0;
}

static int role_bounds_sanity_check(void *key, void *datum, void *datap)
{
	struct role_datum *upper, *role;
	struct policydb *p = datap;
	int depth = 0;

	upper = role = datum;
	while (upper->bounds) {
		struct ebitmap_node *node;
		unsigned long bit;

		if (++depth == POLICYDB_BOUNDS_MAXDEPTH) {
			printk(KERN_ERR "SELinux: role %s: "
			       "too deep or looped bounds\n",
			       (char *) key);
			return -EINVAL;
		}

		upper = p->role_val_to_struct[upper->bounds - 1];
		ebitmap_for_each_positive_bit(&role->types, node, bit) {
			if (ebitmap_get_bit(&upper->types, bit))
				continue;

			printk(KERN_ERR
			       "SELinux: boundary violated policy: "
			       "role=%s type=%s bounds=%s\n",
			       p->p_role_val_to_name[role->value - 1],
			       p->p_type_val_to_name[bit],
			       p->p_role_val_to_name[upper->value - 1]);

			return -EINVAL;
		}
	}

	return 0;
}

static int type_bounds_sanity_check(void *key, void *datum, void *datap)
{
	struct type_datum *upper, *type;
	struct policydb *p = datap;
	int depth = 0;

	upper = type = datum;
	while (upper->bounds) {
		if (++depth == POLICYDB_BOUNDS_MAXDEPTH) {
			printk(KERN_ERR "SELinux: type %s: "
			       "too deep or looped boundary\n",
			       (char *) key);
			return -EINVAL;
		}

		upper = p->type_val_to_struct[upper->bounds - 1];
		if (upper->attribute) {
			printk(KERN_ERR "SELinux: type %s: "
			       "bounded by attribute %s",
			       (char *) key,
			       p->p_type_val_to_name[upper->value - 1]);
			return -EINVAL;
		}
	}

	return 0;
}

static int policydb_bounds_sanity_check(struct policydb *p)
{
	int rc;

	if (p->policyvers < POLICYDB_VERSION_BOUNDARY)
		return 0;

	rc = hashtab_map(p->p_users.table,
			 user_bounds_sanity_check, p);
	if (rc)
		return rc;

	rc = hashtab_map(p->p_roles.table,
			 role_bounds_sanity_check, p);
	if (rc)
		return rc;

	rc = hashtab_map(p->p_types.table,
			 type_bounds_sanity_check, p);
	if (rc)
		return rc;

	return 0;
}

extern int ss_initialized;

u16 string_to_security_class(struct policydb *p, const char *name)
{
	struct class_datum *cladatum;

	cladatum = hashtab_search(p->p_classes.table, name);
	if (!cladatum)
		return 0;

	return cladatum->value;
}

u32 string_to_av_perm(struct policydb *p, u16 tclass, const char *name)
{
	struct class_datum *cladatum;
	struct perm_datum *perdatum = NULL;
	struct common_datum *comdatum;

	if (!tclass || tclass > p->p_classes.nprim)
		return 0;

	cladatum = p->class_val_to_struct[tclass-1];
	comdatum = cladatum->comdatum;
	if (comdatum)
		perdatum = hashtab_search(comdatum->permissions.table,
					  name);
	if (!perdatum)
		perdatum = hashtab_search(cladatum->permissions.table,
					  name);
	if (!perdatum)
		return 0;

	return 1U << (perdatum->value-1);
}

static int range_read(struct policydb *p, void *fp)
{
	struct range_trans *rt = NULL;
	struct mls_range *r = NULL;
	int i, rc;
	__le32 buf[2];
	u32 nel;

	if (p->policyvers < POLICYDB_VERSION_MLS)
		return 0;

	rc = next_entry(buf, fp, sizeof(u32));
	if (rc)
		goto out;

	nel = le32_to_cpu(buf[0]);
	for (i = 0; i < nel; i++) {
		rc = -ENOMEM;
		rt = kzalloc(sizeof(*rt), GFP_KERNEL);
		if (!rt)
			goto out;

		rc = next_entry(buf, fp, (sizeof(u32) * 2));
		if (rc)
			goto out;

		rt->source_type = le32_to_cpu(buf[0]);
		rt->target_type = le32_to_cpu(buf[1]);
		if (p->policyvers >= POLICYDB_VERSION_RANGETRANS) {
			rc = next_entry(buf, fp, sizeof(u32));
			if (rc)
				goto out;
			rt->target_class = le32_to_cpu(buf[0]);
		} else
			rt->target_class = p->process_class;

		rc = -EINVAL;
		if (!policydb_type_isvalid(p, rt->source_type) ||
		    !policydb_type_isvalid(p, rt->target_type) ||
		    !policydb_class_isvalid(p, rt->target_class))
			goto out;

		rc = -ENOMEM;
		r = kzalloc(sizeof(*r), GFP_KERNEL);
		if (!r)
			goto out;

		rc = mls_read_range_helper(r, fp);
		if (rc)
			goto out;

		rc = -EINVAL;
		if (!mls_range_isvalid(p, r)) {
			printk(KERN_WARNING "SELinux:  rangetrans:  invalid range\n");
			goto out;
		}

		rc = hashtab_insert(p->range_tr, rt, r);
		if (rc)
			goto out;

		rt = NULL;
		r = NULL;
	}
	rangetr_hash_eval(p->range_tr);
	rc = 0;
out:
	kfree(rt);
	kfree(r);
	return rc;
}

static int genfs_read(struct policydb *p, void *fp)
{
	int i, j, rc;
	u32 nel, nel2, len, len2;
	__le32 buf[1];
	struct ocontext *l, *c;
	struct ocontext *newc = NULL;
	struct genfs *genfs_p, *genfs;
	struct genfs *newgenfs = NULL;

	rc = next_entry(buf, fp, sizeof(u32));
	if (rc)
		goto out;
	nel = le32_to_cpu(buf[0]);

	for (i = 0; i < nel; i++) {
		rc = next_entry(buf, fp, sizeof(u32));
		if (rc)
			goto out;
		len = le32_to_cpu(buf[0]);

		rc = -ENOMEM;
		newgenfs = kzalloc(sizeof(*newgenfs), GFP_KERNEL);
		if (!newgenfs)
			goto out;

		rc = -ENOMEM;
		newgenfs->fstype = kmalloc(len + 1, GFP_KERNEL);
		if (!newgenfs->fstype)
			goto out;

		rc = next_entry(newgenfs->fstype, fp, len);
		if (rc)
			goto out;

		newgenfs->fstype[len] = 0;

		for (genfs_p = NULL, genfs = p->genfs; genfs;
		     genfs_p = genfs, genfs = genfs->next) {
			rc = -EINVAL;
			if (strcmp(newgenfs->fstype, genfs->fstype) == 0) {
				printk(KERN_ERR "SELinux:  dup genfs fstype %s\n",
				       newgenfs->fstype);
				goto out;
			}
			if (strcmp(newgenfs->fstype, genfs->fstype) < 0)
				break;
		}
		newgenfs->next = genfs;
		if (genfs_p)
			genfs_p->next = newgenfs;
		else
			p->genfs = newgenfs;
		genfs = newgenfs;
		newgenfs = NULL;

		rc = next_entry(buf, fp, sizeof(u32));
		if (rc)
			goto out;

		nel2 = le32_to_cpu(buf[0]);
		for (j = 0; j < nel2; j++) {
			rc = next_entry(buf, fp, sizeof(u32));
			if (rc)
				goto out;
			len = le32_to_cpu(buf[0]);

			rc = -ENOMEM;
			newc = kzalloc(sizeof(*newc), GFP_KERNEL);
			if (!newc)
				goto out;

			rc = -ENOMEM;
			newc->u.name = kmalloc(len + 1, GFP_KERNEL);
			if (!newc->u.name)
				goto out;

			rc = next_entry(newc->u.name, fp, len);
			if (rc)
				goto out;
			newc->u.name[len] = 0;

			rc = next_entry(buf, fp, sizeof(u32));
			if (rc)
				goto out;

			newc->v.sclass = le32_to_cpu(buf[0]);
			rc = context_read_and_validate(&newc->context[0], p, fp);
			if (rc)
				goto out;

			for (l = NULL, c = genfs->head; c;
			     l = c, c = c->next) {
				rc = -EINVAL;
				if (!strcmp(newc->u.name, c->u.name) &&
				    (!c->v.sclass || !newc->v.sclass ||
				     newc->v.sclass == c->v.sclass)) {
					printk(KERN_ERR "SELinux:  dup genfs entry (%s,%s)\n",
					       genfs->fstype, c->u.name);
					goto out;
				}
				len = strlen(newc->u.name);
				len2 = strlen(c->u.name);
				if (len > len2)
					break;
			}

			newc->next = c;
			if (l)
				l->next = newc;
			else
				genfs->head = newc;
			newc = NULL;
		}
	}
	rc = 0;
out:
	if (newgenfs)
		kfree(newgenfs->fstype);
	kfree(newgenfs);
	ocontext_destroy(newc, OCON_FSUSE);

	return rc;
}

static int ocontext_read(struct policydb *p, struct policydb_compat_info *info,
			 void *fp)
{
	int i, j, rc;
	u32 nel, len;
	__le32 buf[3];
	struct ocontext *l, *c;
	u32 nodebuf[8];

	for (i = 0; i < info->ocon_num; i++) {
		rc = next_entry(buf, fp, sizeof(u32));
		if (rc)
			goto out;
		nel = le32_to_cpu(buf[0]);

		l = NULL;
		for (j = 0; j < nel; j++) {
			rc = -ENOMEM;
			c = kzalloc(sizeof(*c), GFP_KERNEL);
			if (!c)
				goto out;
			if (l)
				l->next = c;
			else
				p->ocontexts[i] = c;
			l = c;

			switch (i) {
			case OCON_ISID:
				rc = next_entry(buf, fp, sizeof(u32));
				if (rc)
					goto out;

				c->sid[0] = le32_to_cpu(buf[0]);
				rc = context_read_and_validate(&c->context[0], p, fp);
				if (rc)
					goto out;
				break;
			case OCON_FS:
			case OCON_NETIF:
				rc = next_entry(buf, fp, sizeof(u32));
				if (rc)
					goto out;
				len = le32_to_cpu(buf[0]);

				rc = -ENOMEM;
				c->u.name = kmalloc(len + 1, GFP_KERNEL);
				if (!c->u.name)
					goto out;

				rc = next_entry(c->u.name, fp, len);
				if (rc)
					goto out;

				c->u.name[len] = 0;
				rc = context_read_and_validate(&c->context[0], p, fp);
				if (rc)
					goto out;
				rc = context_read_and_validate(&c->context[1], p, fp);
				if (rc)
					goto out;
				break;
			case OCON_PORT:
				rc = next_entry(buf, fp, sizeof(u32)*3);
				if (rc)
					goto out;
				c->u.port.protocol = le32_to_cpu(buf[0]);
				c->u.port.low_port = le32_to_cpu(buf[1]);
				c->u.port.high_port = le32_to_cpu(buf[2]);
				rc = context_read_and_validate(&c->context[0], p, fp);
				if (rc)
					goto out;
				break;
			case OCON_NODE:
				rc = next_entry(nodebuf, fp, sizeof(u32) * 2);
				if (rc)
					goto out;
				c->u.node.addr = nodebuf[0]; /* network order */
				c->u.node.mask = nodebuf[1]; /* network order */
				rc = context_read_and_validate(&c->context[0], p, fp);
				if (rc)
					goto out;
				break;
			case OCON_FSUSE:
				rc = next_entry(buf, fp, sizeof(u32)*2);
				if (rc)
					goto out;

				rc = -EINVAL;
				c->v.behavior = le32_to_cpu(buf[0]);
				if (c->v.behavior > SECURITY_FS_USE_NONE)
					goto out;

				rc = -ENOMEM;
				len = le32_to_cpu(buf[1]);
				c->u.name = kmalloc(len + 1, GFP_KERNEL);
				if (!c->u.name)
					goto out;

				rc = next_entry(c->u.name, fp, len);
				if (rc)
					goto out;
				c->u.name[len] = 0;
				rc = context_read_and_validate(&c->context[0], p, fp);
				if (rc)
					goto out;
				break;
			case OCON_NODE6: {
				int k;

				rc = next_entry(nodebuf, fp, sizeof(u32) * 8);
				if (rc)
					goto out;
				for (k = 0; k < 4; k++)
					c->u.node6.addr[k] = nodebuf[k];
				for (k = 0; k < 4; k++)
					c->u.node6.mask[k] = nodebuf[k+4];
				rc = context_read_and_validate(&c->context[0], p, fp);
				if (rc)
					goto out;
				break;
			}
			}
		}
	}
	rc = 0;
out:
	return rc;
}

/*
 * Read the configuration data from a policy database binary
 * representation file into a policy database structure.
 */
int policydb_read(struct policydb *p, void *fp)
{
	struct role_allow *ra, *lra;
	struct role_trans *tr, *ltr;
	int i, j, rc;
	__le32 buf[4];
	u32 len, nprim, nel;

	char *policydb_str;
	struct policydb_compat_info *info;

	rc = policydb_init(p);
	if (rc)
		goto out;

	/* Read the magic number and string length. */
	rc = next_entry(buf, fp, sizeof(u32) * 2);
	if (rc < 0)
		goto bad;

	if (le32_to_cpu(buf[0]) != POLICYDB_MAGIC) {
		printk(KERN_ERR "SELinux:  policydb magic number 0x%x does "
		       "not match expected magic number 0x%x\n",
		       le32_to_cpu(buf[0]), POLICYDB_MAGIC);
		goto bad;
	}

	len = le32_to_cpu(buf[1]);
	if (len != strlen(POLICYDB_STRING)) {
		printk(KERN_ERR "SELinux:  policydb string length %d does not "
		       "match expected length %Zu\n",
		       len, strlen(POLICYDB_STRING));
		goto bad;
	}
	policydb_str = kmalloc(len + 1, GFP_KERNEL);
	if (!policydb_str) {
		printk(KERN_ERR "SELinux:  unable to allocate memory for policydb "
		       "string of length %d\n", len);
		rc = -ENOMEM;
		goto bad;
	}
	rc = next_entry(policydb_str, fp, len);
	if (rc < 0) {
		printk(KERN_ERR "SELinux:  truncated policydb string identifier\n");
		kfree(policydb_str);
		goto bad;
	}
	policydb_str[len] = '\0';
	if (strcmp(policydb_str, POLICYDB_STRING)) {
		printk(KERN_ERR "SELinux:  policydb string %s does not match "
		       "my string %s\n", policydb_str, POLICYDB_STRING);
		kfree(policydb_str);
		goto bad;
	}
	/* Done with policydb_str. */
	kfree(policydb_str);
	policydb_str = NULL;

	/* Read the version and table sizes. */
	rc = next_entry(buf, fp, sizeof(u32)*4);
	if (rc < 0)
		goto bad;

	p->policyvers = le32_to_cpu(buf[0]);
	if (p->policyvers < POLICYDB_VERSION_MIN ||
	    p->policyvers > POLICYDB_VERSION_MAX) {
		printk(KERN_ERR "SELinux:  policydb version %d does not match "
		       "my version range %d-%d\n",
		       le32_to_cpu(buf[0]), POLICYDB_VERSION_MIN, POLICYDB_VERSION_MAX);
		goto bad;
	}

	if ((le32_to_cpu(buf[1]) & POLICYDB_CONFIG_MLS)) {
		p->mls_enabled = 1;

		if (p->policyvers < POLICYDB_VERSION_MLS) {
			printk(KERN_ERR "SELinux: security policydb version %d "
				"(MLS) not backwards compatible\n",
				p->policyvers);
			goto bad;
		}
	}
	p->reject_unknown = !!(le32_to_cpu(buf[1]) & REJECT_UNKNOWN);
	p->allow_unknown = !!(le32_to_cpu(buf[1]) & ALLOW_UNKNOWN);

	if (p->policyvers >= POLICYDB_VERSION_POLCAP &&
	    ebitmap_read(&p->policycaps, fp) != 0)
		goto bad;

	if (p->policyvers >= POLICYDB_VERSION_PERMISSIVE &&
	    ebitmap_read(&p->permissive_map, fp) != 0)
		goto bad;

	info = policydb_lookup_compat(p->policyvers);
	if (!info) {
		printk(KERN_ERR "SELinux:  unable to find policy compat info "
		       "for version %d\n", p->policyvers);
		goto bad;
	}

	if (le32_to_cpu(buf[2]) != info->sym_num ||
		le32_to_cpu(buf[3]) != info->ocon_num) {
		printk(KERN_ERR "SELinux:  policydb table sizes (%d,%d) do "
		       "not match mine (%d,%d)\n", le32_to_cpu(buf[2]),
			le32_to_cpu(buf[3]),
		       info->sym_num, info->ocon_num);
		goto bad;
	}

	for (i = 0; i < info->sym_num; i++) {
		rc = next_entry(buf, fp, sizeof(u32)*2);
		if (rc < 0)
			goto bad;
		nprim = le32_to_cpu(buf[0]);
		nel = le32_to_cpu(buf[1]);
		for (j = 0; j < nel; j++) {
			rc = read_f[i](p, p->symtab[i].table, fp);
			if (rc)
				goto bad;
		}

		p->symtab[i].nprim = nprim;
	}

	rc = avtab_read(&p->te_avtab, fp, p);
	if (rc)
		goto bad;

	if (p->policyvers >= POLICYDB_VERSION_BOOL) {
		rc = cond_read_list(p, fp);
		if (rc)
			goto bad;
	}

	rc = next_entry(buf, fp, sizeof(u32));
	if (rc < 0)
		goto bad;
	nel = le32_to_cpu(buf[0]);
	ltr = NULL;
	for (i = 0; i < nel; i++) {
		tr = kzalloc(sizeof(*tr), GFP_KERNEL);
		if (!tr) {
			rc = -ENOMEM;
			goto bad;
		}
		if (ltr)
			ltr->next = tr;
		else
			p->role_tr = tr;
		rc = next_entry(buf, fp, sizeof(u32)*3);
		if (rc < 0)
			goto bad;
		tr->role = le32_to_cpu(buf[0]);
		tr->type = le32_to_cpu(buf[1]);
		tr->new_role = le32_to_cpu(buf[2]);
		if (!policydb_role_isvalid(p, tr->role) ||
		    !policydb_type_isvalid(p, tr->type) ||
		    !policydb_role_isvalid(p, tr->new_role)) {
			rc = -EINVAL;
			goto bad;
		}
		ltr = tr;
	}

	rc = next_entry(buf, fp, sizeof(u32));
	if (rc < 0)
		goto bad;
	nel = le32_to_cpu(buf[0]);
	lra = NULL;
	for (i = 0; i < nel; i++) {
		ra = kzalloc(sizeof(*ra), GFP_KERNEL);
		if (!ra) {
			rc = -ENOMEM;
			goto bad;
		}
		if (lra)
			lra->next = ra;
		else
			p->role_allow = ra;
		rc = next_entry(buf, fp, sizeof(u32)*2);
		if (rc < 0)
			goto bad;
		ra->role = le32_to_cpu(buf[0]);
		ra->new_role = le32_to_cpu(buf[1]);
		if (!policydb_role_isvalid(p, ra->role) ||
		    !policydb_role_isvalid(p, ra->new_role)) {
			rc = -EINVAL;
			goto bad;
		}
		lra = ra;
	}

	rc = policydb_index_classes(p);
	if (rc)
		goto bad;

	rc = policydb_index_others(p);
	if (rc)
		goto bad;

	p->process_class = string_to_security_class(p, "process");
	if (!p->process_class)
		goto bad;
	p->process_trans_perms = string_to_av_perm(p, p->process_class,
						   "transition");
	p->process_trans_perms |= string_to_av_perm(p, p->process_class,
						    "dyntransition");
	if (!p->process_trans_perms)
		goto bad;

	rc = ocontext_read(p, info, fp);
	if (rc)
		goto bad;

	rc = genfs_read(p, fp);
	if (rc)
		goto bad;

	rc = range_read(p, fp);
	if (rc)
		goto bad;

	rc = -ENOMEM;
	p->type_attr_map_array = flex_array_alloc(sizeof(struct ebitmap),
						  p->p_types.nprim,
						  GFP_KERNEL | __GFP_ZERO);
	if (!p->type_attr_map_array)
		goto bad;

	/* preallocate so we don't have to worry about the put ever failing */
	rc = flex_array_prealloc(p->type_attr_map_array, 0, p->p_types.nprim - 1,
				 GFP_KERNEL | __GFP_ZERO);
	if (rc)
		goto bad;

	for (i = 0; i < p->p_types.nprim; i++) {
		struct ebitmap *e = flex_array_get(p->type_attr_map_array, i);

		BUG_ON(!e);
		ebitmap_init(e);
		if (p->policyvers >= POLICYDB_VERSION_AVTAB) {
			rc = ebitmap_read(e, fp);
			if (rc)
				goto bad;
		}
		/* add the type itself as the degenerate case */
		rc = ebitmap_set_bit(e, i, 1);
		if (rc)
			goto bad;
	}

	rc = policydb_bounds_sanity_check(p);
	if (rc)
		goto bad;

	rc = 0;
out:
	return rc;
bad:
	if (!rc)
		rc = -EINVAL;
	policydb_destroy(p);
	goto out;
}
