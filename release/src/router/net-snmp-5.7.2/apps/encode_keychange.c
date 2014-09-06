/*
 * encode_keychange.c
 *
 * Collect information to build a KeyChange encoding, per the textual
 * convention given in RFC 2274, Section 5.  Compute the value and
 * dump to stdout as a string of hex nibbles.
 *
 *
 * Passphrase material may come from many sources.  The following are
 * checked in order (see get_user_passphrases()):
 *      - Prompt always if -f is given.
 *      - Commandline arguments.
 *      - PASSPHRASE_FILE.
 *      - Prompts on stdout.   Use -P to turn off prompt tags.
 *
 *
 * FIX  Better name?
 * FIX  Change encode_keychange() to take random bits?
 * FIX  QUITFUN not quite appropriate here...
 * FIX  This is slow...
 */

#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <net-snmp/net-snmp-includes.h>

#include <stdlib.h>

/*
 * Globals, &c...
 */
char           *local_progname;
char           *local_passphrase_filename;

#define NL	"\n"

#define USAGE	"Usage: %s [-fhPvV] -t (md5|sha1) [-O \"<old_passphrase>\"][-N \"<new_passphrase>\"][-E [0x]<engineID>]"

#define OPTIONLIST	"E:fhN:O:Pt:vVD"

#define PASSPHRASE_DIR		".snmp"
        /*
         * Rooted at $HOME.
         */
#define PASSPHRASE_FILE		"passphrase.ek"
        /*
         * Format: two lines containing old and new passphrases, nothing more.
         *      
         * XXX  Add creature comforts like: comments and 
         *      tokens identifying passphrases, separate directory check,
         *      check in current directory (?), traverse a path of
         *      directories (?)...
         * FIX  Better name?
         */


int             forcepassphrase = 0,    /* Always prompt for passphrases. */
                promptindicator = 1,    /* Output an indicator that input
                                         *   is requested.                */
                visible = 0,    /* Echo passphrases to terminal.  */
                verbose = 0;    /* Output progress to stderr.     */
size_t          engineid_len = 0;

u_char         *engineid = NULL;        /* Both input & final binary form. */
char           *newpass = NULL, *oldpass = NULL;

char           *transform_type_input = NULL;

const oid      *transform_type = NULL;  /* Type of HMAC hash to use.      */



/*
 * Prototypes.
 */
void            usage_to_file(FILE * ofp);
void            usage_synopsis(FILE * ofp);
int             get_user_passphrases(void);
int             snmp_ttyecho(const int fd, const int echo);
char           *snmp_getpassphrase(const char *prompt, int fvisible);

#ifdef WIN32
#define HAVE_GETPASS 1
char           *getpass(const char *prompt);
int             isatty(int);
int             _cputs(const char *);
int             _getch(void);
#endif

/*******************************************************************-o-******
 */
