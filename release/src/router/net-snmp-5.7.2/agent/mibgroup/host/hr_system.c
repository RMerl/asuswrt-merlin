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

/*
 *  Host Resources MIB - system group implementation - hr_system.c
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "host.h"
#include "host_res.h"
#include "hr_system.h"
#include <net-snmp/agent/auto_nlist.h>

#ifdef HAVE_SYS_PROC_H
#include <sys/param.h>
#include "sys/proc.h"
#endif
#ifndef mingw32
#if HAVE_UTMPX_H
#include <utmpx.h>
#else
#include <utmp.h>
#endif
#endif /* mingw32 */
#include <signal.h>
#include <errno.h>

#ifdef WIN32
#include <lm.h>
#endif

#ifdef linux
#ifdef HAVE_LINUX_TASKS_H
#include <linux/tasks.h>
#else
/*
 * If this file doesn't exist, then there is no hard limit on the number
 * of processes, so return 0 for hrSystemMaxProcesses.  
 */
#define NR_TASKS	0
#endif
#endif

#if defined(hpux10) || defined(hpux11)
#include <sys/pstat.h>
#endif

#if defined(solaris2)
#include <kstat.h>
#include <sys/var.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/openpromio.h>
#endif

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

netsnmp_feature_require(date_n_time)

#if !defined(UTMP_FILE) && defined(_PATH_UTMP)
#define UTMP_FILE _PATH_UTMP
#endif

#if defined(UTMP_FILE) && !HAVE_UTMPX_H
void            setutent(void);
void            endutent(void);
struct utmp    *getutent(void);
#endif                          /* UTMP_FILE */


        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

#if defined(solaris2)
static struct openpromio * op_malloc(size_t size);
static void op_free(struct openpromio *op);

#ifndef NETSNMP_NO_WRITE_SUPPORT
static int set_solaris_bootcommand_parameter(int action, u_char * var_val, u_char var_val_type, size_t var_val_len, u_char * statP, oid * name, size_t name_len);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

static int set_solaris_eeprom_parameter(const char *key, const char *value, size_t value_len);
static int get_solaris_eeprom_parameter(const char *parameter, char *output);
static long     get_max_solaris_processes(void);
#endif

static int      get_load_dev(void);
static int      count_users(void);
extern int      count_processes(void);
extern int      swrun_count_processes(void);

        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

#define	HRSYS_UPTIME		1
#define	HRSYS_DATE		2
#define	HRSYS_LOAD_DEV		3
#define	HRSYS_LOAD_PARAM	4
#define	HRSYS_USERS		5
#define	HRSYS_PROCS		6
#define	HRSYS_MAXPROCS		7

#if defined(solaris2)
#ifndef NETSNMP_NO_WRITE_SUPPORT
struct variable2 hrsystem_variables[] = {
    {HRSYS_UPTIME, ASN_TIMETICKS, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {1}},
    {HRSYS_DATE, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_hrsys, 1, {2}},
    {HRSYS_LOAD_DEV, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {3}},
    {HRSYS_LOAD_PARAM, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_hrsys, 1, {4}},
    {HRSYS_USERS, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {5}},
    {HRSYS_PROCS, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {6}},
    {HRSYS_MAXPROCS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {7}}
};
#else /* !NETSNMP_NO_WRITE_SUPPORT */
struct variable2 hrsystem_variables[] = {
    {HRSYS_UPTIME, ASN_TIMETICKS, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {1}},
    {HRSYS_DATE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {2}},
    {HRSYS_LOAD_DEV, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {3}},
    {HRSYS_LOAD_PARAM, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {4}},
    {HRSYS_USERS, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {5}},
    {HRSYS_PROCS, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {6}},
    {HRSYS_MAXPROCS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {7}}
};
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
#else
struct variable2 hrsystem_variables[] = {
    {HRSYS_UPTIME, ASN_TIMETICKS, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {1}},
    {HRSYS_DATE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {2}},
    {HRSYS_LOAD_DEV, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {3}},
    {HRSYS_LOAD_PARAM, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {4}},
    {HRSYS_USERS, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {5}},
    {HRSYS_PROCS, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {6}},
    {HRSYS_MAXPROCS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrsys, 1, {7}}
};
#endif

