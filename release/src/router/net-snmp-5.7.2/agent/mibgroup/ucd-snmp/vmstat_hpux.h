/*
 *  vmstat_hpux.h
 *  Header file for vmstat_hpux module for UCD-SNMP
 *  Gary Edwards <garye@cadence.com>
 *
 */

/*
 * Prevent accidental double inclusions 
 */
#ifndef _MIBGROUP_VMSTAT_HPUX_H
#define _MIBGROUP_VMSTAT_HPUX_H

/*
 * Directive to include utility module 
 */
config_require(util_funcs/header_generic)

    /*
     * Make 64-bit pstat calls 
     */
#define _PSTAT64
    /*
     * MIB wants CPU_SYSTEM which is CP_SYS + CP_WAIT (see sys/dk.h) 
     */
#define CPU_SYSTEM 9
    /*
     * Time interval to gather system data 
     */
    /*
     * Lower value -> more accurate data, higher value -> less CPU usage 
     */
    /*
     * Value is in seconds 
     */
#define POLL_INTERVAL 60
    /*
     * How many snapshots of system data to keep.  Values returned are over 
     */
    /*
     * time difference between first and last snapshot 
     */
    /*
     * Using POLL_INTERVAL 60 and POLL_VALUES 5 we get the values 
     */
    /*
     * over five minutes, which is a reasonable figure 
     */
#define POLL_VALUES    5
    /*
     * Declared in vmstat_hpux.c, from prototype 
     */
     void            init_vmstat_hpux(void);

#endif                          /* _MIBGROUP_VMSTAT_HPUX_H */
