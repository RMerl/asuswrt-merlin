/*
  Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>

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

#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <arpa/inet.h>

#include <atalk/adouble.h>
#include <atalk/ea.h>
#include <atalk/afp.h>
#include <atalk/logger.h>
#include <atalk/volume.h>
#include <atalk/vfs.h>
#include <atalk/util.h>
#include <atalk/unix.h>
#include <atalk/compat.h>

/*
 * Store Extended Attributes inside .AppleDouble folders as follows:
 *
 * filename "fileWithEAs" with EAs "testEA1" and "testEA2"
 *
 * - create header with with the format struct adouble_ea_ondisk, the file is written to
 *   ".AppleDouble/fileWithEAs::EA"
 * - store EAs in files "fileWithEAs::EA::testEA1" and "fileWithEAs::EA::testEA2"
 */

/* 
 * Build mode for EA header from file mode
 */
static inline mode_t ea_header_mode(mode_t mode)
{
    /* Same as ad_hf_mode(mode) */
    mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH);
    /* Owner must be able to open, read and w-lock it, in order to chmod from eg 0000 -> 0xxxx*/
    mode |= S_IRUSR | S_IWUSR;
    return mode;
}

/* 
 * Build mode for EA file from file mode
 */
static inline mode_t ea_mode(mode_t mode)
{
    /* Same as ad_hf_mode(mode) */
    mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH);
    return mode;
}

/*
  Taken form afpd/desktop.c
*/
static char *mtoupath(const struct vol *vol, const char *mpath)
{
    static char  upath[ MAXPATHLEN + 2]; /* for convert_charset dest_len parameter +2 */
    const char   *m;
    char         *u;
    size_t       inplen;
    size_t       outlen;
    uint16_t     flags = CONV_ESCAPEHEX;

    if (!mpath)
        return NULL;

    if ( *mpath == '\0' ) {
        return( "." );
    }

    m = mpath;
    u = upath;

    inplen = strlen(m);
    outlen = MAXPATHLEN;

    if ((size_t)-1 == (outlen = convert_charset(CH_UTF8_MAC,
                                                vol->v_volcharset,
                                                vol->v_maccharset,
                                                m, inplen, u, outlen, &flags)) ) {
        return NULL;
    }

    return( upath );
}


/*
 * Function: unpack_header
 *
 * Purpose: unpack and verify header file data buffer at ea->ea_data into struct ea
 *
 * Arguments:
 *
 *    ea      (rw) handle to struct ea
 *
 * Returns: 0 on success, -1 on error
 *
 * Effects:
 *
 * Verifies magic and version.
 */
static int unpack_header(struct ea * restrict ea)
{
    int ret = 0;
    unsigned int count = 0;
    uint16_t uint16;
    uint32_t uint32;
    char *buf;

    /* Check magic and version */
    buf = ea->ea_data;
    memcpy(&uint32, buf, sizeof(uint32_t));
    if (uint32 != htonl(EA_MAGIC)) {
        LOG(log_error, logtype_afpd, "unpack_header: wrong magic 0x%08x", uint32);
        ret = -1;
        goto exit;
    }
    buf += 4;
    memcpy(&uint16, buf, sizeof(uint16_t));
    if (uint16 != htons(EA_VERSION)) {
        LOG(log_error, logtype_afpd, "unpack_header: wrong version 0x%04x", uint16);
        ret = -1;
        goto exit;
    }
    buf += 2;

    /* Get EA count */
    memcpy(&uint16, buf, sizeof(uint16_t));
    ea->ea_count = ntohs(uint16);
    LOG(log_debug, logtype_afpd, "unpack_header: number of EAs: %u", ea->ea_count);
    buf += 2;

    if (ea->ea_count == 0)
        return 0;

    /* Allocate storage for the ea_entries array */
    ea->ea_entries = malloc(sizeof(struct ea_entry) * ea->ea_count);
    if ( ! ea->ea_entries) {
        LOG(log_error, logtype_afpd, "unpack_header: OOM");
        ret = -1;
        goto exit;
    }

    buf = ea->ea_data + EA_HEADER_SIZE;
    while (count < ea->ea_count) {
        memcpy(&uint32, buf, 4); /* EA size */
        buf += 4;
        (*(ea->ea_entries))[count].ea_size = ntohl(uint32);
        (*(ea->ea_entries))[count].ea_name = strdup(buf);
        if (! (*(ea->ea_entries))[count].ea_name) {
            LOG(log_error, logtype_afpd, "unpack_header: OOM");
            ret = -1;
            goto exit;
        }
        (*(ea->ea_entries))[count].ea_namelen = strlen((*(ea->ea_entries))[count].ea_name);
        buf += (*(ea->ea_entries))[count].ea_namelen + 1;

        LOG(log_maxdebug, logtype_afpd, "unpack_header: entry no:%u,\"%s\", size: %u, namelen: %u", count,
            (*(ea->ea_entries))[count].ea_name,
            (*(ea->ea_entries))[count].ea_size,
            (*(ea->ea_entries))[count].ea_namelen);

        count++;
    }

exit:
    return ret;
}

/*
 * Function: pack_header
 *
 * Purpose: pack everything from struct ea into buffer at ea->ea_data
 *
 * Arguments:
 *
 *    ea      (rw) handle to struct ea
 *
 * Returns: 0 on success, -1 on error
 *
 * Effects:
 *
 * adjust ea->ea_count in case an ea entry deletetion is detected
 */
static int pack_header(struct ea * restrict ea)
{
    unsigned int count = 0, eacount = 0;
    uint16_t uint16;
    uint32_t uint32;
    size_t bufsize = EA_HEADER_SIZE;

    char *buf = ea->ea_data + EA_HEADER_SIZE;

    LOG(log_debug, logtype_afpd, "pack_header('%s'): ea_count: %u, ea_size: %u",
        ea->filename, ea->ea_count, ea->ea_size);

    if (ea->ea_count == 0)
        /* nothing to do, magic, version and count are still valid in buffer */
        return 0;

    while(count < ea->ea_count) { /* the names */
        /* Check if its a deleted entry */
        if ( ! ((*ea->ea_entries)[count].ea_name)) {
            count++;
            continue;
        }

        bufsize += (*(ea->ea_entries))[count].ea_namelen + 1;
        count++;
        eacount++;
    }

    bufsize += (eacount * 4); /* header + ea_size for each EA */
    if (bufsize > ea->ea_size) {
        /* we must realloc */
        if ( ! (buf = realloc(ea->ea_data, bufsize)) ) {
            LOG(log_error, logtype_afpd, "pack_header: OOM");
            return -1;
        }
        ea->ea_data = buf;
    }
    ea->ea_size = bufsize;

    /* copy count */
    uint16 = htons(eacount);
    memcpy(ea->ea_data + EA_COUNT_OFF, &uint16, 2);

    count = 0;
    buf = ea->ea_data + EA_HEADER_SIZE;
    while (count < ea->ea_count) {
        /* Check if its a deleted entry */
        if ( ! ((*ea->ea_entries)[count].ea_name)) {
            count++;
            continue;
        }

        /* First: EA size */
        uint32 = htonl((*(ea->ea_entries))[count].ea_size);
        memcpy(buf, &uint32, 4);
        buf += 4;

        /* Second: EA name as C-string */
        strcpy(buf, (*(ea->ea_entries))[count].ea_name);
        buf += (*(ea->ea_entries))[count].ea_namelen + 1;

        LOG(log_maxdebug, logtype_afpd, "pack_header: entry no:%u,\"%s\", size: %u, namelen: %u", count,
            (*(ea->ea_entries))[count].ea_name,
            (*(ea->ea_entries))[count].ea_size,
            (*(ea->ea_entries))[count].ea_namelen);

        count++;
    }

    ea->ea_count = eacount;

    LOG(log_debug, logtype_afpd, "pack_header('%s'): ea_count: %u, ea_size: %u",
        ea->filename, ea->ea_count, ea->ea_size);
    
    return 0;
}

