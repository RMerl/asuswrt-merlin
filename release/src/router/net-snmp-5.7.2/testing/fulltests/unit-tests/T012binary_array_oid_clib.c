/* HEADER Testing binary OID array */

static const char test_name[] = "binary-array-of-OIDs-test";
oid o1 = 1;
oid o2 = 2;
oid o3 = 6;
oid o4 = 8;
oid o5 = 9;
oid ox = 7;
oid oy = 10;
netsnmp_index i1, i2, i3, i4, i5, ix, iy, *ip;
const netsnmp_index *const i_last = &i5;
netsnmp_index *a[] = { &ix, &iy };
netsnmp_index *b[] = { &i4, &i2, &i3, &i1, &i5 };
netsnmp_container *c;
int i;

init_snmp(test_name);

c = netsnmp_container_get_binary_array();
c->compare = netsnmp_compare_netsnmp_index;
    
i1.oids = &o1;
i2.oids = &o2;
i3.oids = &o3;
i4.oids = &o4;
i5.oids = &o5;
ix.oids = &ox;
iy.oids = &oy;
i1.len = i2.len = i3.len = i4.len = i5.len = ix.len = iy.len = 1;

for (i = 0; i < sizeof(b)/sizeof(b[0]); ++i)
    CONTAINER_INSERT(c, b[i]);

for (ip = CONTAINER_FIRST(c); ip; ip = CONTAINER_NEXT(c, ip)) {
    for (i = sizeof(b)/sizeof(b[0]) - 1; i >= 0; --i)
        if (c->compare(ip, b[i]) == 0)
            break;
    OKF(i >= 0, ("OID b[%d] = %" NETSNMP_PRIo "d present", i, b[i]->oids[0]));
}

for (i = 0; i < sizeof(b)/sizeof(b[0]); ++i) {
    ip = CONTAINER_FIND(c, b[i]);
    OKF(ip, ("Value b[%d] = %" NETSNMP_PRIo "d present", i, b[i]->oids[0]));
    ip = CONTAINER_NEXT(c, b[i]);
    if (c->compare(b[i], i_last) < 0)
        OKF(ip && c->compare(b[i], ip) < 0,
            ("Successor of b[%d] = %" NETSNMP_PRIo "d is %" NETSNMP_PRIo "d",
             i, b[i]->oids[0], ip->oids[0]));
    else
        OKF(!ip, ("No successor found for b[%d] = %" NETSNMP_PRIo "d", i,
                  b[i]->oids[0]));
}

for (i = 0; i < sizeof(a)/sizeof(a[0]); ++i) {
    ip = CONTAINER_FIND(c, a[i]);
    OKF(!ip, ("a[%d] = %" NETSNMP_PRIo "d absent", i, a[i]->oids[0]));
    ip = CONTAINER_NEXT(c, a[i]);
    if (c->compare(a[i], i_last) < 0)
        OKF(ip && c->compare(ip, a[i]) > 0,
            ("Successor of a[%d] = %" NETSNMP_PRIo "d is %" NETSNMP_PRIo "d",
             i, a[i]->oids[0], ip->oids[0]));
    else
        OKF(!ip, ("No successor found for a[%d] = %" NETSNMP_PRIo "d", i,
                  a[i]->oids[0]));
}

while ((ip = CONTAINER_FIRST(c)))
  CONTAINER_REMOVE(c, ip);
CONTAINER_FREE(c);

snmp_shutdown(test_name);
