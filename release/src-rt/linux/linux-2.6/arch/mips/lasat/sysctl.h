/*
 * LASAT sysctl values
 */

#ifndef _LASAT_SYSCTL_H
#define _LASAT_SYSCTL_H

/* /proc/sys/lasat */
enum {
	LASAT_CPU_HZ=1,
	LASAT_BUS_HZ,
	LASAT_MODEL,
	LASAT_PRID,
	LASAT_IPADDR,
	LASAT_NETMASK,
	LASAT_BCAST,
	LASAT_PASSWORD,
	LASAT_SBOOT,
	LASAT_RTC,
	LASAT_NAMESTR,
	LASAT_TYPESTR,
};

#endif /* _LASAT_SYSCTL_H */
