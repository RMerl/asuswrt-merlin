#ifndef INT64_INCLUDED
#define INT64_INCLUDED

#ifdef __cplusplus
extern          "C" {
#endif

    typedef struct counter64 U64;

#define I64CHARSZ 21

    void            divBy10(U64, U64 *, unsigned int *);
    void            multBy10(U64, U64 *);
    void            incrByU16(U64 *, unsigned int);
    void            incrByU32(U64 *, unsigned int);
    NETSNMP_IMPORT
    void            zeroU64(U64 *);
    int             isZeroU64(const U64 *);
    NETSNMP_IMPORT
    void            printU64(char *, const U64 *);
    NETSNMP_IMPORT
    void            printI64(char *, const U64 *);
    int             read64(U64 *, const char *);
    NETSNMP_IMPORT
    void            u64Subtract(const U64 * pu64one, const U64 * pu64two,
                                U64 * pu64out);
    void            u64Incr(U64 * pu64out, const U64 * pu64one);
    void            u64UpdateCounter(U64 * pu64out, const U64 * pu64one,
                                     const U64 * pu64two);
    void            u64Copy(U64 * pu64one, const U64 * pu64two);

    int             netsnmp_c64_check_for_32bit_wrap(U64 *old_val, U64 *new_val,
                                                     int adjust);
    NETSNMP_IMPORT
    int             netsnmp_c64_check32_and_update(struct counter64 *prev_val,
                                                   struct counter64 *new_val,
                                                   struct counter64 *old_prev_val,
                                                   int *need_wrap_check);

#ifdef __cplusplus
}
#endif
#endif
