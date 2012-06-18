/*
 *  smbumount.c
 *
 *  Copyright (C) 1995-1998 by Volker Lendecke
 *
 */

#include "includes.h"

#include <mntent.h>

#include <asm/types.h>
#include <asm/posix_types.h>
#include <linux/smb.h>
#include <linux/smb_mount.h>
#include <linux/smb_fs.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

/* This is a (hopefully) temporary hack due to the fact that
	sizeof( uid_t ) != sizeof( __kernel_uid_t ) under glibc.
	This may change in the future and smb.h may get fixed in the
	future.  In the mean time, it's ugly hack time - get over it.
*/
#undef SMB_IOC_GETMOUNTUID
#define	SMB_IOC_GETMOUNTUID		_IOR('u', 1, __kernel_uid_t)

#ifndef O_NOFOLLOW
#define O_NOFOLLOW     0400000
#endif

static void
usage(void)
{
        printf("usage: smbumount mountpoint\n");
	printf("Version: %s\n", VERSION);
}

static int
umount_ok(const char *mount_point)
{
	/* we set O_NOFOLLOW to prevent users playing games with symlinks to
	   umount filesystems they don't own */
        int fid = open(mount_point, O_RDONLY|O_NOFOLLOW, 0);
        __kernel_uid_t mount_uid;
	
        if (fid == -1) {
                /* fprintf(stderr, "Could not open %s: %s\n",
                        mount_point, strerror(errno)); */
                return 1;	/* maybe try again */
        }
        
        if (ioctl(fid, SMB_IOC_GETMOUNTUID, &mount_uid) != 0) {
                fprintf(stderr, "%s probably not smb-filesystem\n",
                        mount_point);
                return -1;
        }

        if ((getuid() != 0)
            && (mount_uid != getuid())) {
                fprintf(stderr, "You are not allowed to umount %s\n",
                        mount_point);
                return -1;
        }

        close(fid);
        return 0;
}

#define	MAX_READLINKS	32
/* myrealpath from mount, it could get REAL path under a broken connection */
char *myrealpath(const char *path, char *resolved_path, int maxreslth)
{
	int readlinks = 0,m,n;
	char *npath,*buf;
	char link_path[PATH_MAX + 1];

	npath = resolved_path;

	if(*path != '/')
	{
		if(!getcwd(npath, maxreslth - 2))
			return NULL;
		npath += strlen(npath);
		if(npath[-1] != '/')
			*(npath++) = '/';
		else
		{
			*npath++ = '/';
			path++;
		}
	}

	while(*path != '\0')
	{
		if(*path == '/')
		{
			path++;
			continue;
		}
		if(*path == '.' && (path[1] == '\0' || path[1] == '/'))
		{
			path++;
			continue;
		}
		if(*path == '.' && path[1] == '.' &&
				(path[2] == '\0' || path[2] == '/'))
		{
			path += 2;
			while(npath > resolved_path + 1 &&
					(--npath)[-1] != '/');
			continue;
		}
		while(*path != '\0' && *path != '/')
		{
			if(npath-resolved_path > maxreslth - 2)
				return NULL;
			*npath++ = *path++;
		}
		if(readlinks++ > MAX_READLINKS)
			return NULL;
		*npath = '\0';
		n = readlink(resolved_path, link_path, PATH_MAX);
		if(n < 0)
		{
			if(errno != EINVAL) return NULL;
		}
		else
		{
			link_path[n] = '\0';
			if(*link_path == '/')
				npath = resolved_path;
			else while(*(--npath) != '/');
			m = strlen(path);
			if((buf = malloc(m + n + 1)) == NULL)
			{
				fprintf(stderr,"Not enough memory.\n");
				return NULL;
			}
			memcpy(buf, link_path, n);
			memcpy(buf + n, path, m + 1);
			path = buf;
		}
		*npath++ = '/';
	}
	if(npath != resolved_path + 1)
	{
	    while(npath > resolved_path && npath[-1] == '/')
		npath--;
	    if(npath == resolved_path) return NULL;
	}
	*npath = '\0';
	return resolved_path;
}

