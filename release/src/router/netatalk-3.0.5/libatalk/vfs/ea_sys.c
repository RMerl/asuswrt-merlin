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

/**********************************************************************************
 * EA VFS funcs for storing EAs in nativa filesystem EAs
 **********************************************************************************/

/*
 * Function: sys_get_easize
 *
 * Purpose: get size of a native EA
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
int sys_get_easize(VFS_FUNC_ARGS_EA_GETSIZE)
{
    ssize_t   ret;
    uint32_t  attrsize;

    LOG(log_debug7, logtype_afpd, "sys_getextattr_size(%s): attribute: \"%s\"", uname, attruname);

    if ((oflag & O_NOFOLLOW) ) {
        ret = sys_lgetxattr(uname, attruname, rbuf +4, 0);
    }
    else {
        ret = sys_getxattr(uname, attruname,  rbuf +4, 0);
    }
    
    if (ret == -1) {
        memset(rbuf, 0, 4);
        *rbuflen += 4;
        switch(errno) {
        case OPEN_NOFOLLOW_ERRNO:
            /* its a symlink and client requested O_NOFOLLOW  */
            LOG(log_debug, logtype_afpd, "sys_getextattr_size(%s): symlink with kXAttrNoFollow", uname);
            return AFPERR_MISC;

        case ENOATTR:
        case ENOENT:
            return AFPERR_MISC;

        default:
            LOG(log_debug, logtype_afpd, "sys_getextattr_size: error: %s", strerror(errno));
            return AFPERR_MISC;
        }
    }

    if (ret > MAX_EA_SIZE) 
      ret = MAX_EA_SIZE;

    /* Start building reply packet */
    LOG(log_debug7, logtype_afpd, "sys_getextattr_size(%s): attribute: \"%s\", size: %u", uname, attruname, ret);

    /* length of attribute data */
    attrsize = htonl((uint32_t)ret);
    memcpy(rbuf, &attrsize, 4);
    *rbuflen += 4;

    ret = AFP_OK;
    return ret;
}

/*
 * Function: sys_get_eacontent
 *
 * Purpose: copy native EA into rbuf
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
int sys_get_eacontent(VFS_FUNC_ARGS_EA_GETCONTENT)
{
    ssize_t   ret;
    uint32_t  attrsize;

    /* Start building reply packet */

    maxreply -= MAX_REPLY_EXTRA_BYTES;

    if (maxreply > MAX_EA_SIZE)
        maxreply = MAX_EA_SIZE;

    LOG(log_debug7, logtype_afpd, "sys_getextattr_content(%s): attribute: \"%s\", size: %u", uname, attruname, maxreply);

    if ((oflag & O_NOFOLLOW) ) {
        ret = sys_lgetxattr(uname, attruname, rbuf +4, maxreply);
    }
    else {
        ret = sys_getxattr(uname, attruname,  rbuf +4, maxreply);
    }
    
    if (ret == -1) {
        memset(rbuf, 0, 4);
        *rbuflen += 4;
        switch(errno) {
        case OPEN_NOFOLLOW_ERRNO:
            /* its a symlink and client requested O_NOFOLLOW  */
            LOG(log_debug, logtype_afpd, "sys_getextattr_content(%s): symlink with kXAttrNoFollow", uname);
            return AFPERR_MISC;

        case ENOATTR:
            return AFPERR_MISC;

        default:
            LOG(log_debug, logtype_afpd, "sys_getextattr_content(%s): error: %s", attruname, strerror(errno));
            return AFPERR_MISC;
        }
    }

    /* remember where we must store length of attribute data in rbuf */
    *rbuflen += 4 +ret;

    attrsize = htonl((uint32_t)ret);
    memcpy(rbuf, &attrsize, 4);

    return AFP_OK;
}

/*
 * Function: sys_list_eas
 *
 * Purpose: copy names of native EAs into attrnamebuf
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
 * Increments *rbuflen accordingly.
 * We hide the adouble:ea extended attributes here, but we currently
 * allow reading, writing and deleteting them.
 */
