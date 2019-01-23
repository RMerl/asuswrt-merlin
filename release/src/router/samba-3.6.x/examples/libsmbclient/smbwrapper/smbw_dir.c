/* 
   Unix SMB/Netbios implementation.
   Version 2.0
   SMB wrapper directory functions
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Derrell Lipman 2003-2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "smbw.h"
#include "bsd-strlfunc.h"

/***************************************************** 
determine if a directory handle is a smb one
*******************************************************/
int smbw_dirp(DIR * dirp)
{
        return ((char *) dirp >= (char *) smbw_fd_map &&
                (char *) dirp < (char *) &smbw_fd_map[__FD_SETSIZE] &&
                *(int *) dirp != -1);
}


/***************************************************** 
a wrapper for getdents()
*******************************************************/
int smbw_getdents(unsigned int fd_smbw,
                  struct SMBW_dirent *dirent_external,
                  int count)
{
        int remaining;
        int fd_client = smbw_fd_map[fd_smbw];
        struct smbc_dirent *dirent_internal;


        for (remaining = count;
             remaining > sizeof(struct SMBW_dirent);
             dirent_external++) {

                /*
                 * We do these one at a time because there's otherwise no way
                 * to limit how many smbc_getdents() will return for us, and
                 * if it returns too many, it also doesn't give us offsets to
                 * be able to seek back to where we need to be.  In practice,
                 * this one-at-a-time retrieval isn't a problem because the
                 * time-consuming network transaction is all done at
                 * smbc_opendir() time.
                 */
                dirent_internal = smbc_readdir(fd_client);
                if (dirent_internal == NULL) {
                        break;
                }

                remaining -= sizeof(struct SMBW_dirent);

                dirent_external->d_ino = -1; /* not supported */
		dirent_external->d_off = smbc_telldir(fd_client);
		dirent_external->d_reclen = sizeof(struct SMBW_dirent);
                dirent_external->d_type = dirent_internal->smbc_type;

                smbw_strlcpy(dirent_external->d_name,
                             dirent_internal->name,
                             sizeof(dirent_external->d_name) - 1);
                smbw_strlcpy(dirent_external->d_comment,
                             dirent_internal->comment,
                             sizeof(dirent_external->d_comment) - 1);
	}

	return(count - remaining);
}


/***************************************************** 
a wrapper for chdir()
*******************************************************/
int smbw_chdir(const char *name)
{
        int simulate;
        struct stat statbuf;
        char path[PATH_MAX];
        char *p;

	SMBW_INIT();

	if (!name) {
		errno = EINVAL;
                return -1;
	}

	if (! smbw_path((char *) name)) {
		if ((* smbw_libc.chdir)(name) == 0) {
                        *smbw_cwd = '\0';
                        return 0;
		}

                return -1;
	}

        smbw_fix_path(name, path);

        /* ensure it exists */
        p = path + 6;           /* look just past smb:// */
        simulate = (strchr(p, '/') == NULL);

        /* special case for full-network scan, workgroups, and servers */
        if (! simulate) {
            
            if (smbc_stat(path, &statbuf) < 0) {
                return -1;
            }
            
            /* ensure it's a directory */
            if (! S_ISDIR(statbuf.st_mode)) {
                errno = ENOTDIR;
                return -1;
            }
        }

        smbw_strlcpy(smbw_cwd, path, PATH_MAX);

	/* we don't want the old directory to be busy */
	(* smbw_libc.chdir)("/");

	return 0;
}


/***************************************************** 
a wrapper for mkdir()
*******************************************************/
int smbw_mkdir(const char *fname, mode_t mode)
{
        char path[PATH_MAX];

	if (!fname) {
		errno = EINVAL;
		return -1;
	}

	SMBW_INIT();

        smbw_fix_path(fname, path);
        return smbc_mkdir(path, mode);
}

/***************************************************** 
a wrapper for rmdir()
*******************************************************/
int smbw_rmdir(const char *fname)
{
        char path[PATH_MAX];

	if (!fname) {
		errno = EINVAL;
		return -1;
	}

	SMBW_INIT();

        smbw_fix_path(fname, path);
        return smbc_rmdir(path);
}


/***************************************************** 
a wrapper for getcwd()
*******************************************************/
char *smbw_getcwd(char *buf, size_t size)
{
	SMBW_INIT();

        if (*smbw_cwd == '\0') {
                return (* smbw_libc.getcwd)(buf, size);
        }

        if (buf == NULL) {
                if (size == 0) {
                        size = strlen(smbw_cwd) + 1;
                }
                buf = malloc(size);
                if (buf == NULL) {
                        errno = ENOMEM;
                        return NULL;
                }
        }

        smbw_strlcpy(buf, smbw_cwd, size);
        buf[size-1] = '\0';
	return buf;
}

