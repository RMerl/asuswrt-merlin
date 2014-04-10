/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/types.h>

#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hfs/hfs_mount.h>
#include <hfs/hfs_format.h>

/* Sensible wrappers over the byte-swapping routines */
#include "hfs_endian.h"

#include "../disklib/mntopts.h"


/* This really belongs in mntopts.h */
#define MOPT_PERMISSIONS	{ "perm", 1, MNT_UNKNOWNPERMISSIONS, 0 }

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_PERMISSIONS,
	MOPT_UPDATE,
	{ NULL }
};

#define HFS_MOUNT_TYPE				"hfs"

gid_t	a_gid __P((char *));
uid_t	a_uid __P((char *));
mode_t	a_mask __P((char *));
struct hfs_mnt_encoding * a_encoding __P((char *));
struct hfs_mnt_encoding * get_encoding_pref __P((char *));
int	get_encoding_bias __P((void));
unsigned int  get_default_encoding(void);

void	usage __P((void));


typedef struct CreateDateAttrBuf {
    u_long size;
    struct timespec creationTime;
} CreateDateAttrBuf;

#define	HFS_BLOCK_SIZE	512

/*
 *	This is the straight GMT conversion constant:
 *	00:00:00 January 1, 1970 - 00:00:00 January 1, 1904
 *	(3600 * 24 * ((365 * (1970 - 1904)) + (((1970 - 1904) / 4) + 1)))
 */
#define MAC_GMT_FACTOR		2082844800UL

#define KEXT_LOAD_COMMAND	"/sbin/kextload"

#define ENCODING_MODULE_PATH	"/System/Library/Filesystems/hfs.fs/Encodings/"

#define MXENCDNAMELEN	16	/* Maximun length of encoding name string */

struct hfs_mnt_encoding {
	char	encoding_name[MXENCDNAMELEN];	/* encoding type name */
	u_long	encoding_id;			/* encoding type number */
};


/*
 * Lookup table for hfs encoding names
 * Note: Names must be in alphabetical order
 */
struct hfs_mnt_encoding hfs_mnt_encodinglist[] = {
	{ "Arabic",	          4 },
	{ "Armenian",        24 },
	{ "Bengali",         13 },
	{ "Burmese",         19 },
	{ "Celtic",          39 },
	{ "CentralEurRoman", 29 },
	{ "ChineseSimp",     25 },
	{ "ChineseTrad",      2 },
	{ "Croatian",	     36 },
	{ "Cyrillic",	      7 },
	{ "Devanagari",       9 },
	{ "Ethiopic",        28 },
	{ "Farsi",          140 },
	{ "Gaelic",          40 },
	{ "Georgian",        23 },
	{ "Greek",	          6 },
	{ "Gujarati",        11 },
	{ "Gurmukhi",        10 },
	{ "Hebrew",	          5 },
	{ "Icelandic",	     37 },
	{ "Japanese",	      1 },
	{ "Kannada",         16 },
	{ "Khmer",           20 },
	{ "Korean",	          3 },
	{ "Laotian",         22 },
	{ "Malayalam",       17 },
	{ "Mongolian",       27 },
	{ "Oriya",           12 },
	{ "Roman",	          0 },	/* default */
	{ "Romanian",	     38 },
	{ "Sinhalese",       18 },
	{ "Tamil",           14 },
	{ "Telugu",          15 },
	{ "Thai",	         21 },
	{ "Tibetan",         26 },
	{ "Turkish",	     35 },
	{ "Ukrainian",      152 },
	{ "Vietnamese",      30 },
};


