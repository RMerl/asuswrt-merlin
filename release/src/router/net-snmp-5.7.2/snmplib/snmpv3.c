/*
 * snmpv3.c
 */

#include <net-snmp/net-snmp-config.h>
#include <errno.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_STDLIB_H
#       include <stdlib.h>
#endif

/*
 * Stuff needed for getHwAddress(...) 
 */
#ifdef HAVE_SYS_IOCTL_H
#	include <sys/ioctl.h>
#endif
#ifdef HAVE_NET_IF_H
#	include <net/if.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>
#include <net-snmp/utilities.h>

#include <net-snmp/library/snmpv3.h>
#include <net-snmp/library/callback.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/lcd_time.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/keytools.h>
#include <net-snmp/library/lcd_time.h>
#include <net-snmp/library/snmp_secmod.h>
#include <net-snmp/library/snmpusm.h>
#include <net-snmp/library/transform_oids.h>

#include <net-snmp/net-snmp-features.h>

static u_long   engineBoots = 1;
static unsigned int engineIDType = ENGINEID_TYPE_NETSNMP_RND;
static unsigned char *engineID = NULL;
static size_t   engineIDLength = 0;
static unsigned char *engineIDNic = NULL;
static unsigned int engineIDIsSet = 0;  /* flag if ID set by config */
static unsigned char *oldEngineID = NULL;
static size_t   oldEngineIDLength = 0;
static struct timeval snmpv3starttime;

#if defined(IFHWADDRLEN) && defined(SIOCGIFHWADDR)
static int      getHwAddress(const char *networkDevice, char *addressOut);
#endif

/*******************************************************************-o-******
 * snmpv3_secLevel_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * Line syntax:
 *	defSecurityLevel "noAuthNoPriv" | "authNoPriv" | "authPriv"
 */

int
parse_secLevel_conf(const char *word, char *cptr) {
    if (strcasecmp(cptr, "noAuthNoPriv") == 0 || strcmp(cptr, "1") == 0 ||
	strcasecmp(cptr, "nanp") == 0) {
        return SNMP_SEC_LEVEL_NOAUTH;
    } else if (strcasecmp(cptr, "authNoPriv") == 0 || strcmp(cptr, "2") == 0 ||
	       strcasecmp(cptr, "anp") == 0) {
        return SNMP_SEC_LEVEL_AUTHNOPRIV;
    } else if (strcasecmp(cptr, "authPriv") == 0 || strcmp(cptr, "3") == 0 ||
	       strcasecmp(cptr, "ap") == 0) {
        return SNMP_SEC_LEVEL_AUTHPRIV;
    } else {
        return -1;
    }
}

void
snmpv3_secLevel_conf(const char *word, char *cptr)
{
    int             secLevel;

    if ((secLevel = parse_secLevel_conf( word, cptr )) >= 0 ) {
        netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_SECLEVEL, secLevel);
    } else {
	netsnmp_config_error("Unknown security level: %s", cptr);
    }
    DEBUGMSGTL(("snmpv3", "default secLevel set to: %s = %d\n", cptr,
                netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, 
				   NETSNMP_DS_LIB_SECLEVEL)));
}


NETSNMP_IMPORT int
snmpv3_options(char *optarg, netsnmp_session * session, char **Apsz,
               char **Xpsz, int argc, char *const *argv);
