/*
 * scapitest.c
 *
 * HEADER Testing SCAPI API
 *
 * Expected SUCCESSes:  2 + 10 + 1 for all tests.
 *
 * Returns:
 *      Number of FAILUREs.
 *
 *
 * ASSUMES  No key management functions return non-zero success codes.
 *
 * XXX  Split into individual modules?
 * XXX  Error/fringe conditions should be tested.
 *
 *
 * Test of sc_random.                                           SUCCESSes: 2.
 *      REQUIRES a human to spot check for obvious non-randomness...
 *
 * Test of sc_generate_keyed_hash and sc_check_keyed_hash.      SUCCESSes: 10.
 *
 * Test of sc_encrypt and sc_decrypt.                           SUCCESSes: 1.
 */

#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <net-snmp/library/asn1.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/keytools.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/transform_oids.h>
#include <net-snmp/library/callback.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/snmpusm.h>

#include <stdlib.h>

extern char    *optarg;
extern int      optind, optopt, opterr;

#define DEBUG                   /* */



/*
 * Globals, &c...
 */
char           *local_progname;
int             testcount=0;
int             failcount=0;

#define USAGE	"Usage: %s [-h][-acHr]"
#define OPTIONLIST	"achHr"

int             doalltests = 0, docrypt = 0, dokeyedhash = 0, dorandom = 0;

#define	ALLOPTIONS	(doalltests + docrypt + dokeyedhash + dorandom)



#define LOCAL_MAXBUF	(1024 * 8)
#define NL		"\n"

#define OUTPUT(o)	printf("# %s\n", o);

#define SUCCESS(s)					\
{							\
    printf("# Done with %s\n", s);			\
}

#define FAILED(e, f)                                                    \
{                                                                       \
    if (e != SNMPERR_SUCCESS) {                                         \
                printf("not ok: %d - %s\n", ++testcount, f);            \
		failcount += 1;                                         \
	} else {                                                        \
                printf("ok: %d - %s\n", ++testcount, f);                \
        }                                                               \
    fflush(stdout); \
}

#define BIGSTRING							\
    (const u_char *)                                                    \
    "   A port may be a pleasant retreat for any mind grown weary of"	\
    "the struggle for existence.  The vast expanse of sky, the"		\
    "mobile architecture of the clouds, the chameleon coloration"	\
    "of the sea, the beacons flashing on the shore, together make"	\
    "a prism which is marvellously calculated to entertain but not"	\
    "fatigue the eye.  The lofty ships with their complex webs of"	\
    "rigging, swayed to and fro by the swell in harmonious dance,"	\
    "all help to maintain a taste for rhythm and beauty in the"		\
    "mind.  And above all there is a mysterious, aristrocratic kind"	\
    "of pleasure to be had, for those who have lost all curiosity"	\
    "or ambition, as they strech on the belvedere or lean over the"	\
    "mole to watch the arrivals and departures of other men, those"	\
    "who still have sufficient strength of purpose in them, the"	\
    "urge to travel or enrich themselves."				\
    "	-- Baudelaire"							\
    "	   From _The_Poems_in_Prose_, \"The Port\" (XLI)."

#define BIGSECRET                                               \
    (const u_char *)                                            \
    "Shhhh... Don't tell *anyone* about this.  Not a soul."
#define BKWDSECRET                                              \
    (const u_char *)                                            \
    ".luos a toN  .siht tuoba *enoyna* llet t'noD ...hhhhS"

#define MLCOUNT_MAX	6       /* MAC Length Count Maximum. */



/*
 * Prototypes.
 */
void            usage(void);

int             test_docrypt(void);
int             test_dokeyedhash(void);
int             test_dorandom(void);




int
main(int argc, char **argv)
{
    int             rval = SNMPERR_SUCCESS;
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
        case 'c':
            docrypt = 1;
            break;
        case 'H':
            dokeyedhash = 1;
            break;
        case 'r':
            dorandom = 1;
            break;
        case 'h':
            rval = 0;
        default:
            usage();
            exit(rval);
        }

        argc -= 1;
        argv += 1;
        optind = 1;
    }                           /* endwhile getopt */

    if ((argc > 1)) {
        usage();
        exit(1000);

    } else if (ALLOPTIONS != 1) {
        doalltests = 1;
    }


    /*
     * Test stuff.
     */
    rval = sc_init();
    FAILED(rval, "sc_init() return code");


    if (docrypt || doalltests) {
        test_docrypt();
    }
    if (dokeyedhash || doalltests) {
        test_dokeyedhash();
    }
    if (dorandom || doalltests) {
        test_dorandom();
    }

    printf("1..%d\n", testcount);
    return 0;
}                               /* end main() */