u_long getVolumeCreateDate(const char *device)
{
	int fd = 0;
	off_t offset;
	char * bufPtr;
	HFSMasterDirectoryBlock * mdbPtr;
	u_long volume_create_time = 0;

	bufPtr = (char *)malloc(HFS_BLOCK_SIZE);
	if ( ! bufPtr ) goto exit;

	fd = open( device, O_RDONLY | O_NDELAY, 0 );
	if( fd <= 0 ) goto exit;

	offset = (off_t)(2 * HFS_BLOCK_SIZE);
	if (lseek(fd, offset, SEEK_SET) != offset) goto exit;

	if (read(fd, bufPtr, HFS_BLOCK_SIZE) != HFS_BLOCK_SIZE) goto exit;

	mdbPtr = (HFSMasterDirectoryBlock *) bufPtr;

	/* get the create date from the MDB (embedded case) or Volume Header */
	if ((mdbPtr->drSigWord == SWAP_BE16 (kHFSSigWord)) &&
	    (mdbPtr->drEmbedSigWord == SWAP_BE16 (kHFSPlusSigWord))) {
		/* Embedded volume*/
		volume_create_time = SWAP_BE32 (mdbPtr->drCrDate);
		
	} else if (mdbPtr->drSigWord == kHFSPlusSigWord ) {
		HFSPlusVolumeHeader * volHdrPtr = (HFSPlusVolumeHeader *) bufPtr;

		volume_create_time = SWAP_BE32 (volHdrPtr->createDate);
	} else {
		goto exit;	/* cound not match signature */
	}

	if (volume_create_time > MAC_GMT_FACTOR)
		volume_create_time -= MAC_GMT_FACTOR;
	else
		volume_create_time = 0;	/* don't let date go negative! */

exit:
	if ( fd > 0 )
		close( fd );

	if ( bufPtr )
		free( bufPtr );

	return volume_create_time;
}

void syncCreateDate(const char *mntpt, u_long localCreateTime)
{
	int result;
	char path[256];
	struct attrlist	attributes;
	CreateDateAttrBuf attrReturnBuffer;
	int64_t gmtCreateTime;
	int32_t gmtOffset;
	int32_t newCreateTime;

	snprintf(path, sizeof(path), "%s/", mntpt);

	attributes.bitmapcount	= ATTR_BIT_MAP_COUNT;
	attributes.reserved		= 0;
	attributes.commonattr	= ATTR_CMN_CRTIME;
	attributes.volattr 		= 0;
	attributes.dirattr 		= 0;
	attributes.fileattr 	= 0;
	attributes.forkattr 	= 0;

	result = getattrlist(path, &attributes, &attrReturnBuffer, sizeof(attrReturnBuffer), 0 );
	if (result) return;

	gmtCreateTime = attrReturnBuffer.creationTime.tv_sec;
	gmtOffset = gmtCreateTime - (int64_t) localCreateTime + 900;
	if (gmtOffset > 0) {
		gmtOffset = 1800 * (gmtOffset / 1800);
	} else {
		gmtOffset = -1800 * ((-gmtOffset + 1799) / 1800);
	}
	
	newCreateTime = localCreateTime + gmtOffset;

	/*
	 * if the root directory's create date doesn't match
	 * and its within +/- 15 seconds, then update it
	 */
	if ((newCreateTime != attrReturnBuffer.creationTime.tv_sec) &&
		(( newCreateTime - attrReturnBuffer.creationTime.tv_sec) > -15) &&
		((newCreateTime - attrReturnBuffer.creationTime.tv_sec) < 15)) {

		attrReturnBuffer.creationTime.tv_sec = (u_long) newCreateTime;
		(void) setattrlist (path,
				    &attributes,
				    &attrReturnBuffer.creationTime,
				    sizeof(attrReturnBuffer.creationTime),
				    0);
	}
}

/*
 * load_encoding
 * loads an hfs encoding converter module into the kernel
 *
 * Note: unloading of encoding converter modules is done in the kernel
 */