int
snmpv3_options(char *optarg, netsnmp_session * session, char **Apsz,
               char **Xpsz, int argc, char *const *argv)
{
    char           *cp = optarg;
    int testcase;
    optarg++;
    /*
     * Support '... -3x=value ....' syntax
     */
    if (*optarg == '=') {
        optarg++;
    }
    /*
     * and '.... "-3x value" ....'  (*with* the quotes)
     */
    while (*optarg && isspace((unsigned char)(*optarg))) {
        optarg++;
    }
    /*
     * Finally, handle ".... -3x value ...." syntax
     *   (*without* surrounding quotes)
     */
    if (!*optarg) {
        /*
         * We've run off the end of the argument
         *  so move on the the next.
         */
        optarg = argv[optind++];
        if (optind > argc) {
            fprintf(stderr,
                    "Missing argument after SNMPv3 '-3%c' option.\n", *cp);
            return (-1);
        }
    }

    switch (*cp) {

    case 'Z':
        errno=0;
        session->engineBoots = strtoul(optarg, &cp, 10);
        if (errno || cp == optarg) {
            fprintf(stderr, "Need engine boots value after -3Z flag.\n");
            return (-1);
        }
        if (*cp == ',') {
            char *endptr;
            cp++;
            session->engineTime = strtoul(cp, &endptr, 10);
            if (errno || cp == endptr) {
                fprintf(stderr, "Need engine time after \"-3Z engineBoot,\".\n");
                return (-1);
            }
        } else {
            fprintf(stderr, "Need engine time after \"-3Z engineBoot,\".\n");
            return (-1);
        }
        break;

    case 'e':{
            size_t          ebuf_len = 32, eout_len = 0;
            u_char         *ebuf = (u_char *) malloc(ebuf_len);

            if (ebuf == NULL) {
                fprintf(stderr, "malloc failure processing -3e flag.\n");
                return (-1);
            }
            if (!snmp_hex_to_binary
                (&ebuf, &ebuf_len, &eout_len, 1, optarg)) {
                fprintf(stderr, "Bad engine ID value after -3e flag.\n");
                SNMP_FREE(ebuf);
                return (-1);
            }
            session->securityEngineID = ebuf;
            session->securityEngineIDLen = eout_len;
            break;
        }

    case 'E':{
            size_t          ebuf_len = 32, eout_len = 0;
            u_char         *ebuf = (u_char *) malloc(ebuf_len);

            if (ebuf == NULL) {
                fprintf(stderr, "malloc failure processing -3E flag.\n");
                return (-1);
            }
            if (!snmp_hex_to_binary
                (&ebuf, &ebuf_len, &eout_len, 1, optarg)) {
                fprintf(stderr, "Bad engine ID value after -3E flag.\n");
                SNMP_FREE(ebuf);
                return (-1);
            }
            session->contextEngineID = ebuf;
            session->contextEngineIDLen = eout_len;
            break;
        }

    case 'n':
        session->contextName = optarg;
        session->contextNameLen = strlen(optarg);
        break;

    case 'u':
        session->securityName = optarg;
        session->securityNameLen = strlen(optarg);
        break;

    case 'l':
        if (!strcasecmp(optarg, "noAuthNoPriv") || !strcmp(optarg, "1") ||
            !strcasecmp(optarg, "nanp")) {
            session->securityLevel = SNMP_SEC_LEVEL_NOAUTH;
        } else if (!strcasecmp(optarg, "authNoPriv")
                   || !strcmp(optarg, "2") || !strcasecmp(optarg, "anp")) {
            session->securityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
        } else if (!strcasecmp(optarg, "authPriv") || !strcmp(optarg, "3")
                   || !strcasecmp(optarg, "ap")) {
            session->securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
        } else {
            fprintf(stderr,
                    "Invalid security level specified after -3l flag: %s\n",
                    optarg);
            return (-1);
        }

        break;

#ifdef NETSNMP_SECMOD_USM
    case 'a':
#ifndef NETSNMP_DISABLE_MD5
        if (!strcasecmp(optarg, "MD5")) {
            session->securityAuthProto = usmHMACMD5AuthProtocol;
            session->securityAuthProtoLen = USM_AUTH_PROTO_MD5_LEN;
        } else
#endif
            if (!strcasecmp(optarg, "SHA")) {
            session->securityAuthProto = usmHMACSHA1AuthProtocol;
            session->securityAuthProtoLen = USM_AUTH_PROTO_SHA_LEN;
        } else {
            fprintf(stderr,
                    "Invalid authentication protocol specified after -3a flag: %s\n",
                    optarg);
            return (-1);
        }
        break;

    case 'x':
        testcase = 0;
#ifndef NETSNMP_DISABLE_DES
        if (!strcasecmp(optarg, "DES")) {
            session->securityPrivProto = usmDESPrivProtocol;
            session->securityPrivProtoLen = USM_PRIV_PROTO_DES_LEN;
            testcase = 1;
        }
#endif
#ifdef HAVE_AES
        if (!strcasecmp(optarg, "AES128") ||
            strcasecmp(optarg, "AES")) {
            session->securityPrivProto = usmAES128PrivProtocol;
            session->securityPrivProtoLen = USM_PRIV_PROTO_AES128_LEN;
            testcase = 1;
        }
#endif
        if (testcase == 0) {
            fprintf(stderr,
                    "Invalid privacy protocol specified after -3x flag: %s\n",
                    optarg);
            return (-1);
        }
        break;

    case 'A':
        *Apsz = optarg;
        break;

    case 'X':
        *Xpsz = optarg;
        break;
#endif /* NETSNMP_SECMOD_USM */

    case 'm': {
        size_t bufSize = sizeof(session->securityAuthKey);
        u_char *tmpp = session->securityAuthKey;
        if (!snmp_hex_to_binary(&tmpp, &bufSize,
                                &session->securityAuthKeyLen, 0, optarg)) {
            fprintf(stderr, "Bad key value after -3m flag.\n");
            return (-1);
        }
        break;
    }

    case 'M': {
        size_t bufSize = sizeof(session->securityPrivKey);
        u_char *tmpp = session->securityPrivKey;
        if (!snmp_hex_to_binary(&tmpp, &bufSize,
             &session->securityPrivKeyLen, 0, optarg)) {
            fprintf(stderr, "Bad key value after -3M flag.\n");
            return (-1);
        }
        break;
    }

    case 'k': {
        size_t          kbuf_len = 32, kout_len = 0;
        u_char         *kbuf = (u_char *) malloc(kbuf_len);

        if (kbuf == NULL) {
            fprintf(stderr, "malloc failure processing -3k flag.\n");
            return (-1);
        }
        if (!snmp_hex_to_binary
            (&kbuf, &kbuf_len, &kout_len, 1, optarg)) {
            fprintf(stderr, "Bad key value after -3k flag.\n");
            SNMP_FREE(kbuf);
            return (-1);
        }
        session->securityAuthLocalKey = kbuf;
        session->securityAuthLocalKeyLen = kout_len;
        break;
    }

    case 'K': {
        size_t          kbuf_len = 32, kout_len = 0;
        u_char         *kbuf = (u_char *) malloc(kbuf_len);

        if (kbuf == NULL) {
            fprintf(stderr, "malloc failure processing -3K flag.\n");
            return (-1);
        }
        if (!snmp_hex_to_binary
            (&kbuf, &kbuf_len, &kout_len, 1, optarg)) {
            fprintf(stderr, "Bad key value after -3K flag.\n");
            SNMP_FREE(kbuf);
            return (-1);
        }
        session->securityPrivLocalKey = kbuf;
        session->securityPrivLocalKeyLen = kout_len;
        break;
    }
        
    default:
        fprintf(stderr, "Unknown SNMPv3 option passed to -3: %c.\n", *cp);
        return -1;
    }
    return 0;
}

