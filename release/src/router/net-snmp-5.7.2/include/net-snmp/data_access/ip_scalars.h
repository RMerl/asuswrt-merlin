#ifndef NETSNMP_ACCESS_IP_SCALARS_H
#define NETSNMP_ACCESS_IP_SCALARS_H

#ifdef __cplusplus
extern          "C" {
#endif

int netsnmp_arch_ip_scalars_ipForwarding_get(u_long *value);
int netsnmp_arch_ip_scalars_ipForwarding_set(u_long value);

int netsnmp_arch_ip_scalars_ipv6IpForwarding_get(u_long *value);
int netsnmp_arch_ip_scalars_ipv6IpForwarding_set(u_long value);


#ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_IP_SCALARS_H */