/*
 * Function: ea_addentry
 *
 * Purpose: add one EA into ea->ea_entries[]
 *
 * Arguments:
 *
 *    ea            (rw) pointer to struct ea
 *    attruname     (r) name of EA
 *    attrsize      (r) size of ea
 *    bitmap        (r) bitmap from FP func
 *
 * Returns: new number of EA entries, -1 on error
 *
 * Effects:
 *
 * Grow array ea->ea_entries[]. If ea->ea_entries is still NULL, start allocating.
 * Otherwise realloc and put entry at the end. Increments ea->ea_count.
 */
static int ea_addentry(struct ea * restrict ea,
                       const char * restrict attruname,
                       size_t attrsize,
                       int bitmap)
{
    int ea_existed = 0;
    unsigned int count = 0;
    void *tmprealloc;

    /* First check if an EA of the requested name already exist */
    if (ea->ea_count > 0) {
        while (count < ea->ea_count) {
            if (strcmp(attruname, (*ea->ea_entries)[count].ea_name) == 0) {
                ea_existed = 1;
                LOG(log_debug, logtype_afpd, "ea_addentry('%s', bitmap:0x%x): exists", attruname, bitmap);
                if (bitmap & kXAttrCreate)
                    /* its like O_CREAT|O_EXCL -> fail */
                    return -1;
                (*(ea->ea_entries))[count].ea_size = attrsize;
                return 0;
            }
            count++;
        }
    }

    if ((bitmap & kXAttrReplace) && ! ea_existed)
        /* replace was requested, but EA didn't exist */
        return -1;

    if (ea->ea_count == 0) {
        ea->ea_entries = malloc(sizeof(struct ea_entry));
        if ( ! ea->ea_entries) {
            LOG(log_error, logtype_afpd, "ea_addentry: OOM");
            return -1;
        }
    } else if (! ea_existed) {
        tmprealloc = realloc(ea->ea_entries, sizeof(struct ea_entry) * (ea->ea_count + 1));
        if ( ! tmprealloc) {
            LOG(log_error, logtype_afpd, "ea_addentry: OOM");
            return -1;
        }
        ea->ea_entries = tmprealloc;
    }

    /* We've grown the array, now store the entry */
    (*(ea->ea_entries))[ea->ea_count].ea_size = attrsize;
    (*(ea->ea_entries))[ea->ea_count].ea_name = strdup(attruname);
    if ( ! (*(ea->ea_entries))[ea->ea_count].ea_name) {
        LOG(log_error, logtype_afpd, "ea_addentry: OOM");
        goto error;
    }
    (*(ea->ea_entries))[ea->ea_count].ea_namelen = strlen(attruname);

    ea->ea_count++;
    return ea->ea_count;

error:
    if (ea->ea_count == 0 && ea->ea_entries) {
        /* We just allocated storage but had an error somewhere -> free storage*/
        free(ea->ea_entries);
        ea->ea_entries = NULL;
    }
    ea->ea_count = 0;
    return -1;
}

/*
 * Function: create_ea_header
 *
 * Purpose: create EA header file, only called from ea_open
 *
 * Arguments:
 *
 *    uname       (r)  filename for which we have to create a header
 *    ea          (rw) ea handle with already allocated storage pointed to
 *                     by ea->ea_data
 *
 * Returns: fd of open header file on success, -1 on error, errno semantics:
 *          EEXIST: open with O_CREAT | O_EXCL failed
 *
 * Effects:
 *
 * Creates EA header file and initialize ea->ea_data buffer.
 * Possibe race condition with other afpd processes:
 * we were called because header file didn't exist in eg. ea_open. We then
 * try to create a file with O_CREAT | O_EXCL, but the whole process in not atomic.
 * What do we do then? Someone else is in the process of creating the header too, but
 * it might not have finished it. That means we cant just open, read and use it!
 * We therefor currently just break with an error.
 * On return the header file is still r/w locked.
 */
static int create_ea_header(const char * restrict uname,
                            struct ea * restrict ea)
{
    int fd = -1, err = 0;
    char *ptr;
    uint16_t uint16;
    uint32_t uint32;

    if ((fd = open(uname, O_RDWR | O_CREAT | O_EXCL, 0666 & ~ea->vol->v_umask)) == -1) {
        LOG(log_error, logtype_afpd, "ea_create: open race condition with ea header for file: %s", uname);
        return -1;
    }

    /* lock it */
    if ((write_lock(fd, 0, SEEK_SET, 0)) != 0) {
        LOG(log_error, logtype_afpd, "ea_create: lock race condition with ea header for file: %s", uname);
        err = -1;
        goto exit;
    }

    /* Now init it */
    ptr = ea->ea_data;
    uint32 = htonl(EA_MAGIC);
    memcpy(ptr, &uint32, sizeof(uint32_t));
    ptr += EA_MAGIC_LEN;

    uint16 = htons(EA_VERSION);
    memcpy(ptr, &uint16, sizeof(uint16_t));
    ptr += EA_VERSION_LEN;

    memset(ptr, 0, 2);          /* count */

    ea->ea_size = EA_HEADER_SIZE;
    ea->ea_inited = EA_INITED;

exit:
    if (err != 0) {
        close(fd);
        fd = -1;
    }
    return fd;
}

/*
 * Function: write_ea
 *
 * Purpose: write an EA to disk
 *
 * Arguments:
 *
 *    ea         (r) struct ea handle
 *    attruname  (r) EA name
 *    ibuf       (r) buffer with EA content
 *    attrsize   (r) size of EA
 *
 * Returns: 0 on success, -1 on error
 *
 * Effects:
 *
 * Creates/overwrites EA file.
 *
 */
