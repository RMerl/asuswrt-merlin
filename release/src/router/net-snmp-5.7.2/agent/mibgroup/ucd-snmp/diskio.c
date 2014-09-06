/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>

/*
 * needed by util_funcs.h 
 */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <math.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "util_funcs/header_simple_table.h"

/*
 * include our .h file 
 */
#include "diskio.h"

#define CACHE_TIMEOUT 1
static time_t   cache_time = 0;

#ifdef solaris2
#include <kstat.h>

#define MAX_DISKS 128

static kstat_ctl_t *kc;
static kstat_t *ksp;
static kstat_io_t kio;
static int      cache_disknr = -1;
#endif                          /* solaris2 */

#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
/*
 * handle disk statistics via libperfstat
 */
#ifdef HAVE_SYS_PROTOSW_H
#include <sys/protosw.h>
#endif
#include <libperfstat.h>
static perfstat_disk_t *ps_disk;	/* storage for all disk values */
static int ps_numdisks;			/* number of disks in system, may change while running */
#endif

#if defined(bsdi3) || defined(bsdi4)
#include <string.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/diskstats.h>
#endif                          /* bsdi */

#if defined (freebsd4) || defined(freebsd5)
#include <sys/param.h>
#if __FreeBSD_version >= 500101
#include <sys/resource.h>       /* for CPUSTATES in devstat.h */
#elif !defined(dragonfly)
#include <sys/dkstat.h>
#endif
#include <devstat.h>
#include <net-snmp/utilities.h>

#include <math.h>
/* sampling interval, in seconds */
#define DISKIO_SAMPLE_INTERVAL 5

#endif                          /* freebsd */

#if defined(freebsd5) && __FreeBSD_version >= 500107
  #define GETDEVS(x) devstat_getdevs(NULL, (x))
#else
  #define GETDEVS(x) getdevs((x))
#endif

#if defined (linux)
#define DISKIO_SAMPLE_INTERVAL 5
void devla_getstats(unsigned int regno, void * dummy);
#endif /* linux */

#if defined (darwin)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>

static mach_port_t masterPort;		/* to communicate with I/O Kit	*/
#endif                          /* darwin */

#ifndef solaris2
static int      getstats(void);
#endif

#if defined (freebsd4) || defined(freebsd5)
void		devla_getstats(unsigned int regno, void *dummy);
#endif

FILE           *file;

#if 0
static void	diskio_free_config(void);
#endif

         /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/


/*
 * this is an optional function called at the time the agent starts up
 * to do any initilizations you might require.  You don't have to
 * create it, as it is optional. 
 */

/*
 * IMPORTANT: If you add or remove this function, you *must* re-run
 * the configure script as it checks for its existance. 
 */

void
init_diskio(void)
{
    /*
     * Define a 'variable' structure that is a representation of our mib. 
     */

    /*
     * first, we have to pick the variable type.  They are all defined in
     * the var_struct.h file in the agent subdirectory.  I'm picking the
     * variable2 structure since the longest sub-component of the oid I
     * want to load is .2.1 and .2.2 so I need at most 2 spaces in the
     * last entry. 
     */

    struct variable2 diskio_variables[] = {
        {DISKIO_INDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {1}},
        {DISKIO_DEVICE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {2}},
        {DISKIO_NREAD, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {3}},
        {DISKIO_NWRITTEN, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {4}},
        {DISKIO_READS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {5}},
        {DISKIO_WRITES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {6}},
#if defined(freebsd4) || defined(freebsd5) || defined(linux)
        {DISKIO_LA1, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {9}},
        {DISKIO_LA5, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {10}},
        {DISKIO_LA15, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {11}},
#endif
        {DISKIO_NREADX, ASN_COUNTER64, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {12}},
        {DISKIO_NWRITTENX, ASN_COUNTER64, NETSNMP_OLDAPI_RONLY,
         var_diskio, 1, {13}},
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath. 
     */
    oid             diskio_variables_oid[] =
        { 1, 3, 6, 1, 4, 1, 2021, 13, 15, 1, 1 };

    /*
     * register ourselves with the agent to handle our mib tree
     * 
     * This is a macro defined in ../../snmp_vars.h.  The arguments are:
     * 
     * descr:   A short description of the mib group being loaded.
     * var:     The variable structure to load.
     * vartype: The variable structure used to define it (variable2, variable4, ...)
     * theoid:  A *initialized* *exact length* oid pointer.
     * (sizeof(theoid) *must* return the number of elements!)  
     */
    REGISTER_MIB("diskio", diskio_variables, variable2,
                 diskio_variables_oid);

#ifdef solaris2
    kc = kstat_open();

    if (kc == NULL)
        snmp_log(LOG_ERR, "diskio: Couldn't open kstat\n");
#endif

#ifdef darwin
    /*
     * Get the I/O Kit communication handle.
     */
    IOMasterPort(bootstrap_port, &masterPort);
#endif

#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    /*
     * initialize values to gather information on first request
     */
    ps_numdisks = 0;
    ps_disk = NULL;
#endif

#if defined (freebsd4) || defined(freebsd5) || defined(linux)
    devla_getstats(0, NULL);
    /* collect LA data regularly */
    snmp_alarm_register(DISKIO_SAMPLE_INTERVAL, SA_REPEAT, devla_getstats, NULL);
#endif


#ifdef linux
    char *app = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                      NETSNMP_DS_LIB_APPTYPE);
    netsnmp_ds_register_config(ASN_BOOLEAN, app, "diskio_exclude_fd",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DISKIO_NO_FD);
    netsnmp_ds_register_config(ASN_BOOLEAN, app, "diskio_exclude_loop",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DISKIO_NO_LOOP);
    netsnmp_ds_register_config(ASN_BOOLEAN, app, "diskio_exclude_ram",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DISKIO_NO_RAM);

        /* or possible an exclusion pattern? */
#endif
}