oid             hrsystem_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 1 };


void
init_hr_system(void)
{
#ifdef NPROC_SYMBOL
    auto_nlist(NPROC_SYMBOL, 0, 0);
#endif

    REGISTER_MIB("host/hr_system", hrsystem_variables, variable2,
                 hrsystem_variables_oid);
} /* end init_hr_system */

/*
 * header_hrsys(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 */

int
header_hrsys(struct variable *vp,
             oid * name,
             size_t * length,
             int exact, size_t * var_len, WriteMethod ** write_method)
{
#define HRSYS_NAME_LENGTH	9
    oid             newname[MAX_OID_LEN];
    int             result;

    DEBUGMSGTL(("host/hr_system", "var_hrsys: "));
    DEBUGMSGOID(("host/hr_system", name, *length));
    DEBUGMSG(("host/hr_system", " %d\n", exact));

    memcpy((char *) newname, (char *) vp->name, vp->namelen * sizeof(oid));
    newname[HRSYS_NAME_LENGTH] = 0;
    result = snmp_oid_compare(name, *length, newname, vp->namelen + 1);
    if ((exact && (result != 0)) || (!exact && (result >= 0)))
        return (MATCH_FAILED);
    memcpy((char *) name, (char *) newname,
           (vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;

    *write_method = (WriteMethod*)0;
    *var_len = sizeof(long);    /* default to 'long' results */
    return (MATCH_SUCCEEDED);
} /* end header_hrsys */


        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

u_char         *
var_hrsys(struct variable * vp,
          oid * name,
          size_t * length,
          int exact, size_t * var_len, WriteMethod ** write_method)
{
    static char     string[129]; /* per MIB, max size is 128 */
#if defined(solaris2)
    /* max size of nvram property */
    char bootparam[8192];
#endif
    time_t          now;
#if !(defined(NR_TASKS) || defined(solaris2) || defined(hpux10) || defined(hpux11))
    int             nproc = 0;
#endif
#ifdef linux
    FILE           *fp;
#endif
#if NETSNMP_CAN_USE_SYSCTL && defined(CTL_KERN) && defined(KERN_MAXPROC)
    static int      maxproc_mib[] = { CTL_KERN, KERN_MAXPROC };
    size_t          buf_size;
#endif
#if defined(hpux10) || defined(hpux11)
    struct pst_static pst_buf;
#endif

    if (header_hrsys(vp, name, length, exact, var_len, write_method) ==
        MATCH_FAILED)
        return NULL;

    switch (vp->magic) {
    case HRSYS_UPTIME:
        long_return = get_uptime();
        return (u_char *) & long_return;
    case HRSYS_DATE:
#if defined(HAVE_MKTIME) && defined(HAVE_STIME)
#ifndef NETSNMP_NO_WRITE_SUPPORT 
        *write_method=ns_set_time;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
#endif
        time(&now);
        return (u_char *) date_n_time(&now, var_len);
    case HRSYS_LOAD_DEV:
        long_return = get_load_dev();
        return (u_char *) & long_return;
    case HRSYS_LOAD_PARAM:
#ifdef linux
        if((fp = fopen("/proc/cmdline", "r")) != NULL) {
            fgets(string, sizeof(string), fp);
            fclose(fp);
        } else {
            return NULL;
        }
#elif defined(solaris2)
#ifndef NETSNMP_NO_WRITE_SUPPORT
        *write_method=set_solaris_bootcommand_parameter;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        if ( get_solaris_eeprom_parameter("boot-command",bootparam) ) {
            snmp_log(LOG_ERR,"unable to lookup boot-command from eeprom\n");
            return NULL;
        }
        strlcpy(string,bootparam,sizeof(string));
#else
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        sprintf(string, "ask Dave");    /* XXX */
#endif
        *var_len = strlen(string);
        return (u_char *) string;
    case HRSYS_USERS:
        long_return = count_users();
        return (u_char *) & long_return;
    case HRSYS_PROCS:
#if USING_HOST_DATA_ACCESS_SWRUN_MODULE
        long_return = swrun_count_processes();
#elif USING_HOST_HR_SWRUN_MODULE
        long_return = count_processes();
#else
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = 0;
#endif
        return (u_char *) & long_return;
    case HRSYS_MAXPROCS:
#if defined(NR_TASKS)
        long_return = NR_TASKS; /* <linux/tasks.h> */
#elif NETSNMP_CAN_USE_SYSCTL && defined(CTL_KERN) && defined(KERN_MAXPROC)
        buf_size = sizeof(nproc);
        if (sysctl(maxproc_mib, 2, &nproc, &buf_size, NULL, 0) < 0)
            return NULL;
        long_return = nproc;
#elif defined(hpux10) || defined(hpux11)
        pstat_getstatic(&pst_buf, sizeof(struct pst_static), 1, 0);
        long_return = pst_buf.max_proc;
#elif defined(solaris2)
    long_return=get_max_solaris_processes();
    if(long_return == -1) return NULL;
#elif defined(NPROC_SYMBOL)
        auto_nlist(NPROC_SYMBOL, (char *) &nproc, sizeof(int));
        long_return = nproc;
#else
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = 0;
#endif
        return (u_char *) & long_return;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_hrsys\n",
                    vp->magic));
    }
    return NULL;
} /* end var_hrsys */


        /*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/


#if defined(solaris2)

/* functions for malloc and freeing openpromio structure */
static struct openpromio * op_malloc(size_t size)
{
    struct openpromio *op;
    op=malloc(sizeof(struct openpromio) + size);
    if(op == NULL) {
        snmp_log(LOG_ERR,"unable to malloc memory\n");
        return NULL;
    }

