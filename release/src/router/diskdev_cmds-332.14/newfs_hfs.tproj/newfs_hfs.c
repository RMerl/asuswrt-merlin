/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
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


#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#if LINUX
#include <time.h>
#endif

#if !LINUX
#include <IOKit/storage/IOMediaBSDClient.h>
#endif

#include <hfs/hfs_format.h>
#include "newfs_hfs.h"

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define	NOVAL       (-1)
#define UMASK       (0755)
#define	ACCESSMASK  (0777)

#define ROUNDUP(x,y) (((x)+(y)-1)/(y)*(y))

static void getnodeopts __P((char* optlist));
static void getclumpopts __P((char* optlist));
static gid_t a_gid __P((char *));
static uid_t a_uid __P((char *));
static mode_t a_mask __P((char *));
static int hfs_newfs __P((char *device, int forceHFS, int isRaw));
static void validate_hfsplus_block_size __P((UInt64 sectorCount, UInt32 sectorSize));
static void hfsplus_params __P((UInt64 sectorCount, UInt32 sectorSize, hfsparams_t *defaults));
static void hfs_params __P((UInt32 sectorCount, UInt32 sectorSize, hfsparams_t *defaults));
static UInt32 clumpsizecalc __P((UInt32 clumpblocks));
static UInt32 CalcBTreeClumpSize __P((UInt32 blockSize, UInt32 nodeSize, UInt32 driveBlocks, int catalog));
static UInt32 CalcHFSPlusBTreeClumpSize __P((UInt32 blockSize, UInt32 nodeSize, UInt64 sectors, int catalog));
static void usage __P((void));


char	*progname;
char	gVolumeName[kHFSPlusMaxFileNameChars + 1] = {kDefaultVolumeNameStr};
#if !LINUX
char	rawdevice[MAXPATHLEN]; 
#endif
char	blkdevice[MAXPATHLEN];
UInt32	gBlockSize = 0;
UInt32	gNextCNID = kHFSFirstUserCatalogNodeID;

time_t  createtime;

int	gNoCreate = FALSE;
int	gWrapper = FALSE;
int	gUserCatNodeSize = FALSE;
int	gCaseSensitive = FALSE;

#define JOURNAL_DEFAULT_SIZE (8*1024*1024)
int     gJournaled = FALSE;
char    *gJournalDevice = NULL;
int     gJournalSize = JOURNAL_DEFAULT_SIZE;

uid_t	gUserID = (uid_t)NOVAL;
gid_t	gGroupID = (gid_t)NOVAL;
mode_t	gModeMask = (mode_t)NOVAL;

UInt32	catnodesiz = 8192;
UInt32	extnodesiz = 4096;
UInt32	atrnodesiz = 4096;

UInt32	catclumpblks = 0;
UInt32	extclumpblks = 0;
UInt32	atrclumpblks = 0;
UInt32	bmclumpblks = 0;
UInt32	rsrclumpblks = 0;
UInt32	datclumpblks = 0;
UInt32	hfsgrowblks = 0;      /* maximum growable size of wrapper */


