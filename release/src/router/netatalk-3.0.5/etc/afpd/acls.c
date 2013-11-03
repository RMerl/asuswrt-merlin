/*
  Copyright (c) 2008, 2009, 2010 Frank Lahm <franklahm@gmail.com>
  Copyright (c) 2011 Laura Mueller <laura-mueller@uni-duesseldorf.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#ifdef HAVE_SOLARIS_ACLS
#include <sys/acl.h>
#endif
#ifdef HAVE_POSIX_ACLS
#include <sys/acl.h>
#endif
#ifdef HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
#endif

#include <atalk/errchk.h>
#include <atalk/adouble.h>
#include <atalk/vfs.h>
#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/logger.h>
#include <atalk/uuid.h>
#include <atalk/acl.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/unix.h>
#include <atalk/netatalk_conf.h>

#include "directory.h"
#include "desktop.h"
#include "volume.h"
#include "fork.h"
#include "unix.h"
#include "acls.h"
#include "acl_mappings.h"
#include "auth.h"

/* for map_acl() */
#define SOLARIS_2_DARWIN       1
#define DARWIN_2_SOLARIS       2
#define POSIX_DEFAULT_2_DARWIN 3
#define POSIX_ACCESS_2_DARWIN  4
#define DARWIN_2_POSIX_DEFAULT 5
#define DARWIN_2_POSIX_ACCESS  6

#define MAP_MASK               31
#define IS_DIR                 32

/* bit flags for set_acl() and map_aces_darwin_to_posix() */
#define HAS_DEFAULT_ACL 0x01
#define HAS_EXT_DEFAULT_ACL 0x02

/********************************************************
 * Solaris funcs
 ********************************************************/

#ifdef HAVE_SOLARIS_ACLS

/*! 
 * Compile access rights for a user to one file-system object
 *
 * This combines combines all access rights for a user to one fs-object and
 * returns the result as a Darwin allowed rights ACE.
 * This must honor trivial ACEs which are a mode_t mapping.
 *
 * @param obj            (r) handle
 * @param path           (r) path to filesystem object
 * @param sb             (rw) struct stat of path
 * @param ma             (rw) UARights struct
 * @param rights_out     (w) mapped Darwin ACL rights
 *
 * @returns                  0 or -1 on error
 */
static int solaris_acl_rights(const AFPObj *obj,
                              const char *path,
                              struct stat *sb,
                              struct maccess *ma,
                              uint32_t *rights_out)
{
    EC_INIT;
    int      i, ace_count, checkgroup;
    ace_t    *aces = NULL;
    uid_t    who;
    uint16_t flags, type;
    uint32_t rights, allowed_rights = 0, denied_rights = 0, darwin_rights;

    /* Get ACL from file/dir */
    EC_NEG1_LOG(ace_count = get_nfsv4_acl(path, &aces));

    if (ace_count == 0)
        goto EC_CLEANUP;

    /* Now check requested rights */
    i = 0;
    do { /* Loop through ACEs */
        who = aces[i].a_who;
        flags = aces[i].a_flags;
        type = aces[i].a_type;
        rights = aces[i].a_access_mask;

        if (flags & ACE_INHERIT_ONLY_ACE)
            continue;

        /* Now the tricky part: decide if ACE effects our user. I'll explain:
           if its a dedicated (non trivial) ACE for the user
           OR
           if its a ACE for a group we're member of
           OR
           if its a trivial ACE_OWNER ACE and requested UUID is the owner
           OR
           if its a trivial ACE_GROUP ACE and requested UUID is group
           OR
           if its a trivial ACE_EVERYONE ACE
           THEN
           process ACE */
        if (((who == obj->uid) && !(flags & (ACE_TRIVIAL|ACE_IDENTIFIER_GROUP)))
            ||
            ((flags & ACE_IDENTIFIER_GROUP) && !(flags & ACE_GROUP) && gmem(who, obj->ngroups, obj->groups))
            ||
            ((flags & ACE_OWNER) && (obj->uid == sb->st_uid))
            ||
            ((flags & ACE_GROUP) && !(obj->uid == sb->st_uid) && gmem(sb->st_gid, obj->ngroups, obj->groups))
            ||
            (flags & ACE_EVERYONE && !(obj->uid == sb->st_uid) && !gmem(sb->st_gid, obj->ngroups, obj->groups))
            ) {
            /* Found an applicable ACE */
            if (type == ACE_ACCESS_ALLOWED_ACE_TYPE)
                allowed_rights |= rights;
            else if (type == ACE_ACCESS_DENIED_ACE_TYPE)
                /* Only or to denied rights if not previously allowed !! */
                denied_rights |= ((!allowed_rights) & rights);
        }
    } while (++i < ace_count);


    /* Darwin likes to ask for "delete_child" on dir,
       "write_data" is actually the same, so we add that for dirs */
    if (S_ISDIR(sb->st_mode) && (allowed_rights & ACE_WRITE_DATA))
        allowed_rights |= ACE_DELETE_CHILD;

    /* Remove denied from allowed rights */
    allowed_rights &= ~denied_rights;

    /* map rights */
    darwin_rights = 0;
    for (i=0; nfsv4_to_darwin_rights[i].from != 0; i++) {
        if (allowed_rights & nfsv4_to_darwin_rights[i].from)
            darwin_rights |= nfsv4_to_darwin_rights[i].to;
    }

    LOG(log_maxdebug, logtype_afpd, "rights: 0x%08x", darwin_rights);

    if (rights_out)
        *rights_out = darwin_rights;

    if (ma && obj->options.flags & OPTION_ACL2MACCESS) {
        if (darwin_rights & DARWIN_ACE_READ_DATA)
            ma->ma_user |= AR_UREAD;
        if (darwin_rights & DARWIN_ACE_WRITE_DATA)
            ma->ma_user |= AR_UWRITE;
        if (darwin_rights & (DARWIN_ACE_EXECUTE | DARWIN_ACE_SEARCH))
            ma->ma_user |= AR_USEARCH;
    }

    if (sb && obj->options.flags & OPTION_ACL2MODE) {
        if (darwin_rights & DARWIN_ACE_READ_DATA)
            sb->st_mode |= S_IRUSR;
        if (darwin_rights & DARWIN_ACE_WRITE_DATA)
            sb->st_mode |= S_IWUSR;
        if (darwin_rights & (DARWIN_ACE_EXECUTE | DARWIN_ACE_SEARCH))
            sb->st_mode |= S_IXUSR;
    }

EC_CLEANUP:
    if (aces) free(aces);

    EC_EXIT;
}

/*
  Maps ACE array from Solaris to Darwin. Darwin ACEs are stored in network byte order.
  Return numer of mapped ACEs or -1 on error.
  All errors while mapping (e.g. getting UUIDs from LDAP) are fatal.
*/
static int map_aces_solaris_to_darwin(const ace_t *aces,
                                      darwin_ace_t *darwin_aces,
                                      int ace_count)
{
    EC_INIT;
    int i, count = 0;
    uint32_t flags;
    uint32_t rights;
    struct passwd *pwd = NULL;
    struct group *grp = NULL;

    LOG(log_maxdebug, logtype_afpd, "map_aces_solaris_to_darwin: parsing %d ACES", ace_count);

    while(ace_count--) {
        LOG(log_maxdebug, logtype_afpd, "ACE No. %d", ace_count + 1);
        /* if its a ACE resulting from nfsv4 mode mapping, discard it */
        if (aces->a_flags & (ACE_OWNER | ACE_GROUP | ACE_EVERYONE)) {
            LOG(log_debug, logtype_afpd, "trivial ACE");
            aces++;
            continue;
        }

        if ( ! (aces->a_flags & ACE_IDENTIFIER_GROUP) ) { /* user ace */
            LOG(log_debug, logtype_afpd, "uid: %d", aces->a_who);
            EC_NULL_LOG(pwd = getpwuid(aces->a_who));
            LOG(log_debug, logtype_afpd, "uid: %d -> name: %s", aces->a_who, pwd->pw_name);
            EC_ZERO_LOG(getuuidfromname(pwd->pw_name,
                                        UUID_USER,
                                        darwin_aces->darwin_ace_uuid));
        } else { /* group ace */
            LOG(log_debug, logtype_afpd, "gid: %d", aces->a_who);
            EC_NULL_LOG(grp = getgrgid(aces->a_who));
            LOG(log_debug, logtype_afpd, "gid: %d -> name: %s", aces->a_who, grp->gr_name);
            EC_ZERO_LOG(getuuidfromname(grp->gr_name,
                                        UUID_GROUP,
                                        darwin_aces->darwin_ace_uuid));
        }

        /* map flags */
        if (aces->a_type == ACE_ACCESS_ALLOWED_ACE_TYPE)
            flags = DARWIN_ACE_FLAGS_PERMIT;
        else if (aces->a_type == ACE_ACCESS_DENIED_ACE_TYPE)
            flags = DARWIN_ACE_FLAGS_DENY;
        else {          /* unsupported type */
            aces++;
            continue;
        }
        for(i=0; nfsv4_to_darwin_flags[i].from != 0; i++) {
            if (aces->a_flags & nfsv4_to_darwin_flags[i].from)
                flags |= nfsv4_to_darwin_flags[i].to;
        }
        darwin_aces->darwin_ace_flags = htonl(flags);

        /* map rights */
        rights = 0;
        for (i=0; nfsv4_to_darwin_rights[i].from != 0; i++) {
            if (aces->a_access_mask & nfsv4_to_darwin_rights[i].from)
                rights |= nfsv4_to_darwin_rights[i].to;
        }
        darwin_aces->darwin_ace_rights = htonl(rights);

        count++;
        aces++;
        darwin_aces++;
    }

    return count;
EC_CLEANUP:
    EC_EXIT;
}

