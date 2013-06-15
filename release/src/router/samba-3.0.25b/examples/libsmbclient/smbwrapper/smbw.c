/* 
   Unix SMB/Netbios implementation.
   Version 2.0
   SMB wrapper functions
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Derrell Lipman 2003-2005
   
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
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include "smbw.h"
#include "bsd-strlfunc.h"

typedef enum StartupType
{
        StartupType_Fake,
        StartupType_Real
} StartupType;

int smbw_fd_map[__FD_SETSIZE];
int smbw_ref_count[__FD_SETSIZE];
char smbw_cwd[PATH_MAX];
char smbw_prefix[] = SMBW_PREFIX;

/* needs to be here because of dumb include files on some systems */
int creat_bits = O_WRONLY|O_CREAT|O_TRUNC;

int smbw_initialized = 0;

static int debug_level = 0;

static SMBCCTX *smbw_ctx;

extern int smbw_debug;


/*****************************************************
smbw_ref -- manipulate reference counts
******************************************************/
int smbw_ref(int client_fd, Ref_Count_Type type, ...)
{
        va_list ap;

        /* client id values begin at SMBC_BASE_FC. */
        client_fd -= SMBC_BASE_FD;

        va_start(ap, type);
        switch(type)
        {
        case SMBW_RCT_Increment:
                return ++smbw_ref_count[client_fd];

        case SMBW_RCT_Decrement:
                return --smbw_ref_count[client_fd];

        case SMBW_RCT_Get:
                return smbw_ref_count[client_fd];

        case SMBW_RCT_Set:
                return (smbw_ref_count[client_fd] = va_arg(ap, int));
        }
        va_end(ap);

        /* never gets here */
        return -1;
}


/*
 * Return a username and password given a server and share name
 *
 * Returns 0 upon success;
 * non-zero otherwise, and errno is set to indicate the error.
 */
static void get_envvar_auth_data(const char *srv, 
                                 const char *shr,
                                 char *wg, int wglen, 
                                 char *un, int unlen,
                                 char *pw, int pwlen)
{
        char *u;
        char *p;
        char *w;

	/* Fall back to environment variables */

	w = getenv("WORKGROUP");
	if (w == NULL) w = "";

	u = getenv("USER");
	if (u == NULL) u = "";

	p = getenv("PASSWORD");
	if (p == NULL) p = "";

        smbw_strlcpy(wg, w, wglen);
        smbw_strlcpy(un, u, unlen);
        smbw_strlcpy(pw, p, pwlen);
}

static smbc_get_auth_data_fn get_auth_data_fn = get_envvar_auth_data;

/*****************************************************
set the get auth data function
******************************************************/
void smbw_set_auth_data_fn(smbc_get_auth_data_fn fn)
{
	get_auth_data_fn = fn;
}


/*****************************************************
ensure that all connections are terminated upon exit
******************************************************/
static void do_shutdown(void)
{
        if (smbw_ctx != NULL) {
                smbc_free_context(smbw_ctx, 1);
        }
}


/***************************************************** 
initialise structures
*******************************************************/
static void do_init(StartupType startupType)
{
        int i;
        char *p;

	smbw_initialized = 1;   /* this must be first to avoid recursion! */

        smbw_ctx = NULL;        /* don't free context until it's established */

        /* initially, no file descriptors are mapped */
        for (i = 0; i < __FD_SETSIZE; i++) {
                smbw_fd_map[i] = -1;
                smbw_ref_count[i] = 0;
        }

        /* See if we've been told to start in a particular directory */
        if ((p=getenv("SMBW_DIR")) != NULL) {
                smbw_strlcpy(smbw_cwd, p, PATH_MAX);

                /* we don't want the old directory to be busy */
                (* smbw_libc.chdir)("/");
                
        } else {
                *smbw_cwd = '\0';
        }

	if ((p=getenv("DEBUG"))) {
		debug_level = atoi(p);
	}

        if ((smbw_ctx = smbc_new_context()) == NULL) {
                fprintf(stderr, "Could not create a context.\n");
                exit(1);
        }
        
        smbw_ctx->debug = debug_level;
        smbw_ctx->callbacks.auth_fn = get_auth_data_fn;
        smbw_ctx->options.browse_max_lmb_count = 0;
        smbw_ctx->options.urlencode_readdir_entries = 1;
        smbw_ctx->options.one_share_per_server = 1;
        
        if (smbc_init_context(smbw_ctx) == NULL) {
                fprintf(stderr, "Could not initialize context.\n");
                exit(1);
        }
                
        smbc_set_context(smbw_ctx);

        /* if not real startup, exit handler has already been established */
        if (startupType == StartupType_Real) {
            atexit(do_shutdown);
        }
}