#if 0
/* to do: make sure diskio_free_config() gets invoked upon SIGHUP. */
static void
diskio_free_config(void)
{
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
			   NETSNMP_DS_AGENT_DISKIO_NO_FD,   0);
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
			   NETSNMP_DS_AGENT_DISKIO_NO_LOOP, 0);
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
			   NETSNMP_DS_AGENT_DISKIO_NO_RAM,  0);
}
#endif


#ifdef solaris2
int
get_disk(int disknr)
{
    time_t          now;
    int             i = 0;
    kstat_t *tksp;

    now = time(NULL);
    if (disknr == cache_disknr && cache_time + CACHE_TIMEOUT > now) {
        return 1;
    }

    /*
     * could be optimiced by checking if cache_disknr<=disknr
     * if so, just reread the data - not going through the whole chain
     * from kc->kc_chain 
     */

    for (tksp = kc->kc_chain; tksp != NULL; tksp = tksp->ks_next) {
        if (tksp->ks_type == KSTAT_TYPE_IO
            && !strcmp(tksp->ks_class, "disk")) {
            if (i == disknr) {
                if (kstat_read(kc, tksp, &kio) == -1)
                    snmp_log(LOG_ERR, "diskio: kstat_read failed\n");
		ksp = tksp;
                cache_time = now;
                cache_disknr = disknr;
                return 1;
            } else {
                i++;
            }
        }
    }
    return 0;
}


u_char         *
var_diskio(struct variable * vp,
           oid * name,
           size_t * length,
           int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * define any variables we might return as static! 
     */
    static long     long_ret;
    static struct counter64 c64_ret;

    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, MAX_DISKS))
        return NULL;


    if (get_disk(name[*length - 1] - 1) == 0)
        return NULL;


    /*
     * We can now simply test on vp's magic number, defined in diskio.h 
     */
    switch (vp->magic) {
    case DISKIO_INDEX:
        long_ret = (long) name[*length - 1];
        return (u_char *) & long_ret;
    case DISKIO_DEVICE:
        *var_len = strlen(ksp->ks_name);
        return (u_char *) ksp->ks_name;
    case DISKIO_NREAD:
        long_ret = (uint32_t) kio.nread;
        return (u_char *) & long_ret;
    case DISKIO_NWRITTEN:
        long_ret = (uint32_t) kio.nwritten;
        return (u_char *) & long_ret;
    case DISKIO_NREADX:
        *var_len = sizeof(struct counter64);
        c64_ret.low = kio.nread & 0xffffffff;;
        c64_ret.high = kio.nread >> 32;
        return (u_char *) & c64_ret;
    case DISKIO_NWRITTENX:
        *var_len = sizeof(struct counter64);
        c64_ret.low = kio.nwritten & 0xffffffff;;
        c64_ret.high = kio.nwritten >> 32;
        return (u_char *) & c64_ret;
    case DISKIO_READS:
        long_ret = (uint32_t) kio.reads;
        return (u_char *) & long_ret;
    case DISKIO_WRITES:
        long_ret = (uint32_t) kio.writes;
        return (u_char *) & long_ret;

    default:
        ERROR_MSG("diskio.c: don't know how to handle this request.");
    }
    /*
     * if we fall to here, fail by returning NULL 
     */
    return NULL;
}
#endif                          /* solaris2 */

#if defined(bsdi3) || defined(bsdi4)
static int      ndisk;
static struct diskstats *dk;
static char   **dkname;

static int
getstats(void)
{
    time_t          now;
    int             mib[2];
    char           *t, *tp;
    size_t          size, dkn_size;
    int             i;

    now = time(NULL);
    if (cache_time + CACHE_TIMEOUT > now) {
        return 1;
    }
    mib[0] = CTL_HW;
    mib[1] = HW_DISKSTATS;
    size = 0;
    if (sysctl(mib, 2, NULL, &size, NULL, 0) < 0) {
        perror("Can't get size of HW_DISKSTATS mib");
        return 0;
    }
    if (ndisk != size / sizeof(*dk)) {
        if (dk)
            free(dk);
        if (dkname) {
            for (i = 0; i < ndisk; i++)
                if (dkname[i])
                    free(dkname[i]);
            free(dkname);
        }
        ndisk = size / sizeof(*dk);
        if (ndisk == 0)
            return 0;
        dkname = malloc(ndisk * sizeof(char *));
        mib[0] = CTL_HW;
        mib[1] = HW_DISKNAMES;
        if (sysctl(mib, 2, NULL, &dkn_size, NULL, 0) < 0) {
            perror("Can't get size of HW_DISKNAMES mib");
            return 0;
        }
        tp = t = malloc(dkn_size);
        if (sysctl(mib, 2, t, &dkn_size, NULL, 0) < 0) {
            perror("Can't get size of HW_DISKNAMES mib");
            return 0;
        }
        for (i = 0; i < ndisk; i++) {
            dkname[i] = strdup(tp);
            tp += strlen(tp) + 1;
        }
        free(t);
        dk = malloc(ndisk * sizeof(*dk));
    }
    mib[0] = CTL_HW;
    mib[1] = HW_DISKSTATS;
    if (sysctl(mib, 2, dk, &size, NULL, 0) < 0) {
        perror("Can't get HW_DISKSTATS mib");
        return 0;
    }
    cache_time = now;
    return 1;
}