/* Make a canonical pathname from PATH.  Returns a freshly malloced string.
   It is up the *caller* to ensure that the PATH is sensible.  i.e.
   canonicalize ("/dev/fd0/.") returns "/dev/fd0" even though ``/dev/fd0/.''
   is not a legal pathname for ``/dev/fd0''  Anything we cannot parse
   we return unmodified.   */
static char *
canonicalize (char *path)
{
	char *npath,*canonical = malloc (PATH_MAX + 1);
	int i;

	if (!canonical) {
		fprintf(stderr, "Error! Not enough memory!\n");
		return NULL;
	}

	if (strlen(path) > PATH_MAX) {
		fprintf(stderr, "Mount point string too long\n");
		return NULL;
	}

	if (path == NULL)
		return NULL;

/*	if (realpath (path, canonical)) */
	if(myrealpath(path, canonical, PATH_MAX))
		return canonical;

	pstrcpy (canonical, path);
	if((i = strlen(canonical)) > 1 && i <= PATH_MAX)
	{
		path = canonical + i;
		while(*(--path) == '/')
			*path = '\0';
	}
	return canonical;
}


int 
main(int argc, char *argv[])
{
        int fd;
        char* mount_point;
        struct mntent *mnt;
        FILE* mtab;
        FILE* new_mtab;

        if (argc != 2) {
                usage();
                exit(1);
        }

        if (geteuid() != 0) {
                fprintf(stderr, "smbumount must be installed suid root\n");
                exit(1);
        }

        mount_point = canonicalize(argv[1]);

	if (mount_point == NULL)
	{
		exit(1);
	}

        if ((fd = umount_ok(mount_point)) != 0) {
		if(fd == 1)
		{
			if((fd = umount_ok(mount_point)) != 0)
			{
				if(fd == 1)
				{
					fprintf(stderr, "Could not open %s: %s\n",
						mount_point, strerror(errno));
				}
				exit(1);
			}
		}
		else exit(1);
        }

#if !defined(MNT_DETACH)
 #define	MNT_DETACH	2
#endif
	
        if (umount(mount_point) != 0) {
                /* fprintf(stderr, "Could not umount %s: %s\n,Trying lazy umount.\n",
                        mount_point, strerror(errno)); */
		if(umount2(mount_point,MNT_DETACH) != 0)
		{
			fprintf(stderr, "Lazy umount failed.\n");
			return 1;
		}
        }

        if ((fd = open(MOUNTED"~", O_RDWR|O_CREAT|O_EXCL, 0600)) == -1)
        {
                fprintf(stderr, "Can't get "MOUNTED"~ lock file");
                return 1;
        }
        close(fd);
	
        if ((mtab = setmntent(MOUNTED, "r")) == NULL) {
                fprintf(stderr, "Can't open " MOUNTED ": %s\n",
                        strerror(errno));
                return 1;
        }

#define MOUNTED_TMP MOUNTED".tmp"

        if ((new_mtab = setmntent(MOUNTED_TMP, "w")) == NULL) {
                fprintf(stderr, "Can't open " MOUNTED_TMP ": %s\n",
                        strerror(errno));
                endmntent(mtab);
                return 1;
        }

        while ((mnt = getmntent(mtab)) != NULL) {
                if (strcmp(mnt->mnt_dir, mount_point) != 0) {
                        addmntent(new_mtab, mnt);
                }
        }

        endmntent(mtab);

        if (fchmod (fileno (new_mtab), S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0) {
                fprintf(stderr, "Error changing mode of %s: %s\n",
                        MOUNTED_TMP, strerror(errno));
                exit(1);
        }

        endmntent(new_mtab);

        if (rename(MOUNTED_TMP, MOUNTED) < 0) {
                fprintf(stderr, "Cannot rename %s to %s: %s\n",
                        MOUNTED, MOUNTED_TMP, strerror(errno));
                exit(1);
        }

        if (unlink(MOUNTED"~") == -1)
        {
                fprintf(stderr, "Can't remove "MOUNTED"~");
                return 1;
        }

	return 0;
}	