/*******************************************************************-o-******
 * setup_engineID
 *
 * Parameters:
 *	**eidp
 *	 *text	Printable (?) text to be plugged into the snmpEngineID.
 *
 * Return:
 *	Length of allocated engineID string in bytes,  -OR-
 *	-1 on error.
 *
 *
 * Create an snmpEngineID using text and the local IP address.  If eidp
 * is defined, use it to return a pointer to the newly allocated data.
 * Otherwise, use the result to define engineID defined in this module.
 *
 * Line syntax:
 *	engineID <text> | NULL
 *
 * XXX	What if a node has multiple interfaces?
 * XXX	What if multiple engines all choose the same address?
 *      (answer:  You're screwed, because you might need a kul database
 *       which is dependant on the current engineID.  Enumeration and other
 *       tricks won't work). 
 */
int
setup_engineID(u_char ** eidp, const char *text)
{
    int             enterpriseid = htonl(NETSNMP_ENTERPRISE_OID),
        netsnmpoid = htonl(NETSNMP_OID),
        localsetup = (eidp) ? 0 : 1;

    /*
     * Use local engineID if *eidp == NULL.  
     */
#ifdef HAVE_GETHOSTNAME
    u_char          buf[SNMP_MAXBUF_SMALL];
    struct hostent *hent = NULL;
#endif
    u_char         *bufp = NULL;
    size_t          len;
    int             localEngineIDType = engineIDType;
    int             tmpint;
    time_t          tmptime;

    engineIDIsSet = 1;

#ifdef HAVE_GETHOSTNAME
#ifdef AF_INET6
    /*
     * see if they selected IPV4 or IPV6 support 
     */
    if ((ENGINEID_TYPE_IPV6 == localEngineIDType) ||
        (ENGINEID_TYPE_IPV4 == localEngineIDType)) {
        /*
         * get the host name and save the information 
         */
        gethostname((char *) buf, sizeof(buf));
        hent = netsnmp_gethostbyname((char *) buf);
        if (hent && hent->h_addrtype == AF_INET6) {
            localEngineIDType = ENGINEID_TYPE_IPV6;
        } else {
            /*
             * Not IPV6 so we go with default 
             */
            localEngineIDType = ENGINEID_TYPE_IPV4;
        }
    }
#else
    /*
     * No IPV6 support.  Check if they selected IPV6 engineID type.
     *  If so make it IPV4 instead 
     */
    if (ENGINEID_TYPE_IPV6 == localEngineIDType) {
        localEngineIDType = ENGINEID_TYPE_IPV4;
    }
    if (ENGINEID_TYPE_IPV4 == localEngineIDType) {
        /*
         * get the host name and save the information 
         */
        gethostname((char *) buf, sizeof(buf));
        hent = netsnmp_gethostbyname((char *) buf);
    }
#endif
#endif                          /* HAVE_GETHOSTNAME */

    /*
     * Determine if we have text and if so setup our localEngineIDType
     * * appropriately.  
     */
    if (NULL != text) {
        engineIDType = localEngineIDType = ENGINEID_TYPE_TEXT;
    }
    /*
     * Determine length of the engineID string. 
     */
    len = 5;                    /* always have 5 leading bytes */
    switch (localEngineIDType) {
    case ENGINEID_TYPE_TEXT:
        if (NULL == text) {
            snmp_log(LOG_ERR,
                     "Can't set up engineID of type text from an empty string.\n");
            return -1;
        }
        len += strlen(text);    /* 5 leading bytes+text. No NULL char */
        break;
#if defined(IFHWADDRLEN) && defined(SIOCGIFHWADDR)
    case ENGINEID_TYPE_MACADDR:        /* MAC address */
        len += 6;               /* + 6 bytes for MAC address */
        break;
#endif
    case ENGINEID_TYPE_IPV4:   /* IPv4 */
        len += 4;               /* + 4 byte IPV4 address */
        break;
    case ENGINEID_TYPE_IPV6:   /* IPv6 */
        len += 16;              /* + 16 byte IPV6 address */
        break;
    case ENGINEID_TYPE_NETSNMP_RND:        /* Net-SNMP specific encoding */
        if (engineID)           /* already setup, keep current value */
            return engineIDLength;
        if (oldEngineID) {
            len = oldEngineIDLength;
        } else {
            len += sizeof(int) + sizeof(time_t);
        }
        break;
    default:
        snmp_log(LOG_ERR,
                 "Unknown EngineID type requested for setup (%d).  Using IPv4.\n",
                 localEngineIDType);
        localEngineIDType = ENGINEID_TYPE_IPV4; /* make into IPV4 */
        len += 4;               /* + 4 byte IPv4 address */
        break;
    }                           /* switch */


    /*
     * Allocate memory and store enterprise ID.
     */
    if ((bufp = (u_char *) malloc(len)) == NULL) {
        snmp_log_perror("setup_engineID malloc");
        return -1;
    }
    if (localEngineIDType == ENGINEID_TYPE_NETSNMP_RND)
        /*
         * we must use the net-snmp enterprise id here, regardless 
         */
        memcpy(bufp, &netsnmpoid, sizeof(netsnmpoid));    /* XXX Must be 4 bytes! */
    else
        memcpy(bufp, &enterpriseid, sizeof(enterpriseid));      /* XXX Must be 4 bytes! */

    bufp[0] |= 0x80;


    /*
     * Store the given text  -OR-   the first found IP address
     *  -OR-  the MAC address  -OR-  random elements
     * (the latter being the recommended default)
     */
    switch (localEngineIDType) {
    case ENGINEID_TYPE_NETSNMP_RND:
        if (oldEngineID) {
            /*
             * keep our previous notion of the engineID 
             */
            memcpy(bufp, oldEngineID, oldEngineIDLength);
        } else {
            /*
             * Here we've desigend our own ENGINEID that is not based on
             * an address which may change and may even become conflicting
             * in the future like most of the default v3 engineID types
             * suffer from.
             * 
             * Ours is built from 2 fairly random elements: a random number and
             * the current time in seconds.  This method suffers from boxes
             * that may not have a correct clock setting and random number
             * seed at startup, but few OSes should have that problem.
             */
            bufp[4] = ENGINEID_TYPE_NETSNMP_RND;
            tmpint = random();
            memcpy(bufp + 5, &tmpint, sizeof(tmpint));
            tmptime = time(NULL);
            memcpy(bufp + 5 + sizeof(tmpint), &tmptime, sizeof(tmptime));
        }
        break;
    case ENGINEID_TYPE_TEXT:
        bufp[4] = ENGINEID_TYPE_TEXT;
        memcpy((char *) bufp + 5, (text), strlen(text));
        break;
#ifdef HAVE_GETHOSTNAME
#ifdef AF_INET6
    case ENGINEID_TYPE_IPV6:
        bufp[4] = ENGINEID_TYPE_IPV6;
        memcpy(bufp + 5, hent->h_addr_list[0], hent->h_length);
        break;
#endif
#endif
#if defined(IFHWADDRLEN) && defined(SIOCGIFHWADDR)
    case ENGINEID_TYPE_MACADDR:
        {
            int             x;
            bufp[4] = ENGINEID_TYPE_MACADDR;
            /*
             * use default NIC if none provided 
             */
            if (NULL == engineIDNic) {
	      x = getHwAddress(DEFAULT_NIC, (char *)&bufp[5]);
            } else {
	      x = getHwAddress((char *)engineIDNic, (char *)&bufp[5]);
            }
            if (0 != x)
                /*
                 * function failed fill MAC address with zeros 
                 */
            {
                memset(&bufp[5], 0, 6);
            }
        }
        break;
#endif
    case ENGINEID_TYPE_IPV4:
    default:
        bufp[4] = ENGINEID_TYPE_IPV4;
#ifdef HAVE_GETHOSTNAME
        if (hent && hent->h_addrtype == AF_INET) {
            memcpy(bufp + 5, hent->h_addr_list[0], hent->h_length);
        } else {                /* Unknown address type.  Default to 127.0.0.1. */

            bufp[5] = 127;
            bufp[6] = 0;
            bufp[7] = 0;
            bufp[8] = 1;
        }
#else                           /* HAVE_GETHOSTNAME */
        /*
         * Unknown address type.  Default to 127.0.0.1. 
         */
        bufp[5] = 127;
        bufp[6] = 0;
        bufp[7] = 0;
        bufp[8] = 1;
#endif                          /* HAVE_GETHOSTNAME */
        break;
    }

    /*
     * Pass the string back to the calling environment, or use it for
     * our local engineID.
     */
    if (localsetup) {
        SNMP_FREE(engineID);
        engineID = bufp;
        engineIDLength = len;

    } else {
        *eidp = bufp;
    }


    return len;

}                               /* end setup_engineID() */