static int write_ea(const struct ea * restrict ea,
                    const char * restrict attruname,
                    const char * restrict ibuf,
                    size_t attrsize)
{
    int fd = -1, ret = AFP_OK;
    struct stat st;
    char *eaname;

    if ((eaname = ea_path(ea, attruname, 1)) == NULL) {
        LOG(log_error, logtype_afpd, "write_ea('%s'): ea_path error", attruname);
        return AFPERR_MISC;
    }

    LOG(log_maxdebug, logtype_afpd, "write_ea('%s')", eaname);

    /* Check if it exists, remove if yes*/
    if ((stat(eaname, &st)) == 0) {
        if ((unlink(eaname)) != 0) {
            if (errno == EACCES)
                return AFPERR_ACCESS;
            else
                return AFPERR_MISC;
        }
    }

    if ((fd = open(eaname, O_RDWR | O_CREAT | O_EXCL, 0666 & ~ea->vol->v_umask)) == -1) {
        LOG(log_error, logtype_afpd, "write_ea: open race condition: %s", eaname);
        return -1;
    }

    /* lock it */
    if ((write_lock(fd, 0, SEEK_SET, 0)) != 0) {
        LOG(log_error, logtype_afpd, "write_ea: open race condition: %s", eaname);
        ret = -1;
        goto exit;
    }

    if (write(fd, ibuf, attrsize) != (ssize_t)attrsize) {
        LOG(log_error, logtype_afpd, "write_ea('%s'): write: %s", eaname, strerror(errno));
        ret = -1;
        goto exit;
    }

exit:
    if (fd != -1)
        close(fd); /* and unlock */
    return ret;
}

/*
 * Function: ea_delentry
 *
 * Purpose: delete one EA from ea->ea_entries[]
 *
 * Arguments:
 *
 *    ea            (rw) pointer to struct ea
 *    attruname     (r) EA name
 *
 * Returns: new number of EA entries, -1 on error
 *
 * Effects:
 *
 * Remove entry from  ea->ea_entries[]. Decrement ea->ea_count.
 * Marks it as unused just by freeing name and setting it to NULL.
 * ea_close and pack_buffer must honor this.
 */
static int ea_delentry(struct ea * restrict ea, const char * restrict attruname)
{
    int ret = 0;
    unsigned int count = 0;

    if (ea->ea_count == 0) {
        LOG(log_error, logtype_afpd, "ea_delentry('%s'): illegal ea_count of 0 on deletion",
            attruname);
        return -1;
    }

    while (count < ea->ea_count) {
        /* search matching EA */
        if ((*ea->ea_entries)[count].ea_name &&
            strcmp(attruname, (*ea->ea_entries)[count].ea_name) == 0) {
            free((*ea->ea_entries)[count].ea_name);
            (*ea->ea_entries)[count].ea_name = NULL;

            LOG(log_debug, logtype_afpd, "ea_delentry('%s'): deleted no %u/%u",
                attruname, count + 1, ea->ea_count);

            break;
        }
        count++;
    }

    return ret;
}

/*
 * Function: delete_ea_file
 *
 * Purpose: delete EA file from disk
 *
 * Arguments:
 *
 *    ea         (r) struct ea handle
 *    attruname  (r) EA name
 *
 * Returns: 0 on success, -1 on error
 */
static int delete_ea_file(const struct ea * restrict ea, const char *eaname)
{
    int ret = 0;
    char *eafile;
    struct stat st;

    if ((eafile = ea_path(ea, eaname, 1)) == NULL) {
        LOG(log_error, logtype_afpd, "delete_ea_file('%s'): ea_path error", eaname);
        return -1;
    }

    /* Check if it exists, remove if yes*/
    if ((stat(eafile, &st)) == 0) {
        if ((unlink(eafile)) != 0) {
            LOG(log_error, logtype_afpd, "delete_ea_file('%s'): unlink: %s",
                eafile, strerror(errno));
            ret = -1;
        } else
            LOG(log_debug, logtype_afpd, "delete_ea_file('%s'): success", eafile);
    }

    return ret;
}

/*************************************************************************************
 * ea_path, ea_open and ea_close are only global so that dbd can call them
 *************************************************************************************/

/*
 * Function: ea_path
 *
 * Purpose: return name of ea header filename
 *
 * Arguments:
 *
 *    ea           (r) ea handle
 *    eaname       (r) name of EA or NULL
 *    macname      (r) if != 0 call mtoupath on eaname
 *
 * Returns: pointer to name in static buffer, NULL on error
 *
 * Effects:
 *
 * Calls ad_open, copies buffer, appends "::EA" and if supplied append eanme
 * Files: "file" -> "file/.AppleDouble/file::EA"
 * Dirs: "dir" -> "dir/.AppleDouble/.Parent::EA"
 * "file" with EA "myEA" -> "file/.AppleDouble/file::EA:myEA"
 */
char *ea_path(const struct ea * restrict ea, const char * restrict eaname, int macname)
{
    const char *adname;
    static char pathbuf[MAXPATHLEN + 1];

    /* get name of a adouble file from uname */
    adname = ea->vol->ad_path(ea->filename, (ea->ea_flags & EA_DIR) ? ADFLAGS_DIR : 0);
    /* copy it so we can work with it */
    strlcpy(pathbuf, adname, MAXPATHLEN + 1);
    /* append "::EA" */
    strlcat(pathbuf, "::EA", MAXPATHLEN + 1);

    if (eaname) {
        strlcat(pathbuf, "::", MAXPATHLEN + 1);
        if (macname)
            if ((eaname = mtoupath(ea->vol, eaname)) == NULL)
                return NULL;
        strlcat(pathbuf, eaname, MAXPATHLEN + 1);
    }

    return pathbuf;
}

/*
 * Function: ea_open
 *
 * Purpose: open EA header file, create if it doesnt exits and called with O_CREATE
 *
 * Arguments:
 *
 *    vol         (r) current volume
 *    uname       (r) filename for which we have to open a header
 *    flags       (r) EA_CREATE: create if it doesn't exist (without it won't be created)
 *                    EA_RDONLY: open read only
 *                    EA_RDWR: open read/write
 *                    Eiterh EA_RDONLY or EA_RDWR MUST be requested
 *    ea          (w) pointer to a struct ea that we fill
 *
 * Returns: 0 on success
 *         -1 on misc error with errno = EFAULT
 *         -2 if no EA header exists with errno = ENOENT
 *
 * Effects:
 *
 * opens header file and stores fd in ea->ea_fd. Size of file is put into ea->ea_size.
 * number of EAs is stored in ea->ea_count. flags are remembered in ea->ea_flags.
 * file is either read or write locked depending on the open flags.
 * When you're done with struct ea you must call ea_close on it.
 */
int ea_open(const struct vol * restrict vol,
            const char * restrict uname,
            eaflags_t eaflags,
            struct ea * restrict ea)
{
    int ret = 0;
    char *eaname;
    struct stat st;

    /* Enforce usage rules! */
    if ( ! (eaflags & (EA_RDONLY | EA_RDWR))) {
        LOG(log_error, logtype_afpd, "ea_open: called without EA_RDONLY | EA_RDWR", uname);
        return -1;
    }

    /* Set it all to 0 */
    memset(ea, 0, sizeof(struct ea));