/***************************************************** 
initialise structures, real start up vs a fork()
*******************************************************/
void smbw_init(void)
{
    do_init(StartupType_Real);
}


/***************************************************** 
determine if a file descriptor is a smb one
*******************************************************/
int smbw_fd(int smbw_fd)
{
        SMBW_INIT();

        return (smbw_fd >= 0 &&
                smbw_fd < __FD_SETSIZE &&
                smbw_fd_map[smbw_fd] >= SMBC_BASE_FD); /* minimum smbc_ fd */
}


/***************************************************** 
determine if a path is a smb one
*******************************************************/
int smbw_path(const char *name)
{
        int len;
        int ret;
        int saved_errno;

        saved_errno = errno;

        SMBW_INIT();

        len = strlen(smbw_prefix);

        ret = ((strncmp(name, smbw_prefix, len) == 0 &&
                (name[len] == '\0' || name[len] == '/')) ||
               (*name != '/' && *smbw_cwd != '\0'));

        errno = saved_errno;
        return ret;
}


/***************************************************** 
remove redundent stuff from a filename
*******************************************************/
void smbw_clean_fname(char *name)
{
	char *p, *p2;
	int l;
	int modified = 1;

	if (!name) return;

        DEBUG(10, ("Clean [%s]...\n", name));

	while (modified) {
		modified = 0;

		if ((p=strstr(name,"/./"))) {
			modified = 1;
			while (*p) {
				p[0] = p[2];
				p++;
			}
                        DEBUG(10, ("\tclean 1 (/./) produced [%s]\n", name));
		}

		if ((p=strstr(name,"//"))) {
			modified = 1;
			while (*p) {
				p[0] = p[1];
				p++;
			}
                        DEBUG(10, ("\tclean 2 (//) produced [%s]\n", name));
		}

		if (strcmp(name,"/../")==0) {
			modified = 1;
			name[1] = 0;
                        DEBUG(10,("\tclean 3 (^/../$) produced [%s]\n", name));
		}

		if ((p=strstr(name,"/../"))) {
			modified = 1;
			for (p2 = (p > name ? p-1 : p); p2 > name; p2--) {
				if (p2[0] == '/') break;
			}
                        if (p2 > name) p2++;
			while (*p2) {
				p2[0] = p[3];
				p2++;
                                p++;
			}
                        DEBUG(10, ("\tclean 4 (/../) produced [%s]\n", name));
		}

		if (strcmp(name,"/..")==0) {
			modified = 1;
			name[1] = 0;
                        DEBUG(10, ("\tclean 5 (^/..$) produced [%s]\n", name));
		}

		l = strlen(name);
		p = l>=3?(name+l-3):name;
		if (strcmp(p,"/..")==0) {
			modified = 1;
			for (p2=p-1;p2>name;p2--) {
				if (p2[0] == '/') break;
			}
			if (p2==name) {
				p[0] = '/';
				p[1] = 0;
			} else {
				p2[0] = 0;
			}
                        DEBUG(10, ("\tclean 6 (/..) produced [%s]\n", name));
		}

		l = strlen(name);
		p = l>=2?(name+l-2):name;
		if (strcmp(p,"/.")==0) {
                        modified = 1;
			if (p == name) {
				p[1] = 0;
			} else {
				p[0] = 0;
			}
                        DEBUG(10, ("\tclean 7 (/.) produced [%s]\n", name));
		}

		if (strncmp(p=name,"./",2) == 0) {      
			modified = 1;
			do {
				p[0] = p[2];
			} while (*p++);
                        DEBUG(10, ("\tclean 8 (^./) produced [%s]\n", name));
		}

		l = strlen(p=name);
		if (l > 1 && p[l-1] == '/') {
			modified = 1;
			p[l-1] = 0;
                        DEBUG(10, ("\tclean 9 (/) produced [%s]\n", name));
		}
	}
}