u_char         *
var_diskio(struct variable * vp,
           oid * name,
           size_t * length,
           int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    unsigned int    indx;

    if (getstats() == 0)
        return 0;

    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, ndisk))
        return NULL;

    indx = (unsigned int) (name[*length - 1] - 1);
    if (indx >= ndisk)
        return NULL;

    switch (vp->magic) {
    case DISKIO_INDEX:
        long_ret = (long) indx + 1;
        return (u_char *) & long_ret;
    case DISKIO_DEVICE:
        *var_len = strlen(dkname[indx]);
        return (u_char *) dkname[indx];
    case DISKIO_NREAD:
        long_ret =
            (signed long) (dk[indx].dk_sectors * dk[indx].dk_secsize);
        return (u_char *) & long_ret;
    case DISKIO_NWRITTEN:
        return NULL;            /* Sigh... BSD doesn't keep seperate track */
    case DISKIO_READS:
        long_ret = (signed long) dk[indx].dk_xfers;
        return (u_char *) & long_ret;
    case DISKIO_WRITES:
        return NULL;            /* Sigh... BSD doesn't keep seperate track */

    default:
        ERROR_MSG("diskio.c: don't know how to handle this request.");
    }
    return NULL;
}
#endif                          /* bsdi */

#ifdef __NetBSD__
#include <sys/sysctl.h>
static int      ndisk;
#ifdef HW_IOSTATNAMES
static int nmib[2] = {CTL_HW, HW_IOSTATNAMES};
#else
static int nmib[2] = {CTL_HW, HW_DISKNAMES};
#endif
#ifdef HW_DISKSTATS
#include <sys/disk.h>
static int dmib[3] = {CTL_HW, HW_DISKSTATS, sizeof(struct disk_sysctl)};
static struct disk_sysctl *dk;
#endif
#ifdef HW_IOSTATS
#include <sys/iostat.h>
static int dmib[3] = {CTL_HW, HW_IOSTATS, sizeof(struct io_sysctl)};
static struct io_sysctl *dk;
#endif
static char   **dkname;

static int
getstats(void)
{
    time_t          now;
    char           *t, *tp;
    size_t          size, dkn_size;
    int             i;

    now = time(NULL);
    if (cache_time + CACHE_TIMEOUT > now) {
        return 1;
    }
    size = 0;
    if (sysctl(dmib, 3, NULL, &size, NULL, 0) < 0) {
        perror("Can't get size of HW_DISKSTATS/HW_IOSTATS mib");
        return 0;
    }
    if (ndisk != size / dmib[2]) {
        if (dk)
            free(dk);
        if (dkname) {
            for (i = 0; i < ndisk; i++)
                if (dkname[i])
                    free(dkname[i]);
            free(dkname);
        }
        ndisk = size / dmib[2];
        if (ndisk == 0)
            return 0;
        dkname = malloc(ndisk * sizeof(char *));
        dkn_size = 0;
        if (sysctl(nmib, 2, NULL, &dkn_size, NULL, 0) < 0) {
            perror("Can't get size of HW_DISKNAMES mib");
            return 0;
        }
        t = malloc(dkn_size);
        if (sysctl(nmib, 2, t, &dkn_size, NULL, 0) < 0) {
            perror("Can't get size of HW_DISKNAMES mib");
            return 0;
        }
        for (i = 0, tp = strtok(t, " "); tp && i < ndisk; i++,
	    tp = strtok(NULL, " ")) {
            dkname[i] = strdup(tp);
        }
        free(t);
        dk = malloc(ndisk * sizeof(*dk));
    }
    if (sysctl(dmib, 3, dk, &size, NULL, 0) < 0) {
        perror("Can't get HW_DISKSTATS/HW_IOSTATS mib");
        return 0;
    }
    cache_time = now;
    return 1;
}

u_char *
var_diskio(struct variable * vp,
           oid * name,
           size_t * length,
           int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    unsigned int    indx;

    if (getstats() == 0)
        return 0;

    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, ndisk))
        return NULL;

    indx = (unsigned int) (name[*length - 1] - 1);
    if (indx >= ndisk)
        return NULL;

    switch (vp->magic) {
    case DISKIO_INDEX:
        long_ret = (long) indx + 1;
        return (u_char *) & long_ret;

    case DISKIO_DEVICE:
        *var_len = strlen(dkname[indx]);
        return (u_char *) dkname[indx];

    case DISKIO_NREAD:
#ifdef HW_DISKSTATS
     	long_ret = dk[indx].dk_rbytes;
#endif
#ifdef HW_IOSTATS
	if (dk[indx].type == IOSTAT_DISK)
	    long_ret = dk[indx].rbytes;
#endif
        return (u_char *) & long_ret;

    case DISKIO_NWRITTEN:
#ifdef HW_DISKSTATS
     	long_ret = dk[indx].dk_wbytes;
#endif
#ifdef HW_IOSTATS
	if (dk[indx].type == IOSTAT_DISK)
	    long_ret = dk[indx].wbytes;
#endif
        return (u_char *) & long_ret;

    case DISKIO_READS:
#ifdef HW_DISKSTATS
     	long_ret = dk[indx].dk_rxfer;
#endif
#ifdef HW_IOSTATS
	if (dk[indx].type == IOSTAT_DISK)
	    long_ret = dk[indx].rxfer;
#endif
        return (u_char *) & long_ret;

    case DISKIO_WRITES:
#ifdef HW_DISKSTATS
     	long_ret = dk[indx].dk_wxfer;
#endif
#ifdef HW_IOSTATS
	if (dk[indx].type == IOSTAT_DISK)
	    long_ret = dk[indx].wxfer;
#endif
        return (u_char *) & long_ret;

    default:
        ERROR_MSG("diskio.c: don't know how to handle this request.");
    }
    return NULL;
}
#endif /* __NetBSD__ */

#if defined(freebsd4) || defined(freebsd5)

/* disk load average patch by Rojer */

struct dev_la {
#if ( defined(freebsd5) && __FreeBSD_version >= 500107 )
        struct bintime prev;
#else
        struct timeval prev;
#endif
        double la1,la5,la15;
        char name[DEVSTAT_NAME_LEN+5];
        };