int
main(int argc, char **argv)
{
    int             rval = 1;
    size_t          oldKu_len = SNMP_MAXBUF_SMALL,
        newKu_len = SNMP_MAXBUF_SMALL,
        oldkul_len = SNMP_MAXBUF_SMALL,
        newkul_len = SNMP_MAXBUF_SMALL, keychange_len = SNMP_MAXBUF_SMALL;

    char           *s = NULL;
    u_char          oldKu[SNMP_MAXBUF_SMALL],
        newKu[SNMP_MAXBUF_SMALL],
        oldkul[SNMP_MAXBUF_SMALL],
        newkul[SNMP_MAXBUF_SMALL], keychange[SNMP_MAXBUF_SMALL];

    int             i;
    int             arg = 1;

    local_progname = argv[0];
    local_passphrase_filename = (char *) malloc(sizeof(PASSPHRASE_DIR) +
                                                sizeof(PASSPHRASE_FILE) +
                                                4);
    if (!local_passphrase_filename) {
        fprintf(stderr, "%s: out of memory!", local_progname);
        exit(-1);
    }
    sprintf(local_passphrase_filename, "%s/%s", PASSPHRASE_DIR,
            PASSPHRASE_FILE);



    /*
     * Parse.
     */
    for (; (arg < argc) && (argv[arg][0] == '-'); arg++) {
        switch (argv[arg][1]) {
        case 'D':
            snmp_set_do_debugging(1);
            break;
        case 'E':
            engineid = (u_char *) argv[++arg];
            break;
        case 'f':
            forcepassphrase = 1;
            break;
        case 'N':
            newpass = argv[++arg];
            break;
        case 'O':
            oldpass = argv[++arg];
            break;
        case 'P':
            promptindicator = 0;
            break;
        case 't':
            transform_type_input = argv[++arg];
            break;
        case 'v':
            verbose = 1;
            break;
        case 'V':
            visible = 1;
            break;
        case 'h':
            rval = 0;
	    /* fallthrough */
        default:
            usage_to_file(stdout);
            exit(rval);
        }
    }

    if (!transform_type_input) {
        fprintf(stderr, "The -t option is mandatory.\n");
        usage_synopsis(stdout);
        exit(1000);
    }



    /*
     * Convert and error check transform_type.
     */
#ifndef NETSNMP_DISABLE_MD5
    if (!strcmp(transform_type_input, "md5")) {
        transform_type = usmHMACMD5AuthProtocol;

    } else
#endif
        if (!strcmp(transform_type_input, "sha1")) {
        transform_type = usmHMACSHA1AuthProtocol;

    } else {
        fprintf(stderr,
                "Unrecognized hash transform: \"%s\".\n",
                transform_type_input);
        usage_synopsis(stderr);
        QUITFUN(SNMPERR_GENERR, main_quit);
    }

    if (verbose) {
        fprintf(stderr, "Hash:\t\t%s\n",
#ifndef NETSNMP_DISABLE_MD5
                (transform_type == usmHMACMD5AuthProtocol)
                ? "usmHMACMD5AuthProtocol" :
#endif
                "usmHMACSHA1AuthProtocol"
            );
    }



    /*
     * Build engineID.  Accept hex engineID as the bits
     * "in-and-of-themselves", otherwise create an engineID with the
     * given string as text.
     *
     * If no engineID is given, lookup the first IP address for the
     * localhost and use that (see setup_engineID()).
     */
    if (engineid && (tolower(*(engineid + 1)) == 'x')) {
        engineid_len = hex_to_binary2(engineid + 2,
                                      strlen((char *) engineid) - 2,
                                      (char **) &engineid);
        DEBUGMSGTL(("encode_keychange", "engineIDLen: %lu\n",
                    (unsigned long)engineid_len));
    } else {
        engineid_len = setup_engineID(&engineid, (char *) engineid);

    }

#ifdef NETSNMP_ENABLE_TESTING_CODE
    if (verbose) {
        fprintf(stderr, "EngineID:\t%s\n",
                /*
                 * XXX = 
                 */ dump_snmpEngineID(engineid, &engineid_len));
    }
#endif


    /*
     * Get passphrases from user.
     */
    rval = get_user_passphrases();
    QUITFUN(rval, main_quit);

    if (strlen(oldpass) < USM_LENGTH_P_MIN) {
        fprintf(stderr, "Old passphrase must be greater than %d "
                "characters in length.\n", USM_LENGTH_P_MIN);
        QUITFUN(SNMPERR_GENERR, main_quit);

    } else if (strlen(newpass) < USM_LENGTH_P_MIN) {
        fprintf(stderr, "New passphrase must be greater than %d "
                "characters in length.\n", USM_LENGTH_P_MIN);
        QUITFUN(SNMPERR_GENERR, main_quit);
    }

    if (verbose) {
        fprintf(stderr,
                "Old passphrase:\t%s\nNew passphrase:\t%s\n",
                oldpass, newpass);
    }



    /*
     * Compute Ku and Kul's from old and new passphrases, then
     * compute the keychange string & print it out.
     */
    rval = sc_init();
    QUITFUN(rval, main_quit);


    rval = generate_Ku(transform_type, USM_LENGTH_OID_TRANSFORM,
                       (u_char *) oldpass, strlen(oldpass),
                       oldKu, &oldKu_len);
    QUITFUN(rval, main_quit);


    rval = generate_Ku(transform_type, USM_LENGTH_OID_TRANSFORM,
                       (u_char *) newpass, strlen(newpass),
                       newKu, &newKu_len);
    QUITFUN(rval, main_quit);


    DEBUGMSGTL(("encode_keychange", "EID (%lu): ", (unsigned long)engineid_len));
    for (i = 0; i < (int) engineid_len; i++)
        DEBUGMSGTL(("encode_keychange", "%02x", (int) (engineid[i])));
    DEBUGMSGTL(("encode_keychange", "\n"));

    DEBUGMSGTL(("encode_keychange", "old Ku (%lu) (from %s): ", (unsigned long)oldKu_len,
                oldpass));
    for (i = 0; i < (int) oldKu_len; i++)
        DEBUGMSGTL(("encode_keychange", "%02x", (int) (oldKu[i])));
    DEBUGMSGTL(("encode_keychange", "\n"));

    rval = generate_kul(transform_type, USM_LENGTH_OID_TRANSFORM,
                        engineid, engineid_len,
                        oldKu, oldKu_len, oldkul, &oldkul_len);
    QUITFUN(rval, main_quit);


    DEBUGMSGTL(("encode_keychange", "generating old Kul (%lu) (from Ku): ",
                (unsigned long)oldkul_len));
    for (i = 0; i < (int) oldkul_len; i++)
        DEBUGMSGTL(("encode_keychange", "%02x", (int) (oldkul[i])));
    DEBUGMSGTL(("encode_keychange", "\n"));

    rval = generate_kul(transform_type, USM_LENGTH_OID_TRANSFORM,
                        engineid, engineid_len,
                        newKu, newKu_len, newkul, &newkul_len);
    QUITFUN(rval, main_quit);

    DEBUGMSGTL(("encode_keychange", "generating new Kul (%lu) (from Ku): ",
                (unsigned long)oldkul_len));
    for (i = 0; i < (int) newkul_len; i++)
        DEBUGMSGTL(("encode_keychange", "%02x", newkul[i]));
    DEBUGMSGTL(("encode_keychange", "\n"));

    rval = encode_keychange(transform_type, USM_LENGTH_OID_TRANSFORM,
                            oldkul, oldkul_len,
                            newkul, newkul_len, keychange, &keychange_len);
    QUITFUN(rval, main_quit);



    binary_to_hex(keychange, keychange_len, &s);
    printf("%s%s\n", (verbose) ? "KeyChange string:\t" : "",    /* XXX stdout */
           s);


    /*
     * Cleanup.
     */
  main_quit:
    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_SHUTDOWN,
                        NULL);


    SNMP_ZERO(oldpass, strlen(oldpass));
    SNMP_ZERO(newpass, strlen(newpass));

    memset(oldKu, 0, oldKu_len);
    memset(newKu, 0, newKu_len);

    memset(oldkul, 0, oldkul_len);
    memset(newkul, 0, newkul_len);

    SNMP_ZERO(s, strlen(s));

    return rval;

}                               /* end main() */




