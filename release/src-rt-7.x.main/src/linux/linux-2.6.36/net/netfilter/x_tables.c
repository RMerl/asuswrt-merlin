/*
 * x_tables core - Backend for {ip,ip6,arp}_tables
 *
 * Copyright (C) 2006-2006 Harald Welte <laforge@netfilter.org>
 *
 * Based on existing ip_tables code which is
 *   Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 *   Copyright (C) 2000-2005 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <net/net_namespace.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_arp.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_arp/arp_tables.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harald Welte <laforge@netfilter.org>");
MODULE_DESCRIPTION("{ip,ip6,arp,eb}_tables backend module");

#define SMP_ALIGN(x) (((x) + SMP_CACHE_BYTES-1) & ~(SMP_CACHE_BYTES-1))

struct compat_delta {
	struct compat_delta *next;
	unsigned int offset;
	int delta;
};

struct xt_af {
	struct mutex mutex;
	struct list_head match;
	struct list_head target;
#ifdef CONFIG_COMPAT
	struct mutex compat_mutex;
	struct compat_delta *compat_offsets;
#endif
};

static struct xt_af *xt;

static const char *const xt_prefix[NFPROTO_NUMPROTO] = {
	[NFPROTO_UNSPEC] = "x",
	[NFPROTO_IPV4]   = "ip",
	[NFPROTO_ARP]    = "arp",
	[NFPROTO_BRIDGE] = "eb",
	[NFPROTO_IPV6]   = "ip6",
};

/* Allow this many total (re)entries. */
static const unsigned int xt_jumpstack_multiplier = 2;

/* Registration hooks for targets. */
int
xt_register_target(struct xt_target *target)
{
	u_int8_t af = target->family;
	int ret;

	ret = mutex_lock_interruptible(&xt[af].mutex);
	if (ret != 0)
		return ret;
	list_add(&target->list, &xt[af].target);
	mutex_unlock(&xt[af].mutex);
	return ret;
}
EXPORT_SYMBOL(xt_register_target);

void
xt_unregister_target(struct xt_target *target)
{
	u_int8_t af = target->family;

	mutex_lock(&xt[af].mutex);
	list_del(&target->list);
	mutex_unlock(&xt[af].mutex);
}
EXPORT_SYMBOL(xt_unregister_target);

int
xt_register_targets(struct xt_target *target, unsigned int n)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < n; i++) {
		err = xt_register_target(&target[i]);
		if (err)
			goto err;
	}
	return err;

err:
	if (i > 0)
		xt_unregister_targets(target, i);
	return err;
}
EXPORT_SYMBOL(xt_register_targets);

void
xt_unregister_targets(struct xt_target *target, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		xt_unregister_target(&target[i]);
}
EXPORT_SYMBOL(xt_unregister_targets);

int
xt_register_match(struct xt_match *match)
{
	u_int8_t af = match->family;
	int ret;

	ret = mutex_lock_interruptible(&xt[af].mutex);
	if (ret != 0)
		return ret;

	list_add(&match->list, &xt[af].match);
	mutex_unlock(&xt[af].mutex);

	return ret;
}
EXPORT_SYMBOL(xt_register_match);

void
xt_unregister_match(struct xt_match *match)
{
	u_int8_t af = match->family;

	mutex_lock(&xt[af].mutex);
	list_del(&match->list);
	mutex_unlock(&xt[af].mutex);
}
EXPORT_SYMBOL(xt_unregister_match);

int
xt_register_matches(struct xt_match *match, unsigned int n)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < n; i++) {
		err = xt_register_match(&match[i]);
		if (err)
			goto err;
	}
	return err;

err:
	if (i > 0)
		xt_unregister_matches(match, i);
	return err;
}
EXPORT_SYMBOL(xt_register_matches);

void
xt_unregister_matches(struct xt_match *match, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		xt_unregister_match(&match[i]);
}
EXPORT_SYMBOL(xt_unregister_matches);


/*
 * These are weird, but module loading must not be done with mutex
 * held (since they will register), and we have to have a single
 * function to use try_then_request_module().
 */

/* Find match, grabs ref.  Returns ERR_PTR() on error. */
struct xt_match *xt_find_match(u8 af, const char *name, u8 revision)
{
	struct xt_match *m;
	int err = 0;

	if (mutex_lock_interruptible(&xt[af].mutex) != 0)
		return ERR_PTR(-EINTR);

	list_for_each_entry(m, &xt[af].match, list) {
		if (strcmp(m->name, name) == 0) {
			if (m->revision == revision) {
				if (try_module_get(m->me)) {
					mutex_unlock(&xt[af].mutex);
					return m;
				}
			} else
				err = -EPROTOTYPE; /* Found something. */
		}
	}
	mutex_unlock(&xt[af].mutex);

	if (af != NFPROTO_UNSPEC)
		/* Try searching again in the family-independent list */
		return xt_find_match(NFPROTO_UNSPEC, name, revision);

	return ERR_PTR(err);
}
EXPORT_SYMBOL(xt_find_match);

