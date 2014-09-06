/* HEADER Testing the netsnmp_string_time_to_secs API */
int secs;

#define TESTIT(x,y) \
    OKF(y == (secs = netsnmp_string_time_to_secs(x)), \
        ("netsnmp_string_time_to_secs of %s returned %d, not 5", x, secs))

TESTIT("5", 5);
TESTIT("5", 5);

TESTIT("5s", 5);
TESTIT("5S", 5);

TESTIT("5m", 5 * 60);
TESTIT("5M", 5 * 60);

TESTIT("5h", 5 * 60 * 60);
TESTIT("5H", 5 * 60 * 60);

TESTIT("5d", 5 * 60 * 60 * 24);
TESTIT("5D", 5 * 60 * 60 * 24);

TESTIT("5w", 5 * 60 * 60 * 24 * 7);
TESTIT("5W", 5 * 60 * 60 * 24 * 7);

/* now with longer times */
TESTIT("1234", 1234);
TESTIT("1234", 1234);

TESTIT("1234s", 1234);
TESTIT("1234S", 1234);

TESTIT("1234m", 1234 * 60);
TESTIT("1234M", 1234 * 60);

TESTIT("1234h", 1234 * 60 * 60);
TESTIT("1234H", 1234 * 60 * 60);

TESTIT("1234d", 1234 * 60 * 60 * 24);
TESTIT("1234D", 1234 * 60 * 60 * 24);

TESTIT("1234w", 1234 * 60 * 60 * 24 * 7);
TESTIT("1234W", 1234 * 60 * 60 * 24 * 7);


