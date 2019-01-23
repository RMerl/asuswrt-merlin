/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>				/* assert */
#include <errno.h>				/* errno */
#include <net/ethernet.h>			/* ETH_ALEN */
#include <netinet/in.h>				/* struct in6_addr */
#include <sys/socket.h>				/* AF_ */
#include <stdlib.h>				/* malloc, free */
#include <stdio.h>				/* FIXME: debug */
#include <libmnl/libmnl.h>			/* MNL_ALIGN */

#include <libipset/debug.h>			/* D() */
#include <libipset/data.h>			/* ipset_data_* */
#include <libipset/session.h>			/* ipset_cmd */
#include <libipset/utils.h>			/* STREQ */
#include <libipset/types.h>			/* prototypes */

#ifdef ENABLE_SETTYPE_MODULES
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#endif

/* Userspace cache of sets which exists in the kernel */

struct ipset {
	char name[IPSET_MAXNAMELEN];		/* set name */
	const struct ipset_type *type;		/* set type */
	uint8_t family;				/* family */
	struct ipset *next;
};

static struct ipset_type *typelist;		/* registered set types */
static struct ipset *setlist;			/* cached sets */

/**
 * ipset_cache_add - add a set to the cache
 * @name: set name
 * @type: set type structure
 *
 * Add the named set to the internal cache with the specified
 * set type. The set name must be unique.
 *
 * Returns 0 on success or a negative error code.
 */
int
ipset_cache_add(const char *name, const struct ipset_type *type,
		uint8_t family)
{
	struct ipset *s, *n;

	assert(name);
	assert(type);

	n = malloc(sizeof(*n));
	if (n == NULL)
		return -ENOMEM;

	ipset_strlcpy(n->name, name, IPSET_MAXNAMELEN);
	n->type = type;
	n->family = family;
	n->next = NULL;

	if (setlist == NULL) {
		setlist = n;
		return 0;
	}
	for (s = setlist; s->next != NULL; s = s->next) {
		if (STREQ(name, s->name)) {
			free(n);
			return -EEXIST;
		}
	}
	s->next = n;

	return 0;
}

/**
 * ipset_cache_del - delete set from the cache
 * @name: set name
 *
 * Delete the named set from the internal cache. If NULL is
 * specified as setname, the whole cache is emptied.
 *
 * Returns 0 on success or a negative error code.
 */
int
ipset_cache_del(const char *name)
{
	struct ipset *s, *match = NULL, *prev = NULL;

	if (!name) {
		for (s = setlist; s != NULL; ) {
			prev = s;
			s = s->next;
			free(prev);
		}
		setlist = NULL;
		return 0;
	}
	for (s = setlist; s != NULL && match == NULL; s = s->next) {
		if (STREQ(s->name, name)) {
			match = s;
			if (prev == NULL)
				setlist = match->next;
			else
				prev->next = match->next;
		}
		prev = s;
	}
	if (match == NULL)
		return -EEXIST;

	free(match);
	return 0;
}

/**
 * ipset_cache_rename - rename a set in the cache
 * @from: the set to rename
 * @to: the new name of the set
 *
 * Rename the given set in the cache.
 *
 * Returns 0 on success or a negative error code.
 */
int
ipset_cache_rename(const char *from, const char *to)
{
	struct ipset *s;

	assert(from);
	assert(to);

	for (s = setlist; s != NULL; s = s->next) {
		if (STREQ(s->name, from)) {
			ipset_strlcpy(s->name, to, IPSET_MAXNAMELEN);
			return 0;
		}
	}
	return -EEXIST;
}

/**
 * ipset_cache_swap - swap two sets in the cache
 * @from: the first set
 * @to: the second set
 *
 * Swap two existing sets in the cache.
 *
 * Returns 0 on success or a negative error code.
 */
int
ipset_cache_swap(const char *from, const char *to)
{
	struct ipset *s, *a = NULL, *b = NULL;

	assert(from);
	assert(to);

	for (s = setlist; s != NULL && (a == NULL || b == NULL); s = s->next) {
		if (a == NULL && STREQ(s->name, from))
			a = s;
		if (b == NULL && STREQ(s->name, to))
			b = s;
	}
	if (a != NULL && b != NULL) {
		ipset_strlcpy(a->name, to, IPSET_MAXNAMELEN);
		ipset_strlcpy(b->name, from, IPSET_MAXNAMELEN);
		return 0;
	}

	return -EEXIST;
}