    ea->vol = vol;              /* ea_close needs it */
    ea->ea_flags = eaflags;
    ea->dirfd = -1;             /* no *at (cf openat) semantics by default */

    /* Dont care for errors, eg when removing the file is already gone */
    if (!stat(uname, &st) && S_ISDIR(st.st_mode))
        ea->ea_flags |=  EA_DIR;

    if ( ! (ea->filename = strdup(uname))) {
        LOG(log_error, logtype_afpd, "ea_open: OOM");
        return -1;
    }

    eaname = ea_path(ea, NULL, 0);
    LOG(log_maxdebug, logtype_afpd, "ea_open: ea_path: %s", eaname);

    /* Check if it exists, if not create it if EA_CREATE is in eaflags */
    if ((stat(eaname, &st)) != 0) {
        if (errno == ENOENT) {

            /* It doesnt exist */

            if ( ! (eaflags & EA_CREATE)) {
                /* creation was not requested, so return with error */
                ret = -2;
                goto exit;
            }

            /* Now create a header file */

            /* malloc buffer for minimal on disk data */
            ea->ea_data = malloc(EA_HEADER_SIZE);
            if (! ea->ea_data) {
                LOG(log_error, logtype_afpd, "ea_open: OOM");
                ret = -1;
                goto exit;
            }

            /* create it */
            ea->ea_fd = create_ea_header(eaname, ea);
            if (ea->ea_fd == -1) {
                ret = -1;
                goto exit;
            }

            return 0;

        } else {/* errno != ENOENT */
            ret = -1;
            goto exit;
        }
    }

    /* header file exists, so read and parse it */

    /* malloc buffer where we read disk file into */
    if (st.st_size < EA_HEADER_SIZE) {
        LOG(log_error, logtype_afpd, "ea_open('%s'): bogus EA header file", eaname);
        ret = -1;
        goto exit;
    }
    ea->ea_size = st.st_size;
    ea->ea_data = malloc(st.st_size);
    if (! ea->ea_data) {
        LOG(log_error, logtype_afpd, "ea_open: OOM");
        ret = -1;
        goto exit;
    }

    /* Now lock, open and read header file from disk */
    if ((ea->ea_fd = open(eaname, (ea->ea_flags & EA_RDWR) ? O_RDWR : O_RDONLY)) == -1) {
        LOG(log_error, logtype_afpd, "ea_open('%s'): error: %s", eaname, strerror(errno));
        ret = -1;
        goto exit;
    }

    /* lock it */
    if (ea->ea_flags & EA_RDONLY) {
        /* read lock */
        if ((read_lock(ea->ea_fd, 0, SEEK_SET, 0)) != 0) {
            LOG(log_error, logtype_afpd, "ea_open: lock error on  header: %s", eaname);
            ret = -1;
            goto exit;
        }
    } else {  /* EA_RDWR */
        /* write lock */
        if ((write_lock(ea->ea_fd, 0, SEEK_SET, 0)) != 0) {
            LOG(log_error, logtype_afpd, "ea_open: lock error on  header: %s", eaname);
            ret = -1;
            goto exit;
        }
    }

    /* read it */
    if (read(ea->ea_fd, ea->ea_data, ea->ea_size) != (ssize_t)ea->ea_size) {
        LOG(log_error, logtype_afpd, "ea_open: short read on header: %s", eaname);
        ret = -1;
        goto exit;
    }

    if ((unpack_header(ea)) != 0) {
        LOG(log_error, logtype_afpd, "ea_open: error unpacking header for: %s", eaname);
        ret = -1;
        goto exit;
    }

exit:
    switch (ret) {
    case 0:
        ea->ea_inited = EA_INITED;
        break;
    case -1:
        errno = EFAULT; /* force some errno distinguishable from ENOENT */
        /* fall through */
    case -2:
        if (ea->ea_data) {
            free(ea->ea_data);
            ea->ea_data = NULL;
        }
        if (ea->ea_fd) {
            close(ea->ea_fd);
            ea->ea_fd = -1;
        }
        break;
    }

    return ret;
}

/*
 * Function: ea_openat
 *
 * Purpose: openat like wrapper for ea_open, takes a additional file descriptor
 *
 * Arguments:
 *
 *    vol         (r) current volume
 *    sfd         (r) openat like file descriptor
 *    uname       (r) filename for which we have to open a header
 *    flags       (r) EA_CREATE: create if it doesn't exist (without it won't be created)
 *                    EA_RDONLY: open read only
 *                    EA_RDWR: open read/write
 *                    Eiterh EA_RDONLY or EA_RDWR MUST be requested
 *    ea          (w) pointer to a struct ea that we fill
 *
 * Returns: 0 on success
 *         -1 on misc error with errno = EFAULT
 *         -2 if no EA header exists with errno = ENOENT
 *
 * Effects:
 *
 * opens header file and stores fd in ea->ea_fd. Size of file is put into ea->ea_size.
 * number of EAs is stored in ea->ea_count. flags are remembered in ea->ea_flags.
 * file is either read or write locked depending on the open flags.
 * When you're done with struct ea you must call ea_close on it.
 */
int ea_openat(const struct vol * restrict vol,
              int dirfd,
              const char * restrict uname,
              eaflags_t eaflags,
              struct ea * restrict ea)
{
    int ret = 0;
    int cwdfd = -1;

    if (dirfd != -1) {
        if (((cwdfd = open(".", O_RDONLY)) == -1) || (fchdir(dirfd) != 0)) {
            ret = -1;
            goto exit;
        }
    }

    ret = ea_open(vol, uname, eaflags, ea);
    ea->dirfd = dirfd;

    if (dirfd != -1) {
        if (fchdir(cwdfd) != 0) {
            LOG(log_error, logtype_afpd, "ea_openat: cant chdir back, exiting");
            exit(EXITERR_SYS);
        }
    }


exit:
    if (cwdfd != -1)
        close(cwdfd);

    return ret;

}

/*
 * Function: ea_close
 *
 * Purpose: flushes and closes an ea handle
 *
 * Arguments:
 *
 *    ea          (rw) pointer to ea handle
 *
 * Returns: 0 on success, -1 on error
 *
 * Effects:
 *
 * Flushes and then closes and frees all resouces held by ea handle.
 * Pack data in ea into ea_data, then write ea_data to disk
 */