static struct dev_la *devloads = NULL;
static int ndevs = 0;

#if ! ( defined(freebsd5) && __FreeBSD_version >= 500107 )
double devla_timeval_diff(struct timeval *t1, struct timeval *t2) {

        double dt1 = (double) t1->tv_sec + (double) t1->tv_usec * 0.000001;
        double dt2 = (double) t2->tv_sec + (double) t2->tv_usec * 0.000001;

        return dt2-dt1;

        }
#endif

void devla_getstats(unsigned int regno, void *dummy) {

        static struct statinfo *lastat = NULL;
        int i;
        double busy_time, busy_percent;
        static double expon1, expon5, expon15;
        char current_name[DEVSTAT_NAME_LEN+5];

	if (lastat == NULL) {
	    lastat = (struct statinfo *) malloc(sizeof(struct statinfo));
	    if (lastat != NULL)
		lastat->dinfo = (struct devinfo *) calloc(sizeof(struct devinfo), 1);
	    if (lastat == NULL || lastat->dinfo == NULL) {
		    SNMP_FREE(lastat);
		    ERROR_MSG("Memory alloc failure - devla_getstats()\n");
		    return;
	    }
	}

        if ((GETDEVS(lastat)) == -1) {
                ERROR_MSG("can't do getdevs()\n");
                return;
                }

        if (ndevs != 0) {
                for (i=0; i < ndevs; i++) {
                        snprintf(current_name, sizeof(current_name), "%s%d",
                                lastat->dinfo->devices[i].device_name, lastat->dinfo->devices[i].unit_number);
                        if (strcmp(current_name, devloads[i].name)) {
                                ndevs = 0;
                                free(devloads);
                                }
                        }
                }

        if (ndevs == 0) {
                ndevs = lastat->dinfo->numdevs;
                devloads = (struct dev_la *) malloc(ndevs * sizeof(struct dev_la));
                memset(devloads, '\0', ndevs * sizeof(struct dev_la));
                for (i=0; i < ndevs; i++) {
                        devloads[i].la1 = devloads[i].la5 = devloads[i].la15 = 0;
                        memcpy(&devloads[i].prev, &lastat->dinfo->devices[i].busy_time, sizeof(devloads[i].prev));
                        snprintf(devloads[i].name, sizeof(devloads[i].name), "%s%d",
                                lastat->dinfo->devices[i].device_name, lastat->dinfo->devices[i].unit_number);
                        }
                expon1  = exp(-(((double)DISKIO_SAMPLE_INTERVAL) / ((double)60)));
                expon5  = exp(-(((double)DISKIO_SAMPLE_INTERVAL) / ((double)300)));
                expon15 = exp(-(((double)DISKIO_SAMPLE_INTERVAL) / ((double)900)));
                }

        for (i=0; i<ndevs; i++) {
#if defined(freebsd5) && __FreeBSD_version >= 500107
                busy_time = devstat_compute_etime(&lastat->dinfo->devices[i].busy_time, &devloads[i].prev);
#else
                busy_time = devla_timeval_diff(&devloads[i].prev, &lastat->dinfo->devices[i].busy_time);
#endif
                if ( busy_time < 0 )
                    busy_time = 0;   /* Account for possible FP loss of precision near zero */
                busy_percent = busy_time * 100 / DISKIO_SAMPLE_INTERVAL;
                devloads[i].la1 = devloads[i].la1 * expon1 + busy_percent * (1 - expon1);
/*		fprintf(stderr, "(%d) %s: update la1=%.2lf%%\n", i, devloads[i].name, expon1); */
                devloads[i].la5 = devloads[i].la5 * expon5 + busy_percent * (1 - expon5);
                devloads[i].la15 = devloads[i].la15 * expon15 + busy_percent * (1 - expon15);
                memcpy(&devloads[i].prev, &lastat->dinfo->devices[i].busy_time, sizeof(devloads[i].prev));
                }

        }

/* end of disk LA patch */

static int      ndisk;
static struct statinfo *stat;
FILE           *file;

static int
getstats(void)
{
    time_t          now;
    int             i;

    now = time(NULL);
    if (cache_time + CACHE_TIMEOUT > now) {
        return 0;
    }
    if (stat == NULL) {
        stat = (struct statinfo *) malloc(sizeof(struct statinfo));
        if (stat != NULL)
            stat->dinfo = (struct devinfo *) calloc(sizeof(struct devinfo), 1);
        if (stat == NULL || stat->dinfo == NULL) {
		SNMP_FREE(stat);
        	ERROR_MSG("Memory alloc failure - getstats\n");
		return 1;
	}
    }

    if (GETDEVS(stat) == -1) {
        fprintf(stderr, "Can't get devices:%s\n", devstat_errbuf);
        return 1;
    }
    ndisk = stat->dinfo->numdevs;
    /* Gross hack to include device numbers in the device name array */
    for (i = 0; i < ndisk; i++) {
      char *cp = stat->dinfo->devices[i].device_name;
      int len = strlen(cp);
      if (len > DEVSTAT_NAME_LEN - 3)
        len -= 3;
      cp += len;
      sprintf(cp, "%d", stat->dinfo->devices[i].unit_number);
    }
    cache_time = now;
    return 0;
}