void
usage(void)
{
    printf( USAGE
            "" NL
            "	-a		All tests." NL
            "	-c		Test of sc_encrypt()/sc_decrypt()."
            NL
            "	-h		Help."
            NL
            "	-H              Test sc_{generate,check}_keyed_hash()."
            NL
            "	-r              Test sc_random()."
            NL "" NL, local_progname);

}                               /* end usage() */




/*******************************************************************-o-******
 * test_dorandom
 *
 * One large request, one set of short requests.
 *
 * Returns:
 *	Number of failures.
 *
 * XXX	probably should split up into individual options.
 */
int
test_dorandom(void)
{
    int             rval = SNMPERR_SUCCESS,
        origrequest = (1024 * 2),
        origrequest_short = 19, shortcount = 7, i;
    size_t          nbytes = origrequest;
    u_char          buf[LOCAL_MAXBUF];

    OUTPUT("Random test -- large request:");

    rval = sc_random(buf, &nbytes);
    FAILED(rval, "sc_random() return code");

    if (nbytes != origrequest) {
        FAILED(SNMPERR_GENERR,
               "sc_random() returned different than requested.");
    }

    dump_chunk("scapitest", NULL, buf, nbytes);

    SUCCESS("Random test -- large request.");


    OUTPUT("Random test -- short requests:");
    origrequest_short = 16;

    for (i = 0; i < shortcount; i++) {
        nbytes = origrequest_short;
        rval = sc_random(buf, &nbytes);
        FAILED(rval, "sc_random() return code");

        if (nbytes != origrequest_short) {
            FAILED(SNMPERR_GENERR,
                   "sc_random() returned different " "than requested.");
        }

        dump_chunk("scapitest", NULL, buf, nbytes);
    }                           /* endfor */

    SUCCESS("Random test -- short requests.");


    return failcount;

}                               /* end test_dorandom() */



/*******************************************************************-o-******
 * test_dokeyedhash
 *
 * Returns:
 *	Number of failures.
 *
 *
 * Test keyed hashes with a variety of MAC length requests.
 *
 *
 * NOTE Both tests intentionally use the same secret
 *
 * FIX	Get input or output from some other package which hashes...
 * XXX	Could cut this in half with a little indirection...
 */
