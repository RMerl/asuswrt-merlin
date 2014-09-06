/*
 *  vmstat_hpux.c
 *  UCD SNMP module for systemStats section of UCD-SNMP-MIB for HPUX 10.x/11.x
 */

/*
 * To make lint skip the debug code and stop complaining 
 */
#ifdef __lint
#define NETSNMP_NO_DEBUGGING 1
#endif

/*
 * Includes start here 
 */

/*
 * UCD-SNMP config details 
 */
#include <net-snmp/net-snmp-config.h>

/*
 * Standard includes 
 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>

/*
 * pstat and sysinfo structs 
 */
#include <sys/pstat.h>
#include <sys/dk.h>


/*
 * Includes needed for all modules 
 */
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "mibdefs.h"

/*
 * Utility functions for UCD-SNMP 
 */
#include "util_funcs/header_generic.h"

/*
 * Header file for this module 
 */
#include "vmstat.h"
#include "vmstat_hpux.h"

/*
 * Includes end here 
 */


/*
 * Global structures start here 
 */

/*
 * A structure to save data gathered from the kernel pstat interface to.
 * CPUSTATES are defined as (see sys/dk.h):
 * #define CPUSTATES       9       -- number of CPU states
 * #define CP_USER         0       -- user mode of USER process
 * #define CP_NICE         1       -- user mode of USER process at nice priority
 * #define CP_SYS          2       -- kernel mode of USER process
 * #define CP_IDLE         3       -- IDLE mode
 * #define CP_WAIT         4       
 * #define CP_BLOCK        5       -- time blocked on a spinlock
 * #define CP_SWAIT        6       -- time blocked on the kernel semaphore
 * #define CP_INTR         7       -- INTERRUPT mode
 * #define CP_SSYS         8       -- kernel mode of KERNEL process
 */

struct cpu_stat_snapshot {
    time_t          css_time;
    unsigned int    css_cpus;
    unsigned long long css_swapin;
    unsigned long long css_swapout;
    unsigned long long css_blocks_read;
    unsigned long long css_blocks_write;
    unsigned long long css_interrupts;
    unsigned long long css_context_sw;
    unsigned long long css_cpu[CPUSTATES];
};

/*
 * Define a structure to hold kernel static information 
 */
struct pst_static pst;

/*
 * Global structures end here 
 */

/*
 * Global variables start here 
 */

/*
 * Variables for the calculated values, filled in update_stats    
 * Need to be global since we need them in more than one function 
 */
static unsigned long swapin;
static unsigned long swapout;
static unsigned long blocks_read;
static unsigned long blocks_write;
static unsigned long interrupts;
static unsigned long context_sw;

/*
 * Since MIB wants CPU_SYSTEM, which is CP_SYS + CP_WAIT (see sys/dk.h) 
 */
static long     cpu_perc[CPUSTATES + 1];

/*
 * How many snapshots we have already taken, needed for the first 
 * POLL_INTERVAL * POLL_VALUES seconds of agent running 
 */
static unsigned int number_of_snapshots;

/*
 * The place to store the snapshots of system data in 
 */
static struct cpu_stat_snapshot snapshot[POLL_VALUES + 1];

/*
 * And one for the raw counters, which we fill when the raw values are 
 * requested, as opposed to the absolute values, which are taken every 
 * POLL_INTERVAL seconds and calculated over POLL_INTERVAL * POLL_VALUES time 
 */
static struct cpu_stat_snapshot raw_values;

/*
 * Global variables end here 
 */


/*
 * Functions start here 
 */

/*
 * Function prototype 
 */
static void     update_stats(unsigned int registrationNumber,
                             void *clientarg);
static int      take_snapshot(struct cpu_stat_snapshot *css);

/*
 * init_vmstat_hpux starts here 
 */
/*
 * Init function for this module, from prototype 
 * Defines variables handled by this module, defines root OID for 
 * this module and registers it with the agent 
 */

FindVarMethod var_extensible_vmstat;