void smbw_fix_path(const char *src, char *dest)
{
        const char *p;
        int len = strlen(smbw_prefix);

        if (*src == '/') {
                for (p = src + len; *p == '/'; p++)
                        ;
                snprintf(dest, PATH_MAX, "smb://%s", p);
        } else {
                snprintf(dest, PATH_MAX, "%s/%s", smbw_cwd, src);
        }

        smbw_clean_fname(dest + 5);

        DEBUG(10, ("smbw_fix_path(%s) returning [%s]\n", src, dest));
}



/***************************************************** 
a wrapper for open()
*******************************************************/
int smbw_open(const char *fname, int flags, mode_t mode)
{
        int client_fd;
        int smbw_fd;
	char path[PATH_MAX];

	SMBW_INIT();

	if (!fname) {
		errno = EINVAL;
		return -1;
	}

	smbw_fd = (smbw_libc.open)(SMBW_DUMMY, O_WRONLY, 0200);
	if (smbw_fd == -1) {
		errno = EMFILE;
                return -1;
	}

        smbw_fix_path(fname, path);
        if (flags == creat_bits) {
                client_fd =  smbc_creat(path, mode);
        } else {
                client_fd = smbc_open(path, flags, mode);
        }

        if (client_fd < 0) {
                (* smbw_libc.close)(smbw_fd);
                return -1;
        }

        smbw_fd_map[smbw_fd] = client_fd;
        smbw_ref(client_fd, SMBW_RCT_Increment);
        return smbw_fd;
}


/***************************************************** 
a wrapper for pread()

there should really be an smbc_pread() to avoid the two
lseek()s required in this kludge.
*******************************************************/
ssize_t smbw_pread(int smbw_fd, void *buf, size_t count, SMBW_OFF_T ofs)
{
        int client_fd;
	ssize_t ret;
        int saved_errno;
        SMBW_OFF_T old_ofs;
        
        if (count == 0) {
                return 0;
        }

        client_fd = smbw_fd_map[smbw_fd];

        if ((old_ofs = smbc_lseek(client_fd, 0, SEEK_CUR)) < 0 ||
            smbc_lseek(client_fd, ofs, SEEK_SET) < 0) {
                return -1;
        }

        if ((ret = smbc_read(client_fd, buf, count)) < 0) {
                saved_errno = errno;
                (void) smbc_lseek(client_fd, old_ofs, SEEK_SET);
                errno = saved_errno;
                return -1;
        }

        return ret;
}

/***************************************************** 
a wrapper for read()
*******************************************************/
ssize_t smbw_read(int smbw_fd, void *buf, size_t count)
{
        int client_fd;
        
        client_fd = smbw_fd_map[smbw_fd];

        return smbc_read(client_fd, buf, count);
}

	

/***************************************************** 
a wrapper for write()
*******************************************************/
ssize_t smbw_write(int smbw_fd, void *buf, size_t count)
{
        int client_fd;

        client_fd = smbw_fd_map[smbw_fd];

        return smbc_write(client_fd, buf, count);
}

/***************************************************** 
a wrapper for pwrite()
*******************************************************/
ssize_t smbw_pwrite(int smbw_fd, void *buf, size_t count, SMBW_OFF_T ofs)
{
        int saved_errno;
        int client_fd;
	ssize_t ret;
        SMBW_OFF_T old_ofs;
        
        if (count == 0) {
                return 0;
        }

        client_fd = smbw_fd_map[smbw_fd];

        if ((old_ofs = smbc_lseek(client_fd, 0, SEEK_CUR)) < 0 ||
            smbc_lseek(client_fd, ofs, SEEK_SET) < 0) {
                return -1;
        }

        if ((ret = smbc_write(client_fd, buf, count)) < 0) {
                saved_errno = errno;
                (void) smbc_lseek(client_fd, old_ofs, SEEK_SET);
                errno = saved_errno;
                return -1;
        }

        return ret;
}

/***************************************************** 
a wrapper for close()
*******************************************************/
int smbw_close(int smbw_fd)
{
        int client_fd;

        client_fd = smbw_fd_map[smbw_fd];

        if (smbw_ref(client_fd, SMBW_RCT_Decrement) > 0) {
                return 0;
        }
        
        (* smbw_libc.close)(smbw_fd);
        smbw_fd_map[smbw_fd] = -1;
        return smbc_close(client_fd);
}


/***************************************************** 
a wrapper for fcntl()
*******************************************************/
int smbw_fcntl(int smbw_fd, int cmd, long arg)
{
	return 0;
}