/*******************************************************************-o-******
 */
void
usage_synopsis(FILE * ofp)
{
    fprintf(ofp, USAGE "\n\
\n\
    -E [0x]<engineID>		EngineID used for kul generation.\n\
    -f				Force passphrases to be read from stdin.\n\
    -h				Help.\n\
    -N \"<new_passphrase>\"	Passphrase used to generate new Ku.\n\
    -O \"<old_passphrase>\"	Passphrase used to generate old Ku.\n\
    -P				Turn off prompt indicators.\n\
    -t md5 | sha1		HMAC hash transform type.\n\
    -v				Verbose.\n\
    -V				Visible.  Echo passphrases to terminal.\n\
		" NL, local_progname);

}                               /* end usage_synopsis() */

void
usage_to_file(FILE * ofp)
{
    char           *s;

    usage_synopsis(ofp);

    fprintf(ofp, "\n%s\
	a) Commandline options,\n\
	b) The file \"%s/%s\",\n\
	c) stdin  -or-  User input from the terminal.\n\n%s\
		" NL,
   "Only -t is mandatory.  The transform is used to convert P=>Ku, convert\n\
    Ku=>Kul, and to hash the old Kul with the random bits.\n\
\n\
    Passphrase will be taken from the first successful source as follows:\n",
    (s = getenv("HOME")) ? s : "$HOME", local_passphrase_filename,
   "-f will require reading from the stdin/terminal, ignoring a) and b).\n\
    -P will prevent prompts for passphrases to stdout from being printed.\n\
\n\
    <engineID> is interpreted as a hex string when preceeded by \"0x\",\n\
    otherwise it is created to contain \"text\".  If nothing is given,\n\
    <engineID> is constructed from the first IP address for the local host.\n");


    /*
     * FIX -- make this possible?
     * -r [0x]<random_bits> Random bits used in KeyChange XOR.
     * 
     * <engineID> and <random_bits> are interpreted as hex strings when
     * preceeded by \"0x\", otherwise <engineID> is created to contain \"text\"
     * and <random_bits> are the same as the ascii input.
     * 
     * <random_bits> will be generated by SCAPI if not given.  If value is
     * too long, it will be truncated; if too short, the remainder will be
     * filled in with zeros.
     */

}                               /* end usage() */