#define MATCH_FAMILY(type, f)	\
	(f == NFPROTO_UNSPEC || type->family == f || \
	 type->family == NFPROTO_IPSET_IPV46)

bool
ipset_match_typename(const char *name, const struct ipset_type *type)
{
	const char * const *alias = type->alias;

	if (STREQ(name, type->name))
		return true;

	while (alias[0]) {
		if (STREQ(name, alias[0]))
			return true;
		alias++;
	}
	return false;
}

static inline const struct ipset_type *
create_type_get(struct ipset_session *session)
{
	struct ipset_type *t, *match = NULL;
	struct ipset_data *data;
	const char *typename;
	uint8_t family, tmin = 0, tmax = 0;
	uint8_t kmin, kmax;
	int ret;
	bool ignore_family = false;

	data = ipset_session_data(session);
	assert(data);
	typename = ipset_data_get(data, IPSET_OPT_TYPENAME);
	assert(typename);
	family = ipset_data_family(data);

	/* Check registered types in userspace */
	for (t = typelist; t != NULL; t = t->next) {
		/* Skip revisions which are unsupported by the kernel */
		if (t->kernel_check == IPSET_KERNEL_MISMATCH)
			continue;
		if (ipset_match_typename(typename, t)
		    && MATCH_FAMILY(t, family)) {
			if (match == NULL) {
				match = t;
				tmin = tmax = t->revision;
			} else if (t->family == match->family)
				tmin = t->revision;
		}
	}
	if (!match)
		return ipset_errptr(session,
				    "Syntax error: unknown settype %s",
				    typename);

	/* Family is unspecified yet: set from matching set type */
	if (family == NFPROTO_UNSPEC && match->family != NFPROTO_UNSPEC) {
		family = match->family == NFPROTO_IPSET_IPV46 ?
			 NFPROTO_IPV4 : match->family;
		ipset_data_set(data, IPSET_OPT_FAMILY, &family);
		if (match->family == NFPROTO_IPSET_IPV46)
			ignore_family = true;
	}

	if (match->kernel_check == IPSET_KERNEL_OK)
		goto found;

	/* Check kernel */
	ret = ipset_cmd(session, IPSET_CMD_TYPE, 0);
	if (ret != 0)
		return NULL;

	kmin = kmax = *(const uint8_t *)ipset_data_get(data,
						IPSET_OPT_REVISION);
	if (ipset_data_test(data, IPSET_OPT_REVISION_MIN))
		kmin = *(const uint8_t *)ipset_data_get(data,
						IPSET_OPT_REVISION_MIN);

	if (MAX(tmin, kmin) > MIN(tmax, kmax)) {
		if (kmin > tmax)
			return ipset_errptr(session,
				"Kernel supports %s type, family %s "
				"with minimal revision %u while ipset program "
				"with maximal revision %u.\n"
				"You need to upgrade your ipset program.",
				typename,
				family == NFPROTO_IPV4 ? "INET" :
				family == NFPROTO_IPV6 ? "INET6" : "UNSPEC",
				kmin, tmax);
		else
			return ipset_errptr(session,
				"Kernel supports %s type, family %s "
				"with maximal revision %u while ipset program "
				"with minimal revision %u.\n"
				"You need to upgrade your kernel.",
				typename,
				family == NFPROTO_IPV4 ? "INET" :
				family == NFPROTO_IPV6 ? "INET6" : "UNSPEC",
				kmax, tmin);
	}

	/* Disable unsupported revisions */
	for (match = NULL, t = typelist; t != NULL; t = t->next) {
		/* Skip revisions which are unsupported by the kernel */
		if (t->kernel_check == IPSET_KERNEL_MISMATCH)
			continue;
		if (ipset_match_typename(typename, t)
		    && MATCH_FAMILY(t, family)) {
			if (t->revision < kmin || t->revision > kmax)
				t->kernel_check = IPSET_KERNEL_MISMATCH;
			else if (match == NULL)
				match = t;
		}
	}
	match->kernel_check = IPSET_KERNEL_OK;
found:
	ipset_data_set(data, IPSET_OPT_TYPE, match);

	if (ignore_family) {
		/* Overload ignored flag */
		D("set ignored flag to FAMILY");
		ipset_data_ignored(data, IPSET_OPT_FAMILY);
	}
	return match;
}

