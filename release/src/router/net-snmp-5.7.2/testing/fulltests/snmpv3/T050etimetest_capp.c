/*
 * etimetest.c
 *
 * HEADER Testing engineID hashing and timing
 *
 * Expected SUCCESSes for all tests:    3
 *
 * Returns:
 *      Number of FAILUREs.
 *
 * Test of hash_engineID().                             SUCCESSes:  0
 * Test of LCD Engine ID and Time List.                 SUCCESSes:  3
 *
 * FIX  Devise a test for {set,get}_enginetime(..., FALSE).
 */

#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <unistd.h>

#include <net-snmp/library/asn1.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/lcd_time.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/callback.h>

static u_int    dummy_etime, dummy_eboot;       /* For ISENGINEKNOWN(). */


#include <stdlib.h>

extern char    *optarg;
extern int      optind, optopt, opterr;

int             testcount=0;

/*
 * Globals, &c...
 */
char           *local_progname;

#define USAGE	"Usage: %s [-h][-s <seconds>][-aeH]"
#define OPTIONLIST	"aehHs:"

int             doalltests = 0, dohashindex = 0, doetimetest = 0;

#define	ALLOPTIONS	(doalltests + dohashindex + doetimetest)



#define LOCAL_MAXBUF	(1024 * 8)
#define NL		"\n"

#define OUTPUT(o)	fprintf(stdout, "# %s\n", o);

#define SUCCESS(s)					\
{							\
    fprintf(stdout, "# Done with %s\n", s);             \
}

#define FAILED(e, f)                                                    \
{                                                                       \
    if (e != SNMPERR_SUCCESS) {                                         \
                fprintf(stdout, "not ok: %d - %s\n", ++testcount, f);	\
		failcount += 1;                                         \
	} else {                                                        \
                fprintf(stdout, "ok: %d - %s\n", ++testcount, f);	\
        }                                                               \
    fflush(stdout); \
}

#define DETAILINT(s, i) \
    fprintf(stdout, "# %s: %d\n", s, i);

/*
 * Global variables.
 */
int             sleeptime = 2;

#define BLAT "alk;djf;an riu;alicenmrul;aiknglksajhe1 adcfalcenrco2"




/*
 * Prototypes.
 */
void            usage(FILE * ofp);

int             test_etime(void);
int             test_hashindex(void);




int
main(int argc, char **argv)
{
    int             rval = SNMPERR_SUCCESS, failcount = 0;
    char            ch;

    local_progname = argv[0];

    /*
     * Parse.
     */
    while ((ch = getopt(argc, argv, OPTIONLIST)) != EOF) {
        switch (ch) {
        case 'a':
            doalltests = 1;
            break;
        case 'e':
            doetimetest = 1;
            break;
        case 'H':
            dohashindex = 1;
            break;
        case 's':
            sleeptime = atoi(optarg);
            if (sleeptime < 0) {
                usage(stderr);
                exit(1000);
            }
            break;
            break;
        case 'h':
            rval = 0;
        default:
            usage(stdout);
            exit(rval);
        }

        argc -= 1;
        argv += 1;
        if (optarg) {
            argc -= 1;
            argv += 1;
            optarg = NULL;
        }
        optind = 1;
    }                           /* endwhile getopt */

    if ((argc > 1)) {
        usage(stdout);
        exit(1000);

    } else if (ALLOPTIONS != 1) {
        doalltests = 1;
    }

    init_snmp("testing");

    /*
     * test stuff.
     */
    rval = sc_init();
    FAILED(rval, "sc_init()");


    if (dohashindex || doalltests) {
        failcount += test_hashindex();
    }
    if (doetimetest || doalltests) {
        failcount += test_etime();
    }


    fprintf(stdout, "1..%d\n", testcount);
    return 0;
}                               /* end main() */





void
usage(FILE * ofp)
{
    fprintf(ofp,
            USAGE
            "" NL
            "    -a			All tests." NL
            "    -e			Exercise the list of enginetimes."
            NL
            "    -h			Help."
            NL
            "    -H			Test hash_engineID()."
            NL
            "    -s <seconds>	Seconds to pause.  (Default: 0.)"
            NL NL, local_progname);

}                               /* end usage() */




#ifdef EXAMPLE
/*******************************************************************-o-******
 * test_dosomething
 *
 * Returns:
 *	Number of failures.
 *
 *
 * Test template.
 */
int
test_dosomething(void)
{
    int             rval = SNMPERR_SUCCESS, failcount = 0;

  test_dosomething_quit:
    return failcount;

}                               /* end test_dosomething() */
#endif                          /* EXAMPLE */





/*******************************************************************-o-******
 * test_hashindex
 *
 * Returns:
 *	Number of failures.
 *
 *
 * Test hash_engineID().
 */