static int
load_encoding(struct hfs_mnt_encoding *encp)
{
	int pid;
	int loaded;
	union wait status;
	struct stat sb;
	char kmodfile[MAXPATHLEN];
	
	/* MacRoman encoding (0) is built into the kernel */
	if (encp->encoding_id == 0)
		return (0);

	sprintf(kmodfile, "%sHFS_Mac%s.kext", ENCODING_MODULE_PATH, encp->encoding_name);
	if (stat(kmodfile, &sb) == -1) {
		fprintf(stdout, "unable to find: %s\n", kmodfile);
		return (-1);
	}

	loaded = 0;
	pid = fork();
	if (pid == 0) {
		(void) execl(KEXT_LOAD_COMMAND, KEXT_LOAD_COMMAND, kmodfile, NULL);

		exit(1);	/* We can only get here if the exec failed */
	} else if (pid != -1) {
		if ((waitpid(pid, (int *)&status, 0) == pid) && WIFEXITED(status)) {
			/* we attempted a load */
			loaded = 1;
		}
	}

	if (!loaded) {
		fprintf(stderr, "unable to load: %s\n", kmodfile);
		return (-1);
	}
	return (0);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct hfs_mount_args args;
	int ch, mntflags;
	char *dev, dir[MAXPATHLEN];
	int mountStatus;
	struct timeval dummy_timeval; /* gettimeofday() crashes if the first argument is NULL */
	u_long localCreateTime;
	struct hfs_mnt_encoding *encp;

	mntflags = 0;
	encp = NULL;
	(void)memset(&args, '\0', sizeof(struct hfs_mount_args));

   	/*
   	 * For a mount update, the following args must be explictly
   	 * passed in as options to change their value.  On a new
   	 * mount, default values will be computed for all args.
   	 */
	args.flags = VNOVAL;
	args.hfs_uid = (uid_t)VNOVAL;
	args.hfs_gid = (gid_t)VNOVAL;
	args.hfs_mask = (mode_t)VNOVAL;
	args.hfs_encoding = (u_long)VNOVAL;


	optind = optreset = 1;		/* Reset for parse of new argv. */
	while ((ch = getopt(argc, argv, "xu:g:m:e:o:wt:jc")) != EOF)
		switch (ch) {
		case 't': {
			char *ptr;
			args.journal_tbuffer_size = strtoul(optarg, &ptr, 0);
			if (errno != 0) {
				fprintf(stderr, "%s: Invalid tbuffer size %s\n", argv[0], optarg);
				exit(5);
			} else {
				if (*ptr == 'k')
					args.journal_tbuffer_size *= 1024;
				else if (*ptr == 'm')
					args.journal_tbuffer_size *= 1024*1024;
			}
			if (args.flags == VNOVAL) 
				args.flags = HFSFSMNT_EXTENDED_ARGS;
			break;
		}
		case 'j':
			args.journal_disable = 1;
			break;
		case 'c':
			// XXXdbg JOURNAL_NO_GROUP_COMMIT == 0x0001
			args.journal_flags = 0x0001;
			break;
		case 'x':
			if (args.flags == VNOVAL)
				args.flags = 0;
			args.flags |= HFSFSMNT_NOXONFILES;
			break;
		case 'u':
			args.hfs_uid = a_uid(optarg);
			break;
		case 'g':
			args.hfs_gid = a_gid(optarg);
			break;
		case 'm':
			args.hfs_mask = a_mask(optarg);
			break;
		case 'e':
			encp = a_encoding(optarg);
			break;
		case 'o':
			{
				int dummy;
				getmntopts(optarg, mopts, &mntflags, &dummy);
				if (mntflags & MNT_UNKNOWNPERMISSIONS) {
					/*
						The defaults to be supplied in lieu of the on-disk permissions
						(could be overridden by explicit -u, -g, or -m options):
					 */
					if (args.hfs_uid == (uid_t)VNOVAL) args.hfs_uid = UNKNOWNUID;
					if (args.hfs_gid == (gid_t)VNOVAL) args.hfs_gid = UNKNOWNGID;
#if OVERRIDE_UNKNOWN_PERMISSIONS
					if (args.hfs_mask == (mode_t)VNOVAL) args.hfs_mask = ACCESSPERMS;  /* 0777 */
#endif
				};
			}
			break;
		case 'w':
			if (args.flags == VNOVAL)
				args.flags = 0;
			args.flags |= HFSFSMNT_WRAPPER;
			break;
		case '?':
			usage();
			break;
		default:
#if DEBUG
			printf("mount_hfs: ERROR: unrecognized ch = '%c'\n", ch);
#endif
			usage();
		}; /* switch */
    
	argc -= optind;
	argv += optind;

	if (argc != 2) {
#if DEBUG
		printf("mount_hfs: ERROR: argc == %d != 2\n", argc);
#endif
		usage();
	}

	dev = argv[0];

	if (realpath(argv[1], dir) == NULL)
		err(1, "realpath %s", dir);

	args.fspec = dev;

	/* HFS volumes need timezone info to convert local to GMT */
	(void) gettimeofday( &dummy_timeval, &args.hfs_timezone );

	/* load requested encoding (if any) for hfs volume */
	if (encp != NULL) {
		if (load_encoding(encp) != 0)
			exit(1);  /* load failure */
		args.hfs_encoding = encp->encoding_id;
	}
	
	/*
	 * For a new mount (non-update case) fill in default values for all args
	 */
	if ((mntflags & MNT_UPDATE) == 0) {
		struct stat sb;

           	if (args.flags == VNOVAL)
            		args.flags = 0;

		if ((args.hfs_encoding == (u_long)VNOVAL) && (encp == NULL)) {
			args.hfs_encoding = 0;

			/* Check if volume had a previous encoding preference. */
			encp = get_encoding_pref(dev);
			if (encp != NULL) {
				if (load_encoding(encp) == 0)
					args.hfs_encoding = encp->encoding_id;
			}
		}
		/* when the mountpoint is root, use default values */
		if (strcmp(dir, "/") == 0) {
			sb.st_mode = 0777;
			sb.st_uid = 0;
			sb.st_gid = 0;
			
		/* otherwise inherit from the mountpoint */
		} else if (stat(dir, &sb) == -1)
			err(1, "stat %s", dir);

		if (args.hfs_uid == (uid_t)VNOVAL)
			args.hfs_uid = sb.st_uid;

		if (args.hfs_gid == (gid_t)VNOVAL)
			args.hfs_gid = sb.st_gid;

		if (args.hfs_mask == (mode_t)VNOVAL)
			args.hfs_mask = sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	}
#if DEBUG
    printf("mount_hfs: calling mount: \n" );
    printf("\tdevice = %s\n", dev);
    printf("\tmount point = %s\n", dir);
    printf("\tmount flags = 0x%08x\n", mntflags);
    printf("\targ flags = 0x%x\n", args.flags);
    printf("\tuid = %d\n", args.hfs_uid);
    printf("\tgid = %d\n", args.hfs_gid);
    printf("\tmode = %o\n", args.hfs_mask);
    printf("\tencoding = %ld\n", args.hfs_encoding);

#endif
	if ((mntflags & MNT_RDONLY) == 0) {
		/*
		 * get the volume's create date so we can synchronize
		 * it with the root directory create date
		 */
		localCreateTime = getVolumeCreateDate(dev);
	}
	else {
		localCreateTime = 0;
	}

    if ((mountStatus = mount(HFS_MOUNT_TYPE, dir, mntflags, &args)) < 0) {
#if DEBUG
        printf("mount_hfs: error on mount(): error = %d.\n", mountStatus);
#endif
        err(1, NULL);
        };
   
	/*
	 * synchronize the root directory's create date
	 * with the volume's create date
	 */
	if (localCreateTime)
		syncCreateDate(dir, localCreateTime);

    exit(0);
}