void
init_vmstat_hpux(void)
{

    /*
     * Which variables do we service ? 
     */
    struct variable2 extensible_vmstat_variables[] = {
        {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {MIBINDEX}},
        {ERRORNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {ERRORNAME}},
        {SWAPIN, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SWAPIN}},
        {SWAPOUT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SWAPOUT}},
        {IOSENT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IOSENT}},
        {IORECEIVE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IORECEIVE}},
        {SYSINTERRUPTS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SYSINTERRUPTS}},
        {SYSCONTEXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SYSCONTEXT}},
        {CPUUSER, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPUUSER}},
        {CPUSYSTEM, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPUSYSTEM}},
        {CPUIDLE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPUIDLE}},
        {CPURAWUSER, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWUSER}},
        {CPURAWNICE, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWNICE}},
        {CPURAWSYSTEM, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWSYSTEM}},
        {CPURAWIDLE, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWIDLE}},
        {CPURAWWAIT, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWWAIT}},
        {CPURAWKERNEL, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWKERNEL}},
        {IORAWSENT, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IORAWSENT}},
        {IORAWRECEIVE, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IORAWRECEIVE}},
        /*
         * Future use: 
         */
        /*
         * {ERRORFLAG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         *  var_extensible_vmstat, 1, {ERRORFLAG }},
         * {ERRORMSG, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         *  var_extensible_vmstat, 1, {ERRORMSG }}
         */
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're 
     * registering underneath 
     */
    oid             vmstat_variables_oid[] = { NETSNMP_UCDAVIS_MIB, 11 };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    /*
     * LINTED Trust me, I know what I'm doing 
     */
    REGISTER_MIB("ucd-snmp/vmstat", extensible_vmstat_variables, variable2,
                 vmstat_variables_oid);

    /*
     * Start with some useful data 
     */
    update_stats(0, NULL);

    /*
     * update_stats is run every POLL_INTERVAL seconds using this routine 
     * (see 'man snmp_alarm') 
     * This is only executed once to get some useful data in the beginning 
     */
    if (snmp_alarm_register(5, NULL, update_stats, NULL) == 0) {
        snmp_log(LOG_WARNING,
                 "vmstat_hpux (init): snmp_alarm_register failed.\n");
    }
    /*
     * This is the one that runs update_stats every POLL_INTERVAL seconds 
     */
    if (snmp_alarm_register(POLL_INTERVAL, SA_REPEAT, update_stats, NULL)
        == 0) {
        snmp_log(LOG_ERR,
                 "vmstat_hpux (init): snmp_alarm_register failed, cannot service requests.\n");
    }

}                               /* init_vmstat_hpux ends here */

/*
 * Data collection function take_snapshot starts here 
 * Get data from kernel and save into the snapshot strutcs 
 * Argument is the snapshot struct to save to. Global anyway, but looks nicer 
 */
static int
take_snapshot(struct cpu_stat_snapshot *css)
{
    /*
     * Variables start here 
     */

    struct pst_dynamic psd;
    struct pst_processor *psp;
    struct pst_vminfo psv;

    /*
     * time counter 
     */
    time_t          current_time;

    /*
     * The usual stuff to count on, err, by 
     */
    int             i;

    /*
     * Variables end here 
     */

    /*
     * Function starts here 
     */

    /*
     * Get time 
     */
    time(&current_time);

    /*
     * If we have just gotten the data, return the values from last run (skip if-clause) 
     * This happens on a snmpwalk request.  No need to read the pstat again 
     * if we just did it less than 2 seconds ago 
     * Jumps into if-clause either when snapshot is empty or when too old 
     */

    if ((css->css_time == 0) || (current_time > css->css_time + 2)) {
        /*
         * Make sure we clean up before we put new data into snapshot 
         */
        memset(css, 0, sizeof *css);

        /*
         * Update timer 
         */
        css->css_time = current_time;

        if (pstat_getdynamic(&psd, sizeof(psd), (size_t) 1, 0) != -1) {
            css->css_cpus = psd.psd_proc_cnt;
            DEBUGMSGTL(("take_snapshot", "*** Number of CPUs: %d\n",
                        css->css_cpus));

            /*
             * We need a for-loop for the CPU values 
             */
            for (i = 0; i < CPUSTATES; i++) {
                css->css_cpu[i] = (unsigned long long) psd.psd_cpu_time[i];
                DEBUGMSGTL(("take_snapshot",
                            "*** Time for CPU state %d: %d\n", i,
                            psd.psd_cpu_time[i]));
            }
            psp =
                (struct pst_processor *) malloc(css->css_cpus *
                                                sizeof(*psp));
            if (pstat_getprocessor(psp, sizeof(*psp), css->css_cpus, 0) !=
                -1) {
                int             i;
                for (i = 0; i < css->css_cpus; i++) {
                    css->css_blocks_read = psp[i].psp_phread;
                    css->css_blocks_write = psp[i].psp_phwrite;
                }
            } else
                snmp_log(LOG_ERR,
                         "vmstat_hpux (take_snapshot): pstat_getprocessor failed!\n");
        } else
            snmp_log(LOG_ERR,
                     "vmstat_hpux (take_snapshot): pstat_getdynamic failed!\n");

        if (pstat_getvminfo(&psv, sizeof(psv), (size_t) 1, 0) != -1) {
            css->css_swapin = psv.psv_sswpin;
            css->css_swapout = psv.psv_sswpout;
            css->css_interrupts = psv.psv_sintr;
            css->css_context_sw = psv.psv_sswtch;
        } else
            snmp_log(LOG_ERR,
                     "vmstat_hpux (take_snapshot): pstat_getvminfo failed!\n");

    }

    /*
     * All engines running at warp speed, no problems (if there are any engines, that is) 
     */
    return (css->css_cpus > 0 ? 0 : -1);
}                               /* take_snapshot ends here */

