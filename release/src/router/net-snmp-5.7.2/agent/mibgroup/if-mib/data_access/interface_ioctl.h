/*
 * ioctl interface data access header
 *
 * $Id$
 */
#ifndef NETSNMP_ACCESS_INTERFACE_IOCTL_H
#define NETSNMP_ACCESS_INTERFACE_IOCTL_H

/*
 * need ipaddress functions to get ipversions of an interface
*/
config_require(ip-mib/data_access/ipaddress)

#ifdef __cplusplus
extern          "C" {
#endif

/**---------------------------------------------------------------------*/
/**/

int
netsnmp_access_interface_ioctl_physaddr_get(int fd,
                                            netsnmp_interface_entry *ifentry);

int
netsnmp_access_interface_ioctl_flags_get(int fd,
                                         netsnmp_interface_entry *ifentry);

int
netsnmp_access_interface_ioctl_flags_set(int fd,
                                         netsnmp_interface_entry *ifentry,
                                         unsigned int flags,
                                         int and_complement);

int
netsnmp_access_interface_ioctl_mtu_get(int fd,
                                       netsnmp_interface_entry *ifentry);

oid
netsnmp_access_interface_ioctl_ifindex_get(int fd, const char *name);

int
netsnmp_access_interface_ioctl_has_ipv4(int sd, const char *if_name,
                                        int if_index, u_int *flags);

/**---------------------------------------------------------------------*/

# ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_INTERFACE_IOCTL_H */