int
free_engineID(int majorid, int minorid, void *serverarg,
	      void *clientarg)
{
    SNMP_FREE(engineID);
    SNMP_FREE(engineIDNic);
    SNMP_FREE(oldEngineID);
    engineIDIsSet = 0;
    return 0;
}

/*******************************************************************-o-******
 * engineBoots_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * Line syntax:
 *	engineBoots <num_boots>
 */
void
engineBoots_conf(const char *word, char *cptr)
{
    engineBoots = atoi(cptr) + 1;
    DEBUGMSGTL(("snmpv3", "engineBoots: %lu\n", engineBoots));
}

/*******************************************************************-o-******
 * engineIDType_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * Line syntax:
 *	engineIDType <1 or 3>
 *		1 is default for IPv4 engine ID type.  Will automatically
 *		    chose between IPv4 & IPv6 if either 1 or 2 is specified.
 *		2 is for IPv6.
 *		3 is hardware (MAC) address, currently supported under Linux
 */
void
engineIDType_conf(const char *word, char *cptr)
{
    engineIDType = atoi(cptr);
    /*
     * verify valid type selected 
     */
    switch (engineIDType) {
    case ENGINEID_TYPE_IPV4:   /* IPv4 */
    case ENGINEID_TYPE_IPV6:   /* IPv6 */
        /*
         * IPV? is always good 
         */
        break;
#if defined(IFHWADDRLEN) && defined(SIOCGIFHWADDR)
    case ENGINEID_TYPE_MACADDR:        /* MAC address */
        break;
#endif
    default:
        /*
         * unsupported one chosen 
         */
        config_perror("Unsupported enginedIDType, forcing IPv4");
        engineIDType = ENGINEID_TYPE_IPV4;
    }
    DEBUGMSGTL(("snmpv3", "engineIDType: %d\n", engineIDType));
}

