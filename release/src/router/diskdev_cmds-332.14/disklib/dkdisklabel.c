/* Copyright 1999 Apple Computer, Inc.
 *
 * Generate a bsd disk label routine.
 *    Input: open file descriptor to the device node, and
 *           pointer to a disk label structure to fill in
 *    Return: errno status
 *
 * HISTORY
 *
 * 24 Feb 1999 D. Markarian at Apple
 *      Created.
 */

#include <string.h>              /* memset */
#include <sys/types.h>           /* sys/disklabel.h */
#include <sys/param.h>           /* NBPG */
#include <ufs/ufs/dinode.h>      /* ufs/ffs/fs.h */
#include <ufs/ffs/fs.h>          /* BBSIZE, SBSIZE */

#include <sys/disk.h>            /* DKIOCGETBLOCKSIZE ioctl */

#include <sys/disklabel.h>       /* struct disklabel */

u_short dkcksum __P((register struct disklabel *lp));
int dkdisklabelregenerate __P((int fd, struct disklabel * lp, int newblksize));

/*
 * The following two constants set the default block and fragment sizes.
 * Both constants must be a power of 2 and meet the following constraints:
 *	MINBSIZE <= GENBLKSIZE <= MAXBSIZE
 *	sectorsize <= GENFRAGSIZE <= GENBLKSIZE
 *	GENBLKSIZE / GENFRAGSIZE <= 8
 */
#define	GENFRAGSIZE   1024
#define	GENBLKSIZE    NBPG  /* 4096 */

/*
 * Cylinder groups may have up to many cylinders. The actual
 * number used depends upon how much information can be stored
 * on a single cylinder. The default is to use 16 cylinders
 * per group.
 */
#define	GENCPG        16

/*
 * Interleave is physical sector interleave, set up by the
 * formatter or controller when formatting.  When interleaving is
 * in use, logically adjacent sectors are not physically
 * contiguous, but instead are separated by some number of
 * sectors.  It is specified as the ratio of physical sectors
 * traversed per logical sector.  Thus an interleave of 1:1
 * implies contiguous layout, while 2:1 implies that logical
 * sector 0 is separated by one sector from logical sector 1.
 */
#define	GENINTERLEAVE 1

/*
 * Rotational speed; # of data sectors per track. 
 */
#define GENRPM        3600
#define GENNSECTORS   32

int dkdisklabel(int fd, struct disklabel * lp)
{
    return dkdisklabelregenerate(fd, lp, 0);
}

