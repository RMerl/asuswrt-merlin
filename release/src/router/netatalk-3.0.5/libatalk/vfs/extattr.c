/* 
   Unix SMB/CIFS implementation.
   Samba system utilities
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison  1998-2005
   Copyright (C) Timur Bakeyev        2005
   Copyright (C) Bjoern Jacke    2006-2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
   sys_copyxattr modified from LGPL2.1 libattr copyright
   Copyright (C) 2001-2002 Silicon Graphics, Inc.  All Rights Reserved.
   Copyright (C) 2001 Andreas Gruenbacher.
      
   Samba 3.0.28, modified for netatalk.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#if HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#elif HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#ifdef HAVE_SYS_EA_H
#include <sys/ea.h>
#endif

#ifdef HAVE_ATTROPEN

#include <dirent.h>
#endif

#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif

#include <atalk/adouble.h>
#include <atalk/util.h>
#include <atalk/logger.h>
#include <atalk/ea.h>
#include <atalk/compat.h>
#include <atalk/errchk.h>

/******** Solaris EA helper function prototypes ********/
#ifdef HAVE_ATTROPEN
#define SOLARIS_ATTRMODE S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH
static int solaris_write_xattr(int attrfd, const char *value, size_t size);
static ssize_t solaris_read_xattr(int attrfd, void *value, size_t size);
static ssize_t solaris_list_xattr(int attrdirfd, char *list, size_t size);
static int solaris_unlinkat(int attrdirfd, const char *name);
static int solaris_attropen(const char *path, const char *attrpath, int oflag, mode_t mode);
static int solaris_openat(int fildes, const char *path, int oflag, mode_t mode);
#endif

/**************************************************************************
 Wrappers for extented attribute calls. Based on the Linux package with
 support for IRIX and (Net|Free)BSD also. Expand as other systems have them.
****************************************************************************/
static char attr_name[256 +5] = "user.";

static const char *prefix(const char *uname)
{
#if defined(HAVE_ATTROPEN)
	return uname;
#else
	strlcpy(attr_name +5, uname, 256);
	return attr_name;
#endif
}