/*******************************************************************-o-******
 * engineIDNic_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * Line syntax:
 *	engineIDNic <string>
 *		eth0 is default
 */
void
engineIDNic_conf(const char *word, char *cptr)
{
    /*
     * Make sure they haven't already specified the engineID via the
     * * configuration file 
     */
    if (0 == engineIDIsSet)
        /*
         * engineID has NOT been set via configuration file 
         */
    {
        /*
         * See if already set if so erase & release it 
         */
        SNMP_FREE(engineIDNic);
        engineIDNic = (u_char *) malloc(strlen(cptr) + 1);
        if (NULL != engineIDNic) {
            strcpy((char *) engineIDNic, cptr);
            DEBUGMSGTL(("snmpv3", "Initializing engineIDNic: %s\n",
                        engineIDNic));
        } else {
            DEBUGMSGTL(("snmpv3",
                        "Error allocating memory for engineIDNic!\n"));
        }
    } else {
        DEBUGMSGTL(("snmpv3",
                    "NOT setting engineIDNic, engineID already set\n"));
    }
}

/*******************************************************************-o-******
 * engineID_conf
 *
 * Parameters:
 *	*word
 *	*cptr
 *
 * This function reads a string from the configuration file and uses that
 * string to initialize the engineID.  It's assumed to be human readable.
 */
void
engineID_conf(const char *word, char *cptr)
{
    setup_engineID(NULL, cptr);
    DEBUGMSGTL(("snmpv3", "initialized engineID with: %s\n", cptr));
}