/*
  Maps ACE array from Darwin to Solaris. Darwin ACEs are expected in network byte order.
  Return numer of mapped ACEs or -1 on error.
  All errors while mapping (e.g. getting UUIDs from LDAP) are fatal.
*/
static int map_aces_darwin_to_solaris(darwin_ace_t *darwin_aces,
                                      ace_t *nfsv4_aces,
                                      int ace_count)
{
    EC_INIT;
    int i, mapped_aces = 0;
    uint32_t darwin_ace_flags;
    uint32_t darwin_ace_rights;
    uint16_t nfsv4_ace_flags;
    uint32_t nfsv4_ace_rights;
    char *name = NULL;
    uuidtype_t uuidtype;
    struct passwd *pwd;
    struct group *grp;

    while(ace_count--) {
        nfsv4_ace_flags = 0;
        nfsv4_ace_rights = 0;

        /* uid/gid first */
        EC_ZERO(getnamefromuuid(darwin_aces->darwin_ace_uuid, &name, &uuidtype));
        switch (uuidtype) {
        case UUID_USER:
            EC_NULL_LOG(pwd = getpwnam(name));
            nfsv4_aces->a_who = pwd->pw_uid;
            break;
        case UUID_GROUP:
            EC_NULL_LOG(grp = getgrnam(name));
            nfsv4_aces->a_who = (uid_t)(grp->gr_gid);
            nfsv4_ace_flags |= ACE_IDENTIFIER_GROUP;
            break;
        default:
            LOG(log_error, logtype_afpd, "map_aces_darwin_to_solaris: unkown uuidtype");
            EC_FAIL;
        }
        free(name);
        name = NULL;

        /* now type: allow/deny */
        darwin_ace_flags = ntohl(darwin_aces->darwin_ace_flags);
        if (darwin_ace_flags & DARWIN_ACE_FLAGS_PERMIT)
            nfsv4_aces->a_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
        else if (darwin_ace_flags & DARWIN_ACE_FLAGS_DENY)
            nfsv4_aces->a_type = ACE_ACCESS_DENIED_ACE_TYPE;
        else { /* unsupported type */
            darwin_aces++;
            continue;
        }
        /* map flags */
        for(i=0; darwin_to_nfsv4_flags[i].from != 0; i++) {
            if (darwin_ace_flags & darwin_to_nfsv4_flags[i].from)
                nfsv4_ace_flags |= darwin_to_nfsv4_flags[i].to;
        }

        /* map rights */
        darwin_ace_rights = ntohl(darwin_aces->darwin_ace_rights);
        for (i=0; darwin_to_nfsv4_rights[i].from != 0; i++) {
            if (darwin_ace_rights & darwin_to_nfsv4_rights[i].from)
                nfsv4_ace_rights |= darwin_to_nfsv4_rights[i].to;
        }

        LOG(log_debug9, logtype_afpd,
            "map_aces_darwin_to_solaris: ACE flags: Darwin:%08x -> NFSv4:%04x",
            darwin_ace_flags, nfsv4_ace_flags);
        LOG(log_debug9, logtype_afpd,
            "map_aces_darwin_to_solaris: ACE rights: Darwin:%08x -> NFSv4:%08x",
            darwin_ace_rights, nfsv4_ace_rights);

        nfsv4_aces->a_flags = nfsv4_ace_flags;
        nfsv4_aces->a_access_mask = nfsv4_ace_rights;

        mapped_aces++;
        darwin_aces++;
        nfsv4_aces++;
    }

    return mapped_aces;
EC_CLEANUP:
    if (name)
        free(name);
    EC_EXIT;
}
#endif /* HAVE_SOLARIS_ACLS */

/********************************************************
 * POSIX 1e funcs
 ********************************************************/

#ifdef HAVE_POSIX_ACLS

