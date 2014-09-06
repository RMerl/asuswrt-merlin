/*
 *  vmstat_dynix.h
 *  Header file for vmstat_dynix module for UCD-SNMP
 *  Patrick Hess <phess@phess.best.vwh.net>
 *
 */

/*
 * Prevent accidental double inclusions 
 */
#ifndef _MIBGROUP_VMSTAT_DYNIX_H
#define _MIBGROUP_VMSTAT_DYNIX_H

/*
 * Directive to include utility module 
 */
config_require(util_funcs/header_generic)

    /*
     * MIB wants V_CPU_SYSTEM which is sysinfo V_CPU_KERNEL + V_CPU_WAIT 
     */
#define V_CPU_SYSTEM 4
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
     * Declared in vmstat_dynix.c, from prototype 
     */
     void            init_vmstat_dynix(void);

#endif                          /* _MIBGROUP_VMSTAT_DYNIX_H */
