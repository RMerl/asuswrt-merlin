int netsnmp_arch_tcpconn_container_load(netsnmp_container *, u_int);
int netsnmp_arch_tcpconn_entry_init(netsnmp_tcpconn_entry *);
void netsnmp_arch_tcpconn_entry_cleanup(netsnmp_tcpconn_entry *);
int netsnmp_arch_tcpconn_entry_delete(netsnmp_tcpconn_entry *);
int netsnmp_arch_tcpconn_entry_copy(netsnmp_tcpconn_entry *,
                                    netsnmp_tcpconn_entry *);