static uint32_t posix_permset_to_darwin_rights(acl_entry_t e, int is_dir)
{
    EC_INIT;
    uint32_t rights = 0;
    acl_permset_t permset;

    EC_ZERO_LOG(acl_get_permset(e, &permset));

#ifdef HAVE_ACL_GET_PERM_NP
    if (acl_get_perm_np(permset, ACL_READ))
#else
    if (acl_get_perm(permset, ACL_READ))
#endif
        rights = DARWIN_ACE_READ_DATA
            | DARWIN_ACE_READ_EXTATTRIBUTES
            | DARWIN_ACE_READ_ATTRIBUTES
            | DARWIN_ACE_READ_SECURITY;
#ifdef HAVE_ACL_GET_PERM_NP
    if (acl_get_perm_np(permset, ACL_WRITE)) {
#else
    if (acl_get_perm(permset, ACL_WRITE)) {
#endif
        rights |= DARWIN_ACE_WRITE_DATA
            | DARWIN_ACE_APPEND_DATA
            | DARWIN_ACE_WRITE_EXTATTRIBUTES
            | DARWIN_ACE_WRITE_ATTRIBUTES;
        if (is_dir)
            rights |= DARWIN_ACE_DELETE_CHILD;
    }
#ifdef HAVE_ACL_GET_PERM_NP
    if (acl_get_perm_np(permset, ACL_EXECUTE))
#else
    if (acl_get_perm(permset, ACL_EXECUTE))
#endif
        rights |= DARWIN_ACE_EXECUTE;

EC_CLEANUP:
    LOG(log_maxdebug, logtype_afpd, "mapped rights: 0x%08x", rights);
    return rights;
}

/*! 
 * Compile access rights for a user to one file-system object
 *
 * This combines combines all access rights for a user to one fs-object and
 * returns the result as a Darwin allowed rights ACE.
 * This must honor trivial ACEs which are a mode_t mapping.
 *
 * @param path           (r) path to filesystem object
 * @param sb             (r) struct stat of path
 * @param result         (rw) resulting Darwin allow ACE
 *
 * @returns                  0 or -1 on error
 */

static int posix_acl_rights(const AFPObj *obj,
                            const char *path,
                            const struct stat *sb,
                            uint32_t *result)
{
    EC_INIT;
    int entry_id = ACL_FIRST_ENTRY;
    uint32_t rights = 0; /* rights which do not depend on ACL_MASK */
    uint32_t acl_rights = 0; /* rights which are subject to limitations imposed by ACL_MASK */
    uint32_t mask_rights = 0xffffffff;
    uid_t *uid = NULL;
    gid_t *gid = NULL;
    acl_t acl = NULL;
    acl_entry_t e;
    acl_tag_t tag;

    EC_NULL_LOGSTR(acl = acl_get_file(path, ACL_TYPE_ACCESS),
                   "acl_get_file(\"%s\"): %s", fullpathname(path), strerror(errno));

    /* Iterate through all ACEs. If we apply mask_rights later there is no need to iterate twice. */
    while (acl_get_entry(acl, entry_id, &e) == 1) {
        entry_id = ACL_NEXT_ENTRY;
        EC_ZERO_LOG(acl_get_tag_type(e, &tag));

        switch (tag) {
            case ACL_USER_OBJ:
                if (sb->st_uid == obj->uid) {
                    LOG(log_maxdebug, logtype_afpd, "ACL_USER_OBJ: %u", sb->st_uid);
                    rights |= posix_permset_to_darwin_rights(e, S_ISDIR(sb->st_mode));
                }
                break;

            case ACL_USER:
                EC_NULL_LOG(uid = (uid_t *)acl_get_qualifier(e));

                if (*uid == obj->uid) {
                    LOG(log_maxdebug, logtype_afpd, "ACL_USER: %u", *uid);
                    acl_rights |= posix_permset_to_darwin_rights(e, S_ISDIR(sb->st_mode));
                }
                acl_free(uid);
                uid = NULL;
                break;

            case ACL_GROUP_OBJ:
                if (!(sb->st_uid == obj->uid) && gmem(sb->st_gid, obj->ngroups, obj->groups)) {
                    LOG(log_maxdebug, logtype_afpd, "ACL_GROUP_OBJ: %u", sb->st_gid);
                    acl_rights |= posix_permset_to_darwin_rights(e, S_ISDIR(sb->st_mode));
                }
                break;

            case ACL_GROUP:
                EC_NULL_LOG(gid = (gid_t *)acl_get_qualifier(e));

                if (gmem(*gid, obj->ngroups, obj->groups)) {
                    LOG(log_maxdebug, logtype_afpd, "ACL_GROUP: %u", *gid);
                    acl_rights |= posix_permset_to_darwin_rights(e, S_ISDIR(sb->st_mode));
                }
                acl_free(gid);
                gid = NULL;
                break;

            case ACL_MASK:
                mask_rights = posix_permset_to_darwin_rights(e, S_ISDIR(sb->st_mode));
                LOG(log_maxdebug, logtype_afpd, "maskrights: 0x%08x", mask_rights);
                break;

            case ACL_OTHER:
                if (!(sb->st_uid == obj->uid) && !gmem(sb->st_gid, obj->ngroups, obj->groups)) {
                    LOG(log_maxdebug, logtype_afpd, "ACL_OTHER");
                    rights |= posix_permset_to_darwin_rights(e, S_ISDIR(sb->st_mode));
                }
                break;

            default:
                continue;
        }
    } /* while */

    /* apply the mask and collect the rights */
    rights |= (acl_rights & mask_rights);

    *result |= rights;

EC_CLEANUP:
    if (acl) acl_free(acl);
    if (uid) acl_free(uid);
    if (gid) acl_free(gid);
    EC_EXIT;
}

/*!
 * Helper function for posix_acls_to_uaperms() to convert Posix ACL permissions
 * into access rights needed to fill ua_permissions of a FPUnixPrivs structure.
 *
 * @param entry     (r) Posix ACL entry
 *
 * @returns         access rights
 */
static u_char acl_permset_to_uarights(acl_entry_t entry) {
    acl_permset_t permset;
    u_char rights = 0;

    if (acl_get_permset(entry, &permset) == -1)
        return rights;

#ifdef HAVE_ACL_GET_PERM_NP
    if (acl_get_perm_np(permset, ACL_READ))
#else
    if (acl_get_perm(permset, ACL_READ))
#endif
        rights |= AR_UREAD;

#ifdef HAVE_ACL_GET_PERM_NP
    if (acl_get_perm_np(permset, ACL_WRITE))
#else
    if (acl_get_perm(permset, ACL_WRITE))
#endif
        rights |= AR_UWRITE;

#ifdef HAVE_ACL_GET_PERM_NP
    if (acl_get_perm_np(permset, ACL_EXECUTE))
#else
    if (acl_get_perm(permset, ACL_EXECUTE))
#endif
        rights |= AR_USEARCH;

    return rights;
}

/*!
 * Update FPUnixPrivs for a file-system object on a volume supporting ACLs
 *
 * Checks permissions granted by ACLS for a user to one fs-object and
 * updates user and group permissions in given struct maccess. As OS X
 * doesn't conform to Posix 1003.1e Draft 17 it expects proper group
 * permissions in st_mode of struct stat even if the fs-object has an
 * ACL_MASK entry, st_mode gets modified to properly reflect group
 * permissions.
 *
 * @param path           (r) path to filesystem object
 * @param sb             (rw) struct stat of path
 * @param maccess        (rw) struct maccess of path
 *
 * @returns                  0 or -1 on error
 */
static int posix_acls_to_uaperms(const AFPObj *obj, const char *path, struct stat *sb, struct maccess *ma) {
    EC_INIT;

    int entry_id = ACL_FIRST_ENTRY;
    acl_entry_t entry;
    acl_tag_t tag;
    acl_t acl = NULL;
    uid_t *uid;
    gid_t *gid;
    uid_t whoami = geteuid();

    u_char group_rights = 0x00;
    u_char acl_rights = 0x00;
    u_char mask = 0xff;

    EC_NULL_LOG(acl = acl_get_file(path, ACL_TYPE_ACCESS));

    /* iterate through all ACEs */
    while (acl_get_entry(acl, entry_id, &entry) == 1) {
        entry_id = ACL_NEXT_ENTRY;
        EC_ZERO_LOG(acl_get_tag_type(entry, &tag));

        switch (tag) {
            case ACL_USER:
                EC_NULL_LOG(uid = (uid_t *)acl_get_qualifier(entry));

                if (*uid == obj->uid && !(whoami == sb->st_uid)) {
                    LOG(log_maxdebug, logtype_afpd, "ACL_USER: %u", *uid);
                    acl_rights |= acl_permset_to_uarights(entry);
                }
                acl_free(uid);
                break;

            case ACL_GROUP_OBJ:
                group_rights = acl_permset_to_uarights(entry);
                LOG(log_maxdebug, logtype_afpd, "ACL_GROUP_OBJ: %u", sb->st_gid);

                if (gmem(sb->st_gid, obj->ngroups, obj->groups) && !(whoami == sb->st_uid))
                    acl_rights |= group_rights;
                break;

            case ACL_GROUP:
                EC_NULL_LOG(gid = (gid_t *)acl_get_qualifier(entry));

                if (gmem(*gid, obj->ngroups, obj->groups) && !(whoami == sb->st_uid)) {
                    LOG(log_maxdebug, logtype_afpd, "ACL_GROUP: %u", *gid);
                    acl_rights |= acl_permset_to_uarights(entry);
                }
                acl_free(gid);
                break;

            case ACL_MASK:
                mask = acl_permset_to_uarights(entry);
                LOG(log_maxdebug, logtype_afpd, "ACL_MASK: 0x%02x", mask);
                break;

            default:
                break;
        }
    }

    if (obj->options.flags & OPTION_ACL2MACCESS) {
        /* apply the mask and adjust user and group permissions */
        ma->ma_user |= (acl_rights & mask);
        ma->ma_group = (group_rights & mask);
    }

    if (obj->options.flags & OPTION_ACL2MODE) {
        /* update st_mode to properly reflect group permissions */
        sb->st_mode &= ~S_IRWXG;
        if (ma->ma_group & AR_USEARCH)
            sb->st_mode |= S_IXGRP;
        if (ma->ma_group & AR_UWRITE)
            sb->st_mode |= S_IWGRP;
        if (ma->ma_group & AR_UREAD)
            sb->st_mode |= S_IRGRP;
    }

EC_CLEANUP:
    if (acl) acl_free(acl);

    EC_EXIT;
}

#if 0
/*!
 * Add entries of one acl to another acl
 *
 * @param aclp   (rw) destination acl where new aces will be added
 * @param acl    (r)  source acl where aces will be copied from
 *
 * @returns 0 on success, -1 on error
 */
static int acl_add_acl(acl_t *aclp, const acl_t acl)
{
    EC_INIT;
    int id;
    acl_entry_t se, de;

    for (id = ACL_FIRST_ENTRY; acl_get_entry(acl, id, &se) == 1; id = ACL_NEXT_ENTRY) {
        EC_ZERO_LOG_ERR(acl_create_entry(aclp, &de), AFPERR_MISC);
        EC_ZERO_LOG_ERR(acl_copy_entry(de, se), AFPERR_MISC);
    }

EC_CLEANUP:
    EC_EXIT;
}
#endif

/*!
 * Map Darwin ACE rights to POSIX 1e perm
 *
 * We can only map few rights:
 *   DARWIN_ACE_READ_DATA                    -> ACL_READ
 *   DARWIN_ACE_WRITE_DATA                   -> ACL_WRITE
 *   DARWIN_ACE_DELETE_CHILD & (is_dir == 1) -> ACL_WRITE
 *   DARWIN_ACE_EXECUTE                      -> ACL_EXECUTE
 *
 * @param entry             (rw) result of the mapping
 * @param is_dir            (r) 1 for dirs, 0 for files
 *
 * @returns mapping result as acl_perm_t, -1 on error
 */
static acl_perm_t map_darwin_right_to_posix_permset(uint32_t darwin_ace_rights, int is_dir)
{
    acl_perm_t perm = 0;

    if (darwin_ace_rights & DARWIN_ACE_READ_DATA)
        perm |= ACL_READ;

    if (darwin_ace_rights & (DARWIN_ACE_WRITE_DATA | (is_dir ? DARWIN_ACE_DELETE_CHILD : 0)))
        perm |= ACL_WRITE;

    if (darwin_ace_rights & DARWIN_ACE_EXECUTE)
        perm |= ACL_EXECUTE;

    return perm;
}

/*!
 * Add a ACL_USER or ACL_GROUP permission to an ACL, extending existing ACEs
 *
 * Add a permission of "type" for user or group "id" to an ACL. Scan the ACL
 * for existing permissions for this type/id, if there is one add the perm,
 * otherwise create a new ACL entry.
 * perm can be or'ed ACL_READ, ACL_WRITE and ACL_EXECUTE.  
 *
 * @param aclp     (rw) pointer to ACL
 * @param type     (r)  acl_tag_t of ACL_USER or ACL_GROUP
 * @param id       (r)  uid_t uid for ACL_USER, or gid casted to uid_t for ACL_GROUP
 * @param perm     (r)  acl_perm_t permissions to add
 *
 * @returns 0 on success, -1 on failure
 */
static int posix_acl_add_perm(acl_t *aclp, acl_tag_t type, uid_t id, acl_perm_t perm)
{
    EC_INIT;
    uid_t *eid = NULL;
    acl_entry_t e;
    acl_tag_t tag;
    int entry_id = ACL_FIRST_ENTRY;
    acl_permset_t permset;

    int found = 0;
    for ( ; (! found) && acl_get_entry(*aclp, entry_id, &e) == 1; entry_id = ACL_NEXT_ENTRY) {
        EC_ZERO_LOG(acl_get_tag_type(e, &tag));
        if (tag != ACL_USER && tag != ACL_GROUP)
            continue;
        EC_NULL_LOG(eid = (uid_t *)acl_get_qualifier(e));
        if ((*eid == id) && (type == tag)) {
            /* found an ACE for this type/id */
            found = 1;
            EC_ZERO_LOG(acl_get_permset(e, &permset));
            EC_ZERO_LOG(acl_add_perm(permset, perm));
        }

        acl_free(eid);
        eid = NULL;
    }

    if ( ! found) {
        /* there was no existing ACE for this type/id */
        EC_ZERO_LOG(acl_create_entry(aclp, &e));
        EC_ZERO_LOG(acl_set_tag_type(e, type));
        EC_ZERO_LOG(acl_set_qualifier(e, &id));
        EC_ZERO_LOG(acl_get_permset(e, &permset));
        EC_ZERO_LOG(acl_clear_perms(permset));
        EC_ZERO_LOG(acl_add_perm(permset, perm));
        EC_ZERO_LOG(acl_set_permset(e, permset));
    }

EC_CLEANUP:
    if (eid) acl_free(eid);

    EC_EXIT;
}

/*!
 * Map Darwin ACL to POSIX ACL.
 *
 * aclp must point to a acl_init'ed acl_t or an acl_t that can eg contain default ACEs.
 * Mapping pecularities:
 * - we create a default ace (which inherits to files and dirs) if either
     DARWIN_ACE_FLAGS_FILE_INHERIT or DARWIN_ACE_FLAGS_DIRECTORY_INHERIT is requested
 * - we throw away DARWIN_ACE_FLAGS_LIMIT_INHERIT (can't be mapped), thus the ACL will
 *   not be limited
 *
 * @param darwin_aces        (r)  pointer to darwin_aces buffer
 * @param def_aclp           (rw) directories: pointer to an initialized acl_t with
                                  the default acl files: *def_aclp will be NULL
 * @param acc_aclp           (rw) pointer to an initialized acl_t with the access acl
 * @param ace_count          (r)  number of ACEs in darwin_aces buffer
 * @param default_acl_flags  (rw) flags to indicate if the object has a basic default
 *                                acl or an extended default acl.
 *
 * @returns 0 on success storing the result in aclp, -1 on error. default_acl_flags
 * is set to HAS_DEFAULT_ACL|HAS_EXT_DEFAULT_ACL in case there is at least one
 * extended default ace. Otherwise default_acl_flags is left unchanged.
 */
static int map_aces_darwin_to_posix(const darwin_ace_t *darwin_aces,
                                    acl_t *def_aclp,
                                    acl_t *acc_aclp,
                                    int ace_count,
                                    uint32_t *default_acl_flags)
{
    EC_INIT;
    char *name = NULL;
    uuidtype_t uuidtype;
    struct passwd *pwd;
    struct group *grp;
    uid_t id;
    uint32_t darwin_ace_flags, darwin_ace_rights;
    acl_tag_t tag;
    acl_perm_t perm;

    for ( ; ace_count != 0; ace_count--, darwin_aces++) {
        /* type: allow/deny, posix only has allow */
        darwin_ace_flags = ntohl(darwin_aces->darwin_ace_flags);
        if ( ! (darwin_ace_flags & DARWIN_ACE_FLAGS_PERMIT))
            continue;

        darwin_ace_rights = ntohl(darwin_aces->darwin_ace_rights);
        perm = map_darwin_right_to_posix_permset(darwin_ace_rights, (*def_aclp != NULL));
        if (perm == 0)
            continue;       /* dont add empty perm */

        LOG(log_debug, logtype_afpd, "map_ace: no: %u, flags: %08x, darwin: %08x, posix: %02x",
            ace_count, darwin_ace_flags, darwin_ace_rights, perm);

         /* uid/gid */
        EC_ZERO_LOG(getnamefromuuid(darwin_aces->darwin_ace_uuid, &name, &uuidtype));
        switch (uuidtype) {
        case UUID_USER:
            EC_NULL_LOG(pwd = getpwnam(name));
            tag = ACL_USER;
            id = pwd->pw_uid;
            LOG(log_debug, logtype_afpd, "map_ace: name: %s, uid: %u", name, id);
            break;
        case UUID_GROUP:
            EC_NULL_LOG(grp = getgrnam(name));
            tag = ACL_GROUP;
            id = (uid_t)(grp->gr_gid);
            LOG(log_debug, logtype_afpd, "map_ace: name: %s, gid: %u", name, id);
            break;
        default:
            continue;
        }
        free(name);
        name = NULL;

        if (darwin_ace_flags & DARWIN_ACE_INHERIT_CONTROL_FLAGS) {
            if (*def_aclp == NULL) {
                /* ace request inheritane but we haven't got a default acl pointer */
                LOG(log_warning, logtype_afpd, "map_acl: unexpected ACE, flags: 0x%04x",
                    darwin_ace_flags);
                EC_FAIL;
            }
            /* add it as default ace */
            EC_ZERO_LOG(posix_acl_add_perm(def_aclp, tag, id, perm));
            *default_acl_flags = (HAS_DEFAULT_ACL|HAS_EXT_DEFAULT_ACL);

            if (! (darwin_ace_flags & DARWIN_ACE_FLAGS_ONLY_INHERIT))
                /* if it not a "inherit only" ace, it must be added as access aces too */
                EC_ZERO_LOG(posix_acl_add_perm(acc_aclp, tag, id, perm));
        } else {
            EC_ZERO_LOG(posix_acl_add_perm(acc_aclp, tag, id, perm));
        }
    }

EC_CLEANUP:
    if (name)
        free(name);

    EC_EXIT;
}

/*
 * Map ACEs from POSIX to Darwin.
 * type is either POSIX_DEFAULT_2_DARWIN or POSIX_ACCESS_2_DARWIN, cf. acl_get_file.
 * Return number of mapped ACES, -1 on error.
 */
static int map_acl_posix_to_darwin(int type, const acl_t acl, darwin_ace_t *darwin_aces)
{
    EC_INIT;
    int mapped_aces = 0;
    int entry_id = ACL_FIRST_ENTRY;
    acl_entry_t e;
    acl_tag_t tag;
    uid_t *uid = NULL;
    gid_t *gid = NULL;
    struct passwd *pwd = NULL;
    struct group *grp = NULL;
    uint32_t flags;
    uint32_t rights, maskrights = 0;
    darwin_ace_t *saved_darwin_aces = darwin_aces;

    LOG(log_maxdebug, logtype_afpd, "map_aces_posix_to_darwin(%s)",
        (type & MAP_MASK) == POSIX_DEFAULT_2_DARWIN ?
        "POSIX_DEFAULT_2_DARWIN" : "POSIX_ACCESS_2_DARWIN");

    /* itereate through all ACEs */
    while (acl_get_entry(acl, entry_id, &e) == 1) {
        entry_id = ACL_NEXT_ENTRY;

        /* get ACE type */
        EC_ZERO_LOG(acl_get_tag_type(e, &tag));

        /* we return user and group ACE */
        switch (tag) {
        case ACL_USER:
            EC_NULL_LOG(uid = (uid_t *)acl_get_qualifier(e));
            EC_NULL_LOG(pwd = getpwuid(*uid));
            LOG(log_debug, logtype_afpd, "map_aces_posix_to_darwin: uid: %d -> name: %s",
                *uid, pwd->pw_name);
            EC_ZERO_LOG(getuuidfromname(pwd->pw_name, UUID_USER, darwin_aces->darwin_ace_uuid));
            acl_free(uid);
            uid = NULL;
            break;

        case ACL_GROUP:
            EC_NULL_LOG(gid = (gid_t *)acl_get_qualifier(e));
            EC_NULL_LOG(grp = getgrgid(*gid));
            LOG(log_debug, logtype_afpd, "map_aces_posix_to_darwin: gid: %d -> name: %s",
                *gid, grp->gr_name);
            EC_ZERO_LOG(getuuidfromname(grp->gr_name, UUID_GROUP, darwin_aces->darwin_ace_uuid));
            acl_free(gid);
            gid = NULL;
            break;

        case ACL_MASK:
            maskrights = posix_permset_to_darwin_rights(e, type & IS_DIR);
            continue;

        default:
            continue;
        }

        /* flags */
        flags = DARWIN_ACE_FLAGS_PERMIT;
        if ((type & MAP_MASK) == POSIX_DEFAULT_2_DARWIN)
            flags |= DARWIN_ACE_FLAGS_FILE_INHERIT
                | DARWIN_ACE_FLAGS_DIRECTORY_INHERIT
                | DARWIN_ACE_FLAGS_ONLY_INHERIT;
        darwin_aces->darwin_ace_flags = htonl(flags);

        /* rights */
        rights = posix_permset_to_darwin_rights(e, type & IS_DIR);
        darwin_aces->darwin_ace_rights = htonl(rights);

        darwin_aces++;
        mapped_aces++;
    } /* while */

    /* Loop through the mapped ACE buffer once again, applying the mask */
    for (int i = mapped_aces; i > 0; i--) {
        saved_darwin_aces->darwin_ace_rights &= htonl(maskrights);
        saved_darwin_aces++;
    }

    EC_STATUS(mapped_aces);

EC_CLEANUP:
    if (uid) acl_free(uid);
    if (gid) acl_free(gid);
    EC_EXIT;
}
#endif

/*
 * Multiplex ACL mapping (SOLARIS_2_DARWIN, DARWIN_2_SOLARIS, POSIX_2_DARWIN, DARWIN_2_POSIX).
 * Reads from 'aces' buffer, writes to 'rbuf' buffer.
 * Caller must provide buffer.
 * Darwin ACEs are read and written in network byte order.
 * Needs to know how many ACEs are in the ACL (ace_count) for Solaris ACLs.
 * Ignores trivial ACEs.
 * Return no of mapped ACEs or -1 on error.
 */
static int map_acl(int type, void *acl, darwin_ace_t *buf, int ace_count)
{
    int mapped_aces;

    LOG(log_debug9, logtype_afpd, "map_acl: BEGIN");

    switch (type & MAP_MASK) {

#ifdef HAVE_SOLARIS_ACLS
    case SOLARIS_2_DARWIN:
        mapped_aces = map_aces_solaris_to_darwin( acl, buf, ace_count);
        break;

    case DARWIN_2_SOLARIS:
        mapped_aces = map_aces_darwin_to_solaris( buf, acl, ace_count);
        break;
#endif /* HAVE_SOLARIS_ACLS */

#ifdef HAVE_POSIX_ACLS
    case POSIX_DEFAULT_2_DARWIN:
        mapped_aces = map_acl_posix_to_darwin(type, (const acl_t)acl, buf);
        break;

    case POSIX_ACCESS_2_DARWIN:
        mapped_aces = map_acl_posix_to_darwin(type, (const acl_t)acl, buf);
        break;

    case DARWIN_2_POSIX_DEFAULT:
        break;

    case DARWIN_2_POSIX_ACCESS:
        break;
#endif /* HAVE_POSIX_ACLS */

    default:
        mapped_aces = -1;
        break;
    }

    LOG(log_debug9, logtype_afpd, "map_acl: END");
    return mapped_aces;
}

/* Get ACL from object omitting trivial ACEs. Map to Darwin ACL style and
   store Darwin ACL at rbuf. Add length of ACL written to rbuf to *rbuflen.
   Returns 0 on success, -1 on error. */
static int get_and_map_acl(char *name, char *rbuf, size_t *rbuflen)
{
    EC_INIT;
    int mapped_aces = 0;
    int dirflag;
    char *darwin_ace_count = rbuf;
#ifdef HAVE_SOLARIS_ACLS
    int ace_count = 0;
    ace_t *aces = NULL;
#endif
#ifdef HAVE_POSIX_ACLS
    struct stat st;
#endif
    LOG(log_debug9, logtype_afpd, "get_and_map_acl: BEGIN");

    /* Skip length and flags */
    rbuf += 4;
    *rbuf = 0;
    rbuf += 4;

#ifdef HAVE_SOLARIS_ACLS
    EC_NEG1(ace_count = get_nfsv4_acl(name, &aces));
    EC_NEG1(mapped_aces = map_acl(SOLARIS_2_DARWIN, aces, (darwin_ace_t *)rbuf, ace_count));
#endif /* HAVE_SOLARIS_ACLS */

#ifdef HAVE_POSIX_ACLS
    acl_t defacl = NULL , accacl = NULL;

    /* stat to check if its a dir */
    EC_ZERO_LOG(lstat(name, &st));

    /* if its a dir, check for default acl too */
    dirflag = 0;
    if (S_ISDIR(st.st_mode)) {
        dirflag = IS_DIR;
        EC_NULL_LOG(defacl = acl_get_file(name, ACL_TYPE_DEFAULT));
        EC_NEG1(mapped_aces = map_acl(POSIX_DEFAULT_2_DARWIN | dirflag,
                                      defacl,
                                      (darwin_ace_t *)rbuf,
                                      0));
    }

    EC_NULL_LOG(accacl = acl_get_file(name, ACL_TYPE_ACCESS));

    int tmp;
    EC_NEG1(tmp = map_acl(POSIX_ACCESS_2_DARWIN | dirflag,
                          accacl,
                          (darwin_ace_t *)(rbuf + mapped_aces * sizeof(darwin_ace_t)),
                          0));
    mapped_aces += tmp;
#endif /* HAVE_POSIX_ACLS */

    LOG(log_debug, logtype_afpd, "get_and_map_acl: mapped %d ACEs", mapped_aces);

    *rbuflen += sizeof(darwin_acl_header_t) + (mapped_aces * sizeof(darwin_ace_t));
    mapped_aces = htonl(mapped_aces);
    memcpy(darwin_ace_count, &mapped_aces, sizeof(uint32_t));

    EC_STATUS(0);

EC_CLEANUP:
#ifdef HAVE_SOLARIS_ACLS
    if (aces) free(aces);
#endif
#ifdef HAVE_POSIX_ACLS
    if (defacl) acl_free(defacl);
    if (accacl) acl_free(accacl);
#endif /* HAVE_POSIX_ACLS */

    LOG(log_debug9, logtype_afpd, "get_and_map_acl: END");

    EC_EXIT;
}

/* Removes all non-trivial ACLs from object. Returns full AFPERR code. */
static int remove_acl(const struct vol *vol,const char *path, int dir)
{
    int ret = AFP_OK;

#if (defined HAVE_SOLARIS_ACLS || defined HAVE_POSIX_ACLS)
    /* Ressource etc. first */
    if ((ret = vol->vfs->vfs_remove_acl(vol, path, dir)) != AFP_OK)
        return ret;
    /* now the data fork or dir */
    ret = remove_acl_vfs(path);
#endif
    return ret;
}

/*
  Set ACL. Subtleties:
  - the client sends a complete list of ACEs, not only new ones. So we dont need to do
  any combination business (one exception being 'kFileSec_Inherit': see next)
  - client might request that we add inherited ACEs via 'kFileSec_Inherit'.
  We will store inherited ACEs first, which is Darwins canonical order.
  - returns AFPerror code
*/
#ifdef HAVE_SOLARIS_ACLS
static int set_acl(const struct vol *vol,
                   char *name,
                   int inherit,
                   darwin_ace_t *daces,
                   uint32_t ace_count)
{
    EC_INIT;
    int i, nfsv4_ace_count;
    int tocopy_aces_count = 0, new_aces_count = 0, trivial_ace_count = 0;
    ace_t *old_aces, *new_aces = NULL;
    uint16_t flags;

    LOG(log_debug9, logtype_afpd, "set_acl: BEGIN");

    if (inherit)
        /* inherited + trivial ACEs */
        flags = ACE_INHERITED_ACE | ACE_OWNER | ACE_GROUP | ACE_EVERYONE;
    else
        /* only trivial ACEs */
        flags = ACE_OWNER | ACE_GROUP | ACE_EVERYONE;

    /* Get existing ACL and count ACEs which have to be copied */
    if ((nfsv4_ace_count = get_nfsv4_acl(name, &old_aces)) == -1)
        return AFPERR_MISC;
    for ( i=0; i < nfsv4_ace_count; i++) {
        if (old_aces[i].a_flags & flags)
            tocopy_aces_count++;
    }

    /* Now malloc buffer exactly sized to fit all new ACEs */
    if ((new_aces = malloc((ace_count + tocopy_aces_count) * sizeof(ace_t))) == NULL) {
        LOG(log_error, logtype_afpd, "set_acl: malloc %s", strerror(errno));
        EC_STATUS(AFPERR_MISC);
        goto EC_CLEANUP;
    }

    /* Start building new ACL */

    /* Copy local inherited ACEs. Therefore we have 'Darwin canonical order' (see chmod there):
       inherited ACEs first. */
    if (inherit) {
        for (i=0; i < nfsv4_ace_count; i++) {
            if (old_aces[i].a_flags & ACE_INHERITED_ACE) {
                memcpy(&new_aces[new_aces_count], &old_aces[i], sizeof(ace_t));
                new_aces_count++;
            }
        }
    }
    LOG(log_debug7, logtype_afpd, "set_acl: copied %d inherited ACEs", new_aces_count);

    /* Now the ACEs from the client */
    if ((ret = (map_acl(DARWIN_2_SOLARIS,
                        &new_aces[new_aces_count],
                        daces,
                        ace_count))) == -1) {
        EC_STATUS(AFPERR_PARAM);
        goto EC_CLEANUP;
    }
    new_aces_count += ret;
    LOG(log_debug7, logtype_afpd, "set_acl: mapped %d ACEs from client", ret);

    /* Now copy the trivial ACEs */
    for (i=0; i < nfsv4_ace_count; i++) {
        if (old_aces[i].a_flags  & (ACE_OWNER | ACE_GROUP | ACE_EVERYONE)) {
            memcpy(&new_aces[new_aces_count], &old_aces[i], sizeof(ace_t));
            new_aces_count++;
            trivial_ace_count++;
        }
    }
    LOG(log_debug7, logtype_afpd, "set_acl: copied %d trivial ACEs", trivial_ace_count);

    /* Ressourcefork first */
    if ((ret = (vol->vfs->vfs_acl(vol, name, ACE_SETACL, new_aces_count, new_aces))) != 0) {
        LOG(log_debug, logtype_afpd, "set_acl: error setting acl: %s", strerror(errno));
        switch (errno) {
        case EACCES:
        case EPERM:
            EC_STATUS(AFPERR_ACCESS);
            break;
        case ENOENT:
            EC_STATUS(AFP_OK);
            break;
        default:
            EC_STATUS(AFPERR_MISC);
            break;
        }
        goto EC_CLEANUP;
    }
    if ((ret = (acl(name, ACE_SETACL, new_aces_count, new_aces))) != 0) {
        LOG(log_error, logtype_afpd, "set_acl: error setting acl: %s", strerror(errno));
        if (errno == (EACCES | EPERM))
            EC_STATUS(AFPERR_ACCESS);
        else if (errno == ENOENT)
            EC_STATUS(AFPERR_NOITEM);
        else
            EC_STATUS(AFPERR_MISC);
        goto EC_CLEANUP;
    }

    EC_STATUS(AFP_OK);

EC_CLEANUP:
    if (old_aces) free(old_aces);
    if (new_aces) free(new_aces);

    LOG(log_debug9, logtype_afpd, "set_acl: END");
    EC_EXIT;
}
#endif /* HAVE_SOLARIS_ACLS */

#ifdef HAVE_POSIX_ACLS
#ifndef HAVE_ACL_FROM_MODE
static acl_t acl_from_mode(mode_t mode)
{
    acl_t acl;
    acl_entry_t entry;
    acl_permset_t permset;

    if (!(acl = acl_init(3)))
        return NULL;

    if (acl_create_entry(&acl, &entry) != 0)
        goto error;
    acl_set_tag_type(entry, ACL_USER_OBJ);
    acl_get_permset(entry, &permset);
    acl_clear_perms(permset);
    if (mode & S_IRUSR)
        acl_add_perm(permset, ACL_READ);
    if (mode & S_IWUSR)
        acl_add_perm(permset, ACL_WRITE);
    if (mode & S_IXUSR)
        acl_add_perm(permset, ACL_EXECUTE);
    acl_set_permset(entry, permset);

    if (acl_create_entry(&acl, &entry) != 0)
        goto error;
    acl_set_tag_type(entry, ACL_GROUP_OBJ);
    acl_get_permset(entry, &permset);
    acl_clear_perms(permset);
    if (mode & S_IRGRP)
        acl_add_perm(permset, ACL_READ);
    if (mode & S_IWGRP)
        acl_add_perm(permset, ACL_WRITE);
    if (mode & S_IXGRP)
        acl_add_perm(permset, ACL_EXECUTE);
    acl_set_permset(entry, permset);

    if (acl_create_entry(&acl, &entry) != 0)
        goto error;
    acl_set_tag_type(entry, ACL_OTHER);
    acl_get_permset(entry, &permset);
    acl_clear_perms(permset);
    if (mode & S_IROTH)
        acl_add_perm(permset, ACL_READ);
    if (mode & S_IWOTH)
        acl_add_perm(permset, ACL_WRITE);
    if (mode & S_IXOTH)
        acl_add_perm(permset, ACL_EXECUTE);
    acl_set_permset(entry, permset);

    return acl;

error:
    acl_free(acl);
    return NULL;
}
#endif

static int set_acl(const struct vol *vol,
                   const char *name,
                   int inherit _U_,
                   darwin_ace_t *daces,
                   uint32_t ace_count)
{
    EC_INIT;
    struct stat st;
    acl_t default_acl = NULL;
    acl_t access_acl = NULL;
    acl_entry_t entry;
    acl_tag_t tag;
    int entry_id = ACL_FIRST_ENTRY;
    /* flags to indicate if the object has a minimal default acl and/or an extended
     * default acl.
     */
    uint32_t default_acl_flags = 0;

    LOG(log_maxdebug, logtype_afpd, "set_acl: BEGIN");

    EC_NULL_LOG_ERR(access_acl = acl_get_file(name, ACL_TYPE_ACCESS), AFPERR_MISC);

    /* Iterate through acl and remove all extended acl entries. */
    while (acl_get_entry(access_acl, entry_id, &entry) == 1) {
        entry_id = ACL_NEXT_ENTRY;
        EC_ZERO_LOG(acl_get_tag_type(entry, &tag));

        if ((tag == ACL_USER) || (tag == ACL_GROUP) || (tag == ACL_MASK)) {
            EC_ZERO_LOG_ERR(acl_delete_entry(access_acl, entry), AFPERR_MISC);
        }
    } /* while */

   /* In case we are acting on a directory prepare a default acl. For files default_acl will be NULL.
    * If the directory already has a default acl it will be preserved.
    */
   EC_ZERO_LOG_ERR(lstat(name, &st), AFPERR_NOOBJ);

   if (S_ISDIR(st.st_mode)) {
       default_acl = acl_get_file(name, ACL_TYPE_DEFAULT);

       if (default_acl) {
           /* If default_acl is not empty then the dir has a default acl. */
           if (acl_get_entry(default_acl, ACL_FIRST_ENTRY, &entry) == 1)
               default_acl_flags = HAS_DEFAULT_ACL;

           acl_free(default_acl);
       }
       default_acl = acl_dup(access_acl);
    }
    /* adds the clients aces */
    EC_ZERO_ERR(map_aces_darwin_to_posix(daces, &default_acl, &access_acl, ace_count, &default_acl_flags), AFPERR_MISC);

    /* calcuate ACL mask */
    EC_ZERO_LOG_ERR(acl_calc_mask(&access_acl), AFPERR_MISC);

    /* is it ok? */
    EC_ZERO_LOG_ERR(acl_valid(access_acl), AFPERR_MISC);

    /* set it */
    EC_ZERO_LOG_ERR(acl_set_file(name, ACL_TYPE_ACCESS, access_acl), AFPERR_MISC);
    EC_ZERO_LOG_ERR(vol->vfs->vfs_acl(vol, name, ACL_TYPE_ACCESS, 0, access_acl), AFPERR_MISC);

    if (default_acl) {
        /* If the dir has an extended default acl it's ACL_MASK must be updated.*/
        if (default_acl_flags & HAS_EXT_DEFAULT_ACL)
            EC_ZERO_LOG_ERR(acl_calc_mask(&default_acl), AFPERR_MISC);

        if (default_acl_flags) {
            EC_ZERO_LOG_ERR(acl_valid(default_acl), AFPERR_MISC);
            EC_ZERO_LOG_ERR(acl_set_file(name, ACL_TYPE_DEFAULT, default_acl), AFPERR_MISC);
            EC_ZERO_LOG_ERR(vol->vfs->vfs_acl(vol, name, ACL_TYPE_DEFAULT, 0, default_acl), AFPERR_MISC);
        }
    }

EC_CLEANUP:
    if (access_acl) acl_free(access_acl);
    if (default_acl) acl_free(default_acl);

    LOG(log_maxdebug, logtype_afpd, "set_acl: END");
    EC_EXIT;
}
#endif /* HAVE_POSIX_ACLS */

/*!
 * Checks if a given UUID has requested_rights(type darwin_ace_rights) for path.
 *
 * Note: this gets called frequently and is a good place for optimizations !
 *
 * @param vol              (r) volume
 * @param dir              (rw) directory
 * @param path             (r) path to filesystem object
 * @param uuid             (r) UUID of user
 * @param requested_rights (r) requested Darwin ACE
 *
 * @returns                    AFP result code
*/
static int check_acl_access(const AFPObj *obj,
                            const struct vol *vol,
                            struct dir *dir,
                            const char *path,
                            const uuidp_t uuid,
                            uint32_t requested_rights)
{
    int            ret;
    uint32_t       allowed_rights = 0;
    char           *username = NULL;
    struct stat    st;
    bstring        parent = NULL;
    int            is_dir;

    LOG(log_maxdebug, logtype_afpd, "check_acl_access(dir: \"%s\", path: \"%s\", curdir: \"%s\", 0x%08x)",
        cfrombstr(dir->d_fullpath), path, getcwdpath(), requested_rights);

    AFP_ASSERT(vol);

    /* This check is not used anymore, as OS X Server seems to be ignoring too */
#if 0
    /* Get uid or gid from UUID */
    EC_ZERO_ERR(getnamefromuuid(uuid, &username, &uuidtype), AFPERR_PARAM);
    switch (uuidtype) {
    case UUID_USER:
        break;
    case UUID_GROUP:
        LOG(log_warning, logtype_afpd, "check_acl_access: afp_access not supported for groups");
        EC_STATUS(AFPERR_MISC);
        goto EC_CLEANUP;
    default:
        EC_STATUS(AFPERR_MISC);
        goto EC_CLEANUP;
    }
#endif

    EC_ZERO_LOG_ERR(ostat(path, &st, vol_syml_opt(vol)), AFPERR_PARAM);

    is_dir = !strcmp(path, ".");

    if (is_dir && (curdir->d_rights_cache != 0xffffffff)) {
        /* its a dir and the cache value is valid */
        allowed_rights = curdir->d_rights_cache;
        LOG(log_debug, logtype_afpd, "check_access: allowed rights from dircache: 0x%08x", allowed_rights);
    } else {
#ifdef HAVE_SOLARIS_ACLS
        EC_ZERO_LOG(solaris_acl_rights(obj, path, &st, NULL, &allowed_rights));
#endif
#ifdef HAVE_POSIX_ACLS
        EC_ZERO_LOG(posix_acl_rights(obj, path, &st, &allowed_rights));
#endif
        /*
         * The DARWIN_ACE_DELETE right might implicitly result from write acces to the parent
         * directory. As it seems the 10.6 AFP client is puzzled when this right is not
         * allowed where a delete would succeed because the parent dir gives write perms.
         * So we check the parent dir for write access and set the right accordingly.
         * Currentyl acl2ownermode calls us with dir = NULL, because it doesn't make sense
         * there to do this extra check -- afaict.
         */
        if (vol && dir && (requested_rights & DARWIN_ACE_DELETE)) {
            int i;
            uint32_t parent_rights = 0;

            if (curdir->d_did == DIRDID_ROOT_PARENT) {
                /* use volume path */
                EC_NULL_LOG_ERR(parent = bfromcstr(vol->v_path), AFPERR_MISC);
            } else {
                /* build path for parent */
                EC_NULL_LOG_ERR(parent = bstrcpy(curdir->d_fullpath), AFPERR_MISC);
                EC_ZERO_LOG_ERR(bconchar(parent, '/'), AFPERR_MISC);
                EC_ZERO_LOG_ERR(bcatcstr(parent, path), AFPERR_MISC);
                EC_NEG1_LOG_ERR(i = bstrrchr(parent, '/'), AFPERR_MISC);
                EC_ZERO_LOG_ERR(binsertch(parent, i, 1, 0), AFPERR_MISC);
            }

            LOG(log_debug, logtype_afpd,"parent: %s", cfrombstr(parent));
            EC_ZERO_LOG_ERR(lstat(cfrombstr(parent), &st), AFPERR_MISC);

#ifdef HAVE_SOLARIS_ACLS
            EC_ZERO_LOG(solaris_acl_rights(obj, cfrombstr(parent), &st, NULL, &parent_rights));
#endif
#ifdef HAVE_POSIX_ACLS
            EC_ZERO_LOG(posix_acl_rights(obj, path, &st, &parent_rights));
#endif
            if (parent_rights & (DARWIN_ACE_WRITE_DATA | DARWIN_ACE_DELETE_CHILD))
                allowed_rights |= DARWIN_ACE_DELETE; /* man, that was a lot of work! */
        }

        if (is_dir) {
            /* Without DARWIN_ACE_DELETE set OS X 10.6 refuses to rename subdirectories in a
             * directory.
             */
            if (allowed_rights & DARWIN_ACE_ADD_SUBDIRECTORY)
                allowed_rights |= DARWIN_ACE_DELETE;

            curdir->d_rights_cache = allowed_rights;
        }
        LOG(log_debug, logtype_afpd, "allowed rights: 0x%08x", allowed_rights);
    }

    if ((requested_rights & allowed_rights) != requested_rights) {
        LOG(log_debug, logtype_afpd, "some requested right wasn't allowed: 0x%08x / 0x%08x",
            requested_rights, allowed_rights);
        EC_STATUS(AFPERR_ACCESS);
    } else {
        LOG(log_debug, logtype_afpd, "all requested rights are allowed: 0x%08x",
            requested_rights);
        EC_STATUS(AFP_OK);
    }

EC_CLEANUP:
    if (username) free(username);
    if (parent) bdestroy(parent);

    EC_EXIT;
}

/********************************************************
 * Interface
 ********************************************************/

int afp_access(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    int         ret;
    struct vol      *vol;
    struct dir      *dir;
    uint32_t            did, darwin_ace_rights;
    uint16_t        vid;
    struct path         *s_path;
    uuidp_t             uuid;

    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof( vid ));
    ibuf += sizeof(vid);
    if (NULL == ( vol = getvolbyvid( vid ))) {
        LOG(log_error, logtype_afpd, "afp_access: error getting volid:%d", vid);
        return AFPERR_NOOBJ;
    }

    memcpy(&did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did ))) {
        LOG(log_error, logtype_afpd, "afp_access: error getting did:%d", did);
        return afp_errno;
    }

    /* Skip bitmap */
    ibuf += 2;

    /* Store UUID address */
    uuid = (uuidp_t)ibuf;
    ibuf += UUID_BINSIZE;

    /* Store ACE rights */
    memcpy(&darwin_ace_rights, ibuf, 4);
    darwin_ace_rights = ntohl(darwin_ace_rights);
    ibuf += 4;

    /* get full path and handle file/dir subtleties in netatalk code*/
    if (NULL == ( s_path = cname( vol, dir, &ibuf ))) {
        LOG(log_error, logtype_afpd, "afp_getacl: cname got an error!");
        return AFPERR_NOOBJ;
    }
    if (!s_path->st_valid)
        of_statdir(vol, s_path);
    if ( s_path->st_errno != 0 ) {
        LOG(log_error, logtype_afpd, "afp_getacl: cant stat");
        return AFPERR_NOOBJ;
    }

    ret = check_acl_access(obj, vol, dir, s_path->u_name, uuid, darwin_ace_rights);

    return ret;
}

