/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>				/* assert */
#include <errno.h>				/* errno */
#include <string.h>				/* strerror */

#include <libipset/debug.h>			/* D() */
#include <libipset/data.h>			/* ipset_data_get */
#include <libipset/session.h>			/* ipset_err */
#include <libipset/types.h>			/* struct ipset_type */
#include <libipset/utils.h>			/* STRNEQ */
#include <libipset/errcode.h>			/* prototypes */
#include <libipset/linux_ip_set_bitmap.h>	/* bitmap specific errcodes */
#include <libipset/linux_ip_set_hash.h>		/* hash specific errcodes */
#include <libipset/linux_ip_set_list.h>		/* list specific errcodes */

/* Core kernel error codes */
static const struct ipset_errcode_table core_errcode_table[] = {
	/* Generic error codes */
	{ ENOENT, 0,
	  "The set with the given name does not exist" },
	{ EMSGSIZE, 0,
	  "Kernel error received: message could not be created" },
	{ IPSET_ERR_PROTOCOL,  0,
	  "Kernel error received: ipset protocol error" },

	/* CREATE specific error codes */
	{ EEXIST, IPSET_CMD_CREATE,
	  "Set cannot be created: set with the same name already exists" },
	{ IPSET_ERR_FIND_TYPE, 0,
	  "Kernel error received: set type not supported" },
	{ IPSET_ERR_MAX_SETS, 0,
	  "Kernel error received: maximal number of sets reached, "
	  "cannot create more." },
	{ IPSET_ERR_INVALID_NETMASK, 0,
	  "The value of the netmask parameter is invalid" },
	{ IPSET_ERR_INVALID_MARKMASK, 0,
	  "The value of the markmask parameter is invalid" },
	{ IPSET_ERR_INVALID_FAMILY, 0,
	  "Protocol family not supported by the set type" },

	/* DESTROY specific error codes */
	{ IPSET_ERR_BUSY, IPSET_CMD_DESTROY,
	  "Set cannot be destroyed: it is in use by a kernel component" },

	/* FLUSH specific error codes */

	/* RENAME specific error codes */
	{ IPSET_ERR_EXIST_SETNAME2, IPSET_CMD_RENAME,
	  "Set cannot be renamed: a set with the new name already exists" },
	{ IPSET_ERR_REFERENCED, IPSET_CMD_RENAME,
	  "Set cannot be renamed: it is in use by another system" },

	/* SWAP specific error codes */
	{ IPSET_ERR_EXIST_SETNAME2, IPSET_CMD_SWAP,
	  "Sets cannot be swapped: the second set does not exist" },
	{ IPSET_ERR_TYPE_MISMATCH, IPSET_CMD_SWAP,
	  "The sets cannot be swapped: their type does not match" },

	/* LIST/SAVE specific error codes */

	/* Generic (CADT) error codes */
	{ IPSET_ERR_INVALID_CIDR, 0,
	  "The value of the CIDR parameter of the IP address is invalid" },
	{ IPSET_ERR_TIMEOUT, 0,
	  "Timeout cannot be used: set was created without timeout support" },
	{ IPSET_ERR_IPADDR_IPV4, 0,
	  "An IPv4 address is expected, but not received" },
	{ IPSET_ERR_IPADDR_IPV6, 0,
	  "An IPv6 address is expected, but not received" },
	{ IPSET_ERR_COUNTER, 0,
	  "Packet/byte counters cannot be used: set was created without counter support" },
	{ IPSET_ERR_COMMENT, 0,
	  "Comment cannot be used: set was created without comment support" },
	{ IPSET_ERR_SKBINFO, 0,
	  "Skbinfo mapping cannot be used: set was created without skbinfo support" },

	/* ADD specific error codes */
	{ IPSET_ERR_EXIST, IPSET_CMD_ADD,
	  "Element cannot be added to the set: it's already added" },

	/* DEL specific error codes */
	{ IPSET_ERR_EXIST, IPSET_CMD_DEL,
	  "Element cannot be deleted from the set: it's not added" },

	/* TEST specific error codes */

	/* HEADER specific error codes */

	/* TYPE specific error codes */
	{ EEXIST, IPSET_CMD_TYPE,
	  "Kernel error received: set type does not supported" },

	/* PROTOCOL specific error codes */

	{ },
};