void
version_conf(const char *word, char *cptr)
{
    int valid = 0;
#ifndef NETSNMP_DISABLE_SNMPV1
    if ((strcmp(cptr,  "1") == 0) ||
        (strcmp(cptr, "v1") == 0)) {
        netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SNMPVERSION, 
			   NETSNMP_DS_SNMP_VERSION_1);       /* bogus value */
        valid = 1;
    }
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    if ((strcasecmp(cptr,  "2c") == 0) ||
               (strcasecmp(cptr, "v2c") == 0)) {
        netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SNMPVERSION, 
			   NETSNMP_DS_SNMP_VERSION_2c);
        valid = 1;
    }
#endif
    if ((strcasecmp(cptr,  "3" ) == 0) ||
               (strcasecmp(cptr, "v3" ) == 0)) {
        netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SNMPVERSION, 
			   NETSNMP_DS_SNMP_VERSION_3);
        valid = 1;
    }
    if (!valid) {
        config_perror("Unknown version specification");
        return;
    }
    DEBUGMSGTL(("snmpv3", "set default version to %d\n",
                netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, 
				   NETSNMP_DS_LIB_SNMPVERSION)));
}

/*
 * oldengineID_conf(const char *, char *):
 * 
 * Reads a octet string encoded engineID into the oldEngineID and
 * oldEngineIDLen pointers.
 */
void
oldengineID_conf(const char *word, char *cptr)
{
    read_config_read_octet_string(cptr, &oldEngineID, &oldEngineIDLength);
}

/*
 * exactEngineID_conf(const char *, char *):
 * 
 * Reads a octet string encoded engineID into the engineID and
 * engineIDLen pointers.
 */
void
exactEngineID_conf(const char *word, char *cptr)
{
    read_config_read_octet_string(cptr, &engineID, &engineIDLength);
    if (engineIDLength > MAX_ENGINEID_LENGTH) {
	netsnmp_config_error(
	    "exactEngineID '%s' too long; truncating to %d bytes",
	    cptr, MAX_ENGINEID_LENGTH);
        engineID[MAX_ENGINEID_LENGTH - 1] = '\0';
        engineIDLength = MAX_ENGINEID_LENGTH;
    }
    engineIDIsSet = 1;
    engineIDType = ENGINEID_TYPE_EXACT;
}


/*
 * merely call 
 */
netsnmp_feature_child_of(get_enginetime_alarm, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_GET_ENGINETIME_ALARM
void
get_enginetime_alarm(unsigned int regnum, void *clientargs)
{
    /* we do this every so (rarely) often just to make sure we watch
       wrapping of the times() output */
    snmpv3_local_snmpEngineTime();
}
#endif /* NETSNMP_FEATURE_REMOVE_GET_ENGINETIME_ALARM */

/*******************************************************************-o-******
 * init_snmpv3
 *
 * Parameters:
 *	*type	Label for the config file "type" used by calling entity.
 *      
 * Set time and engineID.
 * Set parsing functions for config file tokens.
 * Initialize SNMP Crypto API (SCAPI).
 */
void
init_snmpv3(const char *type)
{
    netsnmp_get_monotonic_clock(&snmpv3starttime);

    if (!type)
        type = "__snmpapp__";

    /*
     * we need to be called back later 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY,
                           SNMP_CALLBACK_POST_READ_CONFIG,
                           init_snmpv3_post_config, NULL);

    snmp_register_callback(SNMP_CALLBACK_LIBRARY,
                           SNMP_CALLBACK_POST_PREMIB_READ_CONFIG,
                           init_snmpv3_post_premib_config, NULL);
    /*
     * we need to be called back later 
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           snmpv3_store, (void *) strdup(type));

    /*
     * initialize submodules 
     */
    /*
     * NOTE: this must be after the callbacks are registered above,
     * since they need to be called before the USM callbacks. 
     */
    init_secmod();

    /*
     * register all our configuration handlers (ack, there's a lot) 
     */

    /*
     * handle engineID setup before everything else which may depend on it 
     */
    register_prenetsnmp_mib_handler(type, "engineID", engineID_conf, NULL,
                                    "string");
    register_prenetsnmp_mib_handler(type, "oldEngineID", oldengineID_conf,
                                    NULL, NULL);
    register_prenetsnmp_mib_handler(type, "exactEngineID", exactEngineID_conf,
                                    NULL, NULL);
    register_prenetsnmp_mib_handler(type, "engineIDType",
                                    engineIDType_conf, NULL, "num");
    register_prenetsnmp_mib_handler(type, "engineIDNic", engineIDNic_conf,
                                    NULL, "string");
    register_config_handler(type, "engineBoots", engineBoots_conf, NULL,
                            NULL);

    /*
     * default store config entries 
     */
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defSecurityName",
			       NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SECNAME);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defContext", 
			       NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_CONTEXT);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defPassphrase",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_PASSPHRASE);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defAuthPassphrase",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_AUTHPASSPHRASE);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defPrivPassphrase",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_PRIVPASSPHRASE);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defAuthMasterKey",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_AUTHMASTERKEY);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defPrivMasterKey",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_PRIVMASTERKEY);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defAuthLocalizedKey",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_AUTHLOCALIZEDKEY);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defPrivLocalizedKey",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_PRIVLOCALIZEDKEY);
    register_config_handler("snmp", "defVersion", version_conf, NULL,
                            "1|2c|3");

    register_config_handler("snmp", "defSecurityLevel",
                            snmpv3_secLevel_conf, NULL,
                            "noAuthNoPriv|authNoPriv|authPriv");
}

