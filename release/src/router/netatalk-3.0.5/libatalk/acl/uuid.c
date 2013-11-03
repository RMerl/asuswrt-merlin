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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <arpa/inet.h>

#include <atalk/logger.h>
#include <atalk/afp.h>
#include <atalk/uuid.h>
#include <atalk/ldapconfig.h>
#include <atalk/util.h>

#include "aclldap.h"
#include "cache.h"

char *uuidtype[] = {"", "USER", "GROUP", "LOCAL"};

/********************************************************
 * Public helper function
 ********************************************************/

static unsigned char local_group_uuid[] = {0xab, 0xcd, 0xef,
                                           0xab, 0xcd, 0xef,
                                           0xab, 0xcd, 0xef,
                                           0xab, 0xcd, 0xef};

static unsigned char local_user_uuid[] = {0xff, 0xff, 0xee, 0xee, 0xdd, 0xdd,
                                          0xcc, 0xcc, 0xbb, 0xbb, 0xaa, 0xaa};

void localuuid_from_id(unsigned char *buf, uuidtype_t type, unsigned int id)
{
    uint32_t tmp;

    switch (type) {
    case UUID_GROUP:
        memcpy(buf, local_group_uuid, 12);
        break;
    case UUID_USER:
    default:
        memcpy(buf, local_user_uuid, 12);
        break;
    }

    tmp = htonl(id);
    memcpy(buf + 12, &tmp, 4);

    return;
}

/*
 * convert ascii string that can include dashes to binary uuid.
 * caller must provide a buffer.
 */
void uuid_string2bin( const char *uuidstring, unsigned char *uuid) {
    int nibble = 1;
    int i = 0;
    unsigned char c, val = 0;
    
    while (*uuidstring && i < UUID_BINSIZE) {
        c = *uuidstring;
        if (c == '-') {
            uuidstring++;
            continue;
        }
        else if (c <= '9')      /* 0-9 */
            c -= '0';
        else if (c <= 'F')  /* A-F */
            c -= 'A' - 10;
        else if (c <= 'f')      /* a-f */
            c-= 'a' - 10;

        if (nibble)
            val = c * 16;
        else
            uuid[i++] = val + c;

        nibble ^= 1;
        uuidstring++;
    }

}

/*!
 * Convert 16 byte binary uuid to neat ascii represantation including dashes.
 * Use defined or default ascii mask for dash placement
 * Returns pointer to static buffer.
 */
const char *uuid_bin2string(const unsigned char *uuid) {
    static char uuidstring[64];
    const char *uuidmask;
    int i = 0;
    unsigned char c;

#ifdef HAVE_LDAP
    if (ldap_uuid_string)
        uuidmask = ldap_uuid_string;
    else
#endif
        uuidmask = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";

    LOG(log_debug, logtype_afpd, "uuid_bin2string{uuid}: mask: %s", uuidmask);
		
    while (i < strlen(uuidmask)) {
        c = *uuid;
        uuid++;
        sprintf(uuidstring + i, "%02X", c);
        i += 2;
        if (uuidmask[i] == '-')
            uuidstring[i++] = '-';
    }
    uuidstring[i] = 0;
    return uuidstring;
}

/********************************************************
 * Interface
 ********************************************************/

/*
 *   name: give me his name
 *   type: and type (UUID_USER or UUID_GROUP)
 *   uuid: pointer to uuid_t storage that the caller must provide
 * returns 0 on success !=0 on errror
 */