/*
 * this defined for HPUX aCC because the aCC doesn't drop the 
 */
/*
 * snmp_parse_args.c functionality if compile with -g, PKY 
 */

void
usage(void)
{
    usage_to_file(stdout);
}





/*******************************************************************-o-******
 * get_user_passphrases
 *
 * Returns:
 *	SNMPERR_SUCCESS		Success.
 *	SNMPERR_GENERR		Otherwise.
 *
 *
 * Acquire new and old passphrases from the user:
 *
 *	+ Always prompt if 'forcepassphrase' is set.
 *	+ Use given arguments if they are defined.
 *	+ Otherwise read file format from PASSPHRASE_FILE.
 *		Sanity check existence and permissions of the path.
 *		ASSUME for now that PASSPHRASE_FILE is rooted only at $HOME.
 *	+ Otherwise prompt user for passphrase(s).
 *		Echo input if 'visible' is set.
 *		Turning off 'promptindicator' makes piping in input cleaner.
 *
 * NOTE Only using forcepassphrase mandates taking both passphrases
 * from the same source.  Otherwise processing continues until both 
 * passphrases are defined.
 */
int
get_user_passphrases(void)
{
    int             rval = SNMPERR_SUCCESS;
    size_t          len;

    char           *obuf = NULL, *nbuf = NULL;

    char            path[SNMP_MAXBUF], buf[SNMP_MAXBUF], *s = NULL;

    struct stat     statbuf;
    FILE           *fp = NULL;



    /*
     * Allow prompts to the user to override all other sources.
     * Nothing to do otherwise if oldpass and newpass are already defined.
     */
    if (forcepassphrase)
        goto get_user_passphrases_prompt;
    if (oldpass && newpass)
        goto get_user_passphrases_quit;



    /*
     * Read passphrases out of PASSPHRASE_FILE.  Sanity check the
     * path for existence and access first.  Refuse to read
     * if the permissions are wrong.
     */
    s = getenv("HOME");
    snprintf(path, sizeof(path), "%s/%s", s, PASSPHRASE_DIR);
    path[ sizeof(path)-1 ] = 0;

    /*
     * Test directory. 
     */
    if (stat(path, &statbuf) < 0) {
        fprintf(stderr, "Cannot access directory \"%s\".\n", path);
        QUITFUN(SNMPERR_GENERR, get_user_passphrases_quit);
#ifndef WIN32
    } else if (statbuf.st_mode & (S_IRWXG | S_IRWXO)) {
        fprintf(stderr,
                "Directory \"%s\" is accessible by group or world.\n",
                path);
        QUITFUN(SNMPERR_GENERR, get_user_passphrases_quit);
#endif                          /* !WIN32 */
    }

    /*
     * Test file. 
     */
    snprintf(path, sizeof(path), "%s/%s", s, local_passphrase_filename);
    path[ sizeof(path)-1 ] = 0;
    if (stat(path, &statbuf) < 0) {
        fprintf(stderr, "Cannot access file \"%s\".\n", path);
        QUITFUN(SNMPERR_GENERR, get_user_passphrases_quit);
#ifndef WIN32
    } else if (statbuf.st_mode & (S_IRWXG | S_IRWXO)) {
        fprintf(stderr,
                "File \"%s\" is accessible by group or world.\n", path);
        QUITFUN(SNMPERR_GENERR, get_user_passphrases_quit);
#endif                          /* !WIN32 */
    }

    /*
     * Open the file. 
     */
    if ((fp = fopen(path, "r")) == NULL) {
        fprintf(stderr, "Cannot open \"%s\".", path);
        QUITFUN(SNMPERR_GENERR, get_user_passphrases_quit);
    }

    /*
     * Read 1st line. 
     */
    if (!fgets(buf, sizeof(buf), fp)) {
        if (verbose) {
            fprintf(stderr, "Passphrase file \"%s\" is empty...\n", path);
        }
        goto get_user_passphrases_prompt;

    } else if (!oldpass) {
        len = strlen(buf);
        if (buf[len - 1] == '\n')
            buf[--len] = '\0';
        oldpass = (char *) calloc(1, len + 1);
        if (oldpass)
            memcpy(oldpass, buf, len + 1);
    }
    /*
     * Read 2nd line. 
     */
    if (!fgets(buf, sizeof(buf), fp)) {
        if (verbose) {
            fprintf(stderr, "Only one line in file \"%s\"...\n", path);
        }

    } else if (!newpass) {
        len = strlen(buf);
        if (buf[len - 1] == '\n')
            buf[--len] = '\0';
        newpass = (char *) calloc(1, len + 1);
        if (newpass)
            memcpy(newpass, buf, len + 1);
    }

    if (oldpass && newpass)
        goto get_user_passphrases_quit;



    /*
     * Prompt the user for passphrase entry.  Visible prompts
     * may be omitted, and invisible entry may turned off.
     */
  get_user_passphrases_prompt:
    if (forcepassphrase) {
        oldpass = newpass = NULL;
    }

    if (!oldpass) {
        oldpass = obuf
            = snmp_getpassphrase((promptindicator) ? "Old passphrase: " :
                                 "", visible);
    }
    if (!newpass) {
        newpass = nbuf
            = snmp_getpassphrase((promptindicator) ? "New passphrase: " :
                                 "", visible);
    }



    /*
     * Check that both passphrases were defined.
     */
    if (oldpass && newpass) {
        goto get_user_passphrases_quit;
    } else {
        rval = SNMPERR_GENERR;
    }


  get_user_passphrases_quit:
    memset(buf, 0, SNMP_MAXBUF);

    if (obuf != oldpass) {
        SNMP_ZERO(obuf, strlen(obuf));
        SNMP_FREE(obuf);
    }
    if (nbuf != newpass) {
        SNMP_ZERO(nbuf, strlen(nbuf));
        SNMP_FREE(nbuf);
    }

    if (fp)
        fclose (fp);
        
    return rval;

}                               /* end get_user_passphrases() */