/*
 * This gets called every POLL_INTERVAL seconds to update the snapshots.  It takes a new snapshot and 
 * drops the oldest one.  This way we move the time window so we always take the values over 
 * POLL_INTERVAL * POLL_VALUES seconds and update the data used every POLL_INTERVAL seconds 
 * The alarm timer is in the init function of this module (snmp_alarm_register) 
 */
/*
 * ARGSUSED0 
 */
static void
update_stats(unsigned int registrationNumber, void *clientarg)
{
    /*
     * The time between the samples we compare 
     */
    time_t          time_diff;

    /*
     * Easier to use these than the snapshots, short hand pointers 
     */
    struct cpu_stat_snapshot *css_old, *css_new;

    /*
     * The usual stuff to count on, err, by 
     */
    int             i;

    /*
     * The sum of the CPU ticks that have passed on the different CPU states, so we can calculate 
     * the percentages of each state 
     */
    unsigned long long cpu_sum = 0;

    DEBUGMSGTL(("ucd-snmp/vmstat_hpux.c:update_stats",
                "updating stats\n"));

    /*
     * Take the current snapshot 
     */
    if (take_snapshot(&snapshot[0]) == -1) {
        snmp_log(LOG_WARNING,
                 "vmstat_hpux (update_stats): Something went wrong with take_snapshot.\n");
        return;
    }

    /*
     * Do we have some data we can use ?  An issue right after the start of the agent 
     */
    if (number_of_snapshots > 0) {
        /*
         * Huh, the number of CPUs changed during run time.  That is indeed s.th. worth noting, we 
         * output a humorous (more or less) syslog message and need to retake the snapshots 
         */
        if (snapshot[0].css_cpus != snapshot[1].css_cpus) {
            if (snapshot[0].css_cpus > snapshot[1].css_cpus) {
                snmp_log(LOG_NOTICE,
                         "vmstat_hpux (update_stats): Cool ! Number of CPUs increased, must be hot-pluggable.\n");
            } else {
                snmp_log(LOG_NOTICE,
                         "vmstat_hpux (update_stats): Lost at least one CPU, RIP.\n");
            }
            /*
             * Make all snapshots but the current one invalid 
             */
            number_of_snapshots = 1;
            /*
             * Move the current one in the "first" [1] slot 
             */
            memmove(&snapshot[1], &snapshot[0], sizeof snapshot[0]);
            /*
             * Erase the current one 
             */
            memset(&snapshot[0], 0, sizeof snapshot[0]);
            /*
             * Try to get a new snapshot in five seconds so we can return s.th. useful 
             */
            if (snmp_alarm_register(5, NULL, update_stats, NULL) == 0) {
                snmp_log(LOG_WARNING,
                         "vmstat_hpux (update_stats): snmp_alarm_register failed.\n");
            }
            return;
        }

        /*
         * Short hand pointers 
         */
        css_new = &snapshot[0];
        css_old = &snapshot[number_of_snapshots];

        /*
         * How much time has passed between the snapshots we get the values from ? 
         */
        time_diff =
            (snapshot[0].css_time -
             snapshot[number_of_snapshots].css_time);

        DEBUGMSGTL(("ucd-snmp/vmstat_hpux.c:update_stats",
                    "time_diff: %lld\n", time_diff));

        /*
         * swapin and swapout are in pages, MIB wants kB/s,so we just need to get kB and seconds 
         * For the others we need to get value per second 
         * Retreive static information to obtain memory page_size 
         */
        if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "vmstat_hpux: pstat_getstatic failed!\n");
        }

        /*
         * LINTED cast needed, really 
         */
        swapin =
            (unsigned int) ((css_new->css_swapin - css_old->css_swapin) *
                            pst.page_size / 1024 / time_diff);
        /*
         * LINTED cast needed, really 
         */
        swapout =
            (unsigned int) ((css_new->css_swapout - css_old->css_swapout) *
                            pst.page_size / 1024 / time_diff);
        /*
         * LINTED cast needed, really 
         */
        blocks_read =
            (unsigned
             int) ((css_new->css_blocks_read -
                    css_old->css_blocks_read) / time_diff);
        /*
         * LINTED cast needed, really 
         */
        blocks_write =
            (unsigned
             int) ((css_new->css_blocks_write -
                    css_old->css_blocks_write) / time_diff);
        /*
         * LINTED cast needed, really 
         */
        interrupts =
            (unsigned
             int) ((css_new->css_interrupts -
                    css_old->css_interrupts) / time_diff);
        /*
         * LINTED cast needed, really 
         */
        context_sw =
            (unsigned
             int) ((css_new->css_context_sw -
                    css_old->css_context_sw) / time_diff);

        /*
         * Loop thru all the CPUSTATES and get the differences 
         */
        for (i = 0; i < CPUSTATES; i++) {
            cpu_sum += (css_new->css_cpu[i] - css_old->css_cpu[i]);
        }

        /*
         * Now calculate the absolute percentage values 
         * Looks somewhat complicated sometimes but tries to get around using floats to increase speed 
         */
        for (i = 0; i < CPUSTATES; i++) {
            /*
             * Since we don't return fractions we use + 0.5 to get between 99 and 101 percent adding the values 
             * together, otherwise we would get less than 100 most of the time 
             */
            /*
             * LINTED has to be 'long' 
             */
            cpu_perc[i] =
                (long) (((css_new->css_cpu[i] -
                          css_old->css_cpu[i]) * 100 +
                         (cpu_sum / 2)) / cpu_sum);
        }

        /*
         * As said before, MIB wants CPU_SYSTEM which is CP_SYS + CP_WAIT 
         */
        /*
         * LINTED has to be 'long' 
         */
        cpu_perc[CPU_SYSTEM] =
            (long) ((((css_new->css_cpu[CP_SYS] - css_old->css_cpu[CP_SYS])
                      + (css_new->css_cpu[CP_WAIT] -
                         css_old->css_cpu[CP_WAIT]))
                     * 100 + (cpu_sum / 2)) / cpu_sum);
    }

    /*
     * Make the current one the first one and move the whole thing one place down 
     */
    memmove(&snapshot[1], &snapshot[0],
            (size_t) (((char *) &snapshot[POLL_VALUES]) -
                      ((char *) &snapshot[0])));

    /*
     * Erase the current one 
     */
    memset(&snapshot[0], 0, sizeof snapshot[0]);

    /*
     * Only important on start up, we keep track of how many snapshots we have taken so far 
     */
    if (number_of_snapshots < POLL_VALUES) {
        number_of_snapshots++;
    }
}                               /* update_stats ends here */

