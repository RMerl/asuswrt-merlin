/*
 * $Id: jffs2-user.h,v 1.1 2004/05/05 11:57:54 dwmw2 Exp $
 *
 * JFFS2 definitions for use in user space only
 */

#ifndef __JFFS2_USER_H__
#define __JFFS2_USER_H__

/* This file is blessed for inclusion by userspace */
#include <linux/jffs2.h>
#include <endian.h>
#include <byteswap.h>

#undef cpu_to_je16
#undef cpu_to_je32
#undef cpu_to_jemode
#undef je16_to_cpu
#undef je32_to_cpu
#undef jemode_to_cpu

extern int target_endian;

#define t16(x) ({ uint16_t __b = (x); (target_endian==__BYTE_ORDER)?__b:bswap_16(__b); })
#define t32(x) ({ uint32_t __b = (x); (target_endian==__BYTE_ORDER)?__b:bswap_32(__b); })

#define cpu_to_je16(x) ((jint16_t){t16(x)})
#define cpu_to_je32(x) ((jint32_t){t32(x)})
#define cpu_to_jemode(x) ((jmode_t){t32(x)})

#define je16_to_cpu(x) (t16((x).v16))
#define je32_to_cpu(x) (t32((x).v32))
#define jemode_to_cpu(x) (t32((x).m))

#define le16_to_cpu(x)	(__BYTE_ORDER==__LITTLE_ENDIAN ? (x) : bswap_16(x))
#define le32_to_cpu(x)	(__BYTE_ORDER==__LITTLE_ENDIAN ? (x) : bswap_32(x))
#define cpu_to_le16(x)	(__BYTE_ORDER==__LITTLE_ENDIAN ? (x) : bswap_16(x))
#define cpu_to_le32(x)	(__BYTE_ORDER==__LITTLE_ENDIAN ? (x) : bswap_32(x))

/* XATTR/POSIX-ACL related definition */
/* Namespaces copied from xattr.h and posix_acl_xattr.h */
#define XATTR_USER_PREFIX		"user."
#define XATTR_USER_PREFIX_LEN		(sizeof (XATTR_USER_PREFIX) - 1)
#define XATTR_SECURITY_PREFIX		"security."
#define XATTR_SECURITY_PREFIX_LEN	(sizeof (XATTR_SECURITY_PREFIX) - 1)
#define POSIX_ACL_XATTR_ACCESS		"system.posix_acl_access"
#define POSIX_ACL_XATTR_ACCESS_LEN	(sizeof (POSIX_ACL_XATTR_ACCESS) - 1)
#define POSIX_ACL_XATTR_DEFAULT		"system.posix_acl_default"
#define POSIX_ACL_XATTR_DEFAULT_LEN	(sizeof (POSIX_ACL_XATTR_DEFAULT) - 1)
#define XATTR_TRUSTED_PREFIX		"trusted."
#define XATTR_TRUSTED_PREFIX_LEN	(sizeof (XATTR_TRUSTED_PREFIX) - 1)

struct jffs2_acl_entry {
	jint16_t	e_tag;
	jint16_t	e_perm;
	jint32_t	e_id;
};

struct jffs2_acl_entry_short {
	jint16_t	e_tag;
	jint16_t	e_perm;
};

struct jffs2_acl_header {
	jint32_t	a_version;
};

/* copied from include/linux/posix_acl_xattr.h */
#define POSIX_ACL_XATTR_VERSION 0x0002

struct posix_acl_xattr_entry {
	uint16_t		e_tag;
	uint16_t		e_perm;
	uint32_t		e_id;
};

struct posix_acl_xattr_header {
	uint32_t			a_version;
	struct posix_acl_xattr_entry	a_entries[0];
};

#endif /* __JFFS2_USER_H__ */
