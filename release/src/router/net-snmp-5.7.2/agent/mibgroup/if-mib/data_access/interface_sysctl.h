/*
 *  Interface MIB architecture support
 *
 *  Based on patch 1362403, submited by Rojer
 *
 * $Id$
 */
#ifndef INTERFACE_SYSCTL_H
#define INTERFACE_SYSCTL_H 

#ifdef __cplusplus
extern          "C" {
#endif

int
netsnmp_access_interface_sysctl_container_load(netsnmp_container* container,
                                               u_int load_flags);

void
netsnmp_access_interface_sysctl_ifmedia_to_speed(int media, u_int *speed,
                                                 u_int *speed_high);

int
netsnmp_access_interface_sysctl_get_if_speed(char *name, u_int *speed,
                                             u_int *speed_high);

# ifdef __cplusplus
}
#endif

#endif