u_char         *
var_diskio(struct variable * vp,
           oid * name,
           size_t * length,
           int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static struct   counter64 c64_ret;
    long long       longlong_ret;
    unsigned int    indx;

    if (getstats() == 1) {
        return NULL;
    }


    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, ndisk)) {
        return NULL;
    }

    indx = (unsigned int) (name[*length - 1] - 1);

    if (indx >= ndisk)
        return NULL;

    switch (vp->magic) {
    case DISKIO_INDEX:
        long_ret = (long) indx + 1;;
        return (u_char *) & long_ret;
    case DISKIO_DEVICE:
        *var_len = strlen(stat->dinfo->devices[indx].device_name);
        return (u_char *) stat->dinfo->devices[indx].device_name;
    case DISKIO_NREAD:
#if defined(freebsd5) && __FreeBSD_version >= 500107
        long_ret = (signed long) stat->dinfo->devices[indx].bytes[DEVSTAT_READ];
#else
        long_ret = (signed long) stat->dinfo->devices[indx].bytes_read;
#endif
        return (u_char *) & long_ret;
    case DISKIO_NWRITTEN:
#if defined(freebsd5) && __FreeBSD_version >= 500107
        long_ret = (signed long) stat->dinfo->devices[indx].bytes[DEVSTAT_WRITE];
#else
        long_ret = (signed long) stat->dinfo->devices[indx].bytes_written;
#endif
        return (u_char *) & long_ret;
    case DISKIO_NREADX:
        *var_len = sizeof(struct counter64);
#if defined(freebsd5) && __FreeBSD_version >= 500107
        longlong_ret = stat->dinfo->devices[indx].bytes[DEVSTAT_READ];
#else
        longlong_ret = stat->dinfo->devices[indx].bytes_read;
#endif
        c64_ret.low = longlong_ret & 0xffffffff;
        c64_ret.high = longlong_ret >> 32;
        return (u_char *) & c64_ret;
    case DISKIO_NWRITTENX:
        *var_len = sizeof(struct counter64);
#if defined(freebsd5) && __FreeBSD_version >= 500107
        longlong_ret = stat->dinfo->devices[indx].bytes[DEVSTAT_WRITE];
#else
        longlong_ret = stat->dinfo->devices[indx].bytes_written;
#endif
        c64_ret.low = longlong_ret & 0xffffffff;
        c64_ret.high = longlong_ret >> 32;
        return (u_char *) & c64_ret;
    case DISKIO_READS:
#if defined(freebsd5) && __FreeBSD_version >= 500107
        long_ret = (signed long) stat->dinfo->devices[indx].operations[DEVSTAT_READ];
#else
        long_ret = (signed long) stat->dinfo->devices[indx].num_reads;
#endif
        return (u_char *) & long_ret;
    case DISKIO_WRITES:
#if defined(freebsd5) && __FreeBSD_version >= 500107
        long_ret = (signed long) stat->dinfo->devices[indx].operations[DEVSTAT_WRITE];
#else
        long_ret = (signed long) stat->dinfo->devices[indx].num_writes;
#endif
        return (u_char *) & long_ret;
    case DISKIO_LA1:
	long_ret = devloads[indx].la1;
	return (u_char *) & long_ret;
    case DISKIO_LA5:
        long_ret = devloads[indx].la5;
        return (u_char *) & long_ret;
    case DISKIO_LA15:
        long_ret = devloads[indx].la15;
        return (u_char *) & long_ret;

    default:
        ERROR_MSG("diskio.c: don't know how to handle this request.");
    }
    return NULL;
}
#endif                          /* freebsd4 */


#ifdef linux

#define DISK_INCR 2

typedef struct linux_diskio
{
    int major;
    int  minor;
    unsigned long  blocks;
    char name[256];
    unsigned long  rio;
    unsigned long  rmerge;
    unsigned long  rsect;
    unsigned long  ruse;
    unsigned long  wio;
    unsigned long  wmerge;
    unsigned long  wsect;
    unsigned long  wuse;
    unsigned long  running;
    unsigned long  use;
    unsigned long  aveq;
} linux_diskio;

/* disk load averages */
typedef struct linux_diskio_la
{
    unsigned long use_prev;
    double la1, la5, la15;
} linux_diskio_la;

typedef struct linux_diskio_header
{
    linux_diskio* indices;
    int length;
    int alloc;
} linux_diskio_header;

typedef struct linux_diskio_la_header
{
    linux_diskio_la * indices;   
    int length;
} linux_diskio_la_header;

static linux_diskio_header head;
static linux_diskio_la_header la_head;

void devla_getstats(unsigned int regno, void * dummy) {

    static double expon1, expon5, expon15;
    double busy_time, busy_percent;
    int idx;

    if (getstats() == 1) {
        ERROR_MSG("can't do diskio getstats()\n");
        return;
    }

    if (!la_head.length) {
        la_head.indices = (linux_diskio_la *) malloc(head.length * sizeof(linux_diskio_la));
        for (idx=0; idx<head.length; idx++) {
            la_head.indices[idx].la1 = la_head.indices[idx].la5 = la_head.indices[idx].la15 = 0.; 
            la_head.indices[idx].use_prev = head.indices[idx].use;
        }
        la_head.length = head.length;
        expon1 = exp(-(((double)DISKIO_SAMPLE_INTERVAL) / ((double)60)));
        expon5 = exp(-(((double)DISKIO_SAMPLE_INTERVAL) / ((double)300)));
        expon15 = exp(-(((double)DISKIO_SAMPLE_INTERVAL) / ((double)900)));
    }
    else if (head.length - la_head.length) {
        la_head.indices = (linux_diskio_la *) realloc(la_head.indices, head.length * sizeof(linux_diskio_la));
        for (idx=la_head.length; idx<head.length; idx++) {
            la_head.indices[idx].la1 = la_head.indices[idx].la5 = la_head.indices[idx].la15 = 0.; 
            la_head.indices[idx].use_prev = head.indices[idx].use;
        }
        la_head.length = head.length;
    }

    for (idx=0; idx<head.length; idx++) {
        busy_time = head.indices[idx].use - la_head.indices[idx].use_prev;
        busy_percent = busy_time * 100. / ((double) DISKIO_SAMPLE_INTERVAL) / 1000.;
        la_head.indices[idx].la1 = la_head.indices[idx].la1 * expon1 + busy_percent * (1. - expon1);
        la_head.indices[idx].la5 = la_head.indices[idx].la5 * expon5 + busy_percent * (1. - expon5);
        la_head.indices[idx].la15 = la_head.indices[idx].la15 * expon15 + busy_percent * (1. - expon15);
        /*
          fprintf(stderr, "(%d) update la1=%f la5=%f la15=%f\n",
          idx, la_head.indices[idx].la1, la_head.indices[idx].la5, la_head.indices[idx].la15);   
        */
        la_head.indices[idx].use_prev = head.indices[idx].use;
    }
}