int ea_close(struct ea * restrict ea)
{
    int ret = 0; 
    unsigned int count = 0;
    char *eaname;
    struct stat st;

    LOG(log_debug, logtype_afpd, "ea_close('%s')", ea->filename);

    if (ea->ea_inited != EA_INITED) {
        LOG(log_warning, logtype_afpd, "ea_close('%s'): non initialized ea", ea->filename);
        return 0;
    }

    /* pack header and write it to disk if it was opened EA_RDWR*/
    if (ea->ea_flags & EA_RDWR) {
        if ((pack_header(ea)) != 0) {
            LOG(log_error, logtype_afpd, "ea_close: pack header");
            ret = -1;
        } else {
            if (ea->ea_count == 0) {
                /* Check if EA header exists and remove it */
                eaname = ea_path(ea, NULL, 0);
                if ((statat(ea->dirfd, eaname, &st)) == 0) {
                    if ((netatalk_unlinkat(ea->dirfd, eaname)) != 0) {
                        LOG(log_error, logtype_afpd, "ea_close('%s'): unlink: %s",
                            eaname, strerror(errno));
                        ret = -1;
                    }
                    else
                        LOG(log_debug, logtype_afpd, "ea_close(unlink '%s'): success", eaname);
                } else {
                    /* stat error */
                    if (errno != ENOENT) {
                        LOG(log_error, logtype_afpd, "ea_close('%s'): stat: %s",
                            eaname, strerror(errno));
                        ret = -1;
                    }
                }
            } else { /* ea->ea_count > 0 */
                if ((lseek(ea->ea_fd, 0, SEEK_SET)) == -1) {
                    LOG(log_error, logtype_afpd, "ea_close: lseek: %s", strerror(errno));
                    ret = -1;
                    goto exit;
                }

                if ((ftruncate(ea->ea_fd, 0)) == -1) {
                    LOG(log_error, logtype_afpd, "ea_close: ftruncate: %s", strerror(errno));
                    ret = -1;
                    goto exit;
                }

                if (write(ea->ea_fd, ea->ea_data, ea->ea_size) != (ssize_t)ea->ea_size) {
                    LOG(log_error, logtype_afpd, "ea_close: write: %s", strerror(errno));
                    ret = -1;
                }
            }
        }
    }

exit:
    /* free names */
    while(count < ea->ea_count) {
        if ( (*ea->ea_entries)[count].ea_name ) {
            free((*ea->ea_entries)[count].ea_name);
            (*ea->ea_entries)[count].ea_name = NULL;
        }
        count++;
    }
    ea->ea_count = 0;

    if (ea->filename) {
        free(ea->filename);
        ea->filename = NULL;
    }

    if (ea->ea_entries) {
        free(ea->ea_entries);
        ea->ea_entries = NULL;
    }

    if (ea->ea_data) {
        free(ea->ea_data);
        ea->ea_data = NULL;
    }
    if (ea->ea_fd != -1) {
        close(ea->ea_fd);       /* also releases the fcntl lock */
        ea->ea_fd = -1;
    }

    return 0;
}



/************************************************************************************
 * VFS funcs called from afp_ea* funcs
 ************************************************************************************/

/*
 * Function: get_easize
 *
 * Purpose: get size of an EA
 *
 * Arguments:
 *
 *    vol          (r) current volume
 *    rbuf         (w) DSI reply buffer
 *    rbuflen      (rw) current length of data in reply buffer
 *    uname        (r) filename
 *    oflag        (r) link and create flag
 *    attruname    (r) name of attribute
 *
 * Returns: AFP code: AFP_OK on success or appropiate AFP error code
 *
 * Effects:
 *
 * Copies EA size into rbuf in network order. Increments *rbuflen +4.
 */
int get_easize(VFS_FUNC_ARGS_EA_GETSIZE)
{
    int ret = AFPERR_MISC;
    unsigned int count = 0;
    uint32_t uint32;
    struct ea ea;

    LOG(log_debug, logtype_afpd, "get_easize: file: %s", uname);

    if ((ea_open(vol, uname, EA_RDONLY, &ea)) != 0) {
        if (errno != ENOENT)
            LOG(log_error, logtype_afpd, "get_easize: error calling ea_open for file: %s", uname);

        memset(rbuf, 0, 4);
        *rbuflen += 4;
        return ret;
    }

    while (count < ea.ea_count) {
        if (strcmp(attruname, (*ea.ea_entries)[count].ea_name) == 0) {
            uint32 = htonl((*ea.ea_entries)[count].ea_size);
            memcpy(rbuf, &uint32, 4);
            *rbuflen += 4;
            ret = AFP_OK;

            LOG(log_debug, logtype_afpd, "get_easize(\"%s\"): size: %u",
                attruname, (*ea.ea_entries)[count].ea_size);
            break;
        }
        count++;
    }

    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "get_easize: error closing ea handle for file: %s", uname);
        return AFPERR_MISC;
    }

    return ret;
}

/*
 * Function: get_eacontent
 *
 * Purpose: copy EA into rbuf
 *
 * Arguments:
 *
 *    vol          (r) current volume
 *    rbuf         (w) DSI reply buffer
 *    rbuflen      (rw) current length of data in reply buffer
 *    uname        (r) filename
 *    oflag        (r) link and create flag
 *    attruname    (r) name of attribute
 *    maxreply     (r) maximum EA size as of current specs/real-life
 *
 * Returns: AFP code: AFP_OK on success or appropiate AFP error code
 *
 * Effects:
 *
 * Copies EA into rbuf. Increments *rbuflen accordingly.
 */
int get_eacontent(VFS_FUNC_ARGS_EA_GETCONTENT)
{
    int ret = AFPERR_MISC, fd = -1;
    unsigned int count = 0;
    uint32_t uint32;
    size_t toread;
    struct ea ea;
    char *eafile;

    LOG(log_debug, logtype_afpd, "get_eacontent('%s/%s')", uname, attruname);

    if ((ea_open(vol, uname, EA_RDONLY, &ea)) != 0) {
        if (errno != ENOENT)
            LOG(log_error, logtype_afpd, "get_eacontent('%s'): ea_open error", uname);
        memset(rbuf, 0, 4);
        *rbuflen += 4;
        return ret;
    }

    while (count < ea.ea_count) {
        if (strcmp(attruname, (*ea.ea_entries)[count].ea_name) == 0) {
            if ( (eafile = ea_path(&ea, attruname, 1)) == NULL) {
                ret = AFPERR_MISC;
                break;
            }

            if ((fd = open(eafile, O_RDONLY)) == -1) {
                LOG(log_error, logtype_afpd, "get_eacontent('%s'): open error: %s", uname, strerror(errno));
                ret = AFPERR_MISC;
                break;
            }

            /* Check how much the client wants, give him what we think is right */
            maxreply -= MAX_REPLY_EXTRA_BYTES;
            if (maxreply > MAX_EA_SIZE)
                maxreply = MAX_EA_SIZE;
            toread = (maxreply < (*ea.ea_entries)[count].ea_size) ? maxreply : (*ea.ea_entries)[count].ea_size;
            LOG(log_debug, logtype_afpd, "get_eacontent('%s'): sending %u bytes", attruname, toread);

            /* Put length of EA data in reply buffer */
            uint32 = htonl(toread);
            memcpy(rbuf, &uint32, 4);
            rbuf += 4;
            *rbuflen += 4;

            if (read(fd, rbuf, toread) != (ssize_t)toread) {
                LOG(log_error, logtype_afpd, "get_eacontent('%s/%s'): short read", uname, attruname);
                close(fd);
                ret = AFPERR_MISC;
                break;
            }
            *rbuflen += toread;
            close(fd);

            ret = AFP_OK;
            break;
        }
        count++;
    }

    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "get_eacontent('%s'): error closing ea handle", uname);
        return AFPERR_MISC;
    }

    return ret;

}