gid_t
a_gid(s)
    char *s;
{
    struct group *gr;
    char *gname;
    gid_t gid = 0;

    if ((gr = getgrnam(s)) != NULL)
        gid = gr->gr_gid;
    else {
        for (gname = s; *s && isdigit(*s); ++s);
        if (!*s)
            gid = atoi(gname);
        else
            errx(1, "unknown group id: %s", gname);
    }
    return (gid);
}

uid_t
a_uid(s)
    char *s;
{
    struct passwd *pw;
    char *uname;
    uid_t uid = 0;

    if ((pw = getpwnam(s)) != NULL)
        uid = pw->pw_uid;
    else {
        for (uname = s; *s && isdigit(*s); ++s);
        if (!*s)
            uid = atoi(uname);
        else
            errx(1, "unknown user id: %s", uname);
    }
    return (uid);
}

mode_t
a_mask(s)
    char *s;
{
    int done, rv;
    char *ep;

    done = 0;
    rv = -1;
    if (*s >= '0' && *s <= '7') {
        done = 1;
        rv = strtol(optarg, &ep, 8);
    }
    if (!done || rv < 0 || *ep)
        errx(1, "invalid file mode: %s", s);
    return (rv);
}

struct hfs_mnt_encoding *
a_encoding(s)
	char *s;
{
	char *uname;
	int i;
	u_long encoding;
	struct hfs_mnt_encoding *p, *q, *enclist;
	int elements = sizeof(hfs_mnt_encodinglist) / sizeof(struct hfs_mnt_encoding);
	int compare;

	/* Use a binary search to find an encoding match */
	p = hfs_mnt_encodinglist;
	q = p + (elements - 1);
	while (p <= q) {
		enclist = p + ((q - p) >> 1);	/* divide by 2 */
		compare = strcmp(s, enclist->encoding_name);
		if (compare < 0)
			q = enclist - 1;
		else if (compare > 0)
			p = enclist + 1;
		else
			return (enclist);
	}

	for (uname = s; *s && isdigit(*s); ++s);

	if (*s) goto unknown;

	encoding = atoi(uname);
	for (i=0, enclist = hfs_mnt_encodinglist; i < elements; i++, enclist++) {
		if (enclist->encoding_id == encoding)
			return (enclist);
	}

unknown:
	errx(1, "unknown encoding: %s", uname);
	return (NULL);
}