int is_excluded(const char *name)
{
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DISKIO_NO_FD)
                           && !(strncmp(name, "fd", 2)))
        return 1;
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DISKIO_NO_LOOP)
                           && !(strncmp(name, "loop", 4)))
        return 1;
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_AGENT_DISKIO_NO_RAM)
                           && !(strncmp(name, "ram", 3)))
        return 1;
    return 0;
}

static int
getstats(void)
{
    FILE* parts;
    time_t now;
    
    now = time(NULL);
    if (cache_time + CACHE_TIMEOUT > now) {
        return 0;
    }

    if (!head.indices) {
	head.alloc = DISK_INCR;
	head.indices = (linux_diskio *)malloc(head.alloc*sizeof(linux_diskio));
    }
    head.length  = 0;

    memset(head.indices, 0, head.alloc*sizeof(linux_diskio));

    /* Is this a 2.6 kernel? */
    parts = fopen("/proc/diskstats", "r");
    if (parts) {
	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), parts)) {
	    linux_diskio* pTemp;
	    if (head.length == head.alloc) {
		head.alloc += DISK_INCR;
		head.indices = (linux_diskio *)realloc(head.indices, head.alloc*sizeof(linux_diskio));
	    }
	    pTemp = &head.indices[head.length];
	    sscanf (buffer, "%d %d", &pTemp->major, &pTemp->minor);
 	    if (sscanf (buffer, "%d %d %s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
		    &pTemp->major, &pTemp->minor, pTemp->name,
		    &pTemp->rio, &pTemp->rmerge, &pTemp->rsect, &pTemp->ruse,
		    &pTemp->wio, &pTemp->wmerge, &pTemp->wsect, &pTemp->wuse,
 		    &pTemp->running, &pTemp->use, &pTemp->aveq) != 14)
		sscanf (buffer, "%d %d %s %lu %lu %lu %lu\n",
		    &pTemp->major, &pTemp->minor, pTemp->name,
		    &pTemp->rio, &pTemp->rsect,
		    &pTemp->wio, &pTemp->wsect);
            if (!is_excluded(pTemp->name))
	        head.length++;
	}
    }
    else {
	/* See if a 2.4 kernel */
	char buffer[1024];
	int rc;
	parts = fopen("/proc/partitions", "r");
	if (!parts) {
	    snmp_log_perror("/proc/partitions");
	    return 1;
	}

	/*
	 * first few fscanfs are garbage we don't care about. skip it.
	 */
	fgets(buffer, sizeof(buffer), parts);
	fgets(buffer, sizeof(buffer), parts);

	while (! feof(parts)) {
	    linux_diskio* pTemp;

	    if (head.length == head.alloc) {
		head.alloc += DISK_INCR;
		head.indices = (linux_diskio *)realloc(head.indices, head.alloc*sizeof(linux_diskio));
	    }
	    pTemp = &head.indices[head.length];

	    rc = fscanf (parts, "%d %d %lu %s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
		    &pTemp->major, &pTemp->minor, &pTemp->blocks, pTemp->name,
		    &pTemp->rio, &pTemp->rmerge, &pTemp->rsect, &pTemp->ruse,
		    &pTemp->wio, &pTemp->wmerge, &pTemp->wsect, &pTemp->wuse,
		    &pTemp->running, &pTemp->use, &pTemp->aveq);
            if (rc != 15) {
               snmp_log(LOG_ERR, "diskio.c: cannot find statistics in /proc/partitions\n");
               fclose(parts);
               return 1;
            }
            if (!is_excluded(pTemp->name))
	        head.length++;
	}
    }

    fclose(parts);
    cache_time = now;
    return 0;
}

u_char *
var_diskio(struct variable * vp,
	   oid * name,
	   size_t * length,
	   int exact,
	   size_t * var_len,
	   WriteMethod ** write_method)
{
    unsigned int indx;
    static unsigned long long_ret;
    static struct counter64 c64_ret;

    if (getstats() == 1) {
	return NULL;
    }

 if (header_simple_table(vp, name, length, exact, var_len, write_method, head.length))
    {
	return NULL;
    }

  indx = (unsigned int) (name[*length - 1] - 1);

  if (indx >= head.length)
    return NULL;

  switch (vp->magic) {
    case DISKIO_INDEX:
      long_ret = indx+1;
      return (u_char *) &long_ret;
    case DISKIO_DEVICE:
      *var_len = strlen(head.indices[indx].name);
      return (u_char *) head.indices[indx].name;
    case DISKIO_NREAD:
      long_ret = (head.indices[indx].rsect*512) & 0xffffffff;
      return (u_char *) & long_ret;
    case DISKIO_NWRITTEN:
      long_ret = (head.indices[indx].wsect*512) & 0xffffffff;
      return (u_char *) & long_ret;
    case DISKIO_READS:
      long_ret = head.indices[indx].rio & 0xffffffff;
      return (u_char *) & long_ret;
    case DISKIO_WRITES:
      long_ret = head.indices[indx].wio & 0xffffffff;
      return (u_char *) & long_ret;
    case DISKIO_LA1:
      long_ret = la_head.indices[indx].la1;
      return (u_char *) & long_ret;
    case DISKIO_LA5:
      long_ret = la_head.indices[indx].la5;
      return (u_char *) & long_ret;
    case DISKIO_LA15:
      long_ret = la_head.indices[indx].la15;
      return (u_char *) & long_ret;
    case DISKIO_NREADX:
      *var_len = sizeof(struct counter64);
      c64_ret.low = head.indices[indx].rsect * 512 & 0xffffffff;
      c64_ret.high = head.indices[indx].rsect >> (32 - 9);
      return (u_char *) & c64_ret;
    case DISKIO_NWRITTENX:
      *var_len = sizeof(struct counter64);
      c64_ret.low = head.indices[indx].wsect * 512 & 0xffffffff;
      c64_ret.high = head.indices[indx].wsect >> (32 - 9);
      return (u_char *) & c64_ret;
    default:
	snmp_log(LOG_ERR, "don't know how to handle %d request\n", vp->magic);
  }
  return NULL;
}
#endif  /* linux */

