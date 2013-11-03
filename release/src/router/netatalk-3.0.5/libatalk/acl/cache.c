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
#include <time.h>
#include <errno.h>

#include <atalk/logger.h>
#include <atalk/afp.h>
#include <atalk/uuid.h>
#include "cache.h"

typedef struct cacheduser {
    unsigned long uid;      /* for future use */
    uuidtype_t type;
    unsigned char *uuid;
    char *name;
    time_t creationtime;
    struct cacheduser *prev;
    struct cacheduser *next;
} cacheduser_t;

static cacheduser_t *namecache[256];   /* indexed by hash of name */
static cacheduser_t *uuidcache[256];   /* indexed by hash of uuid */

/********************************************************
 * helper function
 ********************************************************/

void uuidcache_dump(void) {
    int i;
    cacheduser_t *entry;
    char timestr[200];
    struct tm *tmp = NULL;

    for ( i=0 ; i<256; i++) {
        if ((entry = namecache[i]) != NULL) {
            do {
                tmp = localtime(&entry->creationtime);
                if (tmp == NULL)
                    continue;
                if (strftime(timestr, 200, "%c", tmp) == 0)
                    continue;
                LOG(log_debug, logtype_default,
                    "namecache{%d}: name:%s, uuid:%s, type%s: %s, cached: %s",
                    i,
                    entry->name,
                    uuid_bin2string(entry->uuid),
                    (entry->type & UUID_ENOENT) == UUID_ENOENT ? "[negative]" : "",
                    uuidtype[entry->type & UUIDTYPESTR_MASK],
                    timestr);
            } while ((entry = entry->next) != NULL);
        }
    }

    for ( i=0; i<256; i++) {
        if ((entry = uuidcache[i]) != NULL) {
            do {

                tmp = localtime(&entry->creationtime);
                if (tmp == NULL)
                    continue;
                if (strftime(timestr, 200, "%c", tmp) == 0)
                    continue;
                LOG(log_debug, logtype_default,
                    "uuidcache{%d}: uuid:%s, name:%s, type%s: %s, cached: %s",
                    i,
                    uuid_bin2string(entry->uuid),
                    entry->name,
                    (entry->type & UUID_ENOENT) == UUID_ENOENT ? "[negative]" : "",
                    uuidtype[entry->type & UUIDTYPESTR_MASK],
                    timestr);
            } while ((entry = entry->next) != NULL);
        }
    }
}

/* hash string it into unsigned char */
static unsigned char hashstring(unsigned char *str) {
    unsigned long hash = 5381;
    unsigned char index;
    int c;
    while ((c = *str++) != 0)
        hash = ((hash << 5) + hash) ^ c; /* (hash * 33) ^ c */

    index = 85 ^ (hash & 0xff);
    while ((hash = hash >> 8) != 0)
        index ^= (hash & 0xff);

    return index;
}

/* hash atalk_uuid_t into unsigned char */
static unsigned char hashuuid(uuidp_t uuid) {
    unsigned char index = 83;
    int i;

    for (i=0; i<16; i++) {
        index ^= uuid[i];
        index += uuid[i];
    }
    return index;
}

/********************************************************
 * Interface
 ********************************************************/

int add_cachebyname( const char *inname, const uuidp_t inuuid, const uuidtype_t type, const unsigned long uid _U_) {
    int ret = 0;
    char *name = NULL;
    unsigned char *uuid = NULL;
    cacheduser_t *cacheduser = NULL;
    unsigned char hash;

    /* allocate mem and copy values */
    name = malloc(strlen(inname)+1);
    if (!name) {
        LOG(log_error, logtype_default, "add_cachebyname: mallor error");
        ret = -1;
        goto cleanup;
    }

    uuid = malloc(UUID_BINSIZE);
    if (!uuid) {
        LOG(log_error, logtype_default, "add_cachebyname: mallor error");
        ret = -1;
        goto cleanup;
    }

    cacheduser = malloc(sizeof(cacheduser_t));
    if (!cacheduser) {
        LOG(log_error, logtype_default, "add_cachebyname: mallor error");
        ret = -1;
        goto cleanup;
    }

    strcpy(name, inname);
    memcpy(uuid, inuuid, UUID_BINSIZE);

    /* fill in the cacheduser */
    cacheduser->name = name;
    cacheduser->uuid = uuid;
    cacheduser->type = type;
    cacheduser->creationtime = time(NULL);
    cacheduser->prev = NULL;
    cacheduser->next = NULL;

    /* get hash */
    hash = hashstring((unsigned char *)name);

    /* insert cache entry into cache array at head of queue */
    if (namecache[hash] == NULL) {
        /* this queue is empty */
        namecache[hash] = cacheduser;
    } else {
        cacheduser->next = namecache[hash];
        namecache[hash]->prev = cacheduser;
        namecache[hash] = cacheduser;
    }

cleanup:
    if (ret != 0) {
        if (name)
            free(name);
        if (uuid)
            free(uuid);
        if (cacheduser)
            free(cacheduser);
    }

    return ret;
}