struct xt_match *
xt_request_find_match(uint8_t nfproto, const char *name, uint8_t revision)
{
	struct xt_match *match;

	match = try_then_request_module(xt_find_match(nfproto, name, revision),
					"%st_%s", xt_prefix[nfproto], name);
	return (match != NULL) ? match : ERR_PTR(-ENOENT);
}
EXPORT_SYMBOL_GPL(xt_request_find_match);

/* Find target, grabs ref.  Returns ERR_PTR() on error. */
struct xt_target *xt_find_target(u8 af, const char *name, u8 revision)
{
	struct xt_target *t;
	int err = 0;

	if (mutex_lock_interruptible(&xt[af].mutex) != 0)
		return ERR_PTR(-EINTR);

	list_for_each_entry(t, &xt[af].target, list) {
		if (strcmp(t->name, name) == 0) {
			if (t->revision == revision) {
				if (try_module_get(t->me)) {
					mutex_unlock(&xt[af].mutex);
					return t;
				}
			} else
				err = -EPROTOTYPE; /* Found something. */
		}
	}
	mutex_unlock(&xt[af].mutex);

	if (af != NFPROTO_UNSPEC)
		/* Try searching again in the family-independent list */
		return xt_find_target(NFPROTO_UNSPEC, name, revision);

	return ERR_PTR(err);
}
EXPORT_SYMBOL(xt_find_target);

struct xt_target *xt_request_find_target(u8 af, const char *name, u8 revision)
{
	struct xt_target *target;

	target = try_then_request_module(xt_find_target(af, name, revision),
					 "%st_%s", xt_prefix[af], name);
	return (target != NULL) ? target : ERR_PTR(-ENOENT);
}
EXPORT_SYMBOL_GPL(xt_request_find_target);

static int match_revfn(u8 af, const char *name, u8 revision, int *bestp)
{
	const struct xt_match *m;
	int have_rev = 0;

	list_for_each_entry(m, &xt[af].match, list) {
		if (strcmp(m->name, name) == 0) {
			if (m->revision > *bestp)
				*bestp = m->revision;
			if (m->revision == revision)
				have_rev = 1;
		}
	}

	if (af != NFPROTO_UNSPEC && !have_rev)
		return match_revfn(NFPROTO_UNSPEC, name, revision, bestp);

	return have_rev;
}

static int target_revfn(u8 af, const char *name, u8 revision, int *bestp)
{
	const struct xt_target *t;
	int have_rev = 0;

	list_for_each_entry(t, &xt[af].target, list) {
		if (strcmp(t->name, name) == 0) {
			if (t->revision > *bestp)
				*bestp = t->revision;
			if (t->revision == revision)
				have_rev = 1;
		}
	}

	if (af != NFPROTO_UNSPEC && !have_rev)
		return target_revfn(NFPROTO_UNSPEC, name, revision, bestp);

	return have_rev;
}

/* Returns true or false (if no such extension at all) */
int xt_find_revision(u8 af, const char *name, u8 revision, int target,
		     int *err)
{
	int have_rev, best = -1;

	if (mutex_lock_interruptible(&xt[af].mutex) != 0) {
		*err = -EINTR;
		return 1;
	}
	if (target == 1)
		have_rev = target_revfn(af, name, revision, &best);
	else
		have_rev = match_revfn(af, name, revision, &best);
	mutex_unlock(&xt[af].mutex);

	/* Nothing at all?  Return 0 to try loading module. */
	if (best == -1) {
		*err = -ENOENT;
		return 0;
	}

	*err = best;
	if (!have_rev)
		*err = -EPROTONOSUPPORT;
	return 1;
}
EXPORT_SYMBOL_GPL(xt_find_revision);

static char *textify_hooks(char *buf, size_t size, unsigned int mask)
{
	static const char *const names[] = {
		"PREROUTING", "INPUT", "FORWARD",
		"OUTPUT", "POSTROUTING", "BROUTING",
	};
	unsigned int i;
	char *p = buf;
	bool np = false;
	int res;

	*p = '\0';
	for (i = 0; i < ARRAY_SIZE(names); ++i) {
		if (!(mask & (1 << i)))
			continue;
		res = snprintf(p, size, "%s%s", np ? "/" : "", names[i]);
		if (res > 0) {
			size -= res;
			p += res;
		}
		np = true;
	}

	return buf;
}