#if defined(darwin)

#define MAXDRIVES	16	/* most drives we will record */
#define MAXDRIVENAME	31	/* largest drive name we allow */

#define kIDXBytesRead		0	/* used as index into the stats array in a drivestats struct */
#define kIDXBytesWritten	1
#define kIDXNumReads		2
#define kIDXNumWrites		3
#define kIDXLast		3

struct drivestats {
    char name[MAXDRIVENAME + 1];
    long bsd_unit_number;
    long stats[kIDXLast+1];
};

static struct drivestats drivestat[MAXDRIVES];

static mach_port_t masterPort;		/* to communicate with I/O Kit	*/

static int num_drives;			/* number of drives detected	*/

static int
collect_drive_stats(io_registry_entry_t driver, long *stats)
{
    CFNumberRef     number;
    CFDictionaryRef properties;
    CFDictionaryRef statistics;
    long            value;
    kern_return_t   status;
    int             i;


    /*
     * If the drive goes away, we may not get any properties
     * for it.  So take some defaults. Nb: use memset ??
     */
    for (i = 0; i < kIDXLast; i++) {
	stats[i] = 0;
    }

    /* retrieve the properties */
    status = IORegistryEntryCreateCFProperties(driver, (CFMutableDictionaryRef *)&properties,
					       kCFAllocatorDefault, kNilOptions);
    if (status != KERN_SUCCESS) {
	snmp_log(LOG_ERR, "diskio: device has no properties\n");
/*	fprintf(stderr, "device has no properties\n"); */
	return (1);
    }

    /* retrieve statistics from properties */
    statistics = (CFDictionaryRef)CFDictionaryGetValue(properties,
						       CFSTR(kIOBlockStorageDriverStatisticsKey));
    if (statistics) {

	/* Now hand me the crystals. */
	if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
						 CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey)))) {
	    CFNumberGetValue(number, kCFNumberSInt32Type, &value);
	    stats[kIDXBytesRead] = value;
	}

	if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
						 CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey)))) {
	    CFNumberGetValue(number, kCFNumberSInt32Type, &value);
	    stats[kIDXBytesWritten] = value;
	}

	if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
						 CFSTR(kIOBlockStorageDriverStatisticsReadsKey)))) {
	    CFNumberGetValue(number, kCFNumberSInt32Type, &value);
	    stats[kIDXNumReads] = value;
	}
	if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
						 CFSTR(kIOBlockStorageDriverStatisticsWritesKey)))) {
	    CFNumberGetValue(number, kCFNumberSInt32Type, &value);
	    stats[kIDXNumWrites] = value;
	}
    }
    /* we're done with the properties, release them */
    CFRelease(properties);
    return (0);
}

/*
 * Check whether an IORegistryEntry refers to a valid
 * I/O device, and if so, collect the information.
 */
static int
handle_drive(io_registry_entry_t drive, struct drivestats * dstat)
{
    io_registry_entry_t parent;
    CFMutableDictionaryRef     properties;
    CFStringRef         name;
    CFNumberRef         number;
    kern_return_t       status;

    /* get drive's parent */
    status = IORegistryEntryGetParentEntry(drive, kIOServicePlane, &parent);
    if (status != KERN_SUCCESS) {
	snmp_log(LOG_ERR, "diskio: device has no parent\n");
/*	fprintf(stderr, "device has no parent\n"); */
	return(1);
    }

    if (IOObjectConformsTo(parent, "IOBlockStorageDriver")) {

	/* get drive properties */
	status = IORegistryEntryCreateCFProperties(drive, &properties,
					    kCFAllocatorDefault, kNilOptions);
	if (status != KERN_SUCCESS) {
	    snmp_log(LOG_ERR, "diskio: device has no properties\n");
/*	    fprintf(stderr, "device has no properties\n"); */
	    return(1);
	}

	/* get BSD name and unitnumber from properties */
	name = (CFStringRef)CFDictionaryGetValue(properties,
					  CFSTR(kIOBSDNameKey));
	number = (CFNumberRef)CFDictionaryGetValue(properties,
					    CFSTR(kIOBSDUnitKey));

	/* Collect stats and if succesful store them with the name and unitnumber */
	if (name && number && !collect_drive_stats(parent, dstat->stats)) {

	    CFStringGetCString(name, dstat->name, MAXDRIVENAME, CFStringGetSystemEncoding());
	    CFNumberGetValue(number, kCFNumberSInt32Type, &dstat->bsd_unit_number);
	    num_drives++;
	}

	/* clean up, return success */
	CFRelease(properties);
	return(0);
    }

    /* failed, don't keep parent */
    IOObjectRelease(parent);
    return(1);
}