int dkdisklabelregenerate(int fd, struct disklabel * lp, int newblksize)
{
    /*
     * Generate a bsd-style disk label for the specified device node.
     */

    int blksize;
    int error;
    int index;
    long long numblks;

    /* obtain the size of the media (in blocks) */
    if ( (error = ioctl(fd, DKIOCGETBLOCKCOUNT, &numblks)) < 0 )
        return(error);

    /* obtain the block size of the media */
    if ( (error = ioctl(fd, DKIOCGETBLOCKSIZE, &blksize)) < 0 )
        return(error);

    /* adjust the size of the media with newblksize should it be specified */
    if (newblksize)
    {
        numblks = (int)((((long long) numblks) * blksize) / newblksize);
        blksize = newblksize;
    }

    /*
     * clear the disk label structure and then fill in the appropriate fields;
     * we comment out lines that are initializations to zero with //, since it
     * is redundant work
     */
    memset(lp, 0, sizeof(struct disklabel));

    lp->d_magic       = DISKMAGIC;        /* the magic number */
//  lp->d_type        = 0;                /* drive type */
//  lp->d_subtype     = 0;                /* controller/d_type specific */
//  lp->d_typename[0] = 0;                /* type name, e.g. "eagle" */
    /* 
     * d_packname contains the pack identifier and is returned when
     * the disklabel is read off the disk or in-core copy.
     * d_boot0 and d_boot1 are the (optional) names of the
     * primary (block 0) and secondary (block 1-15) bootstraps
     * as found in /usr/mdec.  These are returned when using
     * getdiskbyname(3) to retrieve the values from /etc/disktab.
     */
//  lp->d_packname[0] = 0;                /* pack identifier */

    /* disk geometry: */
    lp->d_secsize        = blksize;       /* # of bytes per sector */
    lp->d_nsectors       = GENNSECTORS;   /* # of data sectors per track */
                                          /* # of tracks per cylinder */
    if (numblks < 8*32*1024)              /* <=528.4 MB */
        lp->d_ntracks = 16;
    else if (numblks < 16*32*1024)        /* <=1.057 GB */
        lp->d_ntracks = 32;
    else if (numblks < 32*32*1024)        /* <=2.114 GB */
        lp->d_ntracks = 54;
    else if (numblks < 64*32*1024)        /* <=4.228 GB */
        lp->d_ntracks = 128;
    else                                  /* > 4.228 GB */
        lp->d_ntracks = 255;
                                          /* # of data cylinders per unit */
    lp->d_ncylinders     = numblks / lp->d_ntracks / lp->d_nsectors;
	                                  /* # of data sectors per cylinder */
    lp->d_secpercyl      = lp->d_nsectors * lp->d_ntracks;
    lp->d_secperunit     = numblks;       /* # of data sectors per unit */
    /*
     * Spares (bad sector replacements) below are not counted in
     * d_nsectors or d_secpercyl.  Spare sectors are assumed to
     * be physical sectors which occupy space at the end of each
     * track and/or cylinder.
     */
//  lp->d_sparespertrack = 0;             /* # of spare sectors per track */
//  lp->d_sparespercyl   = 0;             /* # of data sectors per unit */
    /*
     * Alternate cylinders include maintenance, replacement, configuration
     * description areas, etc.
     */
//  lp->d_acylinders = 0;                 /* # of alt. cylinders per unit */

    /* hardware characteristics: */
    /*
     * d_interleave, d_trackskew and d_cylskew describe perturbations
     * in the media format used to compensate for a slow controller.
     * Interleave is physical sector interleave, set up by the
     * formatter or controller when formatting.  When interleaving is
     * in use, logically adjacent sectors are not physically
     * contiguous, but instead are separated by some number of
     * sectors.  It is specified as the ratio of physical sectors
     * traversed per logical sector.  Thus an interleave of 1:1
     * implies contiguous layout, while 2:1 implies that logical
     * sector 0 is separated by one sector from logical sector 1.
     * d_trackskew is the offset of sector 0 on track N relative to
     * sector 0 on track N-1 on the same cylinder.  Finally, d_cylskew
     * is the offset of sector 0 on cylinder N relative to sector 0
     * on cylinder N-1.
     */
    lp->d_rpm            = GENRPM;        /* rotational speed */
    lp->d_interleave     = GENINTERLEAVE; /* hardware sector interleave */
//  lp->d_trackskew      = 0;             /* sector 0 skew, per track */
//  lp->d_cylskew        = 0;             /* sector 0 skew, per cylinder */
//  lp->d_headswitch     = 0;             /* head switch time, usec */
//  lp->d_trkseek        = 0;             /* track-to-track seek, usec */
//  lp->d_flags          = 0;             /* generic flags */
//  lp->d_drivedata[0-4] = 0;             /* drive-type specific information */
//  lp->d_spare[0-4]     = 0;             /* reserved for future use */
    lp->d_magic2         = DISKMAGIC;     /* the magic number (again) */
//  lp->d_checksum       = 0;             /* xor of data incl. partitions */

    /* filesystem and partition information: */
    lp->d_npartitions    = MAXPARTITIONS; /* number of partitions */
    lp->d_bbsize         = BBSIZE;        /* size of boot area at sn0, bytes */
    lp->d_sbsize         = SBSIZE;        /* max size of fs superblock, bytes */

    for (index = 0; index < MAXPARTITIONS; index++)
    {
        struct partition * pp = &(lp->d_partitions[index]);
        pp->p_size   = numblks;                         /* number of sectors */
//      pp->p_offset = 0;                               /* starting sector */
        pp->p_fsize  = MAX(GENFRAGSIZE, blksize);       /* fs fragment size */
        pp->p_fstype = FS_BSDFFS;                       /* fs type */
        pp->p_frag   = MIN(8, GENBLKSIZE / pp->p_fsize);/* fs fragments/block */
        pp->p_cpg    = GENCPG;                          /* fs cylinders/group */
    }

    /* compute a checksum on the resulting structure */
    lp->d_checksum = dkcksum(lp);

    return 0; /* success */
}
