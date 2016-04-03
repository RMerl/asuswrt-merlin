/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_TYPES_H
#define LIBIPSET_TYPES_H

#include <stddef.h>				/* NULL */
#include <stdint.h>				/* uintxx_t */

#include <libipset/data.h>			/* enum ipset_opt */
#include <libipset/parse.h>			/* ipset_parsefn */
#include <libipset/print.h>			/* ipset_printfn */
#include <libipset/linux_ip_set.h>		/* IPSET_MAXNAMELEN */
#include <libipset/nfproto.h>			/* for NFPROTO_ */

/* Family rules:
 * - NFPROTO_UNSPEC:		type is family-neutral
 * - NFPROTO_IPV4:		type supports IPv4 only
 * - NFPROTO_IPV6:		type supports IPv6 only
 * Special (userspace) ipset-only extra value:
 * - NFPROTO_IPSET_IPV46:	type supports both IPv4 and IPv6
 */
enum {
	NFPROTO_IPSET_IPV46 = 255,
};

/* The maximal type dimension userspace supports */
#define IPSET_DIM_UMAX		3

/* Parser options */
enum {
	IPSET_NO_ARG = -1,
	IPSET_OPTIONAL_ARG,
	IPSET_MANDATORY_ARG,
	IPSET_MANDATORY_ARG2,
};

struct ipset_session;

/* Parse and print type-specific arguments */
struct ipset_arg {
	const char *name[2];		/* option names */
	int has_arg;			/* mandatory/optional/no arg */
	enum ipset_opt opt;		/* argumentum type */
	ipset_parsefn parse;		/* parser function */
	ipset_printfn print;		/* printing function */
};

/* Type check against the kernel */
enum {
	IPSET_KERNEL_MISMATCH = -1,
	IPSET_KERNEL_CHECK_NEEDED,
	IPSET_KERNEL_OK,
};

/* How element parts are parsed */
struct ipset_elem {
	ipset_parsefn parse;			/* elem parser function */
	ipset_printfn print;			/* elem print function */
	enum ipset_opt opt;			/* elem option */
};

/* The set types in userspace
 * we could collapse 'args' and 'mandatory' to two-element lists
 * but for the readability the full list is supported.
  */
struct ipset_type {
	const char *name;
	uint8_t revision;			/* revision number */
	uint8_t family;				/* supported family */
	uint8_t dimension;			/* elem dimension */
	int8_t kernel_check;			/* kernel check */
	bool last_elem_optional;		/* last element optional */
	struct ipset_elem elem[IPSET_DIM_UMAX];	/* parse elem */
	ipset_parsefn compat_parse_elem;	/* compatibility parser */
	const struct ipset_arg *args[IPSET_CADT_MAX]; /* create/ADT args besides elem */
	uint64_t mandatory[IPSET_CADT_MAX];	/* create/ADT mandatory flags */
	uint64_t full[IPSET_CADT_MAX];		/* full args flags */
	const char *usage;			/* terse usage */
	void (*usagefn)(void);			/* additional usage */
	const char *description;		/* short revision description */
	struct ipset_type *next;
	const char *alias[];			/* name alias(es) */
};

#ifdef __cplusplus
extern "C" {
#endif

extern int ipset_cache_add(const char *name, const struct ipset_type *type,
			   uint8_t family);
extern int ipset_cache_del(const char *name);
extern int ipset_cache_rename(const char *from, const char *to);
extern int ipset_cache_swap(const char *from, const char *to);

extern int ipset_cache_init(void);
extern void ipset_cache_fini(void);

extern const struct ipset_type *
	ipset_type_get(struct ipset_session *session, enum ipset_cmd cmd);
extern const struct ipset_type *
	ipset_type_check(struct ipset_session *session);

extern int ipset_type_add(struct ipset_type *type);
extern const struct ipset_type *ipset_types(void);
extern const char *ipset_typename_resolve(const char *str);
extern bool ipset_match_typename(const char *str,
				 const struct ipset_type *t);
extern void ipset_load_types(void);

#ifdef __cplusplus
}
#endif

#ifdef TYPE_INCLUSIVE
#	ifdef _INIT
#		undef _init
#		define _init _INIT
#	endif
#else
#	undef _init
#	define _init __attribute__((constructor)) _INIT
#endif

#endif /* LIBIPSET_TYPES_H */