    memset(op, 0, sizeof(struct openpromio)+size);
    op->oprom_size=size;

    return op;
}

static void op_free(struct openpromio *op) {
    free(op);
}

#ifndef NETSNMP_NO_WRITE_SUPPORT
static int
set_solaris_bootcommand_parameter(int action,
            u_char * var_val,
            u_char var_val_type,
            size_t var_val_len,
            u_char * statP, oid * name, size_t name_len) {

    static char old_value[1024],*p_old_value=old_value;
    int status=0;

    switch (action) {
        case RESERVE1:
            /* check type */
            if (var_val_type != ASN_OCTET_STR) {
                snmp_log(LOG_ERR,"write to set_solaris_bootcommand_parameter not ASN_OCTET_STR\n");
                return SNMP_ERR_WRONGTYPE;
            }
            break;

        case RESERVE2: {
            /* create copy of old value */
            if(statP) {
                int old_val_len=strlen(statP);
                if(old_val_len >= sizeof(old_value)) {
                    p_old_value=(char *)malloc(old_val_len+1);
                    if(p_old_value==NULL) {
                        snmp_log(LOG_ERR,"unable to malloc memory\n");
                        return SNMP_ERR_GENERR;
                    } 
                }
                strlcpy(p_old_value,statP,old_val_len+1);
            } else { 
                p_old_value=NULL;
            }
            break;
        }

        case ACTION: {
            status=set_solaris_eeprom_parameter("boot-command",(char *)var_val,var_val_len);
            if(status!=0) return SNMP_ERR_GENERR;
            break;
        }

        case UNDO: {
            /* revert to old value */
            if(p_old_value) { 
                status=set_solaris_eeprom_parameter("boot-command",(char *)p_old_value,strlen(p_old_value));
                p_old_value=old_value; 
                if(status!=0) return SNMP_ERR_GENERR;
            }
            break;
        }

        case FREE:
        case COMMIT:
            /* free memory if necessary */
            if(p_old_value !=  old_value) {
                free(p_old_value);
                p_old_value=old_value;
            }
            break;
    }
    return SNMP_ERR_NOERROR;
}
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