int xt_check_match(struct xt_mtchk_param *par,
		   unsigned int size, u_int8_t proto, bool inv_proto)
{
	int ret;

	if (XT_ALIGN(par->match->matchsize) != size &&
	    par->match->matchsize != -1) {
		/*
		 * ebt_among is exempt from centralized matchsize checking
		 * because it uses a dynamic-size data set.
		 */
		pr_err("%s_tables: %s.%u match: invalid size "
		       "%u (kernel) != (user) %u\n",
		       xt_prefix[par->family], par->match->name,
		       par->match->revision,
		       XT_ALIGN(par->match->matchsize), size);
		return -EINVAL;
	}
	if (par->match->table != NULL &&
	    strcmp(par->match->table, par->table) != 0) {
		pr_err("%s_tables: %s match: only valid in %s table, not %s\n",
		       xt_prefix[par->family], par->match->name,
		       par->match->table, par->table);
		return -EINVAL;
	}
	if (par->match->hooks && (par->hook_mask & ~par->match->hooks) != 0) {
		char used[64], allow[64];

		pr_err("%s_tables: %s match: used from hooks %s, but only "
		       "valid from %s\n",
		       xt_prefix[par->family], par->match->name,
		       textify_hooks(used, sizeof(used), par->hook_mask),
		       textify_hooks(allow, sizeof(allow), par->match->hooks));
		return -EINVAL;
	}
	if (par->match->proto && (par->match->proto != proto || inv_proto)) {
		pr_err("%s_tables: %s match: only valid for protocol %u\n",
		       xt_prefix[par->family], par->match->name,
		       par->match->proto);
		return -EINVAL;
	}
	if (par->match->checkentry != NULL) {
		ret = par->match->checkentry(par);
		if (ret < 0)
			return ret;
		else if (ret > 0)
			/* Flag up potential errors. */
			return -EIO;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(xt_check_match);

#ifdef CONFIG_COMPAT
int xt_compat_add_offset(u_int8_t af, unsigned int offset, short delta)
{
	struct compat_delta *tmp;

	tmp = kmalloc(sizeof(struct compat_delta), GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	tmp->offset = offset;
	tmp->delta = delta;

	if (xt[af].compat_offsets) {
		tmp->next = xt[af].compat_offsets->next;
		xt[af].compat_offsets->next = tmp;
	} else {
		xt[af].compat_offsets = tmp;
		tmp->next = NULL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(xt_compat_add_offset);

void xt_compat_flush_offsets(u_int8_t af)
{
	struct compat_delta *tmp, *next;

	if (xt[af].compat_offsets) {
		for (tmp = xt[af].compat_offsets; tmp; tmp = next) {
			next = tmp->next;
			kfree(tmp);
		}
		xt[af].compat_offsets = NULL;
	}
}
EXPORT_SYMBOL_GPL(xt_compat_flush_offsets);

int xt_compat_calc_jump(u_int8_t af, unsigned int offset)
{
	struct compat_delta *tmp;
	int delta;

	for (tmp = xt[af].compat_offsets, delta = 0; tmp; tmp = tmp->next)
		if (tmp->offset < offset)
			delta += tmp->delta;
	return delta;
}
EXPORT_SYMBOL_GPL(xt_compat_calc_jump);

int xt_compat_match_offset(const struct xt_match *match)
{
	u_int16_t csize = match->compatsize ? : match->matchsize;
	return XT_ALIGN(match->matchsize) - COMPAT_XT_ALIGN(csize);
}
EXPORT_SYMBOL_GPL(xt_compat_match_offset);

int xt_compat_match_from_user(struct xt_entry_match *m, void **dstptr,
			      unsigned int *size)
{
	const struct xt_match *match = m->u.kernel.match;
	struct compat_xt_entry_match *cm = (struct compat_xt_entry_match *)m;
	int pad, off = xt_compat_match_offset(match);
	u_int16_t msize = cm->u.user.match_size;

	m = *dstptr;
	memcpy(m, cm, sizeof(*cm));
	if (match->compat_from_user)
		match->compat_from_user(m->data, cm->data);
	else
		memcpy(m->data, cm->data, msize - sizeof(*cm));
	pad = XT_ALIGN(match->matchsize) - match->matchsize;
	if (pad > 0)
		memset(m->data + match->matchsize, 0, pad);

	msize += off;
	m->u.user.match_size = msize;

	*size += off;
	*dstptr += msize;
	return 0;
}
EXPORT_SYMBOL_GPL(xt_compat_match_from_user);

int xt_compat_match_to_user(const struct xt_entry_match *m,
			    void __user **dstptr, unsigned int *size)
{
	const struct xt_match *match = m->u.kernel.match;
	struct compat_xt_entry_match __user *cm = *dstptr;
	int off = xt_compat_match_offset(match);
	u_int16_t msize = m->u.user.match_size - off;

	if (copy_to_user(cm, m, sizeof(*cm)) ||
	    put_user(msize, &cm->u.user.match_size) ||
	    copy_to_user(cm->u.user.name, m->u.kernel.match->name,
			 strlen(m->u.kernel.match->name) + 1))
		return -EFAULT;

	if (match->compat_to_user) {
		if (match->compat_to_user((void __user *)cm->data, m->data))
			return -EFAULT;
	} else {
		if (copy_to_user(cm->data, m->data, msize - sizeof(*cm)))
			return -EFAULT;
	}

	*size -= off;
	*dstptr += msize;
	return 0;
}
EXPORT_SYMBOL_GPL(xt_compat_match_to_user);
#endif /* CONFIG_COMPAT */

int xt_check_target(struct xt_tgchk_param *par,
		    unsigned int size, u_int8_t proto, bool inv_proto)
{
	int ret;

	if (XT_ALIGN(par->target->targetsize) != size) {
		pr_err("%s_tables: %s.%u target: invalid size "
		       "%u (kernel) != (user) %u\n",
		       xt_prefix[par->family], par->target->name,
		       par->target->revision,
		       XT_ALIGN(par->target->targetsize), size);
		return -EINVAL;
	}
	if (par->target->table != NULL &&
	    strcmp(par->target->table, par->table) != 0) {
		pr_err("%s_tables: %s target: only valid in %s table, not %s\n",
		       xt_prefix[par->family], par->target->name,
		       par->target->table, par->table);
		return -EINVAL;
	}
	if (par->target->hooks && (par->hook_mask & ~par->target->hooks) != 0) {
		char used[64], allow[64];

		pr_err("%s_tables: %s target: used from hooks %s, but only "
		       "usable from %s\n",
		       xt_prefix[par->family], par->target->name,
		       textify_hooks(used, sizeof(used), par->hook_mask),
		       textify_hooks(allow, sizeof(allow), par->target->hooks));
		return -EINVAL;
	}
	if (par->target->proto && (par->target->proto != proto || inv_proto)) {
		pr_err("%s_tables: %s target: only valid for protocol %u\n",
		       xt_prefix[par->family], par->target->name,
		       par->target->proto);
		return -EINVAL;
	}
	if (par->target->checkentry != NULL) {
		ret = par->target->checkentry(par);
		if (ret < 0)
			return ret;
		else if (ret > 0)
			/* Flag up potential errors. */
			return -EIO;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(xt_check_target);

#ifdef CONFIG_COMPAT
int xt_compat_target_offset(const struct xt_target *target)
{
	u_int16_t csize = target->compatsize ? : target->targetsize;
	return XT_ALIGN(target->targetsize) - COMPAT_XT_ALIGN(csize);
}
EXPORT_SYMBOL_GPL(xt_compat_target_offset);

void xt_compat_target_from_user(struct xt_entry_target *t, void **dstptr,
				unsigned int *size)
{
	const struct xt_target *target = t->u.kernel.target;
	struct compat_xt_entry_target *ct = (struct compat_xt_entry_target *)t;
	int pad, off = xt_compat_target_offset(target);
	u_int16_t tsize = ct->u.user.target_size;

	t = *dstptr;
	memcpy(t, ct, sizeof(*ct));
	if (target->compat_from_user)
		target->compat_from_user(t->data, ct->data);
	else
		memcpy(t->data, ct->data, tsize - sizeof(*ct));
	pad = XT_ALIGN(target->targetsize) - target->targetsize;
	if (pad > 0)
		memset(t->data + target->targetsize, 0, pad);

	tsize += off;
	t->u.user.target_size = tsize;

	*size += off;
	*dstptr += tsize;
}
EXPORT_SYMBOL_GPL(xt_compat_target_from_user);

int xt_compat_target_to_user(const struct xt_entry_target *t,
			     void __user **dstptr, unsigned int *size)
{
	const struct xt_target *target = t->u.kernel.target;
	struct compat_xt_entry_target __user *ct = *dstptr;
	int off = xt_compat_target_offset(target);
	u_int16_t tsize = t->u.user.target_size - off;

	if (copy_to_user(ct, t, sizeof(*ct)) ||
	    put_user(tsize, &ct->u.user.target_size) ||
	    copy_to_user(ct->u.user.name, t->u.kernel.target->name,
			 strlen(t->u.kernel.target->name) + 1))
		return -EFAULT;

	if (target->compat_to_user) {
		if (target->compat_to_user((void __user *)ct->data, t->data))
			return -EFAULT;
	} else {
		if (copy_to_user(ct->data, t->data, tsize - sizeof(*ct)))
			return -EFAULT;
	}

	*size -= off;
	*dstptr += tsize;
	return 0;
}
EXPORT_SYMBOL_GPL(xt_compat_target_to_user);
#endif

struct xt_table_info *xt_alloc_table_info(unsigned int size)
{
	struct xt_table_info *newinfo;
	int cpu;

	/* Pedantry: prevent them from hitting BUG() in vmalloc.c --RR */
	if ((SMP_ALIGN(size) >> PAGE_SHIFT) + 2 > totalram_pages)
		return NULL;

	newinfo = kzalloc(XT_TABLE_INFO_SZ, GFP_KERNEL);
	if (!newinfo)
		return NULL;

	newinfo->size = size;

	for_each_possible_cpu(cpu) {
		if (size <= PAGE_SIZE)
			newinfo->entries[cpu] = kmalloc_node(size,
							GFP_KERNEL,
							cpu_to_node(cpu));
		else
			newinfo->entries[cpu] = vmalloc_node(size,
							cpu_to_node(cpu));

		if (newinfo->entries[cpu] == NULL) {
			xt_free_table_info(newinfo);
			return NULL;
		}
	}

	return newinfo;
}
EXPORT_SYMBOL(xt_alloc_table_info);

void xt_free_table_info(struct xt_table_info *info)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		if (info->size <= PAGE_SIZE)
			kfree(info->entries[cpu]);
		else
			vfree(info->entries[cpu]);
	}

	if (info->jumpstack != NULL) {
		if (sizeof(void *) * info->stacksize > PAGE_SIZE) {
			for_each_possible_cpu(cpu)
				vfree(info->jumpstack[cpu]);
		} else {
			for_each_possible_cpu(cpu)
				kfree(info->jumpstack[cpu]);
		}
	}

	if (sizeof(void **) * nr_cpu_ids > PAGE_SIZE)
		vfree(info->jumpstack);
	else
		kfree(info->jumpstack);

	free_percpu(info->stackptr);

	kfree(info);
}
EXPORT_SYMBOL(xt_free_table_info);

/* Find table by name, grabs mutex & ref.  Returns ERR_PTR() on error. */
struct xt_table *xt_find_table_lock(struct net *net, u_int8_t af,
				    const char *name)
{
	struct xt_table *t;

	if (mutex_lock_interruptible(&xt[af].mutex) != 0)
		return ERR_PTR(-EINTR);

	list_for_each_entry(t, &net->xt.tables[af], list)
		if (strcmp(t->name, name) == 0 && try_module_get(t->me))
			return t;
	mutex_unlock(&xt[af].mutex);
	return NULL;
}
EXPORT_SYMBOL_GPL(xt_find_table_lock);

void xt_table_unlock(struct xt_table *table)
{
	mutex_unlock(&xt[table->af].mutex);
}
EXPORT_SYMBOL_GPL(xt_table_unlock);

#ifdef CONFIG_COMPAT
void xt_compat_lock(u_int8_t af)
{
	mutex_lock(&xt[af].compat_mutex);
}
EXPORT_SYMBOL_GPL(xt_compat_lock);

void xt_compat_unlock(u_int8_t af)
{
	mutex_unlock(&xt[af].compat_mutex);
}
EXPORT_SYMBOL_GPL(xt_compat_unlock);
#endif

DEFINE_PER_CPU(struct xt_info_lock, xt_info_locks);
EXPORT_PER_CPU_SYMBOL_GPL(xt_info_locks);

static int xt_jumpstack_alloc(struct xt_table_info *i)
{
	unsigned int size;
	int cpu;

	i->stackptr = alloc_percpu(unsigned int);
	if (i->stackptr == NULL)
		return -ENOMEM;

	size = sizeof(void **) * nr_cpu_ids;
	if (size > PAGE_SIZE)
		i->jumpstack = vmalloc(size);
	else
		i->jumpstack = kmalloc(size, GFP_KERNEL);
	if (i->jumpstack == NULL)
		return -ENOMEM;
	memset(i->jumpstack, 0, size);

	i->stacksize *= xt_jumpstack_multiplier;
	size = sizeof(void *) * i->stacksize;
	for_each_possible_cpu(cpu) {
		if (size > PAGE_SIZE)
			i->jumpstack[cpu] = vmalloc_node(size,
				cpu_to_node(cpu));
		else
			i->jumpstack[cpu] = kmalloc_node(size,
				GFP_KERNEL, cpu_to_node(cpu));
		if (i->jumpstack[cpu] == NULL)
			/*
			 * Freeing will be done later on by the callers. The
			 * chain is: xt_replace_table -> __do_replace ->
			 * do_replace -> xt_free_table_info.
			 */
			return -ENOMEM;
	}

	return 0;
}

struct xt_table_info *
xt_replace_table(struct xt_table *table,
	      unsigned int num_counters,
	      struct xt_table_info *newinfo,
	      int *error)
{
	struct xt_table_info *private;
	int ret;

	ret = xt_jumpstack_alloc(newinfo);
	if (ret < 0) {
		*error = ret;
		return NULL;
	}

	/* Do the substitution. */
	local_bh_disable();
	private = table->private;

	/* Check inside lock: is the old number correct? */
	if (num_counters != private->number) {
		pr_debug("num_counters != table->private->number (%u/%u)\n",
			 num_counters, private->number);
		local_bh_enable();
		*error = -EAGAIN;
		return NULL;
	}

	table->private = newinfo;
	newinfo->initial_entries = private->initial_entries;

	/*
	 * Even though table entries have now been swapped, other CPU's
	 * may still be using the old entries. This is okay, because
	 * resynchronization happens because of the locking done
	 * during the get_counters() routine.
	 */
	local_bh_enable();

	return private;
}
EXPORT_SYMBOL_GPL(xt_replace_table);

struct xt_table *xt_register_table(struct net *net,
				   const struct xt_table *input_table,
				   struct xt_table_info *bootstrap,
				   struct xt_table_info *newinfo)
{
	int ret;
	struct xt_table_info *private;
	struct xt_table *t, *table;

	/* Don't add one object to multiple lists. */
	table = kmemdup(input_table, sizeof(struct xt_table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto out;
	}

	ret = mutex_lock_interruptible(&xt[table->af].mutex);
	if (ret != 0)
		goto out_free;

	/* Don't autoload: we'd eat our tail... */
	list_for_each_entry(t, &net->xt.tables[table->af], list) {
		if (strcmp(t->name, table->name) == 0) {
			ret = -EEXIST;
			goto unlock;
		}
	}

	/* Simplifies replace_table code. */
	table->private = bootstrap;

	if (!xt_replace_table(table, 0, newinfo, &ret))
		goto unlock;

	private = table->private;
	pr_debug("table->private->number = %u\n", private->number);

	/* save number of initial entries */
	private->initial_entries = private->number;

	list_add(&table->list, &net->xt.tables[table->af]);
	mutex_unlock(&xt[table->af].mutex);
	return table;

 unlock:
	mutex_unlock(&xt[table->af].mutex);
out_free:
	kfree(table);
out:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(xt_register_table);

void *xt_unregister_table(struct xt_table *table)
{
	struct xt_table_info *private;

	mutex_lock(&xt[table->af].mutex);
	private = table->private;
	list_del(&table->list);
	mutex_unlock(&xt[table->af].mutex);
	kfree(table);

	return private;
}
EXPORT_SYMBOL_GPL(xt_unregister_table);

#ifdef CONFIG_PROC_FS
struct xt_names_priv {
	struct seq_net_private p;
	u_int8_t af;
};
static void *xt_table_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct xt_names_priv *priv = seq->private;
	struct net *net = seq_file_net(seq);
	u_int8_t af = priv->af;

	mutex_lock(&xt[af].mutex);
	return seq_list_start(&net->xt.tables[af], *pos);
}

static void *xt_table_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct xt_names_priv *priv = seq->private;
	struct net *net = seq_file_net(seq);
	u_int8_t af = priv->af;

	return seq_list_next(v, &net->xt.tables[af], pos);
}

static void xt_table_seq_stop(struct seq_file *seq, void *v)
{
	struct xt_names_priv *priv = seq->private;
	u_int8_t af = priv->af;

	mutex_unlock(&xt[af].mutex);
}

static int xt_table_seq_show(struct seq_file *seq, void *v)
{
	struct xt_table *table = list_entry(v, struct xt_table, list);

	if (strlen(table->name))
		return seq_printf(seq, "%s\n", table->name);
	else
		return 0;
}

static const struct seq_operations xt_table_seq_ops = {
	.start	= xt_table_seq_start,
	.next	= xt_table_seq_next,
	.stop	= xt_table_seq_stop,
	.show	= xt_table_seq_show,
};

static int xt_table_open(struct inode *inode, struct file *file)
{
	int ret;
	struct xt_names_priv *priv;

	ret = seq_open_net(inode, file, &xt_table_seq_ops,
			   sizeof(struct xt_names_priv));
	if (!ret) {
		priv = ((struct seq_file *)file->private_data)->private;
		priv->af = (unsigned long)PDE(inode)->data;
	}
	return ret;
}

static const struct file_operations xt_table_ops = {
	.owner	 = THIS_MODULE,
	.open	 = xt_table_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = seq_release_net,
};

/*
 * Traverse state for ip{,6}_{tables,matches} for helping crossing
 * the multi-AF mutexes.
 */
struct nf_mttg_trav {
	struct list_head *head, *curr;
	uint8_t class, nfproto;
};

enum {
	MTTG_TRAV_INIT,
	MTTG_TRAV_NFP_UNSPEC,
	MTTG_TRAV_NFP_SPEC,
	MTTG_TRAV_DONE,
};

static void *xt_mttg_seq_next(struct seq_file *seq, void *v, loff_t *ppos,
    bool is_target)
{
	static const uint8_t next_class[] = {
		[MTTG_TRAV_NFP_UNSPEC] = MTTG_TRAV_NFP_SPEC,
		[MTTG_TRAV_NFP_SPEC]   = MTTG_TRAV_DONE,
	};
	struct nf_mttg_trav *trav = seq->private;

	switch (trav->class) {
	case MTTG_TRAV_INIT:
		trav->class = MTTG_TRAV_NFP_UNSPEC;
		mutex_lock(&xt[NFPROTO_UNSPEC].mutex);
		trav->head = trav->curr = is_target ?
			&xt[NFPROTO_UNSPEC].target : &xt[NFPROTO_UNSPEC].match;
 		break;
	case MTTG_TRAV_NFP_UNSPEC:
		trav->curr = trav->curr->next;
		if (trav->curr != trav->head)
			break;
		mutex_unlock(&xt[NFPROTO_UNSPEC].mutex);
		mutex_lock(&xt[trav->nfproto].mutex);
		trav->head = trav->curr = is_target ?
			&xt[trav->nfproto].target : &xt[trav->nfproto].match;
		trav->class = next_class[trav->class];
		break;
	case MTTG_TRAV_NFP_SPEC:
		trav->curr = trav->curr->next;
		if (trav->curr != trav->head)
			break;
		/* fallthru, _stop will unlock */
	default:
		return NULL;
	}

	if (ppos != NULL)
		++*ppos;
	return trav;
}

static void *xt_mttg_seq_start(struct seq_file *seq, loff_t *pos,
    bool is_target)
{
	struct nf_mttg_trav *trav = seq->private;
	unsigned int j;

	trav->class = MTTG_TRAV_INIT;
	for (j = 0; j < *pos; ++j)
		if (xt_mttg_seq_next(seq, NULL, NULL, is_target) == NULL)
			return NULL;
	return trav;
}

static void xt_mttg_seq_stop(struct seq_file *seq, void *v)
{
	struct nf_mttg_trav *trav = seq->private;

	switch (trav->class) {
	case MTTG_TRAV_NFP_UNSPEC:
		mutex_unlock(&xt[NFPROTO_UNSPEC].mutex);
		break;
	case MTTG_TRAV_NFP_SPEC:
		mutex_unlock(&xt[trav->nfproto].mutex);
		break;
	}
}

static void *xt_match_seq_start(struct seq_file *seq, loff_t *pos)
{
	return xt_mttg_seq_start(seq, pos, false);
}

static void *xt_match_seq_next(struct seq_file *seq, void *v, loff_t *ppos)
{
	return xt_mttg_seq_next(seq, v, ppos, false);
}

static int xt_match_seq_show(struct seq_file *seq, void *v)
{
	const struct nf_mttg_trav *trav = seq->private;
	const struct xt_match *match;

	switch (trav->class) {
	case MTTG_TRAV_NFP_UNSPEC:
	case MTTG_TRAV_NFP_SPEC:
		if (trav->curr == trav->head)
			return 0;
		match = list_entry(trav->curr, struct xt_match, list);
		return (*match->name == '\0') ? 0 :
		       seq_printf(seq, "%s\n", match->name);
	}
	return 0;
}

static const struct seq_operations xt_match_seq_ops = {
	.start	= xt_match_seq_start,
	.next	= xt_match_seq_next,
	.stop	= xt_mttg_seq_stop,
	.show	= xt_match_seq_show,
};

static int xt_match_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq;
	struct nf_mttg_trav *trav;
	int ret;

	trav = kmalloc(sizeof(*trav), GFP_KERNEL);
	if (trav == NULL)
		return -ENOMEM;

	ret = seq_open(file, &xt_match_seq_ops);
	if (ret < 0) {
		kfree(trav);
		return ret;
	}

	seq = file->private_data;
	seq->private = trav;
	trav->nfproto = (unsigned long)PDE(inode)->data;
	return 0;
}

static const struct file_operations xt_match_ops = {
	.owner	 = THIS_MODULE,
	.open	 = xt_match_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = seq_release_private,
};

static void *xt_target_seq_start(struct seq_file *seq, loff_t *pos)
{
	return xt_mttg_seq_start(seq, pos, true);
}

static void *xt_target_seq_next(struct seq_file *seq, void *v, loff_t *ppos)
{
	return xt_mttg_seq_next(seq, v, ppos, true);
}

static int xt_target_seq_show(struct seq_file *seq, void *v)
{
	const struct nf_mttg_trav *trav = seq->private;
	const struct xt_target *target;

	switch (trav->class) {
	case MTTG_TRAV_NFP_UNSPEC:
	case MTTG_TRAV_NFP_SPEC:
		if (trav->curr == trav->head)
			return 0;
		target = list_entry(trav->curr, struct xt_target, list);
		return (*target->name == '\0') ? 0 :
		       seq_printf(seq, "%s\n", target->name);
	}
	return 0;
}

static const struct seq_operations xt_target_seq_ops = {
	.start	= xt_target_seq_start,
	.next	= xt_target_seq_next,
	.stop	= xt_mttg_seq_stop,
	.show	= xt_target_seq_show,
};

static int xt_target_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq;
	struct nf_mttg_trav *trav;
	int ret;

	trav = kmalloc(sizeof(*trav), GFP_KERNEL);
	if (trav == NULL)
		return -ENOMEM;

	ret = seq_open(file, &xt_target_seq_ops);
	if (ret < 0) {
		kfree(trav);
		return ret;
	}

	seq = file->private_data;
	seq->private = trav;
	trav->nfproto = (unsigned long)PDE(inode)->data;
	return 0;
}

static const struct file_operations xt_target_ops = {
	.owner	 = THIS_MODULE,
	.open	 = xt_target_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = seq_release_private,
};

#define FORMAT_TABLES	"_tables_names"
#define	FORMAT_MATCHES	"_tables_matches"
#define FORMAT_TARGETS 	"_tables_targets"

#endif /* CONFIG_PROC_FS */

/**
 * xt_hook_link - set up hooks for a new table
 * @table:	table with metadata needed to set up hooks
 * @fn:		Hook function
 *
 * This function will take care of creating and registering the necessary
 * Netfilter hooks for XT tables.
 */
struct nf_hook_ops *xt_hook_link(const struct xt_table *table, nf_hookfn *fn)
{
	unsigned int hook_mask = table->valid_hooks;
	uint8_t i, num_hooks = hweight32(hook_mask);
	uint8_t hooknum;
	struct nf_hook_ops *ops;
	int ret;

	ops = kmalloc(sizeof(*ops) * num_hooks, GFP_KERNEL);
	if (ops == NULL)
		return ERR_PTR(-ENOMEM);

	for (i = 0, hooknum = 0; i < num_hooks && hook_mask != 0;
	     hook_mask >>= 1, ++hooknum) {
		if (!(hook_mask & 1))
			continue;
		ops[i].hook     = fn;
		ops[i].owner    = table->me;
		ops[i].pf       = table->af;
		ops[i].hooknum  = hooknum;
		ops[i].priority = table->priority;
		++i;
	}

	ret = nf_register_hooks(ops, num_hooks);
	if (ret < 0) {
		kfree(ops);
		return ERR_PTR(ret);
	}

	return ops;
}
EXPORT_SYMBOL_GPL(xt_hook_link);

/**
 * xt_hook_unlink - remove hooks for a table
 * @ops:	nf_hook_ops array as returned by nf_hook_link
 * @hook_mask:	the very same mask that was passed to nf_hook_link
 */
void xt_hook_unlink(const struct xt_table *table, struct nf_hook_ops *ops)
{
	nf_unregister_hooks(ops, hweight32(table->valid_hooks));
	kfree(ops);
}
EXPORT_SYMBOL_GPL(xt_hook_unlink);

int xt_proto_init(struct net *net, u_int8_t af)
{
#ifdef CONFIG_PROC_FS
	char buf[XT_FUNCTION_MAXNAMELEN];
	struct proc_dir_entry *proc;
#endif

	if (af >= ARRAY_SIZE(xt_prefix))
		return -EINVAL;


#ifdef CONFIG_PROC_FS
	strlcpy(buf, xt_prefix[af], sizeof(buf));
	strlcat(buf, FORMAT_TABLES, sizeof(buf));
	proc = proc_create_data(buf, 0440, net->proc_net, &xt_table_ops,
				(void *)(unsigned long)af);
	if (!proc)
		goto out;

	strlcpy(buf, xt_prefix[af], sizeof(buf));
	strlcat(buf, FORMAT_MATCHES, sizeof(buf));
	proc = proc_create_data(buf, 0440, net->proc_net, &xt_match_ops,
				(void *)(unsigned long)af);
	if (!proc)
		goto out_remove_tables;

	strlcpy(buf, xt_prefix[af], sizeof(buf));
	strlcat(buf, FORMAT_TARGETS, sizeof(buf));
	proc = proc_create_data(buf, 0440, net->proc_net, &xt_target_ops,
				(void *)(unsigned long)af);
	if (!proc)
		goto out_remove_matches;
#endif

	return 0;

#ifdef CONFIG_PROC_FS
out_remove_matches:
	strlcpy(buf, xt_prefix[af], sizeof(buf));
	strlcat(buf, FORMAT_MATCHES, sizeof(buf));
	proc_net_remove(net, buf);

out_remove_tables:
	strlcpy(buf, xt_prefix[af], sizeof(buf));
	strlcat(buf, FORMAT_TABLES, sizeof(buf));
	proc_net_remove(net, buf);
out:
	return -1;
#endif
}
EXPORT_SYMBOL_GPL(xt_proto_init);

void xt_proto_fini(struct net *net, u_int8_t af)
{
#ifdef CONFIG_PROC_FS
	char buf[XT_FUNCTION_MAXNAMELEN];

	strlcpy(buf, xt_prefix[af], sizeof(buf));
	strlcat(buf, FORMAT_TABLES, sizeof(buf));
	proc_net_remove(net, buf);

	strlcpy(buf, xt_prefix[af], sizeof(buf));
	strlcat(buf, FORMAT_TARGETS, sizeof(buf));
	proc_net_remove(net, buf);

	strlcpy(buf, xt_prefix[af], sizeof(buf));
	strlcat(buf, FORMAT_MATCHES, sizeof(buf));
	proc_net_remove(net, buf);
#endif /*CONFIG_PROC_FS*/
}
EXPORT_SYMBOL_GPL(xt_proto_fini);

static int __net_init xt_net_init(struct net *net)
{
	int i;

	for (i = 0; i < NFPROTO_NUMPROTO; i++)
		INIT_LIST_HEAD(&net->xt.tables[i]);
	return 0;
}

static struct pernet_operations xt_net_ops = {
	.init = xt_net_init,
};

static int __init xt_init(void)
{
	unsigned int i;
	int rv;

	for_each_possible_cpu(i) {
		struct xt_info_lock *lock = &per_cpu(xt_info_locks, i);
		spin_lock_init(&lock->lock);
		lock->readers = 0;
	}

	xt = kmalloc(sizeof(struct xt_af) * NFPROTO_NUMPROTO, GFP_KERNEL);
	if (!xt)
		return -ENOMEM;

	for (i = 0; i < NFPROTO_NUMPROTO; i++) {
		mutex_init(&xt[i].mutex);
#ifdef CONFIG_COMPAT
		mutex_init(&xt[i].compat_mutex);
		xt[i].compat_offsets = NULL;
#endif
		INIT_LIST_HEAD(&xt[i].target);
		INIT_LIST_HEAD(&xt[i].match);
	}
	rv = register_pernet_subsys(&xt_net_ops);
	if (rv < 0)
		kfree(xt);
	return rv;
}

static void __exit xt_fini(void)
{
	unregister_pernet_subsys(&xt_net_ops);
	kfree(xt);
}

module_init(xt_init);
module_exit(xt_fini);