/***************************************************** 
a wrapper for access()
*******************************************************/
int smbw_access(const char *name, int mode)
{
	struct SMBW_stat st;

        SMBW_INIT();

	if (smbw_stat(name, &st)) return -1;

	if (((mode & R_OK) && !(st.s_mode & S_IRUSR)) ||
	    ((mode & W_OK) && !(st.s_mode & S_IWUSR)) ||
	    ((mode & X_OK) && !(st.s_mode & S_IXUSR))) {
		errno = EACCES;
		return -1;
	}
	
	return 0;
}

/***************************************************** 
a wrapper for readlink() - needed for correct errno setting
*******************************************************/
int smbw_readlink(const char *fname, char *buf, size_t bufsize)
{
	struct SMBW_stat st;
	int ret;

        SMBW_INIT();

	ret = smbw_stat(fname, &st);
	if (ret != 0) {
		return -1;
	}
	
	/* it exists - say it isn't a link */
	errno = EINVAL;
	return -1;
}


/***************************************************** 
a wrapper for unlink()
*******************************************************/
int smbw_unlink(const char *fname)
{
        char path[PATH_MAX];
        
        SMBW_INIT();

        smbw_fix_path(fname, path);
        return smbc_unlink(path);
}


/***************************************************** 
a wrapper for rename()
*******************************************************/
int smbw_rename(const char *oldname, const char *newname)
{
        char path_old[PATH_MAX];
        char path_new[PATH_MAX];

        SMBW_INIT();

        smbw_fix_path(oldname, path_old);
        smbw_fix_path(newname, path_new);
        return smbc_rename(path_old, path_new);
}


/***************************************************** 
a wrapper for utimes
*******************************************************/
int smbw_utimes(const char *fname, void *buf)
{
        char path[PATH_MAX];

        smbw_fix_path(fname, path);
	return smbc_utimes(path, buf);
}


/***************************************************** 
a wrapper for utime 
*******************************************************/
int smbw_utime(const char *fname, void *buf)
{
        char path[PATH_MAX];

        smbw_fix_path(fname, path);
        return smbc_utime(path, buf);
}

/***************************************************** 
a wrapper for chown()
*******************************************************/
int smbw_chown(const char *fname, uid_t owner, gid_t group)
{
        /* always indiciate that this is not supported. */
        errno = ENOTSUP;
        return -1;
}

/***************************************************** 
a wrapper for chmod()
*******************************************************/
int smbw_chmod(const char *fname, mode_t newmode)
{
        char path[PATH_MAX];

        smbw_fix_path(fname, path);
        return smbc_chmod(path, newmode);
}


/***************************************************** 
a wrapper for lseek()
*******************************************************/
SMBW_OFF_T smbw_lseek(int smbw_fd,
                      SMBW_OFF_T offset,
                      int whence)
{
        int client_fd;
        SMBW_OFF_T ret;
        
        client_fd = smbw_fd_map[smbw_fd];

        ret = smbc_lseek(client_fd, offset, whence);
        if (smbw_debug)
        {
                printf("smbw_lseek(%d/%d, 0x%llx) returned 0x%llx\n",
                       smbw_fd, client_fd,
                       (unsigned long long) offset,
                       (unsigned long long) ret);
        }
        return ret;
}

/***************************************************** 
a wrapper for dup()
*******************************************************/
int smbw_dup(int smbw_fd)
{
	int fd2;

	fd2 = (smbw_libc.dup)(smbw_fd);
	if (fd2 == -1) {
                return -1;
	}

        smbw_fd_map[fd2] = smbw_fd_map[smbw_fd];
        smbw_ref(smbw_fd_map[smbw_fd], SMBW_RCT_Increment);
	return fd2;
}


/***************************************************** 
a wrapper for dup2()
*******************************************************/
int smbw_dup2(int smbw_fd, int fd2)
{
	if ((* smbw_libc.dup2)(smbw_fd, fd2) != fd2) {
                return -1;
	}

        smbw_fd_map[fd2] = smbw_fd_map[smbw_fd];
        smbw_ref(smbw_fd_map[smbw_fd], SMBW_RCT_Increment);
	return fd2;
}


