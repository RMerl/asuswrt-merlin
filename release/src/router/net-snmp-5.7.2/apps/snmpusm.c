/*
 * snmpusm.c - send snmp SET requests to a network entity to change the
 *             usm user database
 *
 * XXX get engineID dynamically.
 * XXX read passwords from prompts
 * XXX customize responses with user names, etc.
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <ctype.h>
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
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if defined(HAVE_OPENSSL_DH_H) && defined(HAVE_LIBCRYPTO)
#include <openssl/dh.h>
#endif /* HAVE_OPENSSL_DH_H && HAVE_LIBCRYPTO */

#include <net-snmp/net-snmp-includes.h>

int             main(int, char **);

#define CMD_PASSWD_NAME    "passwd"
#define CMD_PASSWD         1
#define CMD_CREATE_NAME    "create"
#define CMD_CREATE         2
#define CMD_DELETE_NAME    "delete"
#define CMD_DELETE         3
#define CMD_CLONEFROM_NAME "cloneFrom"
#define CMD_CLONEFROM      4
#define CMD_ACTIVATE_NAME  "activate"
#define CMD_ACTIVATE       5
#define CMD_DEACTIVATE_NAME "deactivate"
#define CMD_DEACTIVATE     6
#define CMD_CHANGEKEY_NAME  "changekey"
#define CMD_CHANGEKEY      7

#define CMD_NUM    7

static const char *successNotes[CMD_NUM] = {
    "SNMPv3 Key(s) successfully changed.",
    "User successfully created.",
    "User successfully deleted.",
    "User successfully cloned.",
    "User successfully activated.",
    "User successfully deactivated.",
    "SNMPv3 Key(s) successfully changed."
};

#define                   USM_OID_LEN    12
#define                DH_USM_OID_LEN    11

static oid

authKeyOid[MAX_OID_LEN] = { 1, 3, 6, 1, 6, 3, 15, 1, 2, 2, 1, 6 },
ownAuthKeyOid[MAX_OID_LEN] = {1, 3, 6, 1, 6, 3, 15, 1, 2, 2, 1, 7},
privKeyOid[MAX_OID_LEN] = {1, 3, 6, 1, 6, 3, 15, 1, 2, 2, 1, 9},
ownPrivKeyOid[MAX_OID_LEN] = {1, 3, 6, 1, 6, 3, 15, 1, 2, 2, 1, 10},
usmUserCloneFrom[MAX_OID_LEN] = {1, 3, 6, 1, 6, 3, 15, 1, 2, 2, 1, 4},
usmUserSecurityName[MAX_OID_LEN] = {1, 3, 6, 1, 6, 3, 15, 1, 2, 2, 1, 3},
usmUserPublic[MAX_OID_LEN] = {1, 3, 6, 1, 6, 3, 15, 1, 2, 2, 1, 11},
usmUserStatus[MAX_OID_LEN] = {1, 3, 6, 1, 6, 3, 15, 1, 2, 2, 1, 13},
/* diffie helman change key objects */
usmDHUserAuthKeyChange[MAX_OID_LEN] = {1, 3, 6, 1, 3, 101, 1, 1, 2, 1, 1 },
usmDHUserPrivKeyChange[MAX_OID_LEN] = {1, 3, 6, 1, 3, 101, 1, 1, 2, 1, 3 },
#if defined(HAVE_OPENSSL_DH_H) && defined(HAVE_LIBCRYPTO)
usmDHUserOwnAuthKeyChange[MAX_OID_LEN] = {1, 3, 6, 1, 3, 101, 1, 1, 2, 1, 2 },
usmDHUserOwnPrivKeyChange[MAX_OID_LEN] = {1, 3, 6, 1, 3, 101, 1, 1, 2, 1, 4 },
#endif /* HAVE_OPENSSL_DH_H && HAVE_LIBCRYPTO */
usmDHParameters[] = { 1,3,6,1,3,101,1,1,1,0 }
;
size_t usmDHParameters_len = OID_LENGTH(usmDHParameters);

static
oid            *authKeyChange = authKeyOid, *privKeyChange = privKeyOid;
oid            *dhauthKeyChange = usmDHUserAuthKeyChange,
               *dhprivKeyChange = usmDHUserPrivKeyChange;
int             doauthkey = 0, doprivkey = 0, uselocalizedkey = 0;
size_t          usmUserEngineIDLen = 0;
u_char         *usmUserEngineID = NULL;
char           *usmUserPublic_val = NULL;
int             docreateandwait = 0;