/*!
 * Search cache by name and uuid type
 *
 * @args name     (r)  name to search
 * @args type     (rw) type (user or group) of name, returns found type here which might
 *                     mark it as a negative entry
 * @args uuid     (w)  found uuid is returned here
 * @returns       0 on sucess, entry found
 *                -1 no entry found
 */
int search_cachebyname( const char *name, uuidtype_t *type, unsigned char *uuid) {
    int ret;
    unsigned char hash;
    cacheduser_t *entry;
    time_t tim;

    hash = hashstring((unsigned char *)name);

    if (namecache[hash] == NULL)
        return -1;

    entry = namecache[hash];
    while (entry) {
        ret = strcmp(entry->name, name);
        if (ret == 0 && *type == (entry->type & UUIDTYPESTR_MASK)) {
            /* found, now check if expired */
            tim = time(NULL);
            if ((tim - entry->creationtime) > CACHESECONDS) {
                LOG(log_debug, logtype_default, "search_cachebyname: expired: name:\"%s\"", entry->name);
                /* remove item */
                if (entry->prev) {
                    /* 2nd to last in queue */
                    entry->prev->next = entry->next;
                    if (entry->next)
                        /* not the last element */
                        entry->next->prev = entry->prev;
                } else  {
                    /* queue head */
                    if ((namecache[hash] = entry->next) != NULL)
                        namecache[hash]->prev = NULL;
                }
                free(entry->name);
                free(entry->uuid);
                free(entry);
                return -1;
            } else {
                memcpy(uuid, entry->uuid, UUID_BINSIZE);
                *type = entry->type;
                return 0;
            }
        }
        entry = entry->next;
    }

    return -1;
}

/* 
 * Caller must free allocated name
 */
int search_cachebyuuid( uuidp_t uuidp, char **name, uuidtype_t *type) {
    int ret;
    unsigned char hash;
    cacheduser_t *entry;
    time_t tim;

    hash = hashuuid(uuidp);

    if (! uuidcache[hash])
        return -1;

    entry = uuidcache[hash];
    while (entry) {
        ret = memcmp(entry->uuid, uuidp, UUID_BINSIZE);
        if (ret == 0) {
            tim = time(NULL);
            if ((tim - entry->creationtime) > CACHESECONDS) {
                LOG(log_debug, logtype_default, "search_cachebyuuid: expired: name:\'%s\' in queue {%d}", entry->name, hash);
                if (entry->prev) {
                    /* 2nd to last in queue */
                    entry->prev->next = entry->next;
                    if (entry->next)
                        /* not the last element */
                        entry->next->prev = entry->prev;
                } else {
                    /* queue head  */
                    if ((uuidcache[hash] = entry->next) != NULL)
                        uuidcache[hash]->prev = NULL;
                }
                free(entry->name);
                free(entry->uuid);
                free(entry);
                return -1;
            } else {
                *name = malloc(strlen(entry->name)+1);
                strcpy(*name, entry->name);
                *type = entry->type;
                return 0;
            }
        }
        entry = entry->next;
    }

    return -1;
}

int add_cachebyuuid( uuidp_t inuuid, const char *inname, uuidtype_t type, const unsigned long uid _U_) {
    int ret = 0;
    char *name = NULL;
    unsigned char *uuid = NULL;
    cacheduser_t *cacheduser = NULL;
    unsigned char hash;

    /* allocate mem and copy values */
    name = malloc(strlen(inname)+1);
    if (!name) {
        LOG(log_error, logtype_default, "add_cachebyuuid: mallor error");
        ret = -1;
        goto cleanup;
    }

    uuid = malloc(UUID_BINSIZE);
    if (!uuid) {
        LOG(log_error, logtype_default, "add_cachebyuuid: mallor error");
        ret = -1;
        goto cleanup;
    }

    cacheduser = malloc(sizeof(cacheduser_t));
    if (!cacheduser) {
        LOG(log_error, logtype_default, "add_cachebyuuid: mallor error");
        ret = -1;
        goto cleanup;
    }

    strcpy(name, inname);
    memcpy(uuid, inuuid, UUID_BINSIZE);

    /* fill in the cacheduser */
    cacheduser->name = name;
    cacheduser->type = type;
    cacheduser->uuid = uuid;
    cacheduser->creationtime = time(NULL);
    cacheduser->prev = NULL;
    cacheduser->next = NULL;

    /* get hash */
    hash = hashuuid(uuid);

    /* insert cache entry into cache array at head of queue */
    if (uuidcache[hash] == NULL) {
        /* this queue is empty */
        uuidcache[hash] = cacheduser;
    } else {
        cacheduser->next = uuidcache[hash];
        uuidcache[hash]->prev = cacheduser;
        uuidcache[hash] = cacheduser;
    }

cleanup:
    if (ret != 0) {
        if (name)
            free(name);
        if (uuid)
            free(uuid);
        if (cacheduser)
            free(cacheduser);
    }

    return ret;
}