static int set_solaris_eeprom_parameter(const char *key, const char *value,
                                        size_t var_val_len) {

    int status=0;
    char buffer[1024],*pbuffer=buffer;
    
    if( strlen(key)+strlen(value)+16 > sizeof(buffer) ) {
        pbuffer=(char *)malloc(strlen(key)+strlen(value)+16);
    } 

    
    sprintf(pbuffer, "eeprom %s=\"%.*s\"\n", key, var_val_len, value);

    status=system(pbuffer);

    if(pbuffer!=buffer) free(pbuffer);
    return status;
}

static int get_solaris_eeprom_parameter(const char *parameter, char *outbuffer) {

    int fd=0,status=0;
    struct openpromio *openprominfo=NULL;

    fd=open("/dev/openprom",O_RDONLY);
    if ( fd == -1 ) {
        snmp_log_perror("/dev/openprom");
        return 1;
    }

    openprominfo = op_malloc(8192);
    if(!openprominfo) return 1;

    strcpy(openprominfo->oprom_array,parameter);

    status=ioctl(fd,OPROMGETOPT,openprominfo);
    if ( status == -1 ) {
        snmp_log_perror("/dev/openprom");
        close(fd);
        op_free(openprominfo);
        return 1;
    }
    strcpy(outbuffer,openprominfo->oprom_array);

    op_free(openprominfo);

    /* close file */
    close(fd);

    return(0);
}

static long get_max_solaris_processes(void) {

    kstat_ctl_t *ksc=NULL;
    kstat_t *ks=NULL;
    struct var v;
    static long maxprocs=-1;

    /* assume only necessary to compute once, since /etc/system must be modified */
    if (maxprocs == -1) {
        if ( (ksc=kstat_open()) != NULL && 
             (ks=kstat_lookup(ksc, "unix", 0, "var")) != NULL && 
             (kstat_read(ksc, ks, &v) != -1)) {

            maxprocs=v.v_proc;
        }
        if(ksc) {
            kstat_close(ksc);
        }
    }

    return maxprocs;
}

#endif