/***************************************************** 
a wrapper for fchdir()
*******************************************************/
int smbw_fchdir(int fd_smbw)
{
        int ret;

        SMBW_INIT();

        if (! smbw_fd(fd_smbw)) {
                ret = (* smbw_libc.fchdir)(fd_smbw);
                (void) (* smbw_libc.getcwd)(smbw_cwd, PATH_MAX);
                return ret;
        }

        errno = EACCES;
        return -1;
}

/***************************************************** 
open a directory on the server
*******************************************************/
DIR *smbw_opendir(const char *fname)
{
        int fd_client;
        int fd_smbw;
	char path[PATH_MAX];
        DIR * dirp;

	SMBW_INIT();

	if (!fname) {
		errno = EINVAL;
		return NULL;
	}

	fd_smbw = (smbw_libc.open)(SMBW_DUMMY, O_WRONLY, 0200);
	if (fd_smbw == -1) {
		errno = EMFILE;
                return NULL;
	}

        smbw_fix_path(fname, path);
        fd_client =  smbc_opendir(path);

        if (fd_client < 0) {
                (* smbw_libc.close)(fd_smbw);
                return NULL;
        }

        smbw_fd_map[fd_smbw] = fd_client;
        smbw_ref(fd_client, SMBW_RCT_Increment);
        dirp = (DIR *) &smbw_fd_map[fd_smbw];
        return dirp;
}

/***************************************************** 
read one entry from a directory
*******************************************************/
struct SMBW_dirent *smbw_readdir(DIR *dirp)
{
        int fd_smbw;
        int fd_client;
        struct smbc_dirent *dirent_internal;
        static struct SMBW_dirent dirent_external;

        fd_smbw = (int *) dirp - smbw_fd_map;
        fd_client = smbw_fd_map[fd_smbw];

	if ((dirent_internal = smbc_readdir(fd_client)) == NULL) {
		return NULL;
        }
 
        dirent_external.d_ino = -1; /* not supported */
        dirent_external.d_off = smbc_telldir(fd_client);
        dirent_external.d_reclen = sizeof(struct SMBW_dirent);
        dirent_external.d_type = dirent_internal->smbc_type;
        smbw_strlcpy(dirent_external.d_name,
                     dirent_internal->name,
                     sizeof(dirent_external.d_name) - 1);
        smbw_strlcpy(dirent_external.d_comment,
                     dirent_internal->comment,
                     sizeof(dirent_external.d_comment) - 1);

	return &dirent_external;
}

/***************************************************** 
read one entry from a directory in a reentrant fashion
ha!  samba is not re-entrant, and neither is the
libsmbclient library
*******************************************************/
int smbw_readdir_r(DIR *dirp,
                   struct SMBW_dirent *__restrict entry,
                   struct SMBW_dirent **__restrict result)
{
        SMBW_dirent *dirent;

        dirent = smbw_readdir(dirp);

        if (dirent != NULL) {
                *entry = *dirent;
                if (result != NULL) {
                        *result = entry;
                }
                return 0;
        }

        if (result != NULL) {
                *result = NULL;
        }
	return EBADF;
}


/***************************************************** 
close a DIR*
*******************************************************/
int smbw_closedir(DIR *dirp)
{
        int fd_smbw = (int *) dirp - smbw_fd_map;
        int fd_client = smbw_fd_map[fd_smbw];

        (* smbw_libc.close)(fd_smbw);
        if (smbw_ref(fd_client, SMBW_RCT_Decrement) > 0) {
                return 0;
        }
        smbw_fd_map[fd_smbw] = -1;
        return smbc_closedir(fd_client);
}

/***************************************************** 
seek in a directory
*******************************************************/
void smbw_seekdir(DIR *dirp, long long offset)
{
        int fd_smbw = (int *) dirp - smbw_fd_map;
        int fd_client = smbw_fd_map[fd_smbw];

        smbc_lseekdir(fd_client, offset);
}

/***************************************************** 
current loc in a directory
*******************************************************/
long long smbw_telldir(DIR *dirp)
{
        int fd_smbw = (int *) dirp - smbw_fd_map;
        int fd_client = smbw_fd_map[fd_smbw];

        return (long long) smbc_telldir(fd_client);
}