int
get_num(char *str)
{
    int num;
    char *ptr;

    num = strtoul(str, &ptr, 0);

    if (*ptr) {
	if (tolower(*ptr) == 'k') {
	    num *= 1024;
	} else if (tolower(*ptr) == 'm') {
	    num *= (1024 * 1024);
	} else if (tolower(*ptr) == 'g') {
	    num *= (1024 * 1024 * 1024);
	}
    }

    return num;
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	int ch;
	int forceHFS;
#if !LINUX
	char *cp, *special;
	struct statfs *mp;
#endif
	int n;
	
	if ((progname = strrchr(*argv, '/')))
		++progname;
	else
		progname = *argv;

	forceHFS = FALSE;

	while ((ch = getopt(argc, argv, "G:J:M:NU:hswb:c:i:n:v:")) != EOF)
		switch (ch) {
		case 'G':
			gGroupID = a_gid(optarg);
			break;

		case 'J':
			gJournaled = TRUE;
			if (isdigit(optarg[0])) {
			    gJournalSize = get_num(optarg);
			    if (gJournalSize < 512*1024) {
					printf("%s: journal size %dk too small.  Reset to %dk.\n",
						progname, gJournalSize/1024, JOURNAL_DEFAULT_SIZE/1024);
					gJournalSize = JOURNAL_DEFAULT_SIZE;
			    }
			} else {
				/* back up because there was no size argument */
			    optind--;
			}
			break;
			
		case 'N':
			gNoCreate = TRUE;
			break;

		case 'M':
			gModeMask = a_mask(optarg);
			break;

		case 'U':
			gUserID = a_uid(optarg);
			break;

		case 'b':
			gBlockSize = atoi(optarg);
			if (gBlockSize < HFSMINBSIZE)
				fatal("%s: bad allocation block size (too small)", optarg);
			if (gBlockSize > MAXBSIZE) 
				fatal("%s: bad allocation block size (too large)", optarg);
			break;

		case 'c':
			getclumpopts(optarg);
			break;

		case 'h':
			forceHFS = TRUE;
			break;

		case 'i':
			gNextCNID = atoi(optarg);
			/*
			 * make sure its at least kHFSFirstUserCatalogNodeID
			 */
			if (gNextCNID < kHFSFirstUserCatalogNodeID)
				fatal("%s: starting catalog node id too small (must be > 15)", optarg);
			break;

		case 'n':
			getnodeopts(optarg);
			break;

		case 's':
			gCaseSensitive = TRUE;
			break;

		case 'v':
			n = strlen(optarg);
			if (n > (int)(sizeof(gVolumeName) - 1))
				fatal("\"%s\" is too long (%d byte maximum)",
				      optarg, sizeof(gVolumeName) - 1);
			if (n == 0)
				fatal("name required with -v option");
			strcpy(gVolumeName, optarg);
			break;

		case 'w':
			gWrapper = TRUE;
			break;

		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
#if LINUX
	(void) sprintf(blkdevice, "%s", argv[0]);
#else
	special = argv[0];
	cp = strrchr(special, '/');
	if (cp != 0)
		special = cp + 1;
	if (*special == 'r')
		special++;
	(void) sprintf(rawdevice, "%sr%s", _PATH_DEV, special);
	(void) sprintf(blkdevice, "%s%s", _PATH_DEV, special);
#endif

	if (forceHFS && gJournaled) {
		fprintf(stderr, "-h -J: incompatible options specified\n");
		usage();
	}
	if (gCaseSensitive && (forceHFS || gWrapper)) {
		fprintf(stderr, "-s: incompatible options specified\n");
		usage();
	}
	if (gWrapper && forceHFS) {
		fprintf(stderr, "-h -w: incompatible options specified\n");
		usage();
	}
	if (!gWrapper && hfsgrowblks) {
		fprintf(stderr, "g clump option requires -w option\n");
		exit(1);
	}

	/*
	 * Check if target device is aready mounted
	 */
#if LINUX
	// FIXME
#else
	n = getmntinfo(&mp, MNT_NOWAIT);
	if (n == 0)
		fatal("%s: getmntinfo: %s", blkdevice, strerror(errno));

	while (--n >= 0) {
		if (strcmp(blkdevice, mp->f_mntfromname) == 0)
			fatal("%s is mounted on %s", blkdevice, mp->f_mntonname);
		++mp;
	}
#endif

	if (hfs_newfs(blkdevice, forceHFS, true) < 0) {
#if LINUX
		err(1, NULL);
#else
		/* On ENXIO error use the block device (to get de-blocking) */
		if (errno == ENXIO) {
			if (hfs_newfs(blkdevice, forceHFS, false) < 0)
				err(1, NULL);
		} else
			err(1, NULL);
#endif
	}

	exit(0);
}


static void getnodeopts(char* optlist)
{
	char *strp = optlist;
	char *ndarg;
	char *p;
	UInt32 ndsize;
	
	while((ndarg = strsep(&strp, ",")) != NULL && *ndarg != '\0') {

		p = strchr(ndarg, '=');
		if (p == NULL)
			usage();
	
		ndsize = atoi(p+1);

		switch (*ndarg) {
		case 'c':
			if (ndsize < 4096 || ndsize > 32768 || (ndsize & (ndsize-1)) != 0)
				fatal("%s: invalid catalog b-tree node size", ndarg);
			catnodesiz = ndsize;
			gUserCatNodeSize = TRUE;
			break;

		case 'e':
			if (ndsize < 1024 || ndsize > 32768 || (ndsize & (ndsize-1)) != 0)
				fatal("%s: invalid extents b-tree node size", ndarg);
			extnodesiz = ndsize;
			break;

		case 'a':
			if (ndsize < 1024 || ndsize > 32768 || (ndsize & (ndsize-1)) != 0)
				fatal("%s: invalid atrribute b-tree node size", ndarg);
			atrnodesiz = ndsize;
			break;

		default:
			usage();
		}
	}
}


static void getclumpopts(char* optlist)
{
	char *strp = optlist;
	char *ndarg;
	char *p;
	UInt32 clpblocks;
	
	while((ndarg = strsep(&strp, ",")) != NULL && *ndarg != '\0') {

		p = strchr(ndarg, '=');
		if (p == NULL)
			usage();
			
		clpblocks = atoi(p+1);
		
		switch (*ndarg) {
		case 'a':
			atrclumpblks = clpblocks;
			break;
		case 'b':
			bmclumpblks = clpblocks;
			break;
		case 'c':
			catclumpblks = clpblocks;
			break;
		case 'd':
			datclumpblks = clpblocks;
			break;
		case 'e':
			extclumpblks = clpblocks;
			break;
		case 'g':  /* maximum growable size of hfs wrapper */
			hfsgrowblks = clpblocks;
			break;
		case 'r':
			rsrclumpblks = clpblocks;
			break;

		default:
			usage();
		}
	}
}

gid_t
static a_gid(char *s)
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

static uid_t
a_uid(char *s)
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

static mode_t
a_mask(char *s)
{
	int done, rv;
	char *ep;

	done = 0;
	rv = -1;
	if (*s >= '0' && *s <= '7') {
		done = 1;
		rv = strtol(s, &ep, 8);
	}
	if (!done || rv < 0 || *ep)
		errx(1, "invalid access mask: %s", s);
	return (rv);
}


/*
 * Validate the HFS Plus allocation block size in gBlockSize.  If none was
 * specified, then calculate a suitable default.
 *
 * Modifies the global variable gBlockSize.
 */
static void validate_hfsplus_block_size(UInt64 sectorCount, UInt32 sectorSize)
{
	if (gBlockSize == 0) {
		/* Compute a default allocation block size based on volume size */
		gBlockSize = DFL_BLKSIZE;	/* Prefer the default of 4K */
		
		/* Use a larger power of two if total blocks would overflow 32 bits */
		while ((sectorCount / (gBlockSize / sectorSize)) > 0xFFFFFFFF) {
			gBlockSize <<= 1;	/* Must be a power of two */
		}
	} else {
		/* Make sure a user-specified block size is reasonable */
		if ((gBlockSize & (gBlockSize-1)) != 0)
			fatal("%s: bad HFS Plus allocation block size (must be a power of two)", optarg);
	
		if ((sectorCount / (gBlockSize / sectorSize)) > 0xFFFFFFFF)
			fatal("%s: block size is too small for %lld sectors", optarg, gBlockSize, sectorCount);

		if (gBlockSize < HFSOPTIMALBLKSIZE)
			warnx("Warning: %i is a non-optimal block size (4096 would be a better choice)", gBlockSize);
	}
}



static int
hfs_newfs(char *device, int forceHFS, int isRaw)
{
	struct stat stbuf;
	DriveInfo dip;
	int fso = 0;
	int retval = 0;
	hfsparams_t defaults = {0};
#if !LINUX
	u_int64_t maxSectorsPerIO;
#endif

	if (gNoCreate) {
		fso = open( device, O_RDONLY | O_NDELAY, 0 );
	} else {
		fso = open( device, O_WRONLY | O_NDELAY, 0 );
	}

	if (fso < 0)
		fatal("%s: %s", device, strerror(errno));

	if (fstat( fso, &stbuf) < 0)
		fatal("%s: %s", device, strerror(errno));
#if LINUX
	dip.sectorSize = 512;
	dip.sectorsPerIO = 256;

#ifndef	BLKGETSIZE
#define	BLKGETSIZE		_IO(0x12,96)
#endif
#ifndef	BLKGETSIZE64
#define BLKGETSIZE64		_IOR(0x12,114,size_t)
#endif
        
	if (S_ISREG(stbuf.st_mode)) {
                dip.totalSectors = stbuf.st_size / 512;
        } 
	else if (S_ISBLK(stbuf.st_mode)) {
                unsigned long size;
                u_int64_t size64;
                if (!ioctl(fso, BLKGETSIZE64, &size64))
                        dip.totalSectors = size64 / 512;
                else if (!ioctl(fso, BLKGETSIZE, &size))
                        dip.totalSectors = size;
                else
                        fatal("%s: %s", device, strerror(errno));
        } 
	else
                fatal("%s: is not a block device", device);
#else
	if (ioctl(fso, DKIOCGETBLOCKCOUNT, &dip.totalSectors) < 0)
		fatal("%s: %s", device, strerror(errno));

	if (ioctl(fso, DKIOCGETBLOCKSIZE, &dip.sectorSize) < 0)
		fatal("%s: %s", device, strerror(errno));

	if (ioctl(fso, DKIOCGETMAXBLOCKCOUNTWRITE, &maxSectorsPerIO) < 0)
		dip.sectorsPerIO = (128 * 1024) / dip.sectorSize; /* use 128K as default */
	else
		dip.sectorsPerIO = MIN(maxSectorsPerIO, (1024 * 1024) / dip.sectorSize);
#endif
	
        /*
         * The make_hfs code currently does 512 byte sized I/O.
         * If the sector size is bigger than 512, start over
         * using the block device (to get de-blocking).
         */       
#if !LINUX
        if (dip.sectorSize != kBytesPerSector) {
		if (isRaw) {
			close(fso);
			errno = ENXIO;
			return (-1);
		} else {
			if ((dip.sectorSize % kBytesPerSector) != 0)
				fatal("%d is an unsupported sector size\n", dip.sectorSize);

			dip.totalSectors *= (dip.sectorSize / kBytesPerSector);
			dip.sectorsPerIO *= (dip.sectorSize / kBytesPerSector);
			dip.sectorSize = kBytesPerSector;
		}
        }
#endif

	dip.fd = fso;
	dip.sectorOffset = 0;
	time(&createtime);

	if (gWrapper && (dip.totalSectors >= kMaxWrapableSectors)) {
		gWrapper = 0;
		fprintf(stderr, "%s: WARNING: wrapper option ignored since volume size > 256GB\n", progname);
	}

        /*
         * If we're going to make an HFS Plus disk (with or without a wrapper), validate the
         * HFS Plus allocation block size.  This will also calculate a default allocation
         * block size if none (or zero) was specified.
         */
        if (!forceHFS)
            validate_hfsplus_block_size(dip.totalSectors, dip.sectorSize);
        
	/* Make an HFS disk */
	if (forceHFS || gWrapper) {
		hfs_params(dip.totalSectors, dip.sectorSize, &defaults);
		if (gNoCreate == 0) {
			UInt32 totalSectors, sectorOffset;

			retval = make_hfs(&dip, &defaults, &totalSectors, &sectorOffset);
			if (retval)
				fatal("%s: %s", device, strerror(errno));

			if (gWrapper) {
				dip.totalSectors = totalSectors;
				dip.sectorOffset = sectorOffset;
			} else {
				printf("Initialized %s as a %ld MB HFS volume\n",
					device, (long)(dip.totalSectors/2048));
			}
		}
	}

	/* Make an HFS Plus disk */
	if (gWrapper || !forceHFS) {
		
		if ((dip.totalSectors * dip.sectorSize ) < kMinHFSPlusVolumeSize)
			fatal("%s: partition is too small (minimum is %d KB)", device, kMinHFSPlusVolumeSize/1024);

		/*
		 * Above 512GB, enforce partition size to be a multiple of 4K.
		 *
		 * Strictly speaking, the threshold could be as high as 1TB volume size, but
		 * this keeps us well away from any potential edge cases.  Besides, partitions
		 * this large should be 4K aligned for performance.
		 */
		if ((dip.totalSectors >= 0x40000000) && (dip.totalSectors & 7))
		    fatal("%s: partition size not a multiple of 4K.", device);

		hfsplus_params(dip.totalSectors, dip.sectorSize, &defaults);
		if (gNoCreate == 0) {
			retval = make_hfsplus(&dip, &defaults);
			if (retval == 0) {
				printf("Initialized %s as a ", device);
				if (dip.totalSectors > 0x2000000)
					printf("%ld GB",
						(long)((dip.totalSectors + (1024*1024))/(2048*1024)));
				else if (dip.totalSectors > 2048)
					printf("%ld MB",
						(long)((dip.totalSectors + 1024)/2048));
				else
					printf("%ld KB",
						(long)((dip.totalSectors + 1)/2));
				if (gJournaled)
					printf(" HFS Plus volume with a %dk journal\n",
						(int)defaults.journalSize/1024);
				else
					printf(" HFS Plus volume\n");
			}
		}
	}

	if (retval)
		fatal("%s: %s", device, strerror(errno));

	if ( fso > 0 ) {
		close(fso);
	}

	return retval;
}


static void hfsplus_params (UInt64 sectorCount, UInt32 sectorSize, hfsparams_t *defaults)
{
	UInt32	totalBlocks;
	UInt32	minClumpSize;
	UInt32	clumpSize;
	UInt32	oddBitmapBytes;
	UInt32  jscale;
	
	defaults->flags = 0;
	defaults->blockSize = gBlockSize;
	defaults->nextFreeFileID = gNextCNID;
	defaults->createDate = createtime + MAC_GMT_FACTOR;     /* Mac OS GMT time */
	defaults->hfsAlignment = 0;
	defaults->journaledHFS = gJournaled;
	defaults->journalDevice = gJournalDevice;

	/*
	 * If any of the owner, group or mask are set then
	 * make sure they're all valid and pass them down.
	 */
	if (gUserID != (uid_t)NOVAL  ||
	    gGroupID != (gid_t)NOVAL ||
	    gModeMask != (mode_t)NOVAL) {
		defaults->owner = (gUserID == (uid_t)NOVAL) ? geteuid() : gUserID;
		defaults->group = (gGroupID == (gid_t)NOVAL) ? getegid() : gGroupID;
		defaults->mask = (gModeMask == (mode_t)NOVAL) ? UMASK : (gModeMask & ACCESSMASK);
		
		defaults->flags |= kUseAccessPerms;
	}

	//
	// we want at least 8 megs of journal for each 100 gigs of
	// disk space.  We cap the size at 512 megs though.
	//
	jscale = (sectorCount * sectorSize) / ((UInt64)100 * 1024 * 1024 * 1024);

	//
	// only scale if it's the default, otherwise just take what
	// the user specified.
	//
	if (gJournalSize == JOURNAL_DEFAULT_SIZE) {
	    defaults->journalSize = gJournalSize * (jscale + 1);
	} else {
	    defaults->journalSize = gJournalSize;
	}
	if (defaults->journalSize > 512 * 1024 * 1024) {
	    defaults->journalSize = 512 * 1024 * 1024;
	}

	strncpy((char *)defaults->volumeName, gVolumeName, sizeof(defaults->volumeName) - 1);
	defaults->volumeName[sizeof(defaults->volumeName) - 1] = '\0';

	if (rsrclumpblks == 0) {
		if (gBlockSize > DFL_BLKSIZE)
			defaults->rsrcClumpSize = ROUNDUP(kHFSPlusRsrcClumpFactor * DFL_BLKSIZE, gBlockSize);
		else
			defaults->rsrcClumpSize = kHFSPlusRsrcClumpFactor * gBlockSize;
	} else
		defaults->rsrcClumpSize = clumpsizecalc(rsrclumpblks);

	if (datclumpblks == 0) {
		if (gBlockSize > DFL_BLKSIZE)
			defaults->dataClumpSize = ROUNDUP(kHFSPlusRsrcClumpFactor * DFL_BLKSIZE, gBlockSize);
		else
			defaults->dataClumpSize = kHFSPlusRsrcClumpFactor * gBlockSize;
	} else
		defaults->dataClumpSize = clumpsizecalc(datclumpblks);

	/*
	 * The default  b-tree node size is 8K.  However, if the
	 * volume is small (< 1 GB) we use 4K instead.
	 */
	if (!gUserCatNodeSize) {
		if ((gBlockSize < HFSOPTIMALBLKSIZE) ||
		    ((UInt64)(sectorCount * sectorSize) < (UInt64)0x40000000))
			catnodesiz = 4096;
	}

	if (catclumpblks == 0) {
		clumpSize = CalcHFSPlusBTreeClumpSize(gBlockSize, catnodesiz, sectorCount, TRUE);
	}
	else {
		clumpSize = clumpsizecalc(catclumpblks);
		
		if (clumpSize % catnodesiz != 0)
			fatal("c=%ld: clump size is not a multiple of node size\n", clumpSize/gBlockSize);
	}
	defaults->catalogClumpSize = clumpSize;
	defaults->catalogNodeSize = catnodesiz;
	if (gBlockSize < 4096 && gBlockSize < catnodesiz)
		warnx("Warning: block size %i is less than catalog b-tree node size %i", gBlockSize, catnodesiz);

	if (extclumpblks == 0) {
		clumpSize = CalcHFSPlusBTreeClumpSize(gBlockSize, extnodesiz, sectorCount, FALSE);
	}
	else {
		clumpSize = clumpsizecalc(extclumpblks);
		if (clumpSize % extnodesiz != 0)
			fatal("e=%ld: clump size is not a multiple of node size\n", clumpSize/gBlockSize);
	}
	defaults->extentsClumpSize = clumpSize;
	defaults->extentsNodeSize = extnodesiz;
	if (gBlockSize < extnodesiz)
		warnx("Warning: block size %i is less than extents b-tree node size %i", gBlockSize, extnodesiz);

	if (atrclumpblks == 0) {
		clumpSize = 0;
	}
	else {
		clumpSize = clumpsizecalc(atrclumpblks);
		if (clumpSize % atrnodesiz != 0)
			fatal("a=%ld: clump size is not a multiple of node size\n", clumpSize/gBlockSize);
	}
	defaults->attributesClumpSize = clumpSize;
	defaults->attributesNodeSize = atrnodesiz;

	/*
	 * Calculate the number of blocks needed for bitmap (rounded up to a multiple of the block size).
	 */
	
	/*
	 * Figure out how many bytes we need for the given totalBlocks
	 * Note: this minimum value may be too large when it counts the
	 * space used by the wrapper
	 */
	totalBlocks = sectorCount / (gBlockSize / sectorSize);

	minClumpSize = totalBlocks >> 3;	/* convert bits to bytes by dividing by 8 */
	if (totalBlocks & 7)
		++minClumpSize;	/* round up to whole bytes */
	
	/* Round up to a multiple of blockSize */
	if ((oddBitmapBytes = minClumpSize % gBlockSize))
		minClumpSize = minClumpSize - oddBitmapBytes + gBlockSize;

	if (bmclumpblks == 0) {
		clumpSize = minClumpSize;
	}
	else {
		clumpSize = clumpsizecalc(bmclumpblks);

		if (clumpSize < minClumpSize)
			fatal("b=%ld: bitmap clump size is too small\n", clumpSize/gBlockSize);
	}
	defaults->allocationClumpSize = clumpSize;

	if (gCaseSensitive)
		defaults->flags |= kMakeCaseSensitive;

	if (gNoCreate) {
		if (!gWrapper)
			printf("%lld sectors (%u bytes per sector)\n", sectorCount, sectorSize);
		printf("HFS Plus format parameters:\n");
		printf("\tvolume name: \"%s\"\n", gVolumeName);
		printf("\tblock-size: %u\n", defaults->blockSize);
		printf("\ttotal blocks: %u\n", totalBlocks);
		if (gJournaled)
			printf("\tjournal-size: %dk\n", (int)defaults->journalSize/1024);
		printf("\tfirst free catalog node id: %u\n", defaults->nextFreeFileID);
		printf("\tcatalog b-tree node size: %u\n", defaults->catalogNodeSize);
		printf("\tinitial catalog file size: %u\n", defaults->catalogClumpSize);
		printf("\textents b-tree node size: %u\n", defaults->extentsNodeSize);
		printf("\tinitial extents file size: %u\n", defaults->extentsClumpSize);
		printf("\tinitial allocation file size: %u (%u blocks)\n",
			defaults->allocationClumpSize, defaults->allocationClumpSize / gBlockSize);
		printf("\tdata fork clump size: %u\n", defaults->dataClumpSize);
		printf("\tresource fork clump size: %u\n", defaults->rsrcClumpSize);
		if (defaults->flags & kUseAccessPerms) {
			printf("\tuser ID: %d\n", (int)defaults->owner);
			printf("\tgroup ID: %d\n", (int)defaults->group);
			printf("\taccess mask: %o\n", (int)defaults->mask);
		}
	}
}


static void hfs_params(UInt32 sectorCount, UInt32 sectorSize, hfsparams_t *defaults)
{
	UInt32	alBlkSize;
	UInt32	vSectorCount;
	UInt32	defaultBlockSize;
	
	defaults->flags = kMakeStandardHFS;
	defaults->nextFreeFileID = gNextCNID;
	defaults->createDate = createtime + MAC_GMT_FACTOR;     /* Mac OS GMT time */
	defaults->catalogNodeSize = kHFSNodeSize;
	defaults->extentsNodeSize = kHFSNodeSize;
	defaults->attributesNodeSize = 0;
	defaults->attributesClumpSize = 0;

	strncpy((char *)defaults->volumeName, gVolumeName, sizeof(defaults->volumeName) - 1);
	defaults->volumeName[sizeof(defaults->volumeName) - 1] = '\0';

	/* Compute the default allocation block size */
	if (gWrapper && hfsgrowblks) {
		defaults->flags |= kMakeMaxHFSBitmap;
		vSectorCount = ((UInt64)hfsgrowblks * 512) / sectorSize;
		defaultBlockSize = sectorSize * ((vSectorCount >> 16) + 1);
	} else
		defaultBlockSize = sectorSize * ((sectorCount >> 16) + 1);

	if (gWrapper) {
		defaults->flags |= kMakeHFSWrapper;
	
		/* round alBlkSize up to multiple of HFS Plus blockSize */
		alBlkSize = ((defaultBlockSize + gBlockSize - 1) / gBlockSize) * gBlockSize;	

		if (gBlockSize > 4096)
			defaults->hfsAlignment = 4096 / sectorSize;		/* Align to 4K boundary */
		else
			defaults->hfsAlignment = gBlockSize / sectorSize;	/* Align to blockSize boundary */
	} else {
		/* If allocation block size is undefined or invalid calculate itÉ*/
		alBlkSize = gBlockSize;
		defaults->hfsAlignment = 0;
	}

	if ( alBlkSize == 0 || (alBlkSize & 0x1FF) != 0 || alBlkSize < defaultBlockSize)
		alBlkSize = defaultBlockSize;

	defaults->blockSize = alBlkSize;

	defaults->dataClumpSize = alBlkSize * 4;
	defaults->rsrcClumpSize = alBlkSize * 4;
	if ( gWrapper || defaults->dataClumpSize > 0x100000 )
		defaults->dataClumpSize = alBlkSize;

	if (gWrapper) {
		if (alBlkSize == kHFSNodeSize) {
			defaults->extentsClumpSize = (2 * kHFSNodeSize); /* header + root/leaf */
			defaults->catalogClumpSize = (4 * kHFSNodeSize); /* header + root + 2 leaves */
		} else {
			defaults->extentsClumpSize = alBlkSize;
			defaults->catalogClumpSize = alBlkSize;
		}
	} else {
		defaults->catalogClumpSize = CalcBTreeClumpSize(alBlkSize, sectorSize, sectorCount, TRUE);
		defaults->extentsClumpSize = CalcBTreeClumpSize(alBlkSize, sectorSize, sectorCount, FALSE);
	}
	
	if (gNoCreate) {
		printf("%i sectors at %i bytes per sector\n", sectorCount, sectorSize);
		printf("%s format parameters:\n", gWrapper ? "HFS Wrapper" : "HFS");
		printf("\tvolume name: \"%s\"\n", gVolumeName);
		printf("\tblock-size: %i\n", defaults->blockSize);
		printf("\ttotal blocks: %i\n", sectorCount / (alBlkSize / sectorSize) );
		printf("\tfirst free catalog node id: %i\n", defaults->nextFreeFileID);
		printf("\tinitial catalog file size: %i\n", defaults->catalogClumpSize);
		printf("\tinitial extents file size: %i\n", defaults->extentsClumpSize);
		printf("\tfile clump size: %i\n", defaults->dataClumpSize);
		if (hfsgrowblks)
			printf("\twrapper growable from %i to %i sectors\n", sectorCount, hfsgrowblks);
	}
}


static UInt32
clumpsizecalc(UInt32 clumpblocks)
{
	UInt64 clumpsize;

	clumpsize = (UInt64)clumpblocks * (UInt64)gBlockSize;
		
	if (clumpsize & (UInt64)(0xFFFFFFFF00000000ULL))
		fatal("=%ld: too many blocks for clump size!", clumpblocks);

	return ((UInt32)clumpsize);
}


/*
 * CalcBTreeClumpSize
 *	
 * This routine calculates the file clump size for both the catalog and
 * extents overflow files. In general, this is 1/128 the size of the
 * volume up to a maximum of 6 MB.  For really large HFS volumes it will
 * be just 1 allocation block.
 */
static UInt32
CalcBTreeClumpSize(UInt32 blockSize, UInt32 nodeSize, UInt32 driveBlocks, int catalog)
{
	UInt32	clumpSectors;
	UInt32	maximumClumpSectors;
	UInt32	sectorsPerBlock = blockSize >> kLog2SectorSize;
	UInt32	sectorsPerNode = nodeSize >> kLog2SectorSize;
	UInt32	nodeBitsInHeader;
	UInt32	limitClumpSectors;
	
	if (catalog)
		limitClumpSectors = 6 * 1024 * 1024 / 512;	/* overall limit of 6MB */
	else
		limitClumpSectors = 4 * 1024 * 1024 / 512;	/* overall limit of 4MB */
	/* 
	 * For small node sizes (eg., HFS, or default HFS Plus extents), then the clump size will
	 * be as big as the header's map record can handle.  (That is, as big as possible, without
	 * requiring a map node.)
	 * 
	 * But for a 32K node size, this works out to nearly 8GB.  We need to restrict it further.
	 * To avoid arithmetic overflow, we'll calculate things in terms of 512-byte sectors.
	 */
	nodeBitsInHeader = 8 * (nodeSize - sizeof(BTNodeDescriptor)
					- sizeof(BTHeaderRec)
					- kBTreeHeaderUserBytes
					- (4 * sizeof(SInt16)));
	maximumClumpSectors = nodeBitsInHeader * sectorsPerNode;
	
	if ( maximumClumpSectors > limitClumpSectors )
		maximumClumpSectors = limitClumpSectors;

	/*
	 * For very large HFS volumes, the allocation block size might be larger than the arbitrary limit
	 * we set above.  Since we have to allocate at least one allocation block, then use that as the
	 * clump size.
	 * 
	 * Otherwise, we want to use about 1/128 of the volume, again subject to the above limit.
	 * To avoid arithmetic overflow, we continue to work with sectors.
	 * 
	 * But for very small volumes (less than 64K), we'll just use 4 allocation blocks.  And that
	 * will typically be 2KB.
	 */
	if ( sectorsPerBlock >= maximumClumpSectors )
	{
		clumpSectors = sectorsPerBlock;		/* for really large volumes just use one allocation block (HFS only) */
	}
	else
	{
		/*
		 * For large volumes, the default is 1/128 of the volume size, up to the maximumClumpSize
		 */
		if ( driveBlocks > 128 )
		{
			clumpSectors = (driveBlocks / 128);	/* the default is 1/128 of the volume size */
	
			if (clumpSectors > maximumClumpSectors)
				clumpSectors = maximumClumpSectors;
		}
		else
		{
			clumpSectors = sectorsPerBlock * 4;	/* for really small volumes (ie < 64K) */
		}
	}

	/*
	 * And we need to round up to something that is a multiple of both the node size and the allocation block size
	 * so that it will occupy a whole number of allocation blocks, and so a whole number of nodes will fit.
	 * 
	 * For HFS, the node size is always 512, and the allocation block size is always a multiple of 512.  For HFS
	 * Plus, both the node size and allocation block size are powers of 2.  So, it suffices to round up to whichever
	 * value is larger (since the larger value is always a multiple of the smaller value).
	 */

	if ( sectorsPerNode > sectorsPerBlock )
		clumpSectors = (clumpSectors / sectorsPerNode) * sectorsPerNode;	/* truncate to nearest node*/
	else
		clumpSectors = (clumpSectors / sectorsPerBlock) * sectorsPerBlock;	/* truncate to nearest node and allocation block */
	
	/* Finally, convert the clump size to bytes. */
	return clumpSectors << kLog2SectorSize;
}


#define CLUMP_ENTRIES	15

short clumptbl[CLUMP_ENTRIES * 2] = {
/*
 *	    Volume	 Catalog	 Extents
 *	     Size	Clump (MB)	Clump (MB)
 */
	/*   1GB */	  4,		 4,
	/*   2GB */	  6,		 4,
	/*   4GB */	  8,		 4,
	/*   8GB */	 11,		 5,
	/*  16GB */	 14,		 5,
	/*  32GB */	 19,		 6,
	/*  64GB */	 25,		 7,
	/* 128GB */	 34,		 8,
	/* 256GB */	 45,		 9,
	/* 512GB */	 60,		11,
	/*   1TB */	 80,		14,
	/*   2TB */	107,		16,
	/*   4TB */	144,		20,
	/*   8TB */	192,		25,
	/*  16TB */	256,		32
};

/*
 * CalcHFSPlusBTreeClumpSize
 *	
 * This routine calculates the file clump size for either
 * the catalog file or the extents overflow file.
 */
static UInt32
CalcHFSPlusBTreeClumpSize(UInt32 blockSize, UInt32 nodeSize, UInt64 sectors, int catalog)
{
	UInt32 mod = MAX(nodeSize, blockSize);
	UInt32 clumpSize;
	int i;

	/*
	 * The default clump size is 0.8% of the volume size. And
	 * it must also be a multiple of the node and block size.
	 */
	if (sectors < 0x200000) {
		clumpSize = sectors << 2;	/*  0.8 %  */
		if (clumpSize < (8 * nodeSize))
			clumpSize = 8 * nodeSize;
	} else {
		/* turn exponent into table index... */
		for (i = 0, sectors = sectors >> 22;
		     sectors && (i < CLUMP_ENTRIES-1);
		     ++i, sectors = sectors >> 1);
		
		if (catalog)
			clumpSize = clumptbl[0 + (i) * 2] * 1024 * 1024;
		else
			clumpSize = clumptbl[1 + (i) * 2] * 1024 * 1024;
	}
	
	/*
	 * Round the clump size to a multiple of node of node and block size.
	 * NOTE: This rounds down.
	 */
	clumpSize /= mod;
	clumpSize *= mod;
	
	/*
	 * Rounding down could have rounded down to 0 if the block size was
	 * greater than the clump size.  If so, just use one block.
	 */
	if (clumpSize == 0)
		clumpSize = mod;
		
	return (clumpSize);
}


/* VARARGS */
void
#if __STDC__
fatal(const char *fmt, ...)
#else
fatal(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (fcntl(STDERR_FILENO, F_GETFL) < 0) {
		openlog(progname, LOG_CONS, LOG_DAEMON);
		vsyslog(LOG_ERR, fmt, ap);
		closelog();
	} else {
		vwarnx(fmt, ap);
	}
	va_end(ap);
	exit(1);
	/* NOTREACHED */
}


void usage()
{
	fprintf(stderr, "usage: %s [-h | -w] [-N] [hfsplus-options] special-device\n", progname);

	fprintf(stderr, "  options:\n");
	fprintf(stderr, "\t-h create an HFS format filesystem (HFS Plus is the default)\n");
	fprintf(stderr, "\t-N do not create file system, just print out parameters\n");
	fprintf(stderr, "\t-s use case-sensitive filenames (default is case-insensitive)\n");
	fprintf(stderr, "\t-w add a HFS wrapper (i.e. Native Mac OS 9 bootable)\n");

	fprintf(stderr, "  where hfsplus-options are:\n");
	fprintf(stderr, "\t-J [journal-size] make this HFS+ volume journaled\n");
	fprintf(stderr, "\t-G group-id (for root directory)\n");
	fprintf(stderr, "\t-U user-id (for root directory)\n");
	fprintf(stderr, "\t-M access-mask (for root directory)\n");
	fprintf(stderr, "\t-b allocation block size (4096 optimal)\n");
	fprintf(stderr, "\t-c clump size list (comma separated)\n");
	fprintf(stderr, "\t\te=blocks (extents file)\n");
	fprintf(stderr, "\t\tc=blocks (catalog file)\n");
	fprintf(stderr, "\t\ta=blocks (attributes file)\n");
	fprintf(stderr, "\t\tb=blocks (bitmap file)\n");
	fprintf(stderr, "\t\td=blocks (user data fork)\n");
	fprintf(stderr, "\t\tr=blocks (user resource fork)\n");
	fprintf(stderr, "\t-i starting catalog node id\n");
	fprintf(stderr, "\t-n b-tree node size list (comma separated)\n");
	fprintf(stderr, "\t\te=size (extents b-tree)\n");
	fprintf(stderr, "\t\tc=size (catalog b-tree)\n");
	fprintf(stderr, "\t\ta=size (attributes b-tree)\n");
	fprintf(stderr, "\t-v volume name (in ascii or UTF-8)\n");

	fprintf(stderr, "  examples:\n");
	fprintf(stderr, "\t%s -v Untitled /dev/rdisk0s7 \n", progname);
	fprintf(stderr, "\t%s -v Untitled -n c=4096,e=1024 /dev/rdisk0s7 \n", progname);
	fprintf(stderr, "\t%s -w -v Untitled -c b=64,c=1024 /dev/rdisk0s7 \n\n", progname);

	exit(1);
}