/*
 * Function: list_eas
 *
 * Purpose: copy names of EAs into attrnamebuf
 *
 * Arguments:
 *
 *    vol          (r) current volume
 *    attrnamebuf  (w) store names a consecutive C strings here
 *    buflen       (rw) length of names in attrnamebuf
 *    uname        (r) filename
 *    oflag        (r) link and create flag
 *
 * Returns: AFP code: AFP_OK on success or appropiate AFP error code
 *
 * Effects:
 *
 * Copies names of all EAs of uname as consecutive C strings into rbuf.
 * Increments *buflen accordingly.
 */
int list_eas(VFS_FUNC_ARGS_EA_LIST)
{
    unsigned int count = 0;
    int attrbuflen = *buflen, ret = AFP_OK, len;
    char *buf = attrnamebuf;
    struct ea ea;

    LOG(log_debug, logtype_afpd, "list_eas: file: %s", uname);

    if ((ea_open(vol, uname, EA_RDONLY, &ea)) != 0) {
        if (errno != ENOENT) {
            LOG(log_error, logtype_afpd, "list_eas: error calling ea_open for file: %s", uname);
            return AFPERR_MISC;
        }
        else
            return AFP_OK;
    }

    while (count < ea.ea_count) {
        /* Convert name to CH_UTF8_MAC and directly store in in the reply buffer */
        if ( ( len = convert_string(vol->v_volcharset,
                                    CH_UTF8_MAC, 
                                    (*ea.ea_entries)[count].ea_name,
                                    (*ea.ea_entries)[count].ea_namelen,
                                    buf + attrbuflen,
                                    255))
             <= 0 ) {
            ret = AFPERR_MISC;
            goto exit;
        }
        if (len == 255)
            /* convert_string didn't 0-terminate */
            attrnamebuf[attrbuflen + 255] = 0;

        LOG(log_debug7, logtype_afpd, "list_eas(%s): EA: %s",
            uname, (*ea.ea_entries)[count].ea_name);

        attrbuflen += len + 1;
        if (attrbuflen > (ATTRNAMEBUFSIZ - 256)) {
            /* Next EA name could overflow, so bail out with error.
               FIXME: evantually malloc/memcpy/realloc whatever.
               Is it worth it ? */
            LOG(log_warning, logtype_afpd, "list_eas(%s): running out of buffer for EA names", uname);
            ret = AFPERR_MISC;
            goto exit;
        }
        count++;
    }

exit:
    *buflen = attrbuflen;

    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "list_eas: error closing ea handle for file: %s", uname);
        return AFPERR_MISC;
    }

    return ret;
}

/*
 * Function: set_ea
 *
 * Purpose: set a Solaris native EA
 *
 * Arguments:
 *
 *    vol          (r) current volume
 *    uname        (r) filename
 *    attruname    (r) EA name
 *    ibuf         (r) buffer with EA content
 *    attrsize     (r) length EA in ibuf
 *    oflag        (r) link and create flag
 *
 * Returns: AFP code: AFP_OK on success or appropiate AFP error code
 *
 * Effects:
 *
 * Copies names of all EAs of uname as consecutive C strings into rbuf.
 * Increments *rbuflen accordingly.
 */
int set_ea(VFS_FUNC_ARGS_EA_SET)
{
    int ret = AFP_OK;
    struct ea ea;

    LOG(log_debug, logtype_afpd, "set_ea: file: %s", uname);

    if ((ea_open(vol, uname, EA_CREATE | EA_RDWR, &ea)) != 0) {
        LOG(log_error, logtype_afpd, "set_ea('%s'): ea_open error", uname);
        return AFPERR_MISC;
    }

    if ((ea_addentry(&ea, attruname, attrsize, oflag)) == -1) {
        LOG(log_error, logtype_afpd, "set_ea('%s'): ea_addentry error", uname);
        ret = AFPERR_MISC;
        goto exit;
    }

    if ((write_ea(&ea, attruname, ibuf, attrsize)) != 0) {
        LOG(log_error, logtype_afpd, "set_ea('%s'): write_ea error", uname);
        ret = AFPERR_MISC;
        goto exit;
    }

exit:
    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "set_ea('%s'): ea_close error", uname);
        ret = AFPERR_MISC;
        goto exit;
    }

    return ret;
}

/*
 * Function: remove_ea
 *
 * Purpose: remove a EA from a file
 *
 * Arguments:
 *
 *    vol          (r) current volume
 *    uname        (r) filename
 *    attruname    (r) EA name
 *    oflag        (r) link and create flag
 *
 * Returns: AFP code: AFP_OK on success or appropiate AFP error code
 *
 * Effects:
 *
 * Removes EA attruname from file uname.
 */
int remove_ea(VFS_FUNC_ARGS_EA_REMOVE)
{
    int ret = AFP_OK;
    struct ea ea;

    LOG(log_debug, logtype_afpd, "remove_ea('%s/%s')", uname, attruname);

    if ((ea_open(vol, uname, EA_RDWR, &ea)) != 0) {
        LOG(log_error, logtype_afpd, "remove_ea('%s'): ea_open error", uname);
        return AFPERR_MISC;
    }

    if ((ea_delentry(&ea, attruname)) == -1) {
        LOG(log_error, logtype_afpd, "remove_ea('%s'): ea_delentry error", uname);
        ret = AFPERR_MISC;
        goto exit;
    }

    if ((delete_ea_file(&ea, attruname)) != 0) {
        LOG(log_error, logtype_afpd, "remove_ea('%s'): delete_ea error", uname);
        ret = AFPERR_MISC;
        goto exit;
    }

exit:
    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "remove_ea('%s'): ea_close error", uname);
        ret = AFPERR_MISC;
        goto exit;
    }

    return ret;
}

/******************************************************************************************
 * EA VFS funcs that deal with file/dir cp/mv/rm
 ******************************************************************************************/

int ea_deletefile(VFS_FUNC_ARGS_DELETEFILE)
{
    unsigned int count = 0;
    int ret = AFP_OK;
    int cwd = -1;
    struct ea ea;

    LOG(log_debug, logtype_afpd, "ea_deletefile('%s')", file);

    /* Open EA stuff */
    if ((ea_openat(vol, dirfd, file, EA_RDWR, &ea)) != 0) {
        if (errno == ENOENT)
            /* no EA files, nothing to do */
            return AFP_OK;
        else {
            LOG(log_error, logtype_afpd, "ea_deletefile('%s'): error calling ea_open", file);
            return AFPERR_MISC;
        }
    }

    if (dirfd != -1) {
        if (((cwd = open(".", O_RDONLY)) == -1) || (fchdir(dirfd) != 0)) {
            ret = AFPERR_MISC;
            goto exit;
        }
    }

    while (count < ea.ea_count) {
        if ((delete_ea_file(&ea, (*ea.ea_entries)[count].ea_name)) != 0) {
            ret = AFPERR_MISC;
            continue;
        }
        free((*ea.ea_entries)[count].ea_name);
        (*ea.ea_entries)[count].ea_name = NULL;
        count++;
    }

    /* ea_close removes the EA header file for us because all names are NULL */
    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "ea_deletefile('%s'): error closing ea handle", file);
        ret = AFPERR_MISC;
    }

    if (dirfd != -1 && fchdir(cwd) != 0) {
        LOG(log_error, logtype_afpd, "ea_deletefile: cant chdir back. exit!");
        exit(EXITERR_SYS);
    }

