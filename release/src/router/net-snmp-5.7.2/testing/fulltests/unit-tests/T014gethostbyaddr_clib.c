/* HEADER Testing netsnmp_gethostbyaddr() */

SOCK_STARTUP;

{
    int ran_test = 0;
#ifdef HAVE_GETHOSTBYADDR
    struct hostent *h;
    struct in_addr v4loop;
    struct sockaddr_in sin_addr;
    int s;

    v4loop.s_addr = htonl(INADDR_LOOPBACK);
    memset(&sin_addr, 0, sizeof(sin_addr));
    sin_addr.sin_family = AF_INET;
    sin_addr.sin_addr = v4loop;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
        if (bind(s, (struct sockaddr *)&sin_addr, sizeof(sin_addr)) >= 0) {
            h = netsnmp_gethostbyaddr(&v4loop, sizeof(v4loop), AF_INET);
            OKF(h && strcmp(h->h_name, "localhost") == 0,
                ("127.0.0.1 lookup (%s)", h ? h->h_name : "(failed)"));
            ran_test = 1;
        }
        close(s);
    }
#endif
    if (!ran_test)
        OKF(1, ("Skipped IPv4 test"));
}

{
    struct hostent *h;
    static const struct in6_addr v6loop = IN6ADDR_LOOPBACK_INIT;
    struct sockaddr_in6 sin6_addr;
    int s, ran_test = 0;

    memset(&sin6_addr, 0, sizeof(sin6_addr));
    sin6_addr.sin6_family = AF_INET6;
    sin6_addr.sin6_addr = v6loop;
    s = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s >= 0) {
        if (bind(s, (struct sockaddr*)&sin6_addr, sizeof(sin6_addr)) >= 0) {
            h = netsnmp_gethostbyaddr(&v6loop, sizeof(v6loop), AF_INET6);
            OKF(h && strcmp(h->h_name, "localhost") == 0,
                ("::1 lookup (%s)", h ? h->h_name : "(failed)"));
            ran_test = 1;
        }
        close(s);
    }
    if (!ran_test)
        OKF(1, ("Skipped IPv6 test"));
}

SOCK_CLEANUP;