/*
 * *var_extensible_vmstat starts here 
 * The guts of the module, this routine gets called to service a request 
 */
unsigned char *
var_extensible_vmstat(struct variable *vp,
                      oid * name,
                      size_t * length,
                      int exact,
                      size_t * var_len, WriteMethod ** write_method)
{
    /*
     * Needed for returning the values 
     */
    static long     long_ret;
    static char     errmsg[300];

    /*
     * set to 0 as default 
     */
    long_ret = 0;

    /*
     * generic check whether the options passed make sense and whether the 
     * right variable is requested 
     */
    if (header_generic(vp, name, length, exact, var_len, write_method) !=
        MATCH_SUCCEEDED) {
        return (NULL);
    }

    /*
     * The function that actually returns s.th. 
     */
    switch (vp->magic) {
    case MIBINDEX:
        long_ret = 1;
        return ((u_char *) (&long_ret));
    case ERRORNAME:            /* dummy name */
        sprintf(errmsg, "systemStats");
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    case SWAPIN:
        return ((u_char *) (&swapin));
    case SWAPOUT:
        return ((u_char *) (&swapout));
    case IOSENT:
        return ((u_char *) (&blocks_write));
    case IORECEIVE:
        return ((u_char *) (&blocks_read));
    case SYSINTERRUPTS:
        return ((u_char *) (&interrupts));
    case SYSCONTEXT:
        return ((u_char *) (&context_sw));
    case CPUUSER:
        return ((u_char *) (&cpu_perc[CP_USER]));
    case CPUSYSTEM:
        return ((u_char *) (&cpu_perc[CPU_SYSTEM]));
    case CPUIDLE:
        return ((u_char *) (&cpu_perc[CP_IDLE]));
    case CPURAWUSER:
        take_snapshot(&raw_values);
        /*
         * LINTED has to be 'long' 
         */
        long_ret =
            (long) (raw_values.css_cpu[CP_USER] / raw_values.css_cpus);
        return ((u_char *) (&long_ret));
    case CPURAWNICE:
        take_snapshot(&raw_values);
        /*
         * LINTED has to be 'long' 
         */
        long_ret =
            (long) (raw_values.css_cpu[CP_NICE] / raw_values.css_cpus);
        return ((u_char *) (&long_ret));
    case CPURAWSYSTEM:
        take_snapshot(&raw_values);
        /*
         * LINTED has to be 'long' 
         */
        long_ret =
            (long) ((raw_values.css_cpu[CP_SYS] +
                     raw_values.css_cpu[CP_WAIT]) / raw_values.css_cpus);
        return ((u_char *) (&long_ret));
    case CPURAWIDLE:
        take_snapshot(&raw_values);
        /*
         * LINTED has to be 'long' 
         */
        long_ret =
            (long) (raw_values.css_cpu[CP_IDLE] / raw_values.css_cpus);
        return ((u_char *) (&long_ret));
    case CPURAWWAIT:
        take_snapshot(&raw_values);
        /*
         * LINTED has to be 'long' 
         */
        long_ret =
            (long) (raw_values.css_cpu[CP_WAIT] / raw_values.css_cpus);
        return ((u_char *) (&long_ret));
    case CPURAWKERNEL:
        take_snapshot(&raw_values);
        /*
         * LINTED has to be 'long' 
         */
        long_ret =
            (long) (raw_values.css_cpu[CP_SYS] / raw_values.css_cpus);
        return ((u_char *) (&long_ret));
    case IORAWSENT:
        take_snapshot(&raw_values);
        /*
         * LINTED has to be 'long' 
         */
        long_ret = (long) (raw_values.css_blocks_write);
        return ((u_char *) (&long_ret));
    case IORAWRECEIVE:
        take_snapshot(&raw_values);
        /*
         * LINTED has to be 'long' 
         */
        long_ret = (long) (raw_values.css_blocks_read);
        return ((u_char *) (&long_ret));

        /*
         * reserved for future use 
         */
        /*
         * case ERRORFLAG:
         * return((u_char *) (&long_ret));
         * case ERRORMSG:
         * return((u_char *) (&long_ret));
         */
    default:
        snmp_log(LOG_ERR,
                 "vmstat_hpux: Error in request, no match found.\n");
    }
    return (NULL);
}                               /* *var_extensible_vmstat ends here */

/*
 * Functions end here 
 */

/*
 * Program ends here 
 */