/* Bitmap type-specific error codes */
static const struct ipset_errcode_table bitmap_errcode_table[] = {
	/* Generic (CADT) error codes */
	{ IPSET_ERR_BITMAP_RANGE, 0,
	  "Element is out of the range of the set" },
	{ IPSET_ERR_BITMAP_RANGE_SIZE, IPSET_CMD_CREATE,
	  "The range you specified exceeds the size limit of the set type" },
	{ },
};

/* Hash type-specific error codes */
static const struct ipset_errcode_table hash_errcode_table[] = {
	/* Generic (CADT) error codes */
	{ IPSET_ERR_HASH_FULL, 0,
	  "Hash is full, cannot add more elements" },
	{ IPSET_ERR_HASH_ELEM, 0,
	  "Null-valued element, cannot be stored in a hash type of set" },
	{ IPSET_ERR_INVALID_PROTO, 0,
	  "Invalid protocol specified" },
	{ IPSET_ERR_MISSING_PROTO, 0,
	  "Protocol missing, but must be specified" },
	{ IPSET_ERR_HASH_RANGE_UNSUPPORTED, 0,
	  "Range is not supported in the \"net\" component of the element" },
	{ IPSET_ERR_HASH_RANGE, 0,
	  "Invalid range, covers the whole address space" },
	{ },
};

/* List type-specific error codes */
static const struct ipset_errcode_table list_errcode_table[] = {
	/* Generic (CADT) error codes */
	{ IPSET_ERR_NAME, 0,
	  "Set to be added/deleted/tested as element does not exist." },
	{ IPSET_ERR_LOOP, 0,
	  "Sets with list:set type cannot be added to the set." },
	{ IPSET_ERR_BEFORE, 0,
	  "No reference set specified." },
	{ IPSET_ERR_NAMEREF, 0,
	  "The set to which you referred with 'before' or 'after' "
	  "does not exist." },
	{ IPSET_ERR_LIST_FULL, 0,
	  "The set is full, more elements cannot be added." },
	{ IPSET_ERR_REF_EXIST, 0,
	  "The set to which you referred with 'before' or 'after' "
	  "is not added to the set." },
	{ },
};

/* Match set type names */
#define MATCH_TYPENAME(a, b)    STRNEQ(a, b, strlen(b))

/**
 * ipset_errcode - interpret a kernel error code
 * @session: session structure
 * @errcode: errcode
 *
 * Find the error code and print the appropriate
 * error message into the error buffer.
 *
 * Returns -1.
 */
int
ipset_errcode(struct ipset_session *session, enum ipset_cmd cmd, int errcode)
{
	const struct ipset_errcode_table *table = core_errcode_table;
	int i, generic;

	if (errcode >= IPSET_ERR_TYPE_SPECIFIC) {
		const struct ipset_type *type;

		type = ipset_saved_type(session);
		if (type) {
			if (MATCH_TYPENAME(type->name, "bitmap:"))
				table = bitmap_errcode_table;
			else if (MATCH_TYPENAME(type->name, "hash:"))
				table = hash_errcode_table;
			else if (MATCH_TYPENAME(type->name, "list:"))
				table = list_errcode_table;
		}
	}

retry:
	for (i = 0, generic = -1; table[i].errcode; i++) {
		if (table[i].errcode == errcode &&
		    (table[i].cmd == cmd || table[i].cmd == 0)) {
			if (table[i].cmd == 0) {
				generic = i;
				continue;
			}
			return ipset_err(session, table[i].message);
		}
	}
	if (generic != -1)
		return ipset_err(session, table[generic].message);
	/* Fall back to the core table */
	if (table != core_errcode_table) {
		table = core_errcode_table;
		goto retry;
	}
	if (errcode < IPSET_ERR_PRIVATE)
		return ipset_err(session, "Kernel error received: %s",
				 strerror(errcode));
	else
		return ipset_err(session,
				 "Undecoded error %u received from kernel",
				 errcode);
}
