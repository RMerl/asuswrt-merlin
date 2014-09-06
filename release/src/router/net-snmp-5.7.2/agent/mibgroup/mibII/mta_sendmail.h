#ifndef _MIBGROUP_MTA_H
#define _MIBGROUP_MTA_H

config_add_mib(MTA-MIB)
config_add_mib(NETWORK-SERVICES-MIB)

     void            init_mta_sendmail(void);

#endif                          /* _MIBGROUP_MTA_H */
