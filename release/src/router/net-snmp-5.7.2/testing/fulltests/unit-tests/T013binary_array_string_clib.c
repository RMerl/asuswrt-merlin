/* HEADER Testing binary string array */

static const char test_name[] = "binary-array-of-strings-test";
const char o1[] = "zebra";
const char o2[] = "b-two";
const char o3[] = "b";
const char o4[] = "cedar";
const char o5[] = "alpha";
const char ox[] = "dev";
const char oy[] = "aa";
const char* const o_last = o1;
const char *ip;
const char *const a[] = { ox, oy };
const char *const b[] = { o4, o2, o3, o1, o5 };
netsnmp_container *c;
int i;

init_snmp(test_name);

c = netsnmp_container_get_binary_array();
c->compare = (netsnmp_container_compare*)strcmp;
    
for (i = 0; i < sizeof(b)/sizeof(b[0]); ++i)
    CONTAINER_INSERT(c, b[i]);

for (ip = CONTAINER_FIRST(c); ip; ip = CONTAINER_NEXT(c, ip)) {
    for (i = sizeof(b)/sizeof(b[0]) - 1; i >= 0; --i)
        if (c->compare(ip, b[i]) == 0)
            break;
    OKF(i >= 0, ("string b[%d] = \"%s\" present", i, b[i]));
}

for (i = 0; i < sizeof(b)/sizeof(b[0]); ++i) {
    ip = CONTAINER_FIND(c, b[i]);
    OKF(ip, ("b[%d] = \"%s\" present", i, b[i]));
    ip = CONTAINER_NEXT(c, b[i]);
    if (c->compare(b[i], o_last) < 0)
        OKF(ip && c->compare(b[i], ip) < 0,
            ("Successor of b[%d] = \"%s\" is \"%s\"", i, b[i], ip));
    else
        OKF(!ip, ("No successor found for b[%d] = \"%s\"", i, b[i]));
}

for (i = 0; i < sizeof(a)/sizeof(a[0]); ++i) {
    ip = CONTAINER_FIND(c, a[i]);
    OKF(!ip, ("a[%d] = \"%s\" absent", i, a[i]));
    ip = CONTAINER_NEXT(c, a[i]);
    if (c->compare(a[i], o_last) < 0)
        OKF(ip && c->compare(ip, a[i]) > 0,
            ("Successor of a[%d] = \"%s\" is \"%s\"", i, a[i], ip));
    else
        OKF(!ip, ("No successor found for a[%d] = \"%s\"", i, a[i]));
}

while ((ip = CONTAINER_FIRST(c)))
  CONTAINER_REMOVE(c, ip);
CONTAINER_FREE(c);

snmp_shutdown(test_name);
