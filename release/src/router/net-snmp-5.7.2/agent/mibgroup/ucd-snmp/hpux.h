/*
 *  hpux specific mib sections
 *
 */
#ifndef _MIBGROUP_HPUX_H
#define _MIBGROUP_HPUX_H

FindVarMethod   var_hp;
WriteMethod     writeHP;


#define TRAPAGENT 128.120.57.92

#define HPCONF 1
#define HPRECONFIG 2
#define HPFLAG 3
#define HPLOGMASK 4
#define HPSTATUS 6
#define HPTRAP 101

#endif                          /* _MIBGROUP_HPUX_H */
