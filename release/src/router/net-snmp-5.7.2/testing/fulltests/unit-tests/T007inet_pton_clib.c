/* HEADER Testing inet_pton() and inet_ntop() */

int i;
char str[128];
struct in_addr in_addr;
struct in6_addr in6_addr;

static const struct { const char* s; unsigned char b[4]; } in_testdata[] = {
    { "0.0.0.0",         {   0,   0,   0,   0 } },
    { "1.2.3.4",         {   1,   2,   3,   4 } },
    { "255.255.255.255", { 255, 255, 255, 255 } },
};

static const struct { const char* s; unsigned char b[16]; } in6_testdata[] = {
    { "::",   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, },
    { "::1",  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, },
    { "1::",  { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, },
    { "1::1", { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, },
};

for (i = 0; i < sizeof(in_testdata)/sizeof(in_testdata[0]); ++i) {
    const unsigned char *const b = in_testdata[i].b;

    OKF(inet_pton(AF_INET, in_testdata[i].s, &in_addr) == 1,
	("IPv4 inet_pton(%s)", in_testdata[i].s));
    OK(ntohl(in_addr.s_addr)
       == ((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]),
       "inet_pton() value");
    OK(inet_ntop(AF_INET, &in_addr, str, sizeof(str)) == str,
       "IPv4 inet_ntop()");
    OKF(strcmp(in_testdata[i].s, str) == 0,
	("%s =?= %s", in_testdata[i].s, str));
}


for (i = 0; i < sizeof(in6_testdata)/sizeof(in6_testdata[0]); ++i) {
    const unsigned char *const b = in6_testdata[i].b;
    const unsigned char *const r = (void *)&in6_addr;
    int result, j;

    result = inet_pton(AF_INET6, in6_testdata[i].s, &in6_addr);
    OKF(result == 1,
	("IPv6 inet_pton(%s) -> %d; "
	 " %02x%02x:%02x%02x:%02x%02x:%02x%02x"
	 ":%02x%02x:%02x%02x:%02x%02x:%02x%02x",
	 in6_testdata[i].s, result,
	 r[0], r[1], r[ 2], r[ 3], r[ 4], r[ 5], r[ 6], r[ 7],
	 r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]
	 ));
    for (j = 0; j < 16; ++j)
	printf("%02x ", b[j]);
    printf("\n");
    for (j = 0; j < 16; ++j)
	OKF(r[j] == b[j],
	    ("IPv6 inet_pton() value (%#02x =?= %#02x)", r[j], b[j]));
    OK(inet_ntop(AF_INET6, &in6_addr, str, sizeof(str)) == str,
       "IPv6 inet_ntop()");
    OKF(strcmp(in6_testdata[i].s, str) == 0,
	("%s =?= %s", in6_testdata[i].s, str));
}