int sys_getxattrfd(int fd, const char *uname, int oflag, ...)
{
#if defined HAVE_ATTROPEN
    int eafd;
    va_list args;
    mode_t mode = 0;

    if (oflag & O_CREAT) {
        va_start(args, oflag);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    if (oflag & O_CREAT)
        eafd = solaris_openat(fd, uname, oflag | O_XATTR, mode);
    else
        eafd = solaris_openat(fd, uname, oflag | O_XATTR, mode);

    return eafd;
#else
    errno = ENOSYS;
    return -1;
#endif
}

ssize_t sys_getxattr (const char *path, const char *uname, void *value, size_t size)
{
	const char *name = prefix(uname);

#if defined(HAVE_GETXATTR)
#ifndef XATTR_ADD_OPT
	return getxattr(path, name, value, size);
#else
	int options = 0;
	return getxattr(path, name, value, size, 0, options);
#endif
#elif defined(HAVE_GETEA)
	return getea(path, name, value, size);
#elif defined(HAVE_EXTATTR_GET_FILE)
	ssize_t retval;
	/*
	 * The BSD implementation has a nasty habit of silently truncating
	 * the returned value to the size of the buffer, so we have to check
	 * that the buffer is large enough to fit the returned value.
	 */
	if((retval = extattr_get_file(path, EXTATTR_NAMESPACE_USER, uname, NULL, 0)) >= 0) {
        if (size == 0)
            /* size == 0 means only return size */
            return retval;
		if (retval > size) {
			errno = ERANGE;
			return -1;
		}
		if ((retval = extattr_get_file(path, EXTATTR_NAMESPACE_USER, uname, value, size)) >= 0)
			return retval;
	}

	LOG(log_maxdebug, logtype_default, "sys_getxattr: extattr_get_file() failed with: %s\n", strerror(errno));
	return -1;
#elif defined(HAVE_ATTR_GET)
	int retval, flags = 0;
	int valuelength = (int)size;
	char *attrname = strchr(name,'.') + 1;
	
	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	retval = attr_get(path, attrname, (char *)value, &valuelength, flags);

	return retval ? retval : valuelength;
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrfd = solaris_attropen(path, name, O_RDONLY, 0);
	if (attrfd >= 0) {
		ret = solaris_read_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

ssize_t sys_fgetxattr (int filedes, const char *uname, void *value, size_t size)
{
    const char *name = prefix(uname);

#if defined(HAVE_FGETXATTR)
#ifndef XATTR_ADD_OPT
    return fgetxattr(filedes, name, value, size);
#else
    int options = 0;
    return fgetxattr(filedes, name, value, size, 0, options);
#endif
#elif defined(HAVE_FGETEA)
    return fgetea(filedes, name, value, size);
#elif defined(HAVE_EXTATTR_GET_FD)
    char *s;
    ssize_t retval;
    int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
        EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
    const char *attrname = ((s=strchr(name, '.')) == NULL) ? name : s + 1;

    if((retval=extattr_get_fd(filedes, attrnamespace, attrname, NULL, 0)) >= 0) {
        if(retval > size) {
            errno = ERANGE;
            return -1;
        }
        if((retval=extattr_get_fd(filedes, attrnamespace, attrname, value, size)) >= 0)
            return retval;
    }

    LOG(log_debug, logtype_default, "sys_fgetxattr: extattr_get_fd(): %s", strerror(errno));
    return -1;
#elif defined(HAVE_ATTR_GETF)
    int retval, flags = 0;
    int valuelength = (int)size;
    char *attrname = strchr(name,'.') + 1;

    if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

    retval = attr_getf(filedes, attrname, (char *)value, &valuelength, flags);

    return retval ? retval : valuelength;
#elif defined(HAVE_ATTROPEN)
    ssize_t ret = -1;
    int attrfd = solaris_openat(filedes, name, O_RDONLY|O_XATTR, 0);
    if (attrfd >= 0) {
        ret = solaris_read_xattr(attrfd, value, size);
        close(attrfd);
    }
    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

ssize_t sys_lgetxattr (const char *path, const char *uname, void *value, size_t size)
{
	const char *name = prefix(uname);

#if defined(HAVE_LGETXATTR)
	return lgetxattr(path, name, value, size);
#elif defined(HAVE_GETXATTR) && defined(XATTR_ADD_OPT)
	int options = XATTR_NOFOLLOW;
	return getxattr(path, name, value, size, 0, options);
#elif defined(HAVE_LGETEA)
	return lgetea(path, name, value, size);
#elif defined(HAVE_EXTATTR_GET_LINK)
	ssize_t retval;
	if((retval=extattr_get_link(path, EXTATTR_NAMESPACE_USER, uname, NULL, 0)) >= 0) {
		if(retval > size) {
			errno = ERANGE;
			return -1;
		}
		if((retval=extattr_get_link(path, EXTATTR_NAMESPACE_USER, uname, value, size)) >= 0)
			return retval;
	}
	
	LOG(log_maxdebug, logtype_default, "sys_lgetxattr: extattr_get_link() failed with: %s\n", strerror(errno));
	return -1;
#elif defined(HAVE_ATTR_GET)
	int retval, flags = ATTR_DONTFOLLOW;
	int valuelength = (int)size;
	char *attrname = strchr(name,'.') + 1;
	
	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	retval = attr_get(path, attrname, (char *)value, &valuelength, flags);

	return retval ? retval : valuelength;
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrfd = solaris_attropen(path, name, O_RDONLY | O_NOFOLLOW, 0);
	if (attrfd >= 0) {
		ret = solaris_read_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

#if defined(HAVE_EXTATTR_LIST_FILE)

#define EXTATTR_PREFIX(s)	(s), (sizeof((s))-1)

static struct {
        int space;
	const char *name;
	size_t len;
} 
extattr[] = {
	{ EXTATTR_NAMESPACE_SYSTEM, EXTATTR_PREFIX("") },
        { EXTATTR_NAMESPACE_USER, EXTATTR_PREFIX("") },
};

typedef union {
	const char *path;
	int filedes;
} extattr_arg;

static ssize_t bsd_attr_list (int type, extattr_arg arg, char *list, size_t size)
{
	ssize_t list_size;
	int i, len;

    switch(type) {
#if defined(HAVE_EXTATTR_LIST_FILE)
    case 0:
        list_size = extattr_list_file(arg.path, EXTATTR_NAMESPACE_USER, list, size);
        break;
#endif
#if defined(HAVE_EXTATTR_LIST_LINK)
    case 1:
        list_size = extattr_list_link(arg.path, EXTATTR_NAMESPACE_USER, list, size);
        break;
#endif
#if defined(HAVE_EXTATTR_LIST_FD)
    case 2:
        list_size = extattr_list_fd(arg.filedes, EXTATTR_NAMESPACE_USER, list, size);
        break;
#endif
    default:
        errno = ENOSYS;
        return -1;
    }

    /* Some error happend. Errno should be set by the previous call */
    if(list_size < 0)
        return -1;

    /* No attributes */
    if(list_size == 0)
        return 0;

    /* XXX: Call with an empty buffer may be used to calculate
       necessary buffer size. Unfortunately, we can't say, how
       many attributes were returned, so here is the potential
       problem with the emulation.
    */
    if(list == NULL)
        return list_size;

    /* Buffer is too small to fit the results */
    if(list_size > size) {
        errno = ERANGE;
        return -1;
    }

    /* Convert from pascal strings to C strings */
    len = list[0];
    memmove(list, list + 1, list_size);

    for(i = len; i < list_size; ) {
        LOG(log_maxdebug, logtype_afpd, "len: %d, i: %d", len, i);

        len = list[i];
        list[i] = '\0';
        i += len + 1;
    }

	return list_size;
}

#endif

#if defined(HAVE_ATTR_LIST) && defined(HAVE_SYS_ATTRIBUTES_H)
static char attr_buffer[ATTR_MAX_VALUELEN];

static ssize_t irix_attr_list(const char *path, int filedes, char *list, size_t size, int flags)
{
	int retval = 0, index;
	attrlist_cursor_t *cursor = 0;
	int total_size = 0;
	attrlist_t * al = (attrlist_t *)attr_buffer;
	attrlist_ent_t *ae;
	size_t ent_size, left = size;
	char *bp = list;

	while (True) {
	    if (filedes)
		retval = attr_listf(filedes, attr_buffer, ATTR_MAX_VALUELEN, flags, cursor);
	    else
		retval = attr_list(path, attr_buffer, ATTR_MAX_VALUELEN, flags, cursor);
	    if (retval) break;
	    for (index = 0; index < al->al_count; index++) {
		ae = ATTR_ENTRY(attr_buffer, index);
		ent_size = strlen(ae->a_name) + sizeof("user.");
		if (left >= ent_size) {
		    strncpy(bp, "user.", sizeof("user."));
		    strncat(bp, ae->a_name, ent_size - sizeof("user."));
		    bp += ent_size;
		    left -= ent_size;
		} else if (size) {
		    errno = ERANGE;
		    retval = -1;
		    break;
		}
		total_size += ent_size;
	    }
	    if (al->al_more == 0) break;
	}
	if (retval == 0) {
	    flags |= ATTR_ROOT;
	    cursor = 0;
	    while (True) {
		if (filedes)
		    retval = attr_listf(filedes, attr_buffer, ATTR_MAX_VALUELEN, flags, cursor);
		else
		    retval = attr_list(path, attr_buffer, ATTR_MAX_VALUELEN, flags, cursor);
		if (retval) break;
		for (index = 0; index < al->al_count; index++) {
		    ae = ATTR_ENTRY(attr_buffer, index);
		    ent_size = strlen(ae->a_name) + sizeof("system.");
		    if (left >= ent_size) {
			strncpy(bp, "system.", sizeof("system."));
			strncat(bp, ae->a_name, ent_size - sizeof("system."));
			bp += ent_size;
			left -= ent_size;
		    } else if (size) {
			errno = ERANGE;
			retval = -1;
			break;
		    }
		    total_size += ent_size;
		}
		if (al->al_more == 0) break;
	    }
	}
	return (ssize_t)(retval ? retval : total_size);
}

#endif

#if defined(HAVE_LISTXATTR)
static ssize_t remove_user(ssize_t ret, char *list, size_t size)
{
	size_t len;
	char *ptr;
	char *ptr1;
	ssize_t ptrsize;
	
	if (ret <= 0 || size == 0)
		return ret;
	ptrsize = ret;
	ptr = ptr1 = list;
	while (ptrsize > 0) {
		len = strlen(ptr1) +1;
		ptrsize -= len;
		if (strncmp(ptr1, "user.",5)) {
			ptr1 += len;
			continue;
		}
		memmove(ptr, ptr1 +5, len -5);
		ptr += len -5;
		ptr1 += len;
	}
	return ptr -list;
}
#endif

ssize_t sys_listxattr (const char *path, char *list, size_t size)
{
#if defined(HAVE_LISTXATTR)
	ssize_t ret;

#ifndef XATTR_ADD_OPT
	ret = listxattr(path, list, size);
#else
	int options = 0;
	ret = listxattr(path, list, size, options);
#endif
	return remove_user(ret, list, size);

#elif defined(HAVE_LISTEA)
	return listea(path, list, size);
#elif defined(HAVE_EXTATTR_LIST_FILE)
	extattr_arg arg;
	arg.path = path;
	return bsd_attr_list(0, arg, list, size);
#elif defined(HAVE_ATTR_LIST) && defined(HAVE_SYS_ATTRIBUTES_H)
	return irix_attr_list(path, 0, list, size, 0);
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrdirfd = solaris_attropen(path, ".", O_RDONLY, 0);
	if (attrdirfd >= 0) {
		ret = solaris_list_xattr(attrdirfd, list, size);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

ssize_t sys_llistxattr (const char *path, char *list, size_t size)
{
#if defined(HAVE_LLISTXATTR)
	ssize_t ret;

	ret = llistxattr(path, list, size);
	return remove_user(ret, list, size);
#elif defined(HAVE_LISTXATTR) && defined(XATTR_ADD_OPT)
	ssize_t ret;
	int options = XATTR_NOFOLLOW;

	ret = listxattr(path, list, size, options);
	return remove_user(ret, list, size);

#elif defined(HAVE_LLISTEA)
	return llistea(path, list, size);
#elif defined(HAVE_EXTATTR_LIST_LINK)
	extattr_arg arg;
	arg.path = path;
	return bsd_attr_list(1, arg, list, size);
#elif defined(HAVE_ATTR_LIST) && defined(HAVE_SYS_ATTRIBUTES_H)
	return irix_attr_list(path, 0, list, size, ATTR_DONTFOLLOW);
#elif defined(HAVE_ATTROPEN)
	ssize_t ret = -1;
	int attrdirfd = solaris_attropen(path, ".", O_RDONLY | O_NOFOLLOW, 0);
	if (attrdirfd >= 0) {
		ret = solaris_list_xattr(attrdirfd, list, size);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_removexattr (const char *path, const char *uname)
{
	const char *name = prefix(uname);
#if defined(HAVE_REMOVEXATTR)
#ifndef XATTR_ADD_OPT
	return removexattr(path, name);
#else
	int options = 0;
	return removexattr(path, name, options);
#endif
#elif defined(HAVE_REMOVEEA)
	return removeea(path, name);
#elif defined(HAVE_EXTATTR_DELETE_FILE)
	return extattr_delete_file(path, EXTATTR_NAMESPACE_USER, uname);
#elif defined(HAVE_ATTR_REMOVE)
	int flags = 0;
	char *attrname = strchr(name,'.') + 1;
	
	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	return attr_remove(path, attrname, flags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int attrdirfd = solaris_attropen(path, ".", O_RDONLY, 0);
	if (attrdirfd >= 0) {
		ret = solaris_unlinkat(attrdirfd, name);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_lremovexattr (const char *path, const char *uname)
{
	const char *name = prefix(uname);
#if defined(HAVE_LREMOVEXATTR)
	return lremovexattr(path, name);
#elif defined(HAVE_REMOVEXATTR) && defined(XATTR_ADD_OPT)
	int options = XATTR_NOFOLLOW;
	return removexattr(path, name, options);
#elif defined(HAVE_LREMOVEEA)
	return lremoveea(path, name);
#elif defined(HAVE_EXTATTR_DELETE_LINK)
	return extattr_delete_link(path, EXTATTR_NAMESPACE_USER, uname);
#elif defined(HAVE_ATTR_REMOVE)
	int flags = ATTR_DONTFOLLOW;
	char *attrname = strchr(name,'.') + 1;
	
	if (strncmp(name, "system", 6) == 0) flags |= ATTR_ROOT;

	return attr_remove(path, attrname, flags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int attrdirfd = solaris_attropen(path, ".", O_RDONLY | O_NOFOLLOW, 0);
	if (attrdirfd >= 0) {
		ret = solaris_unlinkat(attrdirfd, name);
		close(attrdirfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_setxattr (const char *path, const char *uname, const void *value, size_t size, int flags)
{
	const char *name = prefix(uname);
#if defined(HAVE_SETXATTR)
#ifndef XATTR_ADD_OPT
	return setxattr(path, name, value, size, flags);
#else
	int options = 0;
	return setxattr(path, name, value, size, 0, options);
#endif
#elif defined(HAVE_SETEA)
	return setea(path, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_FILE)
	int retval = 0;
	if (flags) {
		/* Check attribute existence */
		retval = extattr_get_file(path, EXTATTR_NAMESPACE_USER, uname, NULL, 0);
		if (retval < 0) {
			/* REPLACE attribute, that doesn't exist */
			if (flags & XATTR_REPLACE && errno == ENOATTR) {
				errno = ENOATTR;
				return -1;
			}
			/* Ignore other errors */
		}
		else {
			/* CREATE attribute, that already exists */
			if (flags & XATTR_CREATE) {
				errno = EEXIST;
				return -1;
			}
		}
	}
	retval = extattr_set_file(path, EXTATTR_NAMESPACE_USER, uname, value, size);
	return (retval < 0) ? -1 : 0;
#elif defined(HAVE_ATTR_SET)
	int myflags = 0;
	char *attrname = strchr(name,'.') + 1;
	
	if (strncmp(name, "system", 6) == 0) myflags |= ATTR_ROOT;
	if (flags & XATTR_CREATE) myflags |= ATTR_CREATE;
	if (flags & XATTR_REPLACE) myflags |= ATTR_REPLACE;

	return attr_set(path, attrname, (const char *)value, size, myflags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int myflags = O_RDWR;
	int attrfd;
	if (flags & XATTR_CREATE) myflags |= O_EXCL;
	if (!(flags & XATTR_REPLACE)) myflags |= O_CREAT;
	attrfd = solaris_attropen(path, name, myflags, (mode_t) SOLARIS_ATTRMODE);
	if (attrfd >= 0) {
		ret = solaris_write_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

int sys_fsetxattr (int filedes, const char *uname, const void *value, size_t size, int flags)
{
    const char *name = prefix(uname);

#if defined(HAVE_FSETXATTR)
#ifndef XATTR_ADD_OPT
    return fsetxattr(filedes, name, value, size, flags);
#else
    int options = 0;
    return fsetxattr(filedes, name, value, size, 0, options);
#endif
#elif defined(HAVE_FSETEA)
    return fsetea(filedes, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_FD)
    char *s;
    int retval = 0;
    int attrnamespace = (strncmp(name, "system", 6) == 0) ? 
        EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
    const char *attrname = ((s=strchr(name, '.')) == NULL) ? name : s + 1;
    if (flags) {
        /* Check attribute existence */
        retval = extattr_get_fd(filedes, attrnamespace, attrname, NULL, 0);
        if (retval < 0) {
            /* REPLACE attribute, that doesn't exist */
            if (flags & XATTR_REPLACE && errno == ENOATTR) {
                errno = ENOATTR;
                return -1;
            }
            /* Ignore other errors */
        }
        else {
            if (flags & XATTR_CREATE) {
                errno = EEXIST;
                return -1;
            }
        }
    }
    retval = extattr_set_fd(filedes, attrnamespace, attrname, value, size);
    return (retval < 0) ? -1 : 0;
#elif defined(HAVE_ATTR_SETF)
    int myflags = 0;
    char *attrname = strchr(name,'.') + 1;

    if (strncmp(name, "system", 6) == 0) myflags |= ATTR_ROOT;
    if (flags & XATTR_CREATE) myflags |= ATTR_CREATE;
    if (flags & XATTR_REPLACE) myflags |= ATTR_REPLACE;

    return attr_setf(filedes, attrname, (const char *)value, size, myflags);
#elif defined(HAVE_ATTROPEN)
    int ret = -1;
    int myflags = O_RDWR | O_XATTR;
    int attrfd;
    if (flags & XATTR_CREATE) myflags |= O_EXCL;
    if (!(flags & XATTR_REPLACE)) myflags |= O_CREAT;
    attrfd = solaris_openat(filedes, name, myflags, (mode_t) SOLARIS_ATTRMODE);
    if (attrfd >= 0) {
        ret = solaris_write_xattr(attrfd, value, size);
        close(attrfd);
    }
    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

int sys_lsetxattr (const char *path, const char *uname, const void *value, size_t size, int flags)
{
	const char *name = prefix(uname);
#if defined(HAVE_LSETXATTR)
	return lsetxattr(path, name, value, size, flags);
#elif defined(HAVE_SETXATTR) && defined(XATTR_ADD_OPT)
	int options = XATTR_NOFOLLOW;
	return setxattr(path, name, value, size, 0, options);
#elif defined(LSETEA)
	return lsetea(path, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_LINK)
	int retval = 0;
	if (flags) {
		/* Check attribute existence */
		retval = extattr_get_link(path, EXTATTR_NAMESPACE_USER, uname, NULL, 0);
		if (retval < 0) {
			/* REPLACE attribute, that doesn't exist */
			if (flags & XATTR_REPLACE && errno == ENOATTR) {
				errno = ENOATTR;
				return -1;
			}
			/* Ignore other errors */
		}
		else {
			/* CREATE attribute, that already exists */
			if (flags & XATTR_CREATE) {
				errno = EEXIST;
				return -1;
			}
		}
	}

	retval = extattr_set_link(path, EXTATTR_NAMESPACE_USER, uname, value, size);
	return (retval < 0) ? -1 : 0;
#elif defined(HAVE_ATTR_SET)
	int myflags = ATTR_DONTFOLLOW;
	char *attrname = strchr(name,'.') + 1;
	
	if (strncmp(name, "system", 6) == 0) myflags |= ATTR_ROOT;
	if (flags & XATTR_CREATE) myflags |= ATTR_CREATE;
	if (flags & XATTR_REPLACE) myflags |= ATTR_REPLACE;

	return attr_set(path, attrname, (const char *)value, size, myflags);
#elif defined(HAVE_ATTROPEN)
	int ret = -1;
	int myflags = O_RDWR | O_NOFOLLOW;
	int attrfd;
	if (flags & XATTR_CREATE) myflags |= O_EXCL;
	if (!(flags & XATTR_REPLACE)) myflags |= O_CREAT;
	attrfd = solaris_attropen(path, name, myflags, (mode_t) SOLARIS_ATTRMODE);
	if (attrfd >= 0) {
		ret = solaris_write_xattr(attrfd, value, size);
		close(attrfd);
	}
	return ret;
#else
	errno = ENOSYS;
	return -1;
#endif
}

/**************************************************************************
 helper functions for Solaris' EA support
****************************************************************************/
#ifdef HAVE_ATTROPEN
static ssize_t solaris_read_xattr(int attrfd, void *value, size_t size)
{
	struct stat sbuf;

	if (fstat(attrfd, &sbuf) == -1) {
		return -1;
	}

	/* This is to return the current size of the named extended attribute */
	if (size == 0) {
		return sbuf.st_size;
	}

	/* check size and read xattr */
	if (sbuf.st_size > size) {
		return -1;
	}

	return read(attrfd, value, sbuf.st_size);
}

static ssize_t solaris_list_xattr(int attrdirfd, char *list, size_t size)
{
	ssize_t len = 0;
	DIR *dirp;
	struct dirent *de;
	int newfd = dup(attrdirfd);
	/* CAUTION: The originating file descriptor should not be
	            used again following the call to fdopendir().
	            For that reason we dup() the file descriptor
		    here to make things more clear. */
	dirp = fdopendir(newfd);

	while ((de = readdir(dirp))) {
		size_t listlen;
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..") ||
		     !strcmp(de->d_name, "SUNWattr_ro") || !strcmp(de->d_name, "SUNWattr_rw")) 
		{
			/* we don't want "." and ".." here: */
			LOG(log_maxdebug, logtype_default, "skipped EA %s\n",de->d_name);
			continue;
		}

		listlen = strlen(de->d_name);
		if (size == 0) {
			/* return the current size of the list of extended attribute names*/
			len += listlen + 1;
		} else {
			/* check size and copy entry + nul into list. */
			if ((len + listlen + 1) > size) {
				errno = ERANGE;
				len = -1;
				break;
			} else {
				strcpy(list + len, de->d_name);
				len += listlen;
				list[len] = '\0';
				++len;
			}
		}
	}

	if (closedir(dirp) == -1) {
		LOG(log_error, logtype_default, "closedir dirp: %s",strerror(errno));
		return -1;
	}
	return len;
}

static int solaris_unlinkat(int attrdirfd, const char *name)
{
	if (unlinkat(attrdirfd, name, 0) == -1) {
		return -1;
	}
	return 0;
}

static int solaris_attropen(const char *path, const char *attrpath, int oflag, mode_t mode)
{
    EC_INIT;
	int filedes = -1, eafd = -1;

    if ((filedes = open(path, O_RDONLY | (oflag & O_NOFOLLOW), mode)) == -1) {
        switch (errno) {
        case ENOENT:
        case EEXIST:
        case OPEN_NOFOLLOW_ERRNO:
            EC_FAIL;
        default:
            LOG(log_debug, logtype_default, "open(\"%s\"): %s", fullpathname(path), strerror(errno));
            EC_FAIL;
        }
	}

	if ((eafd = openat(filedes, attrpath, oflag | O_XATTR, mode)) == -1) {
        switch (errno) {
        case ENOENT:
        case EEXIST:
            EC_FAIL;
        default:
            LOG(log_debug, logtype_default, "openat(\"%s\"): %s", fullpathname(path), strerror(errno));
            EC_FAIL;
        }
	}
    
EC_CLEANUP:
    if (filedes != -1)
        close(filedes);
    if (ret != 0) {
        if (eafd != -1)
            close(eafd);
        eafd = -1;
    }
    return eafd;
}

static int solaris_openat(int fildes, const char *path, int oflag, mode_t mode)
{
	int filedes;

	if ((filedes = openat(fildes, path, oflag, mode)) == -1) {
        switch (errno) {
        case ENOENT:
        case EEXIST:
        case EACCES:
            break;
        default:
            LOG(log_debug, logtype_default, "openat(\"%s\"): %s",
                path, strerror(errno));
            errno = ENOATTR;
            break;
        }
	}
	return filedes;
}

static int solaris_write_xattr(int attrfd, const char *value, size_t size)
{
	if ((ftruncate(attrfd, 0) == 0) && (write(attrfd, value, size) == size)) {
		return 0;
	} else {
		LOG(log_error, logtype_default, "solaris_write_xattr: %s",
            strerror(errno));
		return -1;
	}
}

#endif /*HAVE_ATTROPEN*/