int sys_list_eas(VFS_FUNC_ARGS_EA_LIST)
{
    ssize_t attrbuflen = *buflen;
    int     ret, len, nlen;
    char    *buf;
    char    *ptr;
        
    buf = malloc(ATTRNAMEBUFSIZ);
    if (!buf)
        return AFPERR_MISC;

    if ((oflag & O_NOFOLLOW)) {
        ret = sys_llistxattr(uname, buf, ATTRNAMEBUFSIZ);
    }
    else {
        ret = sys_listxattr(uname, buf, ATTRNAMEBUFSIZ);
    }

    if (ret == -1) switch(errno) {
        case OPEN_NOFOLLOW_ERRNO:
            /* its a symlink and client requested O_NOFOLLOW, we pretend 0 EAs */
            LOG(log_debug, logtype_afpd, "sys_list_extattr(%s): symlink with kXAttrNoFollow", uname);
            ret = AFP_OK;
            goto exit;
        default:
            LOG(log_debug, logtype_afpd, "sys_list_extattr(%s): error opening atttribute dir: %s", uname, strerror(errno));
            ret = AFPERR_MISC;
            goto exit;
    }
    
    ptr = buf;
    while (ret > 0)  {
        len = strlen(ptr);
        if (NOT_NETATALK_EA(ptr)) {
            /* Convert name to CH_UTF8_MAC and directly store in in the reply buffer */
            if ( 0 >= ( nlen = convert_string(vol->v_volcharset, CH_UTF8_MAC, ptr, len, attrnamebuf + attrbuflen, 256)) ) {
                ret = AFPERR_MISC;
                goto exit;
            }

            LOG(log_debug7, logtype_afpd, "sys_list_extattr(%s): attribute: %s", uname, ptr);

            attrbuflen += nlen + 1;
            if (attrbuflen > (ATTRNAMEBUFSIZ - 256)) {
                /* Next EA name could overflow, so bail out with error.
                   FIXME: evantually malloc/memcpy/realloc whatever.
                   Is it worth it ? */
                LOG(log_warning, logtype_afpd, "sys_list_extattr(%s): running out of buffer for EA names", uname);
                ret = AFPERR_MISC;
                goto exit;
            }
        }
        ret -= len + 1;
        ptr += len + 1;
    }

    ret = AFP_OK;

exit:
    free(buf);
    *buflen = attrbuflen;
    return ret;
}

/*
 * Function: sys_set_ea
 *
 * Purpose: set a native EA
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
 */
int sys_set_ea(VFS_FUNC_ARGS_EA_SET)
{
    int attr_flag;
    int ret;

    attr_flag = 0;
    if ((oflag & O_CREAT) ) 
        attr_flag |= XATTR_CREATE;

    else if ((oflag & O_TRUNC) ) 
        attr_flag |= XATTR_REPLACE;
    
    if ((oflag & O_NOFOLLOW) ) {
        ret = sys_lsetxattr(uname, attruname,  ibuf, attrsize,attr_flag);
    }
    else {
        ret = sys_setxattr(uname, attruname,  ibuf, attrsize, attr_flag);
    }

    if (ret == -1) {
        switch(errno) {
        case OPEN_NOFOLLOW_ERRNO:
            /* its a symlink and client requested O_NOFOLLOW  */
            LOG(log_debug, logtype_afpd, "sys_set_ea(\"%s\", ea:'%s'): symlink with kXAttrNoFollow",
                uname, attruname);
            return AFP_OK;
        case EEXIST:
            LOG(log_debug, logtype_afpd, "sys_set_ea(\"%s/%s\", ea:'%s'): EA already exists",
                getcwdpath(), uname, attruname);
            return AFPERR_EXIST;
        default:
            LOG(log_error, logtype_afpd, "sys_set_ea(\"%s/%s\", ea:'%s', size: %u, flags: %s|%s|%s): %s",
                getcwdpath(), uname, attruname, attrsize, 
                oflag & O_CREAT ? "XATTR_CREATE" : "-",
                oflag & O_TRUNC ? "XATTR_REPLACE" : "-",
                oflag & O_NOFOLLOW ? "O_NOFOLLOW" : "-",
                strerror(errno));
            return AFPERR_MISC;
        }
    }

    return AFP_OK;
}

