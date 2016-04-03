/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_DATA_H
#define LIBIPSET_DATA_H

#include <stdbool.h>				/* bool */
#include <libipset/nf_inet_addr.h>		/* union nf_inet_addr */

/* Data options */
enum ipset_opt {
	IPSET_OPT_NONE = 0,
	/* Common ones */
	IPSET_SETNAME,
	IPSET_OPT_TYPENAME,
	IPSET_OPT_FAMILY,
	/* CADT options */
	IPSET_OPT_IP,
	IPSET_OPT_IP_FROM = IPSET_OPT_IP,
	IPSET_OPT_IP_TO,
	IPSET_OPT_CIDR,
	IPSET_OPT_MARK,
	IPSET_OPT_PORT,
	IPSET_OPT_PORT_FROM = IPSET_OPT_PORT,
	IPSET_OPT_PORT_TO,
	IPSET_OPT_TIMEOUT,
	/* Create-specific options */
	IPSET_OPT_GC,
	IPSET_OPT_HASHSIZE,
	IPSET_OPT_MAXELEM,
	IPSET_OPT_MARKMASK,
	IPSET_OPT_NETMASK,
	IPSET_OPT_PROBES,
	IPSET_OPT_RESIZE,
	IPSET_OPT_SIZE,
	IPSET_OPT_FORCEADD,
	/* Create-specific options, filled out by the kernel */
	IPSET_OPT_ELEMENTS,
	IPSET_OPT_REFERENCES,
	IPSET_OPT_MEMSIZE,
	/* ADT-specific options */
	IPSET_OPT_ETHER,
	IPSET_OPT_NAME,
	IPSET_OPT_NAMEREF,
	IPSET_OPT_IP2,
	IPSET_OPT_CIDR2,
	IPSET_OPT_IP2_TO,
	IPSET_OPT_PROTO,
	IPSET_OPT_IFACE,
	/* Swap/rename to */
	IPSET_OPT_SETNAME2,
	/* Flags */
	IPSET_OPT_EXIST,
	IPSET_OPT_BEFORE,
	IPSET_OPT_PHYSDEV,
	IPSET_OPT_NOMATCH,
	IPSET_OPT_COUNTERS,
	IPSET_OPT_PACKETS,
	IPSET_OPT_BYTES,
	IPSET_OPT_CREATE_COMMENT,
	IPSET_OPT_ADT_COMMENT,
	IPSET_OPT_SKBINFO,
	IPSET_OPT_SKBMARK,
	IPSET_OPT_SKBPRIO,
	IPSET_OPT_SKBQUEUE,
	/* Internal options */
	IPSET_OPT_FLAGS = 48,	/* IPSET_FLAG_EXIST| */
	IPSET_OPT_CADT_FLAGS,	/* IPSET_FLAG_BEFORE| */
	IPSET_OPT_ELEM,
	IPSET_OPT_TYPE,
	IPSET_OPT_LINENO,
	IPSET_OPT_REVISION,
	IPSET_OPT_REVISION_MIN,
	IPSET_OPT_MAX,
};

#define IPSET_FLAG(opt)		(1ULL << (opt))
#define IPSET_FLAGS_ALL		(~0ULL)

#define IPSET_CREATE_FLAGS		\
	(IPSET_FLAG(IPSET_OPT_FAMILY)	\
	| IPSET_FLAG(IPSET_OPT_TYPENAME)\
	| IPSET_FLAG(IPSET_OPT_TYPE)	\
	| IPSET_FLAG(IPSET_OPT_IP)	\
	| IPSET_FLAG(IPSET_OPT_IP_TO)	\
	| IPSET_FLAG(IPSET_OPT_CIDR)	\
	| IPSET_FLAG(IPSET_OPT_PORT)	\
	| IPSET_FLAG(IPSET_OPT_PORT_TO)	\
	| IPSET_FLAG(IPSET_OPT_TIMEOUT)	\
	| IPSET_FLAG(IPSET_OPT_GC)	\
	| IPSET_FLAG(IPSET_OPT_HASHSIZE)\
	| IPSET_FLAG(IPSET_OPT_MAXELEM)	\
	| IPSET_FLAG(IPSET_OPT_MARKMASK)\
	| IPSET_FLAG(IPSET_OPT_NETMASK)	\
	| IPSET_FLAG(IPSET_OPT_PROBES)	\
	| IPSET_FLAG(IPSET_OPT_RESIZE)	\
	| IPSET_FLAG(IPSET_OPT_SIZE)	\
	| IPSET_FLAG(IPSET_OPT_COUNTERS)\
	| IPSET_FLAG(IPSET_OPT_CREATE_COMMENT)\
	| IPSET_FLAG(IPSET_OPT_FORCEADD)\
	| IPSET_FLAG(IPSET_OPT_SKBINFO))

