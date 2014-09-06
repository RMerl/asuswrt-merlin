/*
 *  vmstat_aix4.h
 *  Header file for vmstat_aix4 module for UCD-SNMP
 *  Michael Kukat <michael.kukat@to.com>
 *  see vmstat_aix4.c for revision history and additional comments
 *
 */

/*
 * Prevent accidental double inclusions 
 */
#ifndef _MIBGROUP_VMSTAT_AIX4_H
#define _MIBGROUP_VMSTAT_AIX4_H

    /*
     * Directive to include utility module 
     */
config_require(util_funcs/header_generic)

    /*
     * we have to define the index ourselves, because perfstat structures
     * use named fields in the structure for those.
     */
#define CPU_USER   0
#define CPU_SYSTEM 1
#define CPU_IDLE   2
#define CPU_WAIT   3
#define CPU_STATES 4
    /*
     * Time interval to gather system data 
     * Lower value -> more accurate data, higher value -> less CPU usage 
     * Value is in seconds 
     */
#define POLL_INTERVAL 60
    /*
     * How many snapshots of system data to keep.  Values returned are over 
     * time difference between first and last snapshot 
     * Using POLL_INTERVAL 60 and POLL_VALUES 5 we get the values 
     * over five minutes, which is a reasonable figure 
     */
#define POLL_VALUES    5
    /*
     * Declared in vmstat_aix4.c, from prototype 
     */
     void            init_vmstat_aix4(void);

#endif                          /* _MIBGROUP_VMSTAT_AIX4_H */
