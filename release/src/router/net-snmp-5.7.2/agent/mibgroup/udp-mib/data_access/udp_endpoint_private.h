int netsnmp_arch_udp_endpoint_init(void);
int netsnmp_arch_udp_endpoint_container_load(netsnmp_container *, u_int);
int netsnmp_arch_udp_endpoint_entry_init(netsnmp_udp_endpoint_entry *);
void netsnmp_arch_udp_endpoint_entry_cleanup(netsnmp_udp_endpoint_entry *);
int netsnmp_arch_udp_endpoint_entry_delete(netsnmp_udp_endpoint_entry *);
int netsnmp_arch_udp_endpoint_entry_copy(netsnmp_udp_endpoint_entry *,
                                    netsnmp_udp_endpoint_entry *);