/***************************************************** 
when we fork we have to close all connections and files
in the child
*******************************************************/
int smbw_fork(void)
{
        int i;
	pid_t child_pid;
	int p[2];
	char c = 0;

        SMBW_INIT();

	if (pipe(p)) return (* smbw_libc.fork)();

	child_pid = (* smbw_libc.fork)();

	if (child_pid) {
		/* block the parent for a moment until the sockets are
                   closed */
		(* smbw_libc.close)(p[1]);
		(* smbw_libc.read)(p[0], &c, 1);
		(* smbw_libc.close)(p[0]);
		return child_pid;
        }

	(* smbw_libc.close)(p[0]);

        /* close all server connections and locally-opened files */
        for (i = 0; i < __FD_SETSIZE; i++) {
                if (smbw_fd_map[i] > 0 &&
                    smbw_ref(smbw_fd_map[i], SMBW_RCT_Get) > 0) {
                        
                        smbc_close(smbw_fd_map[i]);
                        smbw_ref(smbw_fd_map[i], SMBW_RCT_Set, 0);
                        (* smbw_libc.close)(i);
                }

                smbw_fd_map[i] = -1;
        }

	/* unblock the parent */
	write(p[1], &c, 1);
	(* smbw_libc.close)(p[1]);

        /* specify directory to start in, if it's simulated smb */
        if (*smbw_cwd != '\0') {
                setenv("SMBW_DIR", smbw_cwd, 1);
        } else {
                unsetenv("SMBW_DIR");
        }

        /* Re-initialize this library for the child */
        do_init(StartupType_Fake);

	/* and continue in the child */
	return 0;
}

int smbw_setxattr(const char *fname,
                  const char *name,
                  const void *value,
                  size_t size,
                  int flags)
{
        char path[PATH_MAX];

        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        smbw_fix_path(fname, path);
        return smbc_setxattr(path, name, value, size, flags);
}

int smbw_lsetxattr(const char *fname,
                   const char *name,
                   const void *value,
                   size_t size,
                   int flags)
{
        char path[PATH_MAX];

        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        smbw_fix_path(fname, path);
        return smbc_lsetxattr(path, name, value, size, flags);
}

int smbw_fsetxattr(int smbw_fd,
                   const char *name,
                   const void *value,
                   size_t size,
                   int flags)
{
        int client_fd;
        
        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        client_fd = smbw_fd_map[smbw_fd];
        return smbc_fsetxattr(client_fd, name, value, size, flags);
}

int smbw_getxattr(const char *fname,
                  const char *name,
                  const void *value,
                  size_t size)
{
        char path[PATH_MAX];

        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        smbw_fix_path(fname, path);

        return smbc_getxattr(path, name, value, size);
}

int smbw_lgetxattr(const char *fname,
                   const char *name,
                   const void *value,
                   size_t size)
{
        char path[PATH_MAX];

        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        smbw_fix_path(fname, path);
        return smbc_lgetxattr(path, name, value, size);
}

int smbw_fgetxattr(int smbw_fd,
                   const char *name,
                   const void *value,
                   size_t size)
{
        int client_fd;
        
        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        client_fd = smbw_fd_map[smbw_fd];
        return smbc_fgetxattr(client_fd, name, value, size);
}

int smbw_removexattr(const char *fname,
                     const char *name)
{
        char path[PATH_MAX];

        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        smbw_fix_path(fname, path);
        return smbc_removexattr(path, name);
}

int smbw_lremovexattr(const char *fname,
                      const char *name)
{
        char path[PATH_MAX];

        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        smbw_fix_path(fname, path);
        return smbc_lremovexattr(path, name);
}

int smbw_fremovexattr(int smbw_fd,
                      const char *name)
{
        int client_fd;
        
        if (strcmp(name, "system.posix_acl_access") == 0)
        {
                name = "system.*";
        }

        client_fd = smbw_fd_map[smbw_fd];
        return smbc_fremovexattr(client_fd, name);
}

int smbw_listxattr(const char *fname,
                   char *list,
                   size_t size)
{
        char path[PATH_MAX];

        smbw_fix_path(fname, path);
        return smbc_listxattr(path, list, size);
}

int smbw_llistxattr(const char *fname,
                    char *list,
                    size_t size)
{
        char path[PATH_MAX];

        smbw_fix_path(fname, path);
        return smbc_llistxattr(path, list, size);
}

int smbw_flistxattr(int smbw_fd,
                    char *list,
                    size_t size)
{
        int client_fd;
        
        client_fd = smbw_fd_map[smbw_fd];
        return smbc_flistxattr(client_fd, list, size);
}