/*
 * initializations for SNMPv3 to be called after the configuration files
 * have been read.
 */

int
init_snmpv3_post_config(int majorid, int minorid, void *serverarg,
                        void *clientarg)
{

    size_t          engineIDLen;
    u_char         *c_engineID;

    c_engineID = snmpv3_generate_engineID(&engineIDLen);

    if (engineIDLen == 0 || !c_engineID) {
        /*
         * Somethine went wrong - help! 
         */
        SNMP_FREE(c_engineID);
        return SNMPERR_GENERR;
    }

    /*
     * if our engineID has changed at all, the boots record must be set to 1 
     */
    if (engineIDLen != oldEngineIDLength ||
        oldEngineID == NULL || c_engineID == NULL ||
        memcmp(oldEngineID, c_engineID, engineIDLen) != 0) {
        engineBoots = 1;
    }

#ifdef NETSNMP_SECMOD_USM
    /*
     * for USM set our local engineTime in the LCD timing cache 
     */
    set_enginetime(c_engineID, engineIDLen,
                   snmpv3_local_snmpEngineBoots(),
                   snmpv3_local_snmpEngineTime(), TRUE);
#endif /* NETSNMP_SECMOD_USM */

    SNMP_FREE(c_engineID);
    return SNMPERR_SUCCESS;
}

int
init_snmpv3_post_premib_config(int majorid, int minorid, void *serverarg,
                               void *clientarg)
{
    if (!engineIDIsSet)
        setup_engineID(NULL, NULL);

    return SNMPERR_SUCCESS;
}

/*******************************************************************-o-******
 * store_snmpv3
 *
 * Parameters:
 *	*type
 */
int
snmpv3_store(int majorID, int minorID, void *serverarg, void *clientarg)
{
    char            line[SNMP_MAXBUF_SMALL];
    u_char          c_engineID[SNMP_MAXBUF_SMALL];
    int             engineIDLen;
    const char     *type = (const char *) clientarg;

    if (type == NULL)           /* should never happen, since the arg is ours */
        type = "unknown";

    sprintf(line, "engineBoots %ld", engineBoots);
    read_config_store(type, line);

    engineIDLen = snmpv3_get_engineID(c_engineID, SNMP_MAXBUF_SMALL);

    if (engineIDLen) {
        /*
         * store the engineID used for this run 
         */
        sprintf(line, "oldEngineID ");
        read_config_save_octet_string(line + strlen(line), c_engineID,
                                      engineIDLen);
        read_config_store(type, line);
    }
    return SNMPERR_SUCCESS;
}                               /* snmpv3_store() */

u_long
snmpv3_local_snmpEngineBoots(void)
{
    return engineBoots;
}


/*******************************************************************-o-******
 * snmpv3_get_engineID
 *
 * Parameters:
 *	*buf
 *	 buflen
 *      
 * Returns:
 *	Length of engineID	On Success
 *	SNMPERR_GENERR		Otherwise.
 *
 *
 * Store engineID in buf; return the length.
 *
 */
size_t
snmpv3_get_engineID(u_char * buf, size_t buflen)
{
    /*
     * Sanity check.
     */
    if (!buf || (buflen < engineIDLength)) {
        return 0;
    }
    if (!engineID) {
        return 0;
    }

    memcpy(buf, engineID, engineIDLength);
    return engineIDLength;

}                               /* end snmpv3_get_engineID() */

/*******************************************************************-o-******
 * snmpv3_clone_engineID
 *
 * Parameters:
 *	**dest
 *       *dest_len
 *       src
 *	 srclen
 *      
 * Returns:
 *	Length of engineID	On Success
 *	0		        Otherwise.
 *
 *
 * Clones engineID, creates memory
 *
 */