#define set_family_and_type(data, match, family) do {		\
	if (family == NFPROTO_UNSPEC && match->family != NFPROTO_UNSPEC) \
		family = match->family == NFPROTO_IPSET_IPV46 ? \
			 NFPROTO_IPV4 : match->family;\
	ipset_data_set(data, IPSET_OPT_FAMILY, &family);	\
	ipset_data_set(data, IPSET_OPT_TYPE, match);		\
} while (0)


static inline const struct ipset_type *
adt_type_get(struct ipset_session *session)
{
	struct ipset_data *data;
	struct ipset *s;
	struct ipset_type *t;
	const struct ipset_type *match;
	const char *setname, *typename;
	const uint8_t *revision;
	uint8_t family = NFPROTO_UNSPEC;
	int ret;

	data = ipset_session_data(session);
	assert(data);
	setname = ipset_data_setname(data);
	assert(setname);

	/* Check existing sets in cache */
	for (s = setlist; s != NULL; s = s->next) {
		if (STREQ(setname, s->name)) {
			ipset_data_set(data, IPSET_OPT_FAMILY, &s->family);
			ipset_data_set(data, IPSET_OPT_TYPE, s->type);
			return s->type;
		}
	}

	/* Check kernel */
	ret = ipset_cmd(session, IPSET_CMD_HEADER, 0);
	if (ret != 0)
		return NULL;

	typename = ipset_data_get(data, IPSET_OPT_TYPENAME);
	revision = ipset_data_get(data, IPSET_OPT_REVISION);
	family = ipset_data_family(data);

	/* Check registered types */
	for (t = typelist, match = NULL;
	     t != NULL && match == NULL; t = t->next) {
		if (t->kernel_check == IPSET_KERNEL_MISMATCH)
			continue;
		if (STREQ(typename, t->name)
		    && MATCH_FAMILY(t, family)
		    && *revision == t->revision) {
			t->kernel_check = IPSET_KERNEL_OK;
			match = t;
		}
	}
	if (!match)
		return ipset_errptr(session,
				    "Kernel-library incompatibility: "
				    "set %s in kernel has got settype %s "
				    "with family %s and revision %u while "
				    "ipset library does not support the "
				    "settype with that family and revision.",
				    setname, typename,
				    family == NFPROTO_IPV4 ? "inet" :
				    family == NFPROTO_IPV6 ? "inet6" : "unspec",
				    *revision);

	set_family_and_type(data, match, family);

	return match;
}

/**
 * ipset_type_get - get a set type from the kernel
 * @session: session structure
 * @cmd: the command which needs the set type
 *
 * Build up and send a private message to the kernel in order to
 * get the set type. When creating the set, we send the typename
 * and family and get the supported revisions of the given set type.
 * When adding/deleting/testing an entry, we send the setname and
 * receive the typename, family and revision.
 *
 * Returns the set type for success and NULL for failure.
 */
const struct ipset_type *
ipset_type_get(struct ipset_session *session, enum ipset_cmd cmd)
{
	assert(session);

	switch (cmd) {
	case IPSET_CMD_CREATE:
		return ipset_data_test(ipset_session_data(session),
				       IPSET_OPT_TYPE)
			? ipset_data_get(ipset_session_data(session),
					 IPSET_OPT_TYPE)
			: create_type_get(session);
	case IPSET_CMD_ADD:
	case IPSET_CMD_DEL:
	case IPSET_CMD_TEST:
		return adt_type_get(session);
	default:
		break;
	}

	assert(cmd == IPSET_CMD_NONE);
	return NULL;
}

/**
 * ipset_type_check - check the set type received from kernel
 * @session: session structure
 *
 * Check the set type received from the kernel (typename, revision,
 * family) against the userspace types looking for a matching type.
 *
 * Returns the set type for success and NULL for failure.
 */
const struct ipset_type *
ipset_type_check(struct ipset_session *session)
{
	const struct ipset_type *t, *match = NULL;
	struct ipset_data *data;
	const char *typename;
	uint8_t family = NFPROTO_UNSPEC, revision;

	assert(session);
	data = ipset_session_data(session);
	assert(data);

	typename = ipset_data_get(data, IPSET_OPT_TYPENAME);
	family = ipset_data_family(data);
	revision = *(const uint8_t *) ipset_data_get(data, IPSET_OPT_REVISION);

	/* Check registered types */
	for (t = typelist; t != NULL && match == NULL; t = t->next) {
		if (t->kernel_check == IPSET_KERNEL_MISMATCH)
			continue;
		if (ipset_match_typename(typename, t)
		    && MATCH_FAMILY(t, family)
		    && t->revision == revision)
			match = t;
	}
	if (!match)
		return ipset_errptr(session,
			     "Kernel and userspace incompatible: "
			     "settype %s with revision %u not supported "
			     "by userspace.", typename, revision);

	set_family_and_type(data, match, family);

	return match;
}