exit:
    if (cwd != -1)
        close(cwd);

    return ret;
}

int ea_renamefile(VFS_FUNC_ARGS_RENAMEFILE)
{
    unsigned int count = 0;
    int    ret = AFP_OK;
    size_t easize;
    char   srceapath[ MAXPATHLEN + 1];
    char   *eapath;
    char   *eaname;
    struct ea srcea;
    struct ea dstea;
    struct adouble ad;

    LOG(log_debug, logtype_afpd, "ea_renamefile('%s'/'%s')", src, dst);
            

    /* Open EA stuff */
    if ((ea_openat(vol, dirfd, src, EA_RDWR, &srcea)) != 0) {
        if (errno == ENOENT)
            /* no EA files, nothing to do */
            return AFP_OK;
        else {
            LOG(log_error, logtype_afpd, "ea_renamefile('%s'/'%s'): ea_open error: '%s'", src, dst, src);
            return AFPERR_MISC;
        }
    }

    if ((ea_open(vol, dst, EA_RDWR | EA_CREATE, &dstea)) != 0) {
        if (errno == ENOENT) {
            /* Possibly the .AppleDouble folder didn't exist, we create it and try again */
            ad_init(&ad, vol); 
            if ((ad_open(&ad, dst, ADFLAGS_HF | ADFLAGS_RDWR | ADFLAGS_CREATE, 0666)) != 0) {
                LOG(log_error, logtype_afpd, "ea_renamefile('%s/%s'): ad_open error: '%s'", src, dst, dst);
                ret = AFPERR_MISC;
                goto exit;
            }
            ad_close(&ad, ADFLAGS_HF);
            if ((ea_open(vol, dst, EA_RDWR | EA_CREATE, &dstea)) != 0) {
                ret = AFPERR_MISC;
                goto exit;
            }
        }
    }

    /* Loop through all EAs: */
    while (count < srcea.ea_count) {
        /* Move EA */
        eaname = (*srcea.ea_entries)[count].ea_name;
        easize = (*srcea.ea_entries)[count].ea_size;

        /* Build src and dst paths for rename() */
        if ((eapath = ea_path(&srcea, eaname, 1)) == NULL) {
            ret = AFPERR_MISC;
            goto exit;
        }
        strcpy(srceapath, eapath);
        if ((eapath = ea_path(&dstea, eaname, 1)) == NULL) {
            ret = AFPERR_MISC;
            goto exit;
        }

        LOG(log_maxdebug, logtype_afpd, "ea_renamefile('%s/%s'): moving EA '%s' to '%s'",
            src, dst, srceapath, eapath);

        /* Add EA to dstea */
        if ((ea_addentry(&dstea, eaname, easize, 0)) == -1) {
            LOG(log_error, logtype_afpd, "ea_renamefile('%s/%s'): moving EA '%s' to '%s'",
                src, dst, srceapath, eapath);
            ret = AFPERR_MISC;
            goto exit;
        }

        /* Remove EA entry from srcea */
        if ((ea_delentry(&srcea, eaname)) == -1) {
            LOG(log_error, logtype_afpd, "ea_renamefile('%s/%s'): moving EA '%s' to '%s'",
                src, dst, srceapath, eapath);
            ea_delentry(&dstea, eaname);
            ret = AFPERR_MISC;
            goto exit;
        }

        /* Now rename the EA */
        if ((unix_rename(dirfd, srceapath, -1, eapath)) < 0) {
            LOG(log_error, logtype_afpd, "ea_renamefile('%s/%s'): moving EA '%s' to '%s'",
                src, dst, srceapath, eapath);
            ret = AFPERR_MISC;
            goto exit;
        }

        count++;
    }


exit:
    ea_close(&srcea);
    ea_close(&dstea);
	return ret;
}

int ea_copyfile(VFS_FUNC_ARGS_COPYFILE)
{
    unsigned int count = 0;
    int    ret = AFP_OK;
    size_t easize;
    char   srceapath[ MAXPATHLEN + 1];
    char   *eapath;
    char   *eaname;
    struct ea srcea;
    struct ea dstea;
    struct adouble ad;

    LOG(log_debug, logtype_afpd, "ea_copyfile('%s'/'%s')", src, dst);

    /* Open EA stuff */
    if ((ea_openat(vol, sfd, src, EA_RDWR, &srcea)) != 0) {
        if (errno == ENOENT)
            /* no EA files, nothing to do */
            return AFP_OK;
        else {
            LOG(log_error, logtype_afpd, "ea_copyfile('%s'/'%s'): ea_open error: '%s'", src, dst, src);
            return AFPERR_MISC;
        }
    }

    if ((ea_open(vol, dst, EA_RDWR | EA_CREATE, &dstea)) != 0) {
        if (errno == ENOENT) {
            /* Possibly the .AppleDouble folder didn't exist, we create it and try again */
            ad_init(&ad, vol);
            if ((ad_open(&ad, dst, ADFLAGS_HF | ADFLAGS_RDWR | ADFLAGS_CREATE, 0666)) != 0) {
                LOG(log_error, logtype_afpd, "ea_copyfile('%s/%s'): ad_open error: '%s'", src, dst, dst);
                ret = AFPERR_MISC;
                goto exit;
            }
            ad_close(&ad, ADFLAGS_HF);
            if ((ea_open(vol, dst, EA_RDWR | EA_CREATE, &dstea)) != 0) {
                ret = AFPERR_MISC;
                goto exit;
            }
        }
    }

    /* Loop through all EAs: */
    while (count < srcea.ea_count) {
        /* Copy EA */
        eaname = (*srcea.ea_entries)[count].ea_name;
        easize = (*srcea.ea_entries)[count].ea_size;

        /* Build src and dst paths for copy_file() */
        if ((eapath = ea_path(&srcea, eaname, 1)) == NULL) {
            ret = AFPERR_MISC;
            goto exit;
        }
        strcpy(srceapath, eapath);
        if ((eapath = ea_path(&dstea, eaname, 1)) == NULL) {
            ret = AFPERR_MISC;
            goto exit;
        }

        LOG(log_maxdebug, logtype_afpd, "ea_copyfile('%s/%s'): copying EA '%s' to '%s'",
            src, dst, srceapath, eapath);

        /* Add EA to dstea */
        if ((ea_addentry(&dstea, eaname, easize, 0)) == -1) {
            LOG(log_error, logtype_afpd, "ea_copyfile('%s/%s'): ea_addentry('%s') error",
                src, dst, eaname);
            ret = AFPERR_MISC;
            goto exit;
        }

        /* Now copy the EA */
        if ((copy_file(sfd, srceapath, eapath, (0666 & ~vol->v_umask))) < 0) {
            LOG(log_error, logtype_afpd, "ea_copyfile('%s/%s'): copying EA '%s' to '%s'",
                src, dst, srceapath, eapath);
            ret = AFPERR_MISC;
            goto exit;
        }

        count++;
    }

exit:
    ea_close(&srcea);
    ea_close(&dstea);
	return ret;
}