int afp_getacl(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol      *vol;
    struct dir      *dir;
    int         ret;
    uint32_t           did;
    uint16_t        vid, bitmap;
    struct path         *s_path;
    struct passwd       *pw;
    struct group        *gr;

    LOG(log_debug9, logtype_afpd, "afp_getacl: BEGIN");
    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof( vid ));
    ibuf += sizeof(vid);
    if (NULL == ( vol = getvolbyvid( vid ))) {
        LOG(log_error, logtype_afpd, "afp_getacl: error getting volid:%d", vid);
        return AFPERR_NOOBJ;
    }

    memcpy(&did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did ))) {
        LOG(log_error, logtype_afpd, "afp_getacl: error getting did:%d", did);
        return afp_errno;
    }

    memcpy(&bitmap, ibuf, sizeof( bitmap ));
    memcpy(rbuf, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );
    rbuf += sizeof( bitmap );
    *rbuflen += sizeof( bitmap );

    /* skip maxreplysize */
    ibuf += 4;

    /* get full path and handle file/dir subtleties in netatalk code*/
    if (NULL == ( s_path = cname( vol, dir, &ibuf ))) {
        LOG(log_error, logtype_afpd, "afp_getacl: cname got an error!");
        return AFPERR_NOOBJ;
    }
    if (!s_path->st_valid)
        of_statdir(vol, s_path);
    if ( s_path->st_errno != 0 ) {
        LOG(log_error, logtype_afpd, "afp_getacl: cant stat");
        return AFPERR_NOOBJ;
    }

    /* Shall we return owner UUID ? */
    if (bitmap & kFileSec_UUID) {
        LOG(log_debug, logtype_afpd, "afp_getacl: client requested files owner user UUID");
        if (NULL == (pw = getpwuid(s_path->st.st_uid))) {
            LOG(log_debug, logtype_afpd, "afp_getacl: local uid: %u", s_path->st.st_uid);
            localuuid_from_id((unsigned char *)rbuf, UUID_USER, s_path->st.st_uid);
        } else {
            LOG(log_debug, logtype_afpd, "afp_getacl: got uid: %d, name: %s", s_path->st.st_uid, pw->pw_name);
            if ((ret = getuuidfromname(pw->pw_name, UUID_USER, (unsigned char *)rbuf)) != 0)
                return AFPERR_MISC;
        }
        rbuf += UUID_BINSIZE;
        *rbuflen += UUID_BINSIZE;
    }

    /* Shall we return group UUID ? */
    if (bitmap & kFileSec_GRPUUID) {
        LOG(log_debug, logtype_afpd, "afp_getacl: client requested files owner group UUID");
        if (NULL == (gr = getgrgid(s_path->st.st_gid))) {
            LOG(log_debug, logtype_afpd, "afp_getacl: local gid: %u", s_path->st.st_gid);
            localuuid_from_id((unsigned char *)rbuf, UUID_GROUP, s_path->st.st_gid);
        } else {
            LOG(log_debug, logtype_afpd, "afp_getacl: got gid: %d, name: %s", s_path->st.st_gid, gr->gr_name);
            if ((ret = getuuidfromname(gr->gr_name, UUID_GROUP, (unsigned char *)rbuf)) != 0)
                return AFPERR_MISC;
        }
        rbuf += UUID_BINSIZE;
        *rbuflen += UUID_BINSIZE;
    }

    /* Shall we return ACL ? */
    if (bitmap & kFileSec_ACL) {
        LOG(log_debug, logtype_afpd, "afp_getacl: client requested files ACL");
        if (get_and_map_acl(s_path->u_name, rbuf, rbuflen) != 0) {
            LOG(log_error, logtype_afpd, "afp_getacl(\"%s/%s\"): mapping error",
                getcwdpath(), s_path->u_name);
            return AFPERR_MISC;
        }
    }

    LOG(log_debug9, logtype_afpd, "afp_getacl: END");
    return AFP_OK;
}

