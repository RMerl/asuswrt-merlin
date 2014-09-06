#ifdef NETSNMP_TRANSPORT_UDP_DOMAIN
netsnmp_udp_ctor();
#endif
#ifdef NETSNMP_TRANSPORT_TCP_DOMAIN
netsnmp_tcp_ctor();
#endif
#ifdef NETSNMP_TRANSPORT_ALIAS_DOMAIN
netsnmp_alias_ctor();
#endif
#ifdef NETSNMP_TRANSPORT_UDPIPV6_DOMAIN
netsnmp_udpipv6_ctor();
#endif
#ifdef NETSNMP_TRANSPORT_TCPIPV6_DOMAIN
netsnmp_tcpipv6_ctor();
#endif