/*
 * Get file system's encoding preference.
 */
struct hfs_mnt_encoding *
get_encoding_pref(char *dev)
{
	char buffer[HFS_BLOCK_SIZE];
	struct hfs_mnt_encoding *enclist;
	HFSMasterDirectoryBlock * mdbp;
	int encoding = -1;
	int elements;
	int fd;
	int i;

	/* Can only load encoding modules if root. */
	if (geteuid() != 0)
		return (NULL);

	fd = open(dev, O_RDONLY | O_NDELAY, 0);
	if (fd == -1)
		return (NULL);

     	if (pread(fd, buffer, sizeof(buffer), 1024) != sizeof(buffer)) {
     		close(fd);
		return (NULL);
	}
    	close(fd);

	mdbp = (HFSMasterDirectoryBlock *) buffer;
	if (SWAP_BE16(mdbp->drSigWord) != kHFSSigWord ||
	    SWAP_BE16(mdbp->drEmbedSigWord) == kHFSPlusSigWord ) {
		return (NULL);
	}
	encoding = GET_HFS_TEXT_ENCODING(SWAP_BE32(mdbp->drFndrInfo[4]));

	if (encoding == -1) {
		encoding = get_encoding_bias();
		if (encoding == 0 || encoding == -1)
			encoding = get_default_encoding();
	}

	elements = sizeof(hfs_mnt_encodinglist) / sizeof(struct hfs_mnt_encoding);
	for (i=0, enclist = hfs_mnt_encodinglist; i < elements; i++, enclist++) {
		if (enclist->encoding_id == encoding)
			return (enclist);
	}

	return (NULL);
}

/*
 * Get kernel's encoding bias.
 */
int
get_encoding_bias()
{
        int mib[3];
        size_t buflen = sizeof(int);
        struct vfsconf vfc;
        int hint = 0;

        if (getvfsbyname("hfs", &vfc) < 0)
		goto error;

        mib[0] = CTL_VFS;
        mib[1] = vfc.vfc_typenum;
        mib[2] = HFS_ENCODINGBIAS;
 
	if (sysctl(mib, 3, &hint, &buflen, NULL, 0) < 0)
 		goto error;
	return (hint);
error:
	return (-1);
}

#define __kCFUserEncodingFileName ("/.CFUserTextEncoding")

unsigned int
get_default_encoding()
{
	struct passwd *passwdp;

	if ((passwdp = getpwuid(0))) {	/* root account */
		char buffer[MAXPATHLEN + 1];
		int fd;

		strcpy(buffer, passwdp->pw_dir);
		strcat(buffer, __kCFUserEncodingFileName);

		if ((fd = open(buffer, O_RDONLY, 0)) > 0) {
			size_t readSize;

			readSize = read(fd, buffer, MAXPATHLEN);
			buffer[(readSize < 0 ? 0 : readSize)] = '\0';
			close(fd);
			return strtol(buffer, NULL, 0);
		}
	}
	return (0);	/* Fallback to smRoman */
}


void
usage()
{
	(void)fprintf(stderr,
               "usage: mount_hfs [-xw] [-u user] [-g group] [-m mask] [-e encoding] [-t tbuffer-size] [-j] [-c] [-o options] special-device filesystem-node\n");
	(void)fprintf(stderr, "   -j disables journaling; -c disables group-commit for journaling\n");
	
	exit(1);
}