void
usage(void)
{
    fprintf(stderr, "Usage: snmpusm ");
    snmp_parse_args_usage(stderr);
    fprintf(stderr, " COMMAND\n\n");
    snmp_parse_args_descriptions(stderr);
    fprintf(stderr, "\nsnmpusm commands:\n");
    fprintf(stderr, "  [options]               create     USER [CLONEFROM-USER]\n");
    fprintf(stderr, "  [options]               delete     USER\n");
    fprintf(stderr, "  [options]               activate   USER\n");
    fprintf(stderr, "  [options]               deactivate USER\n");
    fprintf(stderr, "  [options] [-Cw]         cloneFrom  USER CLONEFROM-USER\n");
    fprintf(stderr, "  [options] [-Ca] [-Cx]   changekey  [USER]\n");
    fprintf(stderr,
            "  [options] [-Ca] [-Cx]   passwd     OLD-PASSPHRASE NEW-PASSPHRASE [USER]\n");
    fprintf(stderr,
            "  [options] (-Ca|-Cx) -Ck passwd     OLD-KEY-OR-PASS NEW-KEY-OR-PASS [USER]\n");
    fprintf(stderr, "\nsnmpusm options:\n");
    fprintf(stderr, "\t-CE ENGINE-ID\tSet usmUserEngineID (e.g. 800000020109840301).\n");
    fprintf(stderr, "\t-Cp STRING\tSet usmUserPublic value to STRING.\n");
    fprintf(stderr, "\t-Cw\t\tCreate the user with createAndWait.\n");
    fprintf(stderr, "\t\t\t(it won't be active until you active it)\n");
    fprintf(stderr, "\t-Cx\t\tChange the privacy key.\n");
    fprintf(stderr, "\t-Ca\t\tChange the authentication key.\n");
    fprintf(stderr, "\t-Ck\t\tAllows to use localized key (must start with 0x)\n");
    fprintf(stderr, "\t\t\tinstead of passphrase.\n");
}

/*
 * setup_oid appends to the oid the index for the engineid/user 
 */
void
setup_oid(oid * it, size_t * len, u_char * id, size_t idlen,
          const char *user)
{
    int             i, itIndex = *len;

    *len = itIndex + 1 + idlen + 1 + strlen(user);

    it[itIndex++] = idlen;
    for (i = 0; i < (int) idlen; i++) {
        it[itIndex++] = id[i];
    }

    it[itIndex++] = strlen(user);
    for (i = 0; i < (int) strlen(user); i++) {
        it[itIndex++] = user[i];
    }

#ifdef NETSNMP_ENABLE_TESTING_CODE
    fprintf(stdout, "setup_oid: ");  
    fprint_objid(stdout, it, *len);  
    fprintf(stdout, "\n");  
#endif
}

#if defined(HAVE_OPENSSL_DH_H) && defined(HAVE_LIBCRYPTO)
int
get_USM_DH_key(netsnmp_variable_list *vars, netsnmp_variable_list *dhvar,
               size_t outkey_len,
               netsnmp_pdu *pdu, const char *keyname,
               oid *keyoid, size_t keyoid_len) {
    u_char *dhkeychange;
    DH *dh;
    BIGNUM *other_pub;
    u_char *key;
    size_t key_len;
            
    dhkeychange = (u_char *) malloc(2 * vars->val_len * sizeof(char));
    if (!dhkeychange)
        return SNMPERR_GENERR;
    
    memcpy(dhkeychange, vars->val.string, vars->val_len);

    {
        const unsigned char *cp = dhvar->val.string;
        dh = d2i_DHparams(NULL, &cp, dhvar->val_len);
    }

    if (!dh || !dh->g || !dh->p) {
        SNMP_FREE(dhkeychange);
        return SNMPERR_GENERR;
    }

    DH_generate_key(dh);
    if (!dh->pub_key) {
        SNMP_FREE(dhkeychange);
        return SNMPERR_GENERR;
    }
            
    if (vars->val_len != (unsigned int)BN_num_bytes(dh->pub_key)) {
        SNMP_FREE(dhkeychange);
        fprintf(stderr,"incorrect diffie-helman lengths (%lu != %d)\n",
                (unsigned long)vars->val_len, BN_num_bytes(dh->pub_key));
        return SNMPERR_GENERR;
    }

    BN_bn2bin(dh->pub_key, dhkeychange + vars->val_len);

    key_len = DH_size(dh);
    if (!key_len) {
        SNMP_FREE(dhkeychange);
        return SNMPERR_GENERR;
    }
    key = (u_char *) malloc(key_len * sizeof(u_char));

    if (!key) {
        SNMP_FREE(dhkeychange);
        return SNMPERR_GENERR;
    }

    other_pub = BN_bin2bn(vars->val.string, vars->val_len, NULL);
    if (!other_pub) {
        SNMP_FREE(dhkeychange);
        SNMP_FREE(key);
        return SNMPERR_GENERR;
    }

    if (DH_compute_key(key, other_pub, dh)) {
        u_char *kp;

        printf("new %s key: 0x", keyname);
        for(kp = key + key_len - outkey_len;
            kp - key < (int)key_len;  kp++) {
            printf("%02x", (unsigned char) *kp);
        }
        printf("\n");
    }

    snmp_pdu_add_variable(pdu, keyoid, keyoid_len,
                          ASN_OCTET_STR, dhkeychange,
                          2 * vars->val_len);

    SNMP_FREE(dhkeychange);
    SNMP_FREE(other_pub);
    SNMP_FREE(key);

    return SNMPERR_SUCCESS;
}
#endif /* HAVE_OPENSSL_DH_H */