/**
 * ipset_type_add - add (register) a userspace set type
 * @type: pointer to the set type structure
 *
 * Add the given set type to the type list. The types
 * are added sorted, in descending revision number.
 *
 * Returns 0 on success or a negative error code.
 */
int
ipset_type_add(struct ipset_type *type)
{
	struct ipset_type *t, *prev;

	assert(type);

	if (strlen(type->name) > IPSET_MAXNAMELEN - 1)
		return -EINVAL;

	/* Add to the list: higher revision numbers first */
	for (t = typelist, prev = NULL; t != NULL; t = t->next) {
		if (STREQ(t->name, type->name)) {
			if (t->revision == type->revision)
				return -EEXIST;
			else if (t->revision < type->revision) {
				type->next = t;
				if (prev)
					prev->next = type;
				else
					typelist = type;
				return 0;
			}
		}
		if (t->next != NULL && STREQ(t->next->name, type->name)) {
			if (t->next->revision == type->revision)
				return -EEXIST;
			else if (t->next->revision < type->revision) {
				type->next = t->next;
				t->next = type;
				return 0;
			}
		}
		prev = t;
	}
	type->next = typelist;
	typelist = type;
	return 0;
}

/**
 * ipset_typename_resolve - resolve typename alias
 * @str: typename or alias
 *
 * Check the typenames (and aliases) and return the
 * preferred name of the set type.
 *
 * Returns the name of the matching set type or NULL.
 */
const char *
ipset_typename_resolve(const char *str)
{
	const struct ipset_type *t;

	for (t = typelist; t != NULL; t = t->next)
		if (ipset_match_typename(str, t))
			return t->name;
	return NULL;
}

/**
 * ipset_types - return the list of the set types
 *
 * The types can be unchecked with respect of the running kernel.
 * Only useful for type specific help.
 *
 * Returns the list of the set types.
 */
const struct ipset_type *
ipset_types(void)
{
	return typelist;
}

/**
 * ipset_cache_init - initialize set cache
 *
 * Initialize the set cache in userspace.
 *
 * Returns 0 on success or a negative error code.
 */
int
ipset_cache_init(void)
{
	return 0;
}

/**
 * ipset_cache_fini - release the set cache
 *
 * Release the set cache.
 */
void
ipset_cache_fini(void)
{
	struct ipset *set;

	while (setlist) {
		set = setlist;
		setlist = setlist->next;
		free(set);
	}
}

extern void ipset_types_init(void);

/**
 * ipset_load_types - load known set types
 *
 * Load in (register) all known set types for the system
 */
void
ipset_load_types(void)
{
#ifdef ENABLE_SETTYPE_MODULES
	const char *dir  = IPSET_MODSDIR;
	const char *next = NULL;
	char   path[256];
	char   file[256];
	struct dirent **list = NULL;
	int    n;
	int    len;
#endif

	if (typelist != NULL)
		return;

	/* Initialize static types */
	ipset_types_init();

#ifdef ENABLE_SETTYPE_MODULES
	/* Initialize dynamic types */
	do {
		next = strchr(dir, ':');
		if (next == NULL)
			next = dir + strlen(dir);

		len = snprintf(path, sizeof(path), "%.*s",
			       (unsigned int)(next - dir), dir);

		if (len >= (int)sizeof(path) || len < 0)
			continue;

		n = scandir(path, &list, NULL, alphasort);
		if (n < 0)
			continue;

		while (n--) {
			if (strstr(list[n]->d_name, ".so") == NULL)
				goto nextf;

			len = snprintf(file, sizeof(file), "%s/%s",
				       path, list[n]->d_name);
			if (len >= (int)sizeof(file) || len < (int)0)
				goto nextf;

			if (dlopen(file, RTLD_NOW) == NULL)
				fprintf(stderr, "%s: %s\n", file, dlerror());

nextf:
			free(list[n]);
		}

		free(list);

		dir = next + 1;
	} while (*next != '\0');
#endif /* ENABLE_SETTYPE_MODULES */
}