int
test_hashindex(void)
{
    int                         /* rval = SNMPERR_SUCCESS,  */
                    failcount = 0;
    const u_char     *s;

    OUTPUT("Visual spot check of hash index outputs.  "
           "(Success or failure not noted.)");

    s = (const u_char *) " A";
    fprintf(stdout, "# %s = %d\n", s, hash_engineID(s, strlen((const char *) s)));

    s = (const u_char *) " BB";
    fprintf(stdout, "# %s = %d\n", s, hash_engineID(s, strlen((const char *) s)));

    s = (const u_char *) " CCC";
    fprintf(stdout, "# %s = %d\n", s, hash_engineID(s, strlen((const char *) s)));

    s = (const u_char *) " DDDD";
    fprintf(stdout, "# %s = %d\n", s, hash_engineID(s, strlen((const char *) s)));

    s = (const u_char *) " EEEEE";
    fprintf(stdout, "# %s = %d\n", s, hash_engineID(s, strlen((const char *) s)));

    s = (const u_char *) BLAT;
    fprintf(stdout, "# %s = %d\n", s, hash_engineID(s, strlen((const char *) s)));


    OUTPUT("Visual spot check -- DONE.");

    return failcount;

}                               /* end test_hashindex() */





/*******************************************************************-o-******
 * test_etime
 *
 * Returns:
 *	Number of failures.
 *
 * Test of LCD Engine ID and Time List.	
 */
int
test_etime(void)
{
    int             rval = SNMPERR_SUCCESS, failcount = 0;
    u_int           etime, eboot;

    /*
     * ------------------------------------ -o-
     */
    OUTPUT("Query of empty list, two set actions.");


    rval = ISENGINEKNOWN((const u_char *) "A", 1);
    if (rval == TRUE) {
        FAILED(SNMPERR_GENERR, "Query of empty list returned TRUE.")
    }


    rval = set_enginetime((const u_char *) "BB", 2, 2, 20, TRUE);
    FAILED(rval, "set_enginetime()");


    rval = set_enginetime((const u_char *) "CCC", 3, 31, 90127, TRUE);
    FAILED(rval, "set_enginetime()");


    SUCCESS("Check of empty list, and two additions.");



    /*
     * ------------------------------------ -o-
     */
    OUTPUT("Add entries using macros, test for existence with macros.");


    rval = ENSURE_ENGINE_RECORD((const u_char *) "DDDD", 4);
    FAILED(rval, "ENSURE_ENGINE_RECORD()");


    rval = MAKENEW_ENGINE_RECORD((const u_char *) "EEEEE", 5);
    if (rval == SNMPERR_SUCCESS) {
        FAILED(rval,
               "MAKENEW_ENGINE_RECORD returned success for "
               "missing record.");
    }


    rval = MAKENEW_ENGINE_RECORD((const u_char *) "BB", 2);
    FAILED(rval, "MAKENEW_ENGINE_RECORD().");


    SUCCESS
        ("Added entries with macros, tested for existence with macros.");



    /*
     * ------------------------------------ -o-
     */
    OUTPUT("Dump the list and then sleep.");

#ifdef NETSNMP_ENABLE_TESTING_CODE
    dump_etimelist();
#endif

    fprintf(stdout, "# Sleeping for %d second%s... ",
            sleeptime, (sleeptime == 1) ? "" : "s");

    sleep(sleeptime);


    /*
     * ------------------------------------ -o-
     */
    OUTPUT
        ("Retrieve data from real/stubbed records, update real/stubbed.");



    rval = get_enginetime((const u_char *) "BB", 2, &eboot, &etime, TRUE);
    FAILED(rval, "get_enginetime().");

    fprintf(stdout, "# BB = <%d,%d>\n", eboot, etime);
    if ((etime < 20) || (eboot < 2)) {
        FAILED(SNMPERR_GENERR,
               "get_enginetime() returned bad values.  (1)");
    }

    rval = get_enginetime((const u_char *) "DDDD", 4, &eboot, &etime, FALSE);
    FAILED(rval, "get_enginetime().");

    fprintf(stdout, "# DDDD = <%d,%d>\n", eboot, etime);
    if ((etime < sleeptime) || (eboot != 0)) {
        FAILED(SNMPERR_GENERR,
               "get_enginetime() returned bad values.  (2)");
    }


    rval = set_enginetime((const u_char *) "CCC", 3, 234, 10000, TRUE);
    FAILED(rval, "set_enginetime().");


    rval = set_enginetime((const u_char *) "EEEEE", 5, 9876, 55555, TRUE);
    FAILED(rval, "set_enginetime().");


    SUCCESS("Retrieval and updates.");



    /*
     * ------------------------------------ -o-
     */
    OUTPUT("Sleep again, then dump the list one last time.");

    fprintf(stdout, "# Sleeping for %d second%s...\n",
            sleeptime, (sleeptime == 1) ? "" : "s");

    sleep(sleeptime);

#ifdef NETSNMP_ENABLE_TESTING_CODE
    dump_etimelist();
#endif

    return failcount;

}                               /* end test_etime() */