static void
optProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
            case 'a':
                doauthkey = 1;
                break;

            case 'x':
                doprivkey = 1;
                break;

	    case 'k':
	        uselocalizedkey = 1;
		break;

	    case 'p':
                if (optind < argc) {
		    usmUserPublic_val =  argv[optind];
                } else {
                    fprintf(stderr, "Bad -Cp option: no argument given\n");
                    exit(1);
                }
                optind++;
                break;

            case 'w':
                docreateandwait = 1;
                break;

	    case 'E': {
	        size_t ebuf_len = MAX_ENGINEID_LENGTH;
                u_char *ebuf;
                if (optind < argc) {
                    if (argv[optind]) {
                        ebuf = (u_char *)malloc(ebuf_len);
                        if (ebuf == NULL) {
                            fprintf(stderr, 
                                    "malloc failure processing -CE option.\n");
                            exit(1);
                        }
		        if (!snmp_hex_to_binary(&ebuf, &ebuf_len,
                                                &usmUserEngineIDLen, 1, argv[optind])) {
                            fprintf(stderr, 
                                    "Bad usmUserEngineID value after -CE option.\n");
		            free(ebuf);
		            exit(1);
		        }
		        usmUserEngineID = ebuf;
		        DEBUGMSGTL(("snmpusm", "usmUserEngineID set to: "));
		        DEBUGMSGHEX(("snmpusm", usmUserEngineID, usmUserEngineIDLen));
		        DEBUGMSG(("snmpusm", "\n"));

                    }
                } else {
                    fprintf(stderr, "Bad -CE option: no argument given\n");
                    exit(1);
                }
                optind++;
                break;
            }

            default:
                fprintf(stderr, "Unknown flag passed to -C: %c\n",
                        optarg[-1]);
                exit(1);
            }
        }
        break;
    }
}

