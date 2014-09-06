/* HEADER Testing 64-bit integer operations (U64). */

int i, j;
char buf[22];
static const int64_t intval[] = {
    0,
    -1,
    1,
    37,
    0x7fffffffUL,
    0x80000000UL,
    0x99999999UL,
    0x7fffffffffffffffULL,
    0x8000000000000000ULL,
};

for (i = 0; i < sizeof(intval)/sizeof(intval[0]); ++i) {
    U64 a, b;
    a.low = (uint32_t)intval[i];
    a.high = (uint32_t)(intval[i] >> 32);
    printI64(buf, &a);
    read64(&b, buf);
    OKF(memcmp(&a, &b, sizeof(a)) == 0,
        ("[%d]: %" PRId64 " <> %s <> %" PRId64, i, intval[i], buf,
         ((uint64_t)b.high) << 32 | b.low));
}

for (i = 0; i < sizeof(intval)/sizeof(intval[0]); ++i) {
    for (j = i; j < sizeof(intval)/sizeof(intval[0]); ++j) {
        U64 a, b;
        uint64_t d;
        a.low = (uint32_t)intval[i];
        a.high = (uint32_t)(intval[i] >> 32);
        b.low = (uint32_t)intval[j];
        b.high = (uint32_t)(intval[j] >> 32);
        u64Incr(&a, &b);
        d = (uint64_t)a.high << 32 | a.low;
        OKF(intval[i] + intval[j] == d,
            ("%" PRId64 " + %" PRId64 " = %" PRId64 " <> %" PRId64, intval[i],
             intval[j], intval[i] + intval[j], d));
    }
}
        
for (i = 0; i < sizeof(intval)/sizeof(intval[0]); ++i) {
    for (j = i; j < sizeof(intval)/sizeof(intval[0]); ++j) {
        U64 a, b, c;
        uint64_t d;
        a.low = (uint32_t)intval[i];
        a.high = (uint32_t)(intval[i] >> 32);
        b.low = (uint32_t)intval[j];
        b.high = (uint32_t)(intval[j] >> 32);
        u64Subtract(&a, &b, &c);
        d = (uint64_t)c.high << 32 | c.low;
        OKF(intval[i] - intval[j] == d,
            ("%" PRId64 " - %" PRId64 " = %" PRId64 " <> %" PRId64, intval[i],
             intval[j], intval[i] - intval[j], d));
    }
}
        
{
    U64 old_val, new_val;
    old_val.low = 7;
    old_val.high = 0;
    new_val = old_val;
    OK(netsnmp_c64_check_for_32bit_wrap(&old_val, &new_val, 0) == 0, "cwrap1");
    new_val.low = 8;
    OK(netsnmp_c64_check_for_32bit_wrap(&old_val, &new_val, 0) == 0, "cwrap2");
    new_val.low = 6;
    OK(netsnmp_c64_check_for_32bit_wrap(&old_val, &new_val, 0) == 32, "cwrap3");
    OK(netsnmp_c64_check_for_32bit_wrap(&old_val, &new_val, 1) == 32
       && new_val.low == 6 && new_val.high == 1, "cwrap4");
    old_val.low = 7;
    old_val.high = 0xffffffffU;
    new_val.low = 7;
    new_val.high = old_val.high;
    OK(netsnmp_c64_check_for_32bit_wrap(&old_val, &new_val, 0) == 0, "cwrap5");
    new_val.low = 8;
    OK(netsnmp_c64_check_for_32bit_wrap(&old_val, &new_val, 0) == 0, "cwrap6");
    new_val.low = 6;
    new_val.high = 0;
    OK(netsnmp_c64_check_for_32bit_wrap(&old_val, &new_val, 0) == 64, "cwrap7");
}