int afp_setacl(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol      *vol;
    struct dir      *dir;
    int         ret;
    uint32_t            did;
    uint16_t        vid, bitmap;
    struct path         *s_path;

    LOG(log_debug9, logtype_afpd, "afp_setacl: BEGIN");
    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof( vid ));
    ibuf += sizeof(vid);
    if (NULL == ( vol = getvolbyvid( vid ))) {
        LOG(log_error, logtype_afpd, "afp_setacl: error getting volid:%d", vid);
        return AFPERR_NOOBJ;
    }

    memcpy(&did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did ))) {
        LOG(log_error, logtype_afpd, "afp_setacl: error getting did:%d", did);
        return afp_errno;
    }

    memcpy(&bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );

    /* get full path and handle file/dir subtleties in netatalk code*/
    if (NULL == ( s_path = cname( vol, dir, &ibuf ))) {
        LOG(log_error, logtype_afpd, "afp_setacl: cname got an error!");
        return AFPERR_NOOBJ;
    }
    if (!s_path->st_valid)
        of_statdir(vol, s_path);
    if ( s_path->st_errno != 0 ) {
        LOG(log_error, logtype_afpd, "afp_setacl: cant stat");
        return AFPERR_NOOBJ;
    }
    LOG(log_debug, logtype_afpd, "afp_setacl: unixname: %s", s_path->u_name);

    /* Padding? */
    if ((unsigned long)ibuf & 1)
        ibuf++;

    /* Start processing request */

    /* Change owner: dont even try */
    if (bitmap & kFileSec_UUID) {
        LOG(log_note, logtype_afpd, "afp_setacl: change owner request, discarded");
        ret = AFPERR_ACCESS;
        ibuf += UUID_BINSIZE;
    }

    /* Change group: certain changes might be allowed, so try it. FIXME: not implemented yet. */
    if (bitmap & kFileSec_UUID) {
        LOG(log_note, logtype_afpd, "afp_setacl: change group request, not supported");
        ret = AFPERR_PARAM;
        ibuf += UUID_BINSIZE;
    }

    /* Remove ACL ? */
    if (bitmap & kFileSec_REMOVEACL) {
        LOG(log_debug, logtype_afpd, "afp_setacl: Remove ACL request.");
        if ((ret = remove_acl(vol, s_path->u_name, S_ISDIR(s_path->st.st_mode))) != AFP_OK)
            LOG(log_error, logtype_afpd, "afp_setacl: error from remove_acl");
    }

    /* Change ACL ? */
    if (bitmap & kFileSec_ACL) {
        LOG(log_debug, logtype_afpd, "afp_setacl: Change ACL request.");
        /*  Get no of ACEs the client put on the wire */
        uint32_t ace_count;
        memcpy(&ace_count, ibuf, sizeof(uint32_t));
        ace_count = htonl(ace_count);
        ibuf += 8;      /* skip ACL flags (see acls.h) */

        ret = set_acl(vol,
                      s_path->u_name,
                      (bitmap & kFileSec_Inherit),
                      (darwin_ace_t *)ibuf,
                      ace_count);
        if (ret == 0)
            ret = AFP_OK;
        else {
            LOG(log_warning, logtype_afpd, "afp_setacl(\"%s/%s\"): error",
                getcwdpath(), s_path->u_name);
            ret = AFPERR_MISC;
        }
    }

    LOG(log_debug9, logtype_afpd, "afp_setacl: END");
    return ret;
}