int
main(int argc, char *argv[])
{
    netsnmp_session session, *ss;
    netsnmp_pdu    *pdu = NULL, *response = NULL;

    int             arg;
    size_t          name_length = USM_OID_LEN;
    size_t          name_length2 = USM_OID_LEN;
    int             status;
    int             exitval = 0;
    int             rval;
    int             command = 0;
    long            longvar;

    size_t          oldKu_len = SNMP_MAXBUF_SMALL,
        newKu_len = SNMP_MAXBUF_SMALL,
        oldkul_len = SNMP_MAXBUF_SMALL,
        oldkulpriv_len = SNMP_MAXBUF_SMALL,
        newkulpriv_len = SNMP_MAXBUF_SMALL,
        newkul_len = SNMP_MAXBUF_SMALL,
        keychange_len = SNMP_MAXBUF_SMALL,
        keychangepriv_len = SNMP_MAXBUF_SMALL;

    char           *newpass = NULL, *oldpass = NULL;
    u_char          oldKu[SNMP_MAXBUF_SMALL],
        newKu[SNMP_MAXBUF_SMALL],
        oldkul[SNMP_MAXBUF_SMALL],
        oldkulpriv[SNMP_MAXBUF_SMALL],
        newkulpriv[SNMP_MAXBUF_SMALL],
        newkul[SNMP_MAXBUF_SMALL], keychange[SNMP_MAXBUF_SMALL],
        keychangepriv[SNMP_MAXBUF_SMALL];

    authKeyChange = authKeyOid;
    privKeyChange = privKeyOid;

    /*
     * get the common command line arguments 
     */
    switch (arg = snmp_parse_args(argc, argv, &session, "C:", optProc)) {
    case NETSNMP_PARSE_ARGS_ERROR:
        exit(1);
    case NETSNMP_PARSE_ARGS_SUCCESS_EXIT:
        exit(0);
    case NETSNMP_PARSE_ARGS_ERROR_USAGE:
        usage();
        exit(1);
    default:
        break;
    }

    if (arg >= argc) {
        fprintf(stderr, "Please specify an operation to perform.\n");
        usage();
        exit(1);
    }

    SOCK_STARTUP;

    /*
     * open an SNMP session 
     */
    /*
     * Note:  this needs to obtain the engineID used below 
     */
    session.flags &= ~SNMP_FLAGS_DONT_PROBE;
    ss = snmp_open(&session);
    if (ss == NULL) {
        /*
         * diagnose snmp_open errors with the input netsnmp_session pointer 
         */
        snmp_sess_perror("snmpusm", &session);
        exit(1);
    }

    /*
     * set usmUserEngineID from ss->contextEngineID
     *   if not already set (via -CE)
     */
    if (usmUserEngineID == NULL) {
      usmUserEngineID    = ss->contextEngineID;
      usmUserEngineIDLen = ss->contextEngineIDLen;
    }

    /*
     * create PDU for SET request and add object names and values to request 
     */
    pdu = snmp_pdu_create(SNMP_MSG_SET);
    if (!pdu) {
        fprintf(stderr, "Failed to create request\n");
        exit(1);
    }


    if (strcmp(argv[arg], CMD_PASSWD_NAME) == 0) {

        /*
         * passwd: change a users password.
         *
         * XXX:  Uses the auth type of the calling user, a MD5 user can't
         *       change a SHA user's key.
         */
        char *passwd_user;

        command = CMD_PASSWD;
        oldpass = argv[++arg];
        newpass = argv[++arg];
        passwd_user = argv[++arg];

        if (doprivkey == 0 && doauthkey == 0)
            doprivkey = doauthkey = 1;

        if (newpass == NULL || strlen(newpass) < USM_LENGTH_P_MIN) {
            fprintf(stderr,
                    "New passphrase must be greater than %d characters in length.\n",
                    USM_LENGTH_P_MIN);
            exit(1);
        }

        if (oldpass == NULL || strlen(oldpass) < USM_LENGTH_P_MIN) {
            fprintf(stderr,
                    "Old passphrase must be greater than %d characters in length.\n",
                    USM_LENGTH_P_MIN);
            exit(1);
        }

        /* 
         * Change the user supplied on command line.
         */
        if ((passwd_user != NULL) && (strlen(passwd_user) > 0)) {
            session.securityName = passwd_user;
        } else {
            /*
             * Use own key object if no user was supplied.
             */
            authKeyChange = ownAuthKeyOid;
            privKeyChange = ownPrivKeyOid;
        }

        /*
         * do we have a securityName?  If not, copy the default 
         */
        if (session.securityName == NULL) {
            session.securityName = 
	      strdup(netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
					   NETSNMP_DS_LIB_SECNAME));
        }

        /*
         * the old Ku is in the session, but we need the new one 
         */
        if (session.securityAuthProto == NULL) {
            /*
             * get .conf set default 
             */
            const oid      *def =
                get_default_authtype(&session.securityAuthProtoLen);
            session.securityAuthProto =
                snmp_duplicate_objid(def, session.securityAuthProtoLen);
        }
        if (session.securityAuthProto == NULL) {
            /*
             * assume MD5 
             */
#ifndef NETSNMP_DISABLE_MD5
            session.securityAuthProtoLen =
                sizeof(usmHMACMD5AuthProtocol) / sizeof(oid);
            session.securityAuthProto =
                snmp_duplicate_objid(usmHMACMD5AuthProtocol,
                                     session.securityAuthProtoLen);
#else
            session.securityAuthProtoLen =
                sizeof(usmHMACSHA1AuthProtocol) / sizeof(oid);
            session.securityAuthProto =
                snmp_duplicate_objid(usmHMACSHA1AuthProtocol,
                                     session.securityAuthProtoLen);
#endif

        }

	if (uselocalizedkey && (strncmp(oldpass, "0x", 2) == 0)) {
	    /*
	     * use the localized key from the command line
	     */
	    u_char *buf;
	    size_t buf_len = SNMP_MAXBUF_SMALL;
	    buf = (u_char *) malloc (buf_len * sizeof(u_char));

	    oldkul_len = 0; /* initialize the offset */
	    if (!snmp_hex_to_binary((u_char **) (&buf), &buf_len, &oldkul_len, 0, oldpass)) {
	      snmp_perror(argv[0]);
	      fprintf(stderr, "generating the old Kul from localized key failed\n");
	      exit(1);
	    }
	    
	    memcpy(oldkul, buf, oldkul_len);
	    SNMP_FREE(buf);
	}
	else {
	    /*
	     * the old Ku is in the session, but we need the new one 
	     */
	    rval = generate_Ku(session.securityAuthProto,
			       session.securityAuthProtoLen,
			       (u_char *) oldpass, strlen(oldpass),
			       oldKu, &oldKu_len);
	    
	    if (rval != SNMPERR_SUCCESS) {
	        snmp_perror(argv[0]);
	        fprintf(stderr, "generating the old Ku failed\n");
	        exit(1);
	    }

	    /*
	     * generate the two Kul's 
	     */
	    rval = generate_kul(session.securityAuthProto,
				session.securityAuthProtoLen,
				usmUserEngineID, usmUserEngineIDLen,
				oldKu, oldKu_len, oldkul, &oldkul_len);
	    
	    if (rval != SNMPERR_SUCCESS) {
	        snmp_perror(argv[0]);
		fprintf(stderr, "generating the old Kul failed\n");
		exit(1);
	    }
	}
	if (uselocalizedkey && (strncmp(newpass, "0x", 2) == 0)) {
	    /*
	     * use the localized key from the command line
	     */
	    u_char *buf;
	    size_t buf_len = SNMP_MAXBUF_SMALL;
	    buf = (u_char *) malloc (buf_len * sizeof(u_char));

	    newkul_len = 0; /* initialize the offset */
	    if (!snmp_hex_to_binary((u_char **) (&buf), &buf_len, &newkul_len, 0, newpass)) {
	      snmp_perror(argv[0]);
	      fprintf(stderr, "generating the new Kul from localized key failed\n");
	      exit(1);
	    }
	    
	    memcpy(newkul, buf, newkul_len);
	    SNMP_FREE(buf);
	} else {
            rval = generate_Ku(session.securityAuthProto,
                               session.securityAuthProtoLen,
                               (u_char *) newpass, strlen(newpass),
                               newKu, &newKu_len);

            if (rval != SNMPERR_SUCCESS) {
                snmp_perror(argv[0]);
                fprintf(stderr, "generating the new Ku failed\n");
                exit(1);
            }

	    rval = generate_kul(session.securityAuthProto,
				session.securityAuthProtoLen,
				usmUserEngineID, usmUserEngineIDLen,
				newKu, newKu_len, newkul, &newkul_len);

	    if (rval != SNMPERR_SUCCESS) {
	        snmp_perror(argv[0]);
		fprintf(stderr, "generating the new Kul failed\n");
		exit(1);
	    }
	}

        /*
         * for encryption, we may need to truncate the key to the proper length
         * so we need two copies.  For simplicity, we always just copy even if
         * they're the same lengths.
         */
        if (doprivkey) {
            if (!session.securityPrivProto) {
                snmp_log(LOG_ERR, "no encryption type specified, which I need in order to know to change the key\n");
                exit(1);
            }
                
#ifndef NETSNMP_DISABLE_DES
            if (ISTRANSFORM(session.securityPrivProto, DESPriv)) {
                /* DES uses a 128 bit key, 64 bits of which is a salt */
                oldkulpriv_len = newkulpriv_len = 16;
            }
#endif
#ifdef HAVE_AES
            if (ISTRANSFORM(session.securityPrivProto, AESPriv)) {
                oldkulpriv_len = newkulpriv_len = 16;
            }
#endif
            memcpy(oldkulpriv, oldkul, oldkulpriv_len);
            memcpy(newkulpriv, newkul, newkulpriv_len);
        }
            

        /*
         * create the keychange string 
         */
	if (doauthkey) {
	  rval = encode_keychange(session.securityAuthProto,
				  session.securityAuthProtoLen,
				  oldkul, oldkul_len,
				  newkul, newkul_len,
				  keychange, &keychange_len);

	  if (rval != SNMPERR_SUCCESS) {
	    snmp_perror(argv[0]);
            fprintf(stderr, "encoding the keychange failed\n");
            usage();
            exit(1);
	  }
	}

        /* which is slightly different for encryption if lengths are
           different */
	if (doprivkey) {
	  rval = encode_keychange(session.securityAuthProto,
                                session.securityAuthProtoLen,
                                oldkulpriv, oldkulpriv_len,
                                newkulpriv, newkulpriv_len,
                                keychangepriv, &keychangepriv_len);

	  if (rval != SNMPERR_SUCCESS) {
            snmp_perror(argv[0]);
            fprintf(stderr, "encoding the keychange failed\n");
            usage();
            exit(1);
	  }
	}

        /*
         * add the keychange string to the outgoing packet 
         */
        if (doauthkey) {
            setup_oid(authKeyChange, &name_length,
                      usmUserEngineID, usmUserEngineIDLen,
                      session.securityName);
            snmp_pdu_add_variable(pdu, authKeyChange, name_length,
                                  ASN_OCTET_STR, keychange, keychange_len);
        }
        if (doprivkey) {
            setup_oid(privKeyChange, &name_length2,
                      usmUserEngineID, usmUserEngineIDLen,
                      session.securityName);
            snmp_pdu_add_variable(pdu, privKeyChange, name_length2,
                                  ASN_OCTET_STR,
                                  keychangepriv, keychangepriv_len);
        }

    } else if (strcmp(argv[arg], CMD_CREATE_NAME) == 0) {
        /*
         * create:  create a user
         *
         * create USER [CLONEFROM]
         */
        if (++arg >= argc) {
            fprintf(stderr, "You must specify the user name to create\n");
            usage();
            exit(1);
        }

        command = CMD_CREATE;

        if (++arg < argc) {
            /*
             * clone the new user from an existing user
             *   (and make them active immediately)
             */
            setup_oid(usmUserStatus, &name_length,
                      usmUserEngineID, usmUserEngineIDLen, argv[arg-1]);
            if (docreateandwait) {
                longvar = RS_CREATEANDWAIT;
            } else {
                longvar = RS_CREATEANDGO;
            }
            snmp_pdu_add_variable(pdu, usmUserStatus, name_length,
                                  ASN_INTEGER, (u_char *) & longvar,
                                  sizeof(longvar));

            name_length = USM_OID_LEN;
            setup_oid(usmUserCloneFrom, &name_length,
                      usmUserEngineID, usmUserEngineIDLen,
                      argv[arg - 1]);
            setup_oid(usmUserSecurityName, &name_length2,
                      usmUserEngineID, usmUserEngineIDLen,
                      argv[arg]);
            snmp_pdu_add_variable(pdu, usmUserCloneFrom, name_length,
                                  ASN_OBJECT_ID,
                                  (u_char *) usmUserSecurityName,
                                  sizeof(oid) * name_length2);
        } else {
            /*
             * create a new (unauthenticated) user from scratch
             * The Net-SNMP agent won't allow such a user to be made active.
             */
            setup_oid(usmUserStatus, &name_length,
                      usmUserEngineID, usmUserEngineIDLen, argv[arg-1]);
            longvar = RS_CREATEANDWAIT;
            snmp_pdu_add_variable(pdu, usmUserStatus, name_length,
                                  ASN_INTEGER, (u_char *) & longvar,
                                  sizeof(longvar));
        }

    } else if (strcmp(argv[arg], CMD_CLONEFROM_NAME) == 0) {
        /*
         * create:  clone a user from another
         *
         * cloneFrom USER FROM
         */
        if (++arg >= argc) {
            fprintf(stderr,
                    "You must specify the user name to operate on\n");
            usage();
            exit(1);
        }

        command = CMD_CLONEFROM;
        setup_oid(usmUserStatus, &name_length,
                  usmUserEngineID, usmUserEngineIDLen, argv[arg]);
        longvar = RS_ACTIVE;
        snmp_pdu_add_variable(pdu, usmUserStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
        name_length = USM_OID_LEN;
        setup_oid(usmUserCloneFrom, &name_length,
                  usmUserEngineID, usmUserEngineIDLen, argv[arg]);

        if (++arg >= argc) {
            fprintf(stderr,
                    "You must specify the user name to clone from\n");
            usage();
            exit(1);
        }

        setup_oid(usmUserSecurityName, &name_length2,
                  usmUserEngineID, usmUserEngineIDLen, argv[arg]);
        snmp_pdu_add_variable(pdu, usmUserCloneFrom, name_length,
                              ASN_OBJECT_ID,
                              (u_char *) usmUserSecurityName,
                              sizeof(oid) * name_length2);

    } else if (strcmp(argv[arg], CMD_DELETE_NAME) == 0) {
        /*
         * delete:  delete a user
         *
         * delete USER
         */
        if (++arg >= argc) {
            fprintf(stderr, "You must specify the user name to delete\n");
            exit(1);
        }

        command = CMD_DELETE;
        setup_oid(usmUserStatus, &name_length,
                  usmUserEngineID, usmUserEngineIDLen, argv[arg]);
        longvar = RS_DESTROY;
        snmp_pdu_add_variable(pdu, usmUserStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
    } else if (strcmp(argv[arg], CMD_ACTIVATE_NAME) == 0) {
        /*
         * activate:  activate a user
         *
         * activate USER
         */
        if (++arg >= argc) {
            fprintf(stderr, "You must specify the user name to activate\n");
            exit(1);
        }

        command = CMD_ACTIVATE;
        setup_oid(usmUserStatus, &name_length,
                  usmUserEngineID, usmUserEngineIDLen, argv[arg]);
        longvar = RS_ACTIVE;
        snmp_pdu_add_variable(pdu, usmUserStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
    } else if (strcmp(argv[arg], CMD_DEACTIVATE_NAME) == 0) {
        /*
         * deactivate:  deactivate a user
         *
         * deactivate USER
         */
        if (++arg >= argc) {
            fprintf(stderr, "You must specify the user name to deactivate\n");
            exit(1);
        }

        command = CMD_DEACTIVATE;
        setup_oid(usmUserStatus, &name_length,
                  usmUserEngineID, usmUserEngineIDLen, argv[arg]);
        longvar = RS_NOTINSERVICE;
        snmp_pdu_add_variable(pdu, usmUserStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
#if defined(HAVE_OPENSSL_DH_H) && defined(HAVE_LIBCRYPTO)
    } else if (strcmp(argv[arg], CMD_CHANGEKEY_NAME) == 0) {
        /*
         * change the key of a user if DH is available
         */

        char *passwd_user;
        netsnmp_pdu *dhpdu, *dhresponse = NULL;
        netsnmp_variable_list *vars, *dhvar;
        
        command = CMD_CHANGEKEY;
        name_length = DH_USM_OID_LEN;
        name_length2 = DH_USM_OID_LEN;

        passwd_user = argv[++arg];

        if (doprivkey == 0 && doauthkey == 0)
            doprivkey = doauthkey = 1;

        /* 
         * Change the user supplied on command line.
         */
        if ((passwd_user != NULL) && (strlen(passwd_user) > 0)) {
            session.securityName = passwd_user;
        } else {
            /*
             * Use own key object if no user was supplied.
             */
            dhauthKeyChange = usmDHUserOwnAuthKeyChange;
            dhprivKeyChange = usmDHUserOwnPrivKeyChange;
        }

        /*
         * do we have a securityName?  If not, copy the default 
         */
        if (session.securityName == NULL) {
            session.securityName = 
	      strdup(netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
					   NETSNMP_DS_LIB_SECNAME));
        }

        /* fetch the needed diffie helman parameters */
        dhpdu = snmp_pdu_create(SNMP_MSG_GET);
        if (!dhpdu) {
            fprintf(stderr, "Failed to create DH request\n");
            exit(1);
        }

        /* get the current DH parameters */
        snmp_add_null_var(dhpdu, usmDHParameters, usmDHParameters_len);
        
        /* maybe the auth key public value */
        if (doauthkey) {
            setup_oid(dhauthKeyChange, &name_length,
                      usmUserEngineID, usmUserEngineIDLen,
                      session.securityName);
            snmp_add_null_var(dhpdu, dhauthKeyChange, name_length);
        }
            
        /* maybe the priv key public value */
        if (doprivkey) {
            setup_oid(dhprivKeyChange, &name_length2,
                      usmUserEngineID, usmUserEngineIDLen,
                      session.securityName);
            snmp_add_null_var(dhpdu, dhprivKeyChange, name_length2);
        }

        /* fetch the values */
        status = snmp_synch_response(ss, dhpdu, &dhresponse);

        if (status != SNMPERR_SUCCESS || dhresponse == NULL ||
            dhresponse->errstat != SNMP_ERR_NOERROR ||
            dhresponse->variables->type != ASN_OCTET_STR) {
            snmp_sess_perror("snmpusm", ss);
            if (dhresponse && dhresponse->variables &&
                dhresponse->variables->type != ASN_OCTET_STR) {
                fprintf(stderr,
                        "Can't get diffie-helman exchange from the agent\n");
                fprintf(stderr,
                        "  (maybe it doesn't support the SNMP-USM-DH-OBJECTS-MIB MIB)\n");
            }
            exitval = 1;
            goto begone;
        }
        
        dhvar = dhresponse->variables;
        vars = dhvar->next_variable;
        /* complete the DH equation & print resulting keys */
        if (doauthkey) {
            if (get_USM_DH_key(vars, dhvar,
                               sc_get_properlength(ss->securityAuthProto,
                                                   ss->securityAuthProtoLen),
                               pdu, "auth",
                               dhauthKeyChange, name_length) != SNMPERR_SUCCESS)
                goto begone;
            vars = vars->next_variable;
        }
        if (doprivkey) {
	    size_t dhprivKeyLen = 0;
#ifndef NETSNMP_DISABLE_DES
	    if (ISTRANSFORM(ss->securityPrivProto, DESPriv)) {
                /* DES uses a 128 bit key, 64 bits of which is a salt */
	        dhprivKeyLen = 16;
	    }
#endif
#ifdef HAVE_AES
	    if (ISTRANSFORM(ss->securityPrivProto, AESPriv)) {
	        dhprivKeyLen = 16;
	    }
#endif
            if (get_USM_DH_key(vars, dhvar,
                               dhprivKeyLen,
                               pdu, "priv",
                               dhprivKeyChange, name_length2)
                != SNMPERR_SUCCESS)
                goto begone;
            vars = vars->next_variable;
        }
        /* snmp_free_pdu(dhresponse); */ /* parts still in use somewhere */
#endif /* HAVE_OPENSSL_DH_H && HAVE_LIBCRYPTO */
    } else {
        fprintf(stderr, "Unknown command\n");
        usage();
        exit(1);
    }

    /*
     * add usmUserPublic if specified (via -Cp)
     */
    if (usmUserPublic_val) {
        name_length = USM_OID_LEN;
	setup_oid(usmUserPublic, &name_length,
		  usmUserEngineID, usmUserEngineIDLen,
		  session.securityName);
	snmp_pdu_add_variable(pdu, usmUserPublic, name_length,
			      ASN_OCTET_STR, usmUserPublic_val, 
			      strlen(usmUserPublic_val));	  
    }

    /*
     * do the request 
     */
    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_SUCCESS) {
        if (response) {
            if (response->errstat == SNMP_ERR_NOERROR) {
                fprintf(stdout, "%s\n", successNotes[command - 1]);
            } else {
                fprintf(stderr, "Error in packet.\nReason: %s\n",
                        snmp_errstring(response->errstat));
                if (response->errindex != 0) {
                    int             count;
                    netsnmp_variable_list *vars;
                    fprintf(stderr, "Failed object: ");
                    for (count = 1, vars = response->variables;
                         vars && count != response->errindex;
                         vars = vars->next_variable, count++)
                        /*EMPTY*/;
                    if (vars)
                        fprint_objid(stderr, vars->name,
                                     vars->name_length);
                    fprintf(stderr, "\n");
                }
                exitval = 2;
            }
        }
    } else if (status == STAT_TIMEOUT) {
        fprintf(stderr, "Timeout: No Response from %s\n",
                session.peername);
        exitval = 1;
    } else {                    /* status == STAT_ERROR */
        snmp_sess_perror("snmpset", ss);
        exitval = 1;
    }

#if defined(HAVE_OPENSSL_DH_H) && defined(HAVE_LIBCRYPTO)
  begone:
#endif /* HAVE_OPENSSL_DH_H && HAVE_LIBCRYPTO */
    if (response)
        snmp_free_pdu(response);
    snmp_close(ss);
    SOCK_CLEANUP;
    return exitval;
}