static int
getstats(void)
{
    time_t                 now;
    io_iterator_t          drivelist;
    io_registry_entry_t    drive;
    CFMutableDictionaryRef match;
    kern_return_t          status;

    now = time(NULL);	/* register current time and check wether cache can be used */
    if (cache_time + CACHE_TIMEOUT > now) {
        return 0;
    }

    /*  Retrieve a list of drives. */
    match = IOServiceMatching("IOMedia");
    CFDictionaryAddValue(match, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
    status = IOServiceGetMatchingServices(masterPort, match, &drivelist);
    if (status != KERN_SUCCESS) {
	snmp_log(LOG_ERR, "diskio: couldn't match whole IOMedia devices\n");
/*	fprintf(stderr,"Couldn't match whole IOMedia devices\n"); */
	return(1);
    }

    num_drives = 0;  /* NB: Incremented by handle_drive */
    while ((drive = IOIteratorNext(drivelist)) && (num_drives < MAXDRIVES)) {
	handle_drive(drive, &drivestat[num_drives]);
	IOObjectRelease(drive);
    }
    IOObjectRelease(drivelist);

    cache_time = now;
    return (0);
}

u_char         *
var_diskio(struct variable * vp,
           oid * name,
           size_t * length,
           int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    unsigned int    indx;

    if (getstats() == 1) {
        return NULL;
    }


    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, num_drives)) {
        return NULL;
    }

    indx = (unsigned int) (name[*length - 1] - 1);

    if (indx >= num_drives)
        return NULL;

    switch (vp->magic) {
	case DISKIO_INDEX:
	    long_ret = (long) drivestat[indx].bsd_unit_number;
	    return (u_char *) & long_ret;
	case DISKIO_DEVICE:
	    *var_len = strlen(drivestat[indx].name);
	    return (u_char *) drivestat[indx].name;
	case DISKIO_NREAD:
	    long_ret = (signed long) drivestat[indx].stats[kIDXBytesRead];
	    return (u_char *) & long_ret;
	case DISKIO_NWRITTEN:
	    long_ret = (signed long) drivestat[indx].stats[kIDXBytesWritten];
	    return (u_char *) & long_ret;
	case DISKIO_READS:
	    long_ret = (signed long) drivestat[indx].stats[kIDXNumReads];
	    return (u_char *) & long_ret;
	case DISKIO_WRITES:
	    long_ret = (signed long) drivestat[indx].stats[kIDXNumWrites];
	    return (u_char *) & long_ret;

	default:
	    ERROR_MSG("diskio.c: don't know how to handle this request.");
    }
    return NULL;
}
#endif                          /* darwin */


#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
/*
 * collect statistics for all disks
 */
int
collect_disks(void)
{
    time_t          now;
    int             i;
    perfstat_id_t   first;

    /* cache valid? if yes, just return */
    now = time(NULL);
    if (ps_disk != NULL && cache_time + CACHE_TIMEOUT > now) {
        return 0;
    }

    /* get number of disks we have */
    i = perfstat_disk(NULL, NULL, sizeof(perfstat_disk_t), 0);
    if(i <= 0) return 1;

    /* if number of disks differs or structures are uninitialized, init them */
    if(i != ps_numdisks || ps_disk == NULL) {
        if(ps_disk != NULL) free(ps_disk);
        ps_numdisks = i;
        ps_disk = malloc(sizeof(perfstat_disk_t) * ps_numdisks);
        if(ps_disk == NULL) return 1;
    }

    /* gather statistics about all disks we have */
    strcpy(first.name, "");
    i = perfstat_disk(&first, ps_disk, sizeof(perfstat_disk_t), ps_numdisks);
    if(i != ps_numdisks) return 1;

    cache_time = now;
    return 0;
}


u_char         *
var_diskio(struct variable * vp,
           oid * name,
           size_t * length,
           int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static struct counter64 c64_ret;
    unsigned int    indx;

    /* get disk statistics */
    if (collect_disks())
        return NULL;

    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, ps_numdisks))
        return NULL;

    indx = (unsigned int) (name[*length - 1] - 1);
    if (indx >= ps_numdisks)
        return NULL;

    /* deliver requested data on requested disk */
    switch (vp->magic) {
    case DISKIO_INDEX:
        long_ret = (long) indx;
        return (u_char *) & long_ret;
    case DISKIO_DEVICE:
        *var_len = strlen(ps_disk[indx].name);
        return (u_char *) ps_disk[indx].name;
    case DISKIO_NREAD:
        long_ret = (signed long) ps_disk[indx].rblks * ps_disk[indx].bsize;
        return (u_char *) & long_ret;
    case DISKIO_NWRITTEN:
        long_ret = (signed long) ps_disk[indx].wblks * ps_disk[indx].bsize;
        return (u_char *) & long_ret;
    case DISKIO_READS:
        long_ret = (signed long) ps_disk[indx].xfers;
        return (u_char *) & long_ret;
    case DISKIO_WRITES:
        long_ret = (signed long) 0;	/* AIX has just one value for read/write transfers */
        return (u_char *) & long_ret;
    case DISKIO_NREADX:
        *var_len = sizeof(struct counter64);
        c64_ret.low = (ps_disk[indx].rblks * ps_disk[indx].bsize) & 0xffffffff;;
        c64_ret.high = (ps_disk[indx].rblks * ps_disk[indx].bsize) >> 32;
        return (u_char *) & c64_ret;
    case DISKIO_NWRITTENX:
        *var_len = sizeof(struct counter64);
        c64_ret.low = (ps_disk[indx].wblks * ps_disk[indx].bsize) & 0xffffffff;;
        c64_ret.high = (ps_disk[indx].wblks * ps_disk[indx].bsize) >> 32;
        return (u_char *) & c64_ret;

    default:
        ERROR_MSG("diskio.c: don't know how to handle this request.");
    }

    /* return NULL in case of error */
    return NULL;
}
#endif                          /* aix 4/5 */
