/*
 * Samba, configurable PMDA
 *
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

typedef int BOOL;

#define IRIX 1

#include <stdio.h>
#include <sys/shm.h>
#include <pcp/pmapi.h>
#ifdef IRIX
#include <pcp/impl.h>
#endif
#include <pcp/pmda.h>
#include "domain.h"
#include "profile.h"
#include "metrics.h"

static pmdaInstid *counttime = NULL;
static pmdaInstid *bytes = NULL;

/*
 * List of instance domains
 */

static pmdaIndom indomtab[] = {
	{COUNT_TIME_INDOM,0,NULL},
	{BYTE_INDOM,0,NULL}
};
/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
/* smbd.smb_count */
    { NULL, { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* smbd.uid_changes */
    { NULL, { PMDA_PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* statcache.lookups */
    { NULL, { PMDA_PMID(1,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* statcache.misses */
    { NULL, { PMDA_PMID(1,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* statcache.hits */
    { NULL, { PMDA_PMID(1,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.num_caches */
    { NULL, { PMDA_PMID(2,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.allocated_caches */
    { NULL, { PMDA_PMID(2,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.read_hits */
    { NULL, { PMDA_PMID(2,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.total_writes */
    { NULL, { PMDA_PMID(2,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.init_writes */
    { NULL, { PMDA_PMID(2,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.abutted_writes */
    { NULL, { PMDA_PMID(2,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.perfect_writes */
    { NULL, { PMDA_PMID(2,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.direct_writes */
    { NULL, { PMDA_PMID(2,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.non_oplock_writes */
    { NULL, { PMDA_PMID(2,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.seek_flush */
    { NULL, { PMDA_PMID(2,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.read_flush */
    { NULL, { PMDA_PMID(2,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.write_flush */
    { NULL, { PMDA_PMID(2,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.readraw_flush */
    { NULL, { PMDA_PMID(2,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.oplock_rel_flush */
    { NULL, { PMDA_PMID(2,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.close_flush */
    { NULL, { PMDA_PMID(2,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.sync_flush */
    { NULL, { PMDA_PMID(2,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* writecache.size_change_flush */
    { NULL, { PMDA_PMID(2,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* counts instance domain */
    { NULL, { PMDA_PMID(3,0), PM_TYPE_U32, COUNT_TIME_INDOM, PM_SEM_COUNTER, 
		{ 0,0,1,0,0,PM_COUNT_ONE} }, },
/* times instance domain */
    { NULL, { PMDA_PMID(4,0), PM_TYPE_U32, COUNT_TIME_INDOM, PM_SEM_COUNTER, 
		{ 0,1,0,0,PM_TIME_USEC,0} }, },
/* bytes instance domain */
    { NULL, { PMDA_PMID(5,0), PM_TYPE_U32, BYTE_INDOM, PM_SEM_COUNTER, 
		{ 1,0,0,PM_SPACE_BYTE,0,0} }, }

};

extern int	errno;
struct profile_stats	*stats;
struct profile_header	*shmheader;
int		shmid = -1;


int
samba_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    

    if (inst != PM_IN_NULL && mdesc->m_desc.indom == PM_INDOM_NULL)
	return PM_ERR_INST;

    if (idp->cluster == 0) {
	switch (idp->item) {
	    case 0:			/* smbd.smb_count */
		atom->ul = stats->smb_count;
		break;
	    case 1:			/* smb.uid_changes */
		atom->ul = stats->uid_changes;
		break;
	    default:
		return PM_ERR_PMID;
	}
    }
    else if (idp->cluster == 1) {	/* statcache */
	switch (idp->item) {
	    case 0:			/* statcache.lookups */
		atom->ul = stats->statcache_lookups;
		break;
	    case 1:			/* statcache.misses */
		atom->ul = stats->statcache_misses;
		break;
	    case 2:			/* statcache.hits */
		atom->ul = stats->statcache_hits;
		break;
	    default:
		return PM_ERR_PMID;
	}
    }
    else if (idp->cluster == 2) {	/* writecache */
	switch (idp->item) {
	    case 0:			/* writecache.num_caches */
		atom->ul = stats->writecache_num_write_caches;
		break;
	    case 1:			/* writecache.allocated_caches */
		atom->ul = stats->writecache_allocated_write_caches;
		break;
	    case 2:			/* writecache.read_hits */
		atom->ul = stats->writecache_read_hits;
		break;
	    case 3:			/* writecache.total_writes */
		atom->ul = stats->writecache_total_writes;
		break;
	    case 4:			/* writecache.init_writes */
		atom->ul = stats->writecache_init_writes;
		break;
	    case 5:			/* writecache.abutted_writes */
		atom->ul = stats->writecache_abutted_writes;
		break;
	    case 6:			/* writecache.perfect_writes */
		atom->ul = stats->writecache_num_perfect_writes;
		break;
	    case 7:			/* writecache.direct_writes */
		atom->ul = stats->writecache_direct_writes;
		break;
	    case 8:			/* writecache.non_oplock_writes */
		atom->ul = stats->writecache_non_oplock_writes;
		break;
	    case 9:			/* writecache.seek_flush */
		atom->ul = stats->writecache_flushed_writes[SEEK_FLUSH];
		break;
	    case 10:			/* writecache.read_flush */
		atom->ul = stats->writecache_flushed_writes[READ_FLUSH];
		break;
	    case 11:			/* writecache.write_flush */
		atom->ul = stats->writecache_flushed_writes[WRITE_FLUSH];
		break;
	    case 12:			/* writecache.readraw_flush */
		atom->ul = stats->writecache_flushed_writes[READRAW_FLUSH];
		break;
	    case 13:			/* writecache.oplock_rel_flush */
		atom->ul = stats->writecache_flushed_writes[OPLOCK_RELEASE_FLUSH];
		break;
	    case 14:			/* writecache.close_flush */
		atom->ul = stats->writecache_flushed_writes[CLOSE_FLUSH];
		break;
	    case 15:			/* writecache.sync_flush */
		atom->ul = stats->writecache_flushed_writes[SYNC_FLUSH];
		break;
	    case 16:			/* writecache.size_change_flush */
		atom->ul = stats->writecache_flushed_writes[SIZECHANGE_FLUSH];
		break;
	    default:
		return PM_ERR_PMID;
	}
    }
    else if (idp->cluster == 3) {	/* counts */
	if (idp->item == 0) {
	    if (inst < indomtab[COUNT_TIME_INDOM].it_numinst) {
		unsigned *p;

		p = (unsigned *)((unsigned)stats + samba_counts[inst].offset);
		atom->ul = *p;
	    }
	    else
		return PM_ERR_INST;
	}
	else
	    return PM_ERR_PMID;
    }
    else if (idp->cluster == 4) {	/* times */
	if (idp->item == 0) {
	    if (inst < indomtab[COUNT_TIME_INDOM].it_numinst) {
		unsigned *p;

		p = (unsigned *)((unsigned)stats + samba_times[inst].offset);
		atom->ul = *p;
	    }
	    else
		return PM_ERR_INST;
	}
	else
	    return PM_ERR_PMID;
    }
    else if (idp->cluster == 5) {	/* bytes */
	if (idp->item == 0) {
	    if (inst < indomtab[BYTE_INDOM].it_numinst) {
		unsigned *p;

		p = (unsigned *)((unsigned)stats + samba_bytes[inst].offset);
		atom->ul = *p;
	    }
	    else
		return PM_ERR_INST;
	}
	else
	    return PM_ERR_PMID;
    }
    else
	return PM_ERR_PMID;
    return 0;
}


void 
samba_init(pmdaInterface *dp)
{
    int inst_count, i;

    if (dp->status != 0)
	return;

    if ((shmid = shmget(PROF_SHMEM_KEY, 0, 0)) == -1) {
	fprintf(stderr, "shmid: %s\n", strerror(errno));
	fprintf(stderr, "samba not compiled with profile support or not running\n");
	exit(1);
    }
    shmheader = (struct profile_header *)shmat(shmid, NULL, SHM_RDONLY);
    if ((int)shmheader == -1) {
	fprintf(stderr, "shmat: %s\n", strerror(errno));
	exit(1);
    }

/*
 * Initialize lists of instances
 */

    inst_count = sizeof(samba_counts)/sizeof(samba_counts[0]);
    counttime = (pmdaInstid *)malloc(inst_count * sizeof(pmdaInstid));
    if (counttime == NULL) {
	__pmNoMem("count&time",inst_count * sizeof(pmdaInstid),PM_FATAL_ERR);
	/* NOTREACHED*/
    }
    for (i = 0; i < inst_count; i++) {
	counttime[i].i_inst = i;
	counttime[i].i_name = samba_counts[i].name;
    }
    indomtab[COUNT_TIME_INDOM].it_numinst = inst_count;
    indomtab[COUNT_TIME_INDOM].it_set = counttime;

    inst_count = sizeof(samba_bytes)/sizeof(samba_bytes[0]);
    bytes = (pmdaInstid *)malloc(inst_count * sizeof(pmdaInstid));
    if (bytes == NULL) {
	__pmNoMem("bytes",inst_count * sizeof(pmdaInstid),PM_FATAL_ERR);
	/* NOTREACHED*/
    }
    for (i = 0; i < inst_count; i++) {
	bytes[i].i_inst = i;
	bytes[i].i_name = samba_bytes[i].name;
    }
    indomtab[BYTE_INDOM].it_numinst = inst_count;
    indomtab[BYTE_INDOM].it_set = bytes;


    pmdaSetFetchCallBack(dp, samba_fetchCallBack);
    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

    /* validate the data */
    if (!shmheader)	/* not mapped yet */
	fprintf(stderr, "samba_init: shmem not mapped\n");
    else if (shmheader->prof_shm_magic != PROF_SHM_MAGIC)
	fprintf(stderr, "samba_init: bad magic\n");
    else if (shmheader->prof_shm_version != PROF_SHM_VERSION)
	fprintf(stderr, "samba_init: bad version %X\n",
			shmheader->prof_shm_version);
    else {
	stats = &shmheader->stats;
	return;		/* looks OK */
    }
    exit(1);
}


int
main(int argc, char **argv)
{
    int			err = 0;
    char		*p;
    pmdaInterface	dispatch;

    for (p = pmProgname = argv[0]; *p; p++)
	if (*p == '/') pmProgname = p+1;

    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmProgname, SAMBA,
		"samba.log", "/var/pcp/pmdas/samba/help");

    if (pmdaGetOpt(argc, argv, "D:d:l:?", &dispatch, &err) != EOF) {
	fprintf(stderr, "Usage: %s [options]\n\n\
Options:\n\
  -d domain    use domain (numeric) for metrics domain of PMDA\n\
  -l logfile   write log into logfile rather than using default log name\n",
	pmProgname);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    samba_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
    /*NOTREACHED*/
}