/*
 * Function: sys_remove_ea
 *
 * Purpose: remove a native EA
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
int sys_remove_ea(VFS_FUNC_ARGS_EA_REMOVE)
{
    int ret;

    if ((oflag & O_NOFOLLOW) ) {
        ret = sys_lremovexattr(uname, attruname);
    }
    else {
        ret = sys_removexattr(uname, attruname);
    }

    if (ret == -1) {
        switch(errno) {
        case OPEN_NOFOLLOW_ERRNO:
            /* its a symlink and client requested O_NOFOLLOW  */
            LOG(log_debug, logtype_afpd, "sys_remove_ea(%s/%s): symlink with kXAttrNoFollow", uname);
            return AFP_OK;
        default:
            LOG(log_debug, logtype_afpd, "sys_remove_ea(%s/%s): error: %s", uname, attruname, strerror(errno));
            return AFPERR_MISC;
        }
    }

    return AFP_OK;
}

/*
 * @brief Copy EAs
 *
 * @note Supports *at semantics, therfor switches back and forth between sfd and cwd
 */
int sys_ea_copyfile(VFS_FUNC_ARGS_COPYFILE)
{
  	int ret = 0;
    int cwd = -1;
	ssize_t size;
	char *names = NULL, *end_names, *name, *value = NULL;
	unsigned int setxattr_ENOTSUP = 0;

    if (sfd != -1) {
        if ((cwd = open(".", O_RDONLY)) == -1) {
            LOG(log_error, logtype_afpd, "sys_ea_copyfile: cant open cwd: %s",
                strerror(errno));
            ret = -1;
            goto getout;
        }
    }

    if (sfd != -1) {
        if (fchdir(sfd) == -1) {
            LOG(log_error, logtype_afpd, "sys_ea_copyfile: cant chdir to sfd: %s",
                strerror(errno));
            ret = -1;
            goto getout;
        }
    }

	size = sys_listxattr(src, NULL, 0);
	if (size < 0) {
		if (errno != ENOSYS && errno != ENOTSUP) {
			ret = -1;
		}
		goto getout;
	}
	names = malloc(size+1);
	if (names == NULL) {
		ret = -1;
		goto getout;
	}
	size = sys_listxattr(src, names, size);
	if (size < 0) {
		ret = -1;
		goto getout;
	} else {
		names[size] = '\0';
		end_names = names + size;
	}

    if (sfd != -1) {
        if (fchdir(cwd) == -1) {
            LOG(log_error, logtype_afpd, "sys_ea_copyfile: cant chdir to cwd: %s",
                strerror(errno));
            ret = -1;
            goto getout;
        }
    }

	for (name = names; name != end_names; name = strchr(name, '\0') + 1) {
		void *old_value;

		/* check if this attribute shall be preserved */
		if (!*name)
			continue;

        if (STRCMP(name, ==, AD_EA_META))
            continue;

        if (sfd != -1) {
            if (fchdir(sfd) == -1) {
                LOG(log_error, logtype_afpd, "sys_ea_copyfile: cant chdir to sfd: %s",
                    strerror(errno));
                ret = -1;
                goto getout;
            }
        }

		size = sys_getxattr (src, name, NULL, 0);
		if (size < 0) {
			ret = -1;
			continue;
		}
		value = realloc(old_value = value, size);
		if (size != 0 && value == NULL) {
			free(old_value);
			ret = -1;
		}
		size = sys_getxattr(src, name, value, size);
		if (size < 0) {
			ret = -1;
			continue;
		}

        if (sfd != -1) {
            if (fchdir(cwd) == -1) {
                LOG(log_error, logtype_afpd, "sys_ea_copyfile: cant chdir to cwd: %s",
                    strerror(errno));
                ret = -1;
                goto getout;
            }
        }

		if (sys_setxattr(dst, name, value, size, 0) != 0) {
			if (errno == ENOTSUP)
				setxattr_ENOTSUP++;
			else {
				if (errno == ENOSYS) {
					ret = -1;
					/* no hope of getting any further */
					break;
				} else {
					ret = -1;
				}
			}
		}
	}
	if (setxattr_ENOTSUP) {
		errno = ENOTSUP;
		ret = -1;
	}

getout:
    if (cwd != -1)
        close(cwd);
        
	free(value);
	free(names);

    if (ret == -1) {
        switch(errno) {
        case ENOENT:
            /* no attribute */
            break;
        case EACCES:
            LOG(log_debug, logtype_afpd, "sys_ea_copyfile(%s, %s): error: %s", src, dst, strerror(errno));
            return AFPERR_ACCESS;
        default:
            LOG(log_error, logtype_afpd, "sys_ea_copyfile(%s, %s): error: %s", src, dst, strerror(errno));
            return AFPERR_MISC;
        }
    }

    return AFP_OK;
}