int
snmpv3_clone_engineID(u_char ** dest, size_t * destlen, u_char * src,
                      size_t srclen)
{
    if (!dest || !destlen)
        return 0;

    SNMP_FREE(*dest);
    *destlen = 0;

    if (srclen && src) {
        *dest = (u_char *) malloc(srclen);
        if (*dest == NULL)
            return 0;
        memmove(*dest, src, srclen);
        *destlen = srclen;
    }
    return *destlen;
}                               /* end snmpv3_clone_engineID() */


/*******************************************************************-o-******
 * snmpv3_generate_engineID
 *
 * Parameters:
 *	*length
 *      
 * Returns:
 *	Pointer to copy of engineID	On Success.
 *	NULL				If malloc() or snmpv3_get_engineID()
 *						fail.
 *
 * Generates a malloced copy of our engineID.
 *
 * 'length' is set to the length of engineID  -OR-  < 0 on failure.
 */
u_char         *
snmpv3_generate_engineID(size_t * length)
{
    u_char         *newID;
    newID = (u_char *) malloc(engineIDLength);

    if (newID) {
        *length = snmpv3_get_engineID(newID, engineIDLength);
    }

    if (*length == 0) {
        SNMP_FREE(newID);
        newID = NULL;
    }

    return newID;

}                               /* end snmpv3_generate_engineID() */

/**
 * Return the value of snmpEngineTime. According to RFC 3414 snmpEngineTime
 * is a 31-bit counter. engineBoots must be incremented every time that
 * counter wraps around.
 *
 * @see See also <a href="http://tools.ietf.org/html/rfc3414">RFC 3414</a>.
 *
 * @note It is assumed that this function is called at least once every
 *   2**31 seconds.
 */
u_long
snmpv3_local_snmpEngineTime(void)
{
#ifdef NETSNMP_FEATURE_CHECKING
    netsnmp_feature_require(calculate_sectime_diff)
#endif /* NETSNMP_FEATURE_CHECKING */

    static uint32_t last_engineTime;
    struct timeval  now;
    uint32_t engineTime;

    netsnmp_get_monotonic_clock(&now);
    engineTime = calculate_sectime_diff(&now, &snmpv3starttime) & 0x7fffffffL;
    if (engineTime < last_engineTime)
        engineBoots++;
    last_engineTime = engineTime;
    return engineTime;
}



/*
 * Code only for Linux systems 
 */
#if defined(IFHWADDRLEN) && defined(SIOCGIFHWADDR)
static int
getHwAddress(const char *networkDevice, /* e.g. "eth0", "eth1" */
             char *addressOut)
{                               /* return address. Len=IFHWADDRLEN */
    /*
     * getHwAddress(...)
     * *
     * *  This function will return a Network Interfaces Card's Hardware
     * *  address (aka MAC address).
     * *
     * *  Input Parameter(s):
     * *      networkDevice - a null terminated string with the name of a network
     * *                      device.  Examples: eth0, eth1, etc...
     * *
     * *  Output Parameter(s):
     * *      addressOut -    This is the binary value of the hardware address.
     * *                      This value is NOT converted into a hexadecimal string.
     * *                      The caller must pre-allocate for a return value of
     * *                      length IFHWADDRLEN
     * *
     * *  Return value:   This function will return zero (0) for success.  If
     * *                  an error occurred the function will return -1.
     * *
     * *  Caveats:    This has only been tested on Ethernet networking cards.
     */
    int             sock;       /* our socket */
    struct ifreq    request;    /* struct which will have HW address */

    if ((NULL == networkDevice) || (NULL == addressOut)) {
        return -1;
    }
    /*
     * In order to find out the hardware (MAC) address of our system under
     * * Linux we must do the following:
     * * 1.  Create a socket
     * * 2.  Do an ioctl(...) call with the SIOCGIFHWADDRLEN operation.
     */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return -1;
    }
    /*
     * erase the request block 
     */
    memset(&request, 0, sizeof(request));
    /*
     * copy the name of the net device we want to find the HW address for 
     */
    strlcpy(request.ifr_name, networkDevice, IFNAMSIZ);
    /*
     * Get the HW address 
     */
    if (ioctl(sock, SIOCGIFHWADDR, &request)) {
        close(sock);
        return -1;
    }
    close(sock);
    memcpy(addressOut, request.ifr_hwaddr.sa_data, IFHWADDRLEN);
    return 0;
}
#endif

#ifdef NETSNMP_ENABLE_TESTING_CODE
/**
 * Set SNMPv3 engineBoots and start time.
 *
 * @note This function does not exist. Go away. It certainly should never be
 *   used, unless in a testing scenario, which is why it was created
 */
void
snmpv3_set_engineBootsAndTime(int boots, int ttime)
{
    engineBoots = boots;
    netsnmp_get_monotonic_clock(&snmpv3starttime);
    snmpv3starttime.tv_sec -= ttime;
}
#endif