int ea_chown(VFS_FUNC_ARGS_CHOWN)
{

    unsigned int count = 0;
    int ret = AFP_OK;
    char *eaname;
    struct ea ea;

    LOG(log_debug, logtype_afpd, "ea_chown('%s')", path);
    /* Open EA stuff */
    if ((ea_open(vol, path, EA_RDWR, &ea)) != 0) {
        if (errno == ENOENT)
            /* no EA files, nothing to do */
            return AFP_OK;
        else {
            LOG(log_error, logtype_afpd, "ea_chown('%s'): error calling ea_open", path);
            return AFPERR_MISC;
        }
    }

    if ((ochown(ea_path(&ea, NULL, 0), uid, gid, vol_syml_opt(vol))) != 0) {
        switch (errno) {
        case EPERM:
        case EACCES:
            ret = AFPERR_ACCESS;
            goto exit;
        default:
            ret = AFPERR_MISC;
            goto exit;
        }
    }

    while (count < ea.ea_count) {
        if ((eaname = ea_path(&ea, (*ea.ea_entries)[count].ea_name, 1)) == NULL) {
            ret = AFPERR_MISC;
            goto exit;
        }
        if ((ochown(eaname, uid, gid, vol_syml_opt(vol))) != 0) {
            switch (errno) {
            case EPERM:
            case EACCES:
                ret = AFPERR_ACCESS;
                goto exit;
            default:
                ret = AFPERR_MISC;
                goto exit;
            }
            continue;
        }

        count++;
    }

exit:
    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "ea_chown('%s'): error closing ea handle", path);
        return AFPERR_MISC;
    }

    return ret;
}

int ea_chmod_file(VFS_FUNC_ARGS_SETFILEMODE)
{

    unsigned int count = 0;
    int ret = AFP_OK;
    const char *eaname;
    struct ea ea;

    LOG(log_debug, logtype_afpd, "ea_chmod_file('%s')", name);
    /* Open EA stuff */
    if ((ea_open(vol, name, EA_RDWR, &ea)) != 0) {
        if (errno == ENOENT)
            /* no EA files, nothing to do */
            return AFP_OK;
        else
            return AFPERR_MISC;
    }

    /* Set mode on EA header file */
    if ((setfilmode(vol, ea_path(&ea, NULL, 0), ea_header_mode(mode), NULL)) != 0) {
        LOG(log_error, logtype_afpd, "ea_chmod_file('%s'): %s", ea_path(&ea, NULL, 0), strerror(errno));
        switch (errno) {
        case EPERM:
        case EACCES:
            ret = AFPERR_ACCESS;
            goto exit;
        default:
            ret = AFPERR_MISC;
            goto exit;
        }
    }

    /* Set mode on EA files */
    while (count < ea.ea_count) {
        if ((eaname = ea_path(&ea, (*ea.ea_entries)[count].ea_name, 1)) == NULL) {
            ret = AFPERR_MISC;
            goto exit;
        }
        if ((setfilmode(vol, eaname, ea_mode(mode), NULL)) != 0) {
            LOG(log_error, logtype_afpd, "ea_chmod_file('%s'): %s", eaname, strerror(errno));
            switch (errno) {
            case EPERM:
            case EACCES:
                ret = AFPERR_ACCESS;
                goto exit;
            default:
                ret = AFPERR_MISC;
                goto exit;
            }
            continue;
        }

        count++;
    }

exit:
    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "ea_chmod_file('%s'): error closing ea handle", name);
        return AFPERR_MISC;
    }

    return ret;
}

int ea_chmod_dir(VFS_FUNC_ARGS_SETDIRUNIXMODE)
{

    int ret = AFP_OK;
    unsigned int count = 0;
    const char *eaname;
    const char *eaname_safe = NULL;
    struct ea ea;

    LOG(log_debug, logtype_afpd, "ea_chmod_dir('%s')", name);
    /* .AppleDouble already might be inaccesible, so we must run as id 0 */
    become_root();

    /* Open EA stuff */
    if ((ea_open(vol, name, EA_RDWR, &ea)) != 0) {
        /* ENOENT --> no EA files, nothing to do */
        if (errno != ENOENT)
            ret = AFPERR_MISC;
        unbecome_root();
        return ret;
    }

    /* Set mode on EA header */
    if ((setfilmode(vol, ea_path(&ea, NULL, 0), ea_header_mode(mode), NULL)) != 0) {
        LOG(log_error, logtype_afpd, "ea_chmod_dir('%s'): %s", ea_path(&ea, NULL, 0), strerror(errno));
        switch (errno) {
        case EPERM:
        case EACCES:
            ret = AFPERR_ACCESS;
            goto exit;
        default:
            ret = AFPERR_MISC;
            goto exit;
        }
    }

    /* Set mode on EA files */
    while (count < ea.ea_count) {
        eaname = (*ea.ea_entries)[count].ea_name;
        /*
         * Be careful with EA names from the EA header!
         * Eg NFS users might have access to them, can inject paths using ../ or /.....
         * FIXME:
         * Until the EA code escapes / in EA name requests from the client, these therefor wont work.
         */
        if ((eaname_safe = strrchr(eaname, '/'))) {
            LOG(log_warning, logtype_afpd, "ea_chmod_dir('%s'): contains a slash", eaname);
            eaname = eaname_safe;
        }
        if ((eaname = ea_path(&ea, eaname, 1)) == NULL) {
            ret = AFPERR_MISC;
            goto exit;
        }
        if ((setfilmode(vol, eaname, ea_mode(mode), NULL)) != 0) {
            LOG(log_error, logtype_afpd, "ea_chmod_dir('%s'): %s", eaname, strerror(errno));
            switch (errno) {
            case EPERM:
            case EACCES:
                ret = AFPERR_ACCESS;
                goto exit;
            default:
                ret = AFPERR_MISC;
                goto exit;
            }
            continue;
        }

        count++;
    }

exit:
    unbecome_root();

    if ((ea_close(&ea)) != 0) {
        LOG(log_error, logtype_afpd, "ea_chmod_dir('%s'): error closing ea handle", name);
        return AFPERR_MISC;
    }

    return ret;
}