int
test_dokeyedhash(void)
{
    int rval = SNMPERR_SUCCESS,
        bigstring_len = strlen((const char *) BIGSTRING),
        secret_len = strlen((const char *) BIGSECRET),
        properlength,
        mlcount = 0;        /* MAC Length count.   */
    size_t          hblen;      /* Hash Buffer length. */

    u_int           hashbuf_len[MLCOUNT_MAX] = {
        LOCAL_MAXBUF,
        USM_MD5_AND_SHA_AUTH_LEN,
        USM_MD5_AND_SHA_AUTH_LEN,
        USM_MD5_AND_SHA_AUTH_LEN,
        7,
        0,
    };

    u_char          hashbuf[LOCAL_MAXBUF];
    char           *s;

  test_dokeyedhash_again:

    OUTPUT("Starting Keyed hash test using MD5 --");

    memset(hashbuf, 0, LOCAL_MAXBUF);
    hblen = hashbuf_len[mlcount];
    properlength = BYTESIZE(SNMP_TRANS_AUTHLEN_HMACMD5);

    rval =
        sc_generate_keyed_hash(usmHMACMD5AuthProtocol,
                               USM_LENGTH_OID_TRANSFORM, BIGSECRET,
                               secret_len, BIGSTRING,
                               bigstring_len,
                               hashbuf, &hblen);
    FAILED(rval, "sc_generate_keyed_hash() return code");

    if (hashbuf_len[mlcount] > properlength) {
        if (hblen != properlength) {
            FAILED(SNMPERR_GENERR, "Wrong MD5 hash length returned.  (1)");
        }

    } else if (hblen != hashbuf_len[mlcount]) {
        FAILED(SNMPERR_GENERR, "Wrong MD5 hash length returned.  (2)");
    }

    rval =
        sc_check_keyed_hash(usmHMACMD5AuthProtocol,
                            USM_LENGTH_OID_TRANSFORM, BIGSECRET,
                            secret_len, BIGSTRING, bigstring_len, hashbuf,
                            hblen);
    FAILED(rval, "sc_check_keyed_hash() return code");

    binary_to_hex(hashbuf, hblen, &s);
    printf("# hash buffer (len=%" NETSNMP_PRIz "u, request=%d):   %s\n",
            hblen, hashbuf_len[mlcount], s);
    SNMP_FREE(s);



    OUTPUT("Starting Keyed hash test using SHA1 --");

    memset(hashbuf, 0, LOCAL_MAXBUF);
    hblen = hashbuf_len[mlcount];
    properlength = BYTESIZE(SNMP_TRANS_AUTHLEN_HMACSHA1);

    rval =
        sc_generate_keyed_hash(usmHMACSHA1AuthProtocol,
                               USM_LENGTH_OID_TRANSFORM, BIGSECRET,
                               secret_len, BIGSTRING, bigstring_len,
                               hashbuf, &hblen);
    FAILED(rval, "sc_generate_keyed_hash() return code");

    if (hashbuf_len[mlcount] > properlength) {
        if (hblen != properlength) {
            FAILED(SNMPERR_GENERR,
                   "Wrong SHA1 hash length returned.  (1)");
        }

    } else if (hblen != hashbuf_len[mlcount]) {
        FAILED(SNMPERR_GENERR, "Wrong SHA1 hash length returned.  (2)");
    }

    rval =
        sc_check_keyed_hash(usmHMACSHA1AuthProtocol,
                            USM_LENGTH_OID_TRANSFORM, BIGSECRET,
                            secret_len, BIGSTRING, bigstring_len, hashbuf,
                            hblen);
    FAILED(rval, "sc_check_keyed_hash() return code");

    binary_to_hex(hashbuf, hblen, &s);
    printf("# hash buffer (len=%" NETSNMP_PRIz "u, request=%d):   %s\n",
            hblen, hashbuf_len[mlcount], s);
    SNMP_FREE(s);

    SUCCESS("Keyed hash test using SHA1.");

    /*
     * Run the basic hash tests but vary the size MAC requests.
     */
    if (hashbuf_len[++mlcount] != 0) {
        goto test_dokeyedhash_again;
    }


    return failcount;

}                               /* end test_dokeyedhash() */





/*******************************************************************-o-******
 * test_docrypt
 *
 * Returns:
 *	Number of failures.
 */
int
test_docrypt(void)
{
    int             rval = SNMPERR_SUCCESS,
        bigstring_len = strlen((const char *) BIGSTRING),
        secret_len = BYTESIZE(SNMP_TRANS_PRIVLEN_1DES),
        iv_len = BYTESIZE(SNMP_TRANS_PRIVLEN_1DES_IV);

    size_t          buf_len = LOCAL_MAXBUF;
    size_t          cryptbuf_len = LOCAL_MAXBUF;

    u_char            buf[LOCAL_MAXBUF],
        cryptbuf[LOCAL_MAXBUF], secret[LOCAL_MAXBUF], iv[LOCAL_MAXBUF];

    OUTPUT("Starting Test 1DES-CBC --");


    memset(buf, 0, LOCAL_MAXBUF);

    memcpy(secret, BIGSECRET, secret_len);
    memcpy(iv, BKWDSECRET, iv_len);

    rval = sc_encrypt(usmDESPrivProtocol, USM_LENGTH_OID_TRANSFORM,
                      secret, secret_len,
                      iv, iv_len,
                      BIGSTRING, bigstring_len, cryptbuf, &cryptbuf_len);
    FAILED(rval, "sc_encrypt() return code.");

    rval = sc_decrypt(usmDESPrivProtocol, USM_LENGTH_OID_TRANSFORM,
                      secret, secret_len,
                      iv, iv_len, cryptbuf, cryptbuf_len, buf, &buf_len);
    FAILED(rval, "sc_decrypt() return code.");

    /* ignore the pad */
    buf_len -= buf[buf_len-1];

    FAILED((buf_len != bigstring_len), "Decrypted buffer is the right length.");
    printf("# original length: %d\n", bigstring_len);
    printf("# output   length: %" NETSNMP_PRIz "u\n", buf_len);

    FAILED((memcmp(buf, BIGSTRING, bigstring_len) != 0),
           "Decrypted buffer is the same as the original plaintext.");
    return failcount;
}                               /* end test_docrypt() */