#define IPSET_ADT_FLAGS			\
	(IPSET_FLAG(IPSET_OPT_IP)	\
	| IPSET_FLAG(IPSET_OPT_IP_TO)	\
	| IPSET_FLAG(IPSET_OPT_CIDR)	\
	| IPSET_FLAG(IPSET_OPT_MARK)	\
	| IPSET_FLAG(IPSET_OPT_PORT)	\
	| IPSET_FLAG(IPSET_OPT_PORT_TO)	\
	| IPSET_FLAG(IPSET_OPT_TIMEOUT)	\
	| IPSET_FLAG(IPSET_OPT_ETHER)	\
	| IPSET_FLAG(IPSET_OPT_NAME)	\
	| IPSET_FLAG(IPSET_OPT_NAMEREF)	\
	| IPSET_FLAG(IPSET_OPT_IP2)	\
	| IPSET_FLAG(IPSET_OPT_CIDR2)	\
	| IPSET_FLAG(IPSET_OPT_PROTO)	\
	| IPSET_FLAG(IPSET_OPT_IFACE) \
	| IPSET_FLAG(IPSET_OPT_CADT_FLAGS)\
	| IPSET_FLAG(IPSET_OPT_BEFORE) \
	| IPSET_FLAG(IPSET_OPT_PHYSDEV) \
	| IPSET_FLAG(IPSET_OPT_NOMATCH) \
	| IPSET_FLAG(IPSET_OPT_PACKETS)	\
	| IPSET_FLAG(IPSET_OPT_BYTES)	\
	| IPSET_FLAG(IPSET_OPT_ADT_COMMENT)\
	| IPSET_FLAG(IPSET_OPT_SKBMARK)	\
	| IPSET_FLAG(IPSET_OPT_SKBPRIO)	\
	| IPSET_FLAG(IPSET_OPT_SKBQUEUE))

struct ipset_data;

#ifdef __cplusplus
extern "C" {
#endif

extern void ipset_strlcpy(char *dst, const char *src, size_t len);
extern void ipset_strlcat(char *dst, const char *src, size_t len);
extern bool ipset_data_flags_test(const struct ipset_data *data,
				  uint64_t flags);
extern void ipset_data_flags_set(struct ipset_data *data, uint64_t flags);
extern void ipset_data_flags_unset(struct ipset_data *data, uint64_t flags);
extern bool ipset_data_ignored(struct ipset_data *data, enum ipset_opt opt);
extern bool ipset_data_test_ignored(struct ipset_data *data,
				    enum ipset_opt opt);

extern int ipset_data_set(struct ipset_data *data, enum ipset_opt opt,
			  const void *value);
extern const void *ipset_data_get(const struct ipset_data *data,
				  enum ipset_opt opt);

static inline bool
ipset_data_test(const struct ipset_data *data, enum ipset_opt opt)
{
	return ipset_data_flags_test(data, IPSET_FLAG(opt));
}

/* Shortcuts */
extern const char *ipset_data_setname(const struct ipset_data *data);
extern uint8_t ipset_data_family(const struct ipset_data *data);
extern uint8_t ipset_data_cidr(const struct ipset_data *data);
extern uint64_t ipset_data_flags(const struct ipset_data *data);

extern void ipset_data_reset(struct ipset_data *data);
extern struct ipset_data *ipset_data_init(void);
extern void ipset_data_fini(struct ipset_data *data);

extern size_t ipset_data_sizeof(enum ipset_opt opt, uint8_t family);

#ifdef __cplusplus
}
#endif

#endif /* LIBIPSET_DATA_H */