/*******************************************************************-o-******
 * snmp_ttyecho
 *
 * Parameters:
 *	fd	Descriptor of terminal on which to toggle echoing.
 *	echo	TRUE if echoing should be on; FALSE otherwise.
 *      
 * Returns:
 *	Previous value of echo setting.
 *
 *
 * FIX	Put HAVE_TCGETATTR in autoconf?
 */
#ifndef HAVE_GETPASS
#ifdef HAVE_TCGETATTR
#include <termios.h>
int
snmp_ttyecho(const int fd, const int echo)
{
    struct termios  tio;
    int             was_echo;


    if (!isatty(fd))
        return (-1);
    tcgetattr(fd, &tio);
    was_echo = (tio.c_lflag & ECHO) != 0;
    if (echo)
        tio.c_lflag |= (ECHO | ECHONL);
    else
        tio.c_lflag &= ~(ECHO | ECHONL);
    tcsetattr(fd, TCSANOW, &tio);

    return (was_echo);

}                               /* end snmp_ttyecho() */

#else
#include <sgtty.h>
int
snmp_ttyecho(const int fd, const int echo)
{
    struct sgttyb   ttyparams;
    int             was_echo;


    if (!isatty(fd))
        was_echo = -1;
    else {
        ioctl(fd, TIOCGETP, &ttyparams);
        was_echo = (ttyparams.sg_flags & ECHO) != 0;
        if (echo)
            ttyparams.sg_flags = ttyparams.sg_flags | ECHO;
        else
            ttyparams.sg_flags = ttyparams.sg_flags & ~ECHO;
        ioctl(fd, TIOCSETP, &ttyparams);
    }

    return (was_echo);

}                               /* end snmp_ttyecho() */
#endif                          /* HAVE_TCGETATTR */
#endif                          /* HAVE_GETPASS */