/********************************************************************
 * ACL funcs interfacing with other parts
 ********************************************************************/

/*!
 * map ACL to user maccess
 *
 * This is the magic function that makes ACLs usable by calculating
 * the access granted by ACEs to the logged in user.
 */
int acltoownermode(const AFPObj *obj, const struct vol *vol, char *path, struct stat *st, struct maccess *ma)
{
    EC_INIT;

    if ( ! (obj->options.flags & (OPTION_ACL2MACCESS | OPTION_ACL2MODE))
         || ! (vol->v_flags & AFPVOL_ACLS))
         return 0;

    LOG(log_maxdebug, logtype_afpd, "acltoownermode(\"%s/%s\", 0x%02x)",
        getcwdpath(), path, ma->ma_user);

#ifdef HAVE_SOLARIS_ACLS
    EC_ZERO_LOG(solaris_acl_rights(obj, path, st, ma, NULL));
#endif

#ifdef HAVE_POSIX_ACLS
    EC_ZERO_LOG(posix_acls_to_uaperms(obj, path, st, ma));
#endif

    LOG(log_maxdebug, logtype_afpd, "resulting user maccess: 0x%02x group maccess: 0x%02x", ma->ma_user, ma->ma_group);

EC_CLEANUP:
    EC_EXIT;
}