int getuuidfromname( const char *name, uuidtype_t type, unsigned char *uuid) {
    int ret = 0;
    uuidtype_t mytype = type;
    char nulluuid[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
#ifdef HAVE_LDAP
    char *uuid_string = NULL;
#endif

    ret = search_cachebyname(name, &mytype, uuid);

    if (ret == 0) {
        /* found in cache */
        LOG(log_debug, logtype_afpd,
            "getuuidfromname{cache}: name: %s, type%s: %s -> UUID: %s",
            name,
            (mytype & UUID_ENOENT) == UUID_ENOENT ? "[negative]" : "",
            uuidtype[type & UUIDTYPESTR_MASK],
            uuid_bin2string(uuid));
        if ((mytype & UUID_ENOENT) == UUID_ENOENT)
            return -1;
    } else  {
        /* if not found in cache */
#ifdef HAVE_LDAP
        if ((ret = ldap_getuuidfromname( name, type, &uuid_string)) == 0) {
            uuid_string2bin( uuid_string, uuid);
            LOG(log_debug, logtype_afpd, "getuuidfromname{LDAP}: name: %s, type: %s -> UUID: %s",
                name, uuidtype[type & UUIDTYPESTR_MASK], uuid_bin2string(uuid));
        } else {
            LOG(log_debug, logtype_afpd, "getuuidfromname(\"%s\",t:%u): no result from ldap search",
                name, type);
        }
#endif
        if (ret != 0) {
            /* Build a local UUID */
            if (type == UUID_USER) {
                struct passwd *pwd;
                if ((pwd = getpwnam(name)) == NULL) {
                    LOG(log_error, logtype_afpd, "getuuidfromname(\"%s\",t:%u): unknown user",
                        name, uuidtype[type & UUIDTYPESTR_MASK]);
                    mytype |= UUID_ENOENT;
                    memcpy(uuid, nulluuid, 16);
                } else {
                    localuuid_from_id(uuid, UUID_USER, pwd->pw_uid);
                    ret = 0;
                    LOG(log_debug, logtype_afpd, "getuuidfromname{local}: name: %s, type: %s -> UUID: %s",
                        name, uuidtype[type & UUIDTYPESTR_MASK], uuid_bin2string(uuid));
                }
            } else {
                struct group *grp;
                if ((grp = getgrnam(name)) == NULL) {
                    LOG(log_error, logtype_afpd, "getuuidfromname(\"%s\",t:%u): unknown user",
                        name, uuidtype[type & UUIDTYPESTR_MASK]);
                    mytype |= UUID_ENOENT;
                    memcpy(uuid, nulluuid, 16);
                } else {
                    localuuid_from_id(uuid, UUID_GROUP, grp->gr_gid);
                    ret = 0;
                    LOG(log_debug, logtype_afpd, "getuuidfromname{local}: name: %s, type: %s -> UUID: %s",
                        name, uuidtype[type & UUIDTYPESTR_MASK], uuid_bin2string(uuid));
                }
            }
        }
        add_cachebyname(name, uuid, mytype, 0);
    }

#ifdef HAVE_LDAP
    if (uuid_string) free(uuid_string);
#endif
    return ret;
}


/*
 * uuidp: pointer to a uuid
 * name: returns allocated buffer from ldap_getnamefromuuid
 * type: returns USER, GROUP or LOCAL
 * return 0 on success !=0 on errror
 *
 * Caller must free name appropiately.
 */
int getnamefromuuid(const uuidp_t uuidp, char **name, uuidtype_t *type) {
    int ret;
    uid_t uid;
    gid_t gid;
    uint32_t tmp;
    struct passwd *pwd;
    struct group *grp;

    if (search_cachebyuuid(uuidp, name, type) == 0) {
        /* found in cache */
        LOG(log_debug, logtype_afpd,
            "getnamefromuuid{cache}: UUID: %s -> name: %s, type%s: %s",
            uuid_bin2string(uuidp),
            *name,
            (*type & UUID_ENOENT) == UUID_ENOENT ? "[negative]" : "",
            uuidtype[(*type) & UUIDTYPESTR_MASK]);
        if ((*type & UUID_ENOENT) == UUID_ENOENT)
            return -1;
        return 0;
    }

    /* not found in cache */

    /* Check if UUID is a client local one */
    if (memcmp(uuidp, local_user_uuid, 12) == 0) {
        *type = UUID_USER;
        memcpy(&tmp, uuidp + 12, sizeof(uint32_t));
        uid = ntohl(tmp);
        if ((pwd = getpwuid(uid)) == NULL) {
            /* not found, add negative entry to cache */
            *name = NULL;
            add_cachebyuuid(uuidp, "UUID_ENOENT", UUID_ENOENT, 0);
            ret = -1;
        } else {
            *name = strdup(pwd->pw_name);
            add_cachebyuuid(uuidp, *name, *type, 0);
            ret = 0;
        }
        LOG(log_debug, logtype_afpd,
            "getnamefromuuid{local}: UUID: %s -> name: %s, type:%s",
            uuid_bin2string(uuidp), *name ? *name : "-", uuidtype[(*type) & UUIDTYPESTR_MASK]);
        return ret;
    } else if (memcmp(uuidp, local_group_uuid, 12) == 0) {
        *type = UUID_GROUP;
        memcpy(&tmp, uuidp + 12, sizeof(uint32_t));
        gid = ntohl(tmp);
        if ((grp = getgrgid(gid)) == NULL) {
            /* not found, add negative entry to cache */
            add_cachebyuuid(uuidp, "UUID_ENOENT", UUID_ENOENT, 0);
            ret = -1;
        } else {
            *name = strdup(grp->gr_name);
            add_cachebyuuid(uuidp, *name, *type, 0);
            ret = 0;
        }
        return ret;
    }

#ifdef HAVE_LDAP
    ret = ldap_getnamefromuuid(uuid_bin2string(uuidp), name, type);
#else
    ret = -1;
#endif

    if (ret != 0) {
        LOG(log_debug, logtype_afpd, "getnamefromuuid(%s): not found",
            uuid_bin2string(uuidp));
        add_cachebyuuid(uuidp, "UUID_ENOENT", UUID_ENOENT, 0);
        return -1;
    }

    add_cachebyuuid(uuidp, *name, *type, 0);

    LOG(log_debug, logtype_afpd, "getnamefromuuid{LDAP}: UUID: %s -> name: %s, type:%s",
        uuid_bin2string(uuidp), *name, uuidtype[(*type) & UUIDTYPESTR_MASK]);

    return 0;
}