/*******************************************************************-o-******
 * snmp_getpassphrase
 *
 * Parameters:
 *	*prompt		(May be NULL.)
 *	 bvisible	TRUE means echo back user input.
 *      
 * Returns:
 *	Pointer to newly allocated, null terminated string containing
 *		passphrase  -OR-
 *	NULL on error.
 *
 *
 * Prompt stdin for a string (or passphrase).  Return a copy of the 
 * input in a null terminated string.
 *
 * FIX	Put HAVE_GETPASS in autoconf.
 */
char           *
snmp_getpassphrase(const char *prompt, int bvisible)
{
    int             ti = 0;
    size_t          len;

    char           *bufp = NULL;
    static char     buffer[SNMP_MAXBUF];

    FILE           *ofp = stdout;


    /*
     * Query stdin for a passphrase.
     */
#ifdef HAVE_GETPASS
    if (isatty(0)) {
        return getpass((prompt) ? prompt : "");
    }
#endif

    fputs((prompt) ? prompt : "", ofp);

    if (!bvisible) {
        ti = snmp_ttyecho(0, 0);
    }

    bufp = fgets(buffer, sizeof(buffer), stdin);

    if (!bvisible) {
        ti = snmp_ttyecho(0, ti);
        fputs("\n", ofp);
    }
    if (!bufp) {
        fprintf(stderr, "Aborted...\n");
        exit(1);
    }


    /*
     * Copy the input and zero out the read-in buffer.
     */
    len = strlen(buffer);
    if (buffer[len - 1] == '\n')
        buffer[--len] = '\0';

    bufp = (char *) calloc(1, len + 1);
    if (bufp)
        memcpy(bufp, buffer, len + 1);

    memset(buffer, 0, SNMP_MAXBUF);


    return bufp;

}                               /* end snmp_getpassphrase() */

#ifdef WIN32

int
snmp_ttyecho(const int fd, const int echo)
{
    return 0;
}

/*
 * stops at the first newline, carrier return, or backspace.
 * WARNING! _getch does NOT read <Ctrl-C>
 */
char           *
getpass(const char *prompt)
{
    static char     pbuf[128];
    int             ch, lim;

    _cputs(prompt);
    for (ch = 0, lim = 0; ch != '\n' && lim < sizeof(pbuf)-1;) {
        ch = _getch();          /* look ma, no echo ! */
        if (ch == '\r' || ch == '\n' || ch == '\b')
            break;
        pbuf[lim++] = ch;
    }
    pbuf[lim] = '\0';
    puts("\n");

    return pbuf;
}
#endif                          /* WIN32 */
