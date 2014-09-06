/* HEADER Testing strtoull(). */

#ifdef HAVE_STRTOULL

OK(1, "Skipping strtoull() test because using strtoull() from C library.\n");;

#else

/*
 * UINT64_C: C99 macro for the suffix for uint64_t constants. 
 */
#ifndef UINT64_C
#ifdef _MSC_VER
#define UINT64_C(c) c##ui64
#else
#define UINT64_C(c) c##ULL
#endif
#endif

/*
 * According to the C99 standard, the constant ULLONG_MAX must be defined in
 * <limits.h>. Define it here for pre-C99 compilers.
 */
#ifndef ULLONG_MAX
#define ULLONG_MAX UINT64_C(0xffffffffffffffff)
#endif

#ifndef PRIu64
#ifdef _MSC_VER
#define PRIu64 "I64u"
#else
#define PRIu64 "llu"
#endif
#endif

struct strtoull_testcase {
    /*
     * inputs 
     */
    const char     *nptr;
    int             base;
    /*
     * expected outputs 
     */
    int             expected_errno;
    int             expected_end;
    uint64_t        expected_result;
};

static const struct strtoull_testcase test_input[] = {
    {"0x0", 0, 0, 3, 0},
    {"1", 0, 0, 1, 1},
    {"0x1", 0, 0, 3, 1},
    {"  -0666", 0, 0, 7, -0666},
    {"  -0x666", 0, 0, 8, -0x666},
    {"18446744073709551614", 0, 0, 20, UINT64_C(0xfffffffffffffffe)},
    {"0xfffffffffffffffe", 0, 0, 18, UINT64_C(0xfffffffffffffffe)},
    {"18446744073709551615", 0, 0, 20, UINT64_C(0xffffffffffffffff)},
    {"0xffffffffffffffff", 0, 0, 18, UINT64_C(0xffffffffffffffff)},
    {"18446744073709551616", 0, ERANGE, 20, UINT64_C(0xffffffffffffffff)},
    {"0x10000000000000000", 0, ERANGE, 19, UINT64_C(0xffffffffffffffff)},
    {"ff", 16, 0, 2, 255},
    {"0xff", 16, 0, 4, 255},
    {" ", 0, 0, 0, 0},
    {"0x", 0, 0, 1, 0},
    {"0x", 8, 0, 1, 0},
    {"0x", 16, 0, 1, 0},
    {"zyyy", 0, 0, 0, 0},
    {"0xfffffffffffffffff", 0, ERANGE, 19, ULLONG_MAX},
    {"0xfffffffffffffffffz", 0, ERANGE, 19, ULLONG_MAX}
};

unsigned int    i;

for (i = 0; i < sizeof(test_input) / sizeof(test_input[0]); i++) {
    const struct strtoull_testcase *const p = &test_input[i];
    char           *endptr;
    uint64_t        result;

    errno = 0;
    result = strtoull(p->nptr, &endptr, p->base);
    OKF(errno == p->expected_errno,
        ("test %d (input \"%s\"): expected errno %d, got errno %d",
         i, p->nptr, p->expected_errno, errno));
    OKF(result == p->expected_result,
        ("test %d (input \"%s\"): expected result %" PRIu64
         ", got result %" PRIu64,
         i, p->nptr, p->expected_result, result));
    OKF(endptr - p->nptr == p->expected_end,
        ("test %d (input \"%s\"): expected end %d, got end %d",
         i, p->nptr, p->expected_end, (int) (endptr - p->nptr)));
}

#endif