#if defined(HAVE_MKTIME) && defined(HAVE_STIME)
#ifndef NETSNMP_NO_WRITE_SUPPORT
int
ns_set_time(int action,
            u_char * var_val,
            u_char var_val_type,
            size_t var_val_len,
            u_char * statP, oid * name, size_t name_len)
{

    static time_t oldtime=0;

    switch (action) {
        case RESERVE1:
            /* check type */
            if (var_val_type != ASN_OCTET_STR) {
                snmp_log(LOG_ERR,"write to ns_set_time not ASN_OCTET_STR\n");
                return SNMP_ERR_WRONGTYPE;
            }
            if (var_val_len != 8 && var_val_len!=11) {
                snmp_log(LOG_ERR,"write to ns_set_time not a proper length\n");
                return SNMP_ERR_WRONGVALUE;
            }
            break;

        case RESERVE2:
            break;

        case FREE:
            break;

        case ACTION: {
            long status=0;
            time_t seconds=0;
            struct tm newtimetm;
            int hours_from_utc= 0;
            int minutes_from_utc= 0;

            if (var_val_len == 11) {
                /* timezone inforamation was present */
                hours_from_utc=(int)var_val[9];
                minutes_from_utc=(int)var_val[10];
            }

            newtimetm.tm_sec=(int)var_val[6];;
            newtimetm.tm_min=(int)var_val[5];
            newtimetm.tm_hour=(int)var_val[4];

            newtimetm.tm_mon=(int)var_val[2]-1;
            newtimetm.tm_year=256*(int)var_val[0]+(int)var_val[1]-1900;
            newtimetm.tm_mday=(int)var_val[3];

            /* determine if day light savings time in effect DST */
            if ( ( hours_from_utc*60*60+minutes_from_utc*60 ) == abs(timezone) ) {
                newtimetm.tm_isdst=0;
            } else {
                newtimetm.tm_isdst=1;
            } 

            /* create copy of old value */
            oldtime=time(NULL);

            seconds=mktime(&newtimetm);
            if(seconds == (time_t)-1) {
                snmp_log(LOG_ERR, "Unable to convert time value\n");
                return SNMP_ERR_GENERR;
            }
            status=stime(&seconds);
            if(status!=0) {
                snmp_log(LOG_ERR, "Unable to set time\n");
                return SNMP_ERR_GENERR;
            }
            break;
        }
        case UNDO: {
            /* revert to old value */
            int status=0;
            if(oldtime != 0) {
                status=stime(&oldtime);
                oldtime=0;    
                if(status!=0) {
                    snmp_log(LOG_ERR, "Unable to set time\n");
                    return SNMP_ERR_GENERR;
                }
            }
            break;
        }

        case COMMIT: {
            oldtime=0;    
            break;
        }
    }
    return SNMP_ERR_NOERROR;
}
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
#endif

                /*
                 *  Return the DeviceIndex corresponding
                 *   to the boot device
                 */
static int
get_load_dev(void)
{
    return (HRDEV_DISK << HRDEV_TYPE_SHIFT);    /* XXX */
} /* end get_load_dev */

static int
count_users(void)
{
    int             total = 0;
#ifndef WIN32
#if HAVE_UTMPX_H
#define setutent setutxent
#define pututline pututxline
#define getutent getutxent
#define endutent endutxent
    struct utmpx   *utmp_p;
#else
    struct utmp    *utmp_p;
#endif

    setutent();
    while ((utmp_p = getutent()) != NULL) {
#ifndef UTMP_HAS_NO_TYPE
        if (utmp_p->ut_type != USER_PROCESS)
            continue;
#endif
#ifndef UTMP_HAS_NO_PID
            /* This block of code fixes zombie user PIDs in the
               utmp/utmpx file that would otherwise be counted as a
               current user */
            if (kill(utmp_p->ut_pid, 0) == -1 && errno == ESRCH) {
                utmp_p->ut_type = DEAD_PROCESS;
                pututline(utmp_p);
                continue;
            }
#endif
            ++total;
    }
    endutent();
#else /* WIN32 */
   /* 
    * TODO - Error checking.
    */
	LPWKSTA_INFO_102 wkinfo;
	NET_API_STATUS nstatus;
	
	nstatus = NetWkstaGetInfo(NULL, 102, (LPBYTE*)&wkinfo);
	if (nstatus != NERR_Success) {
		return 0;
	}
	total = (int)wkinfo->wki102_logged_on_users;

	NetApiBufferFree(wkinfo);	
#endif /* WIN32 */
    return total;
}

#if defined(UTMP_FILE) && !HAVE_UTMPX_H

static FILE    *utmp_file;
static struct utmp utmp_rec;

void
setutent(void)
{
    if (utmp_file)
        fclose(utmp_file);
    utmp_file = fopen(UTMP_FILE, "r");
}

void
endutent(void)
{
    if (utmp_file) {
        fclose(utmp_file);
        utmp_file = NULL;
    }
}

struct utmp    *
getutent(void)
{
    if (!utmp_file)
        return NULL;
    while (fread(&utmp_rec, sizeof(utmp_rec), 1, utmp_file) == 1)
        if (*utmp_rec.ut_name && *utmp_rec.ut_line)
            return &utmp_rec;
    return NULL;
}

#endif                          /* UTMP_FILE */
