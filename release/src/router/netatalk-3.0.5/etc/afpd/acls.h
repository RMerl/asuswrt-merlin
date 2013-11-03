/*
   Copyright (c) 2008,2009 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#ifndef AFPD_ACLS_H 
#define AFPD_ACLS_H

#ifdef HAVE_SOLARIS_ACLS
#include <sys/acl.h>
#endif

#include <atalk/uuid.h>		/* for atalk_uuid_t */

/*
 * This is what Apple says about ACL flags in sys/kauth.h:
 *
 * <Apple> The low 16 bits of the flags field are reserved for filesystem
 * internal use and must be preserved by all APIs.  This includes
 * round-tripping flags through user-space interfaces.
 * The high 16 bits of the flags are used to store attributes and
 * to request specific handling of the ACL. </Apple>
 * 
 * The constants are included for reference. We DONT expect them on
 * the wire! We will ignore and spoil em.
 */

#ifdef HAVE_SOLARIS_ACLS
/* Some stuff for the handling of NFSv4 ACLs */
#define ACE_TRIVIAL (ACE_OWNER | ACE_GROUP | ACE_EVERYONE)
#endif /* HAVE_SOLARIS_ACLS */

/* FPGet|Set Bitmap */
enum {
    kFileSec_UUID      = (1<<0),
    kFileSec_GRPUUID   = (1<<1),
    kFileSec_ACL       = (1<<2),
    kFileSec_REMOVEACL = (1<<3),
    kFileSec_Inherit   = (1<<4)
};

/* ACL Flags */
#define DARWIN_ACL_FLAGS_PRIVATE       (0xffff)
/* inheritance will be deferred until the first rename operation */
#define KAUTH_ACL_DEFER_INHERIT (1<<16)
/* this ACL must not be overwritten as part of an inheritance operation */
#define KAUTH_ACL_NO_INHERIT (1<<17)

/* ACE Flags */
#define DARWIN_ACE_FLAGS_KINDMASK           0xf
#define DARWIN_ACE_FLAGS_PERMIT             (1<<0) /* 0x00000001 */
#define DARWIN_ACE_FLAGS_DENY               (1<<1) /* 0x00000002 */
#define DARWIN_ACE_FLAGS_INHERITED          (1<<4) /* 0x00000010 */
#define DARWIN_ACE_FLAGS_FILE_INHERIT       (1<<5) /* 0x00000020 */
#define DARWIN_ACE_FLAGS_DIRECTORY_INHERIT  (1<<6) /* 0x00000040 */
#define DARWIN_ACE_FLAGS_LIMIT_INHERIT      (1<<7) /* 0x00000080 */
#define DARWIN_ACE_FLAGS_ONLY_INHERIT       (1<<8) /* 0x00000100 */

/* All flag bits controlling ACE inheritance */
#define DARWIN_ACE_INHERIT_CONTROL_FLAGS \
       (DARWIN_ACE_FLAGS_FILE_INHERIT |\
        DARWIN_ACE_FLAGS_DIRECTORY_INHERIT |\
        DARWIN_ACE_FLAGS_LIMIT_INHERIT |\
        DARWIN_ACE_FLAGS_ONLY_INHERIT)

/* ACE Rights */
#define DARWIN_ACE_READ_DATA           0x00000002
#define DARWIN_ACE_LIST_DIRECTORY      0x00000002
#define DARWIN_ACE_WRITE_DATA          0x00000004
#define DARWIN_ACE_ADD_FILE            0x00000004
#define DARWIN_ACE_EXECUTE             0x00000008
#define DARWIN_ACE_SEARCH              0x00000008
#define DARWIN_ACE_DELETE              0x00000010
#define DARWIN_ACE_APPEND_DATA         0x00000020
#define DARWIN_ACE_ADD_SUBDIRECTORY    0x00000020
#define DARWIN_ACE_DELETE_CHILD        0x00000040
#define DARWIN_ACE_READ_ATTRIBUTES     0x00000080
#define DARWIN_ACE_WRITE_ATTRIBUTES    0x00000100
#define DARWIN_ACE_READ_EXTATTRIBUTES  0x00000200
#define DARWIN_ACE_WRITE_EXTATTRIBUTES 0x00000400
#define DARWIN_ACE_READ_SECURITY       0x00000800
#define DARWIN_ACE_WRITE_SECURITY      0x00001000
#define DARWIN_ACE_TAKE_OWNERSHIP      0x00002000

/* Access Control List Entry (ACE) */
typedef struct {
    atalk_uuid_t      darwin_ace_uuid;
    uint32_t    darwin_ace_flags;
    uint32_t    darwin_ace_rights;
} darwin_ace_t;

/* Access Control List */
typedef struct {
    uint32_t darwin_acl_count;
    uint32_t darwin_acl_flags;
} darwin_acl_header_t;

/* FP functions */
int afp_access (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_getacl (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);
int afp_setacl (AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf,  size_t *rbuflen);

/* Parse afp.conf */
extern int acl_ldap_readconfig(char *name);

/* Misc funcs */
extern int acltoownermode(const AFPObj *obj, const struct vol *vol, char *path, struct stat *st, struct maccess *ma);
#endif
