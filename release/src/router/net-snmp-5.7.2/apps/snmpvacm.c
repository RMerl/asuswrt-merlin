/*
 * snmpvacm.c - send snmp SET requests to a network entity to change the
 *             vacm database
 *
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

#include <net-snmp/net-snmp-includes.h>

int             main(int, char **);

#define CMD_CREATESEC2GROUP_NAME    "createSec2Group"
#define CMD_CREATESEC2GROUP         1
#define CMD_DELETESEC2GROUP_NAME    "deleteSec2Group"
#define CMD_DELETESEC2GROUP         2
#define CMD_CREATEACCESS_NAME    	"createAccess"
#define CMD_CREATEACCESS         	3
#define CMD_DELETEACCESS_NAME 		"deleteAccess"
#define CMD_DELETEACCESS      		4
#define CMD_CREATEVIEW_NAME 		"createView"
#define CMD_CREATEVIEW      		5
#define CMD_DELETEVIEW_NAME 		"deleteView"
#define CMD_DELETEVIEW      		6
#define CMD_CREATEAUTH_NAME     	"createAuth"
#define CMD_CREATEAUTH          	7
#define CMD_DELETEAUTH_NAME 		"deleteAuth"
#define CMD_DELETEAUTH      		8

#define CMD_NUM    8

static const char *successNotes[CMD_NUM] = {
    "Sec2group successfully created.",
    "Sec2group successfully deleted.",
    "Access successfully created.",
    "Access successfully deleted.",
    "View successfully created.",
    "View successfully deleted.",
    "AuthAccess successfully created.",
    "AuthAccess successfully deleted."
};

#define                   SEC2GROUP_OID_LEN	11
#define                   ACCESS_OID_LEN   	11
#define                   VIEW_OID_LEN    	12
#define                   AUTH_OID_LEN   	12

static oid      vacmGroupName[MAX_OID_LEN] =
    { 1, 3, 6, 1, 6, 3, 16, 1, 2, 1, 3 },
    vacmSec2GroupStatus[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 2, 1, 5}, vacmAccessContextMatch[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 4, 1, 4}, vacmAccessReadViewName[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 4, 1, 5}, vacmAccessWriteViewName[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 4, 1, 6}, vacmAccessNotifyViewName[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 4, 1, 7}, vacmAccessStatus[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 4, 1, 9}, vacmViewTreeFamilyMask[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 5, 2, 1, 3}, vacmViewTreeFamilyType[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 5, 2, 1, 4},
    vacmViewTreeFamilyStatus[MAX_OID_LEN] = {
1, 3, 6, 1, 6, 3, 16, 1, 5, 2, 1, 6}

;

#define NSVACMACCESSTABLE    1, 3, 6, 1, 4, 1, 8072, 1, 9, 1
static oid nsVacmContextPfx[MAX_OID_LEN]  = { NSVACMACCESSTABLE, 1, 2 };
static oid nsVacmViewName[MAX_OID_LEN]    = { NSVACMACCESSTABLE, 1, 3 };
static oid nsVacmRowStatus[MAX_OID_LEN]   = { NSVACMACCESSTABLE, 1, 5 };

int             viewTreeFamilyType = 1;

void
usage(void)
{
    fprintf(stderr, "Usage: snmpvacm ");
    snmp_parse_args_usage(stderr);
    fprintf(stderr, " COMMAND\n\n");
    snmp_parse_args_descriptions(stderr);
    fprintf(stderr, "\nsnmpvacm commands:\n");
    fprintf(stderr, "        createAccess     GROUPNAME [CONTEXTPREFIX] SECURITYMODEL SECURITYLEVEL CONTEXTMATCH READVIEWNAME WRITEVIEWNAME NOTIFYVIEWNAME\n");
    fprintf(stderr, "        deleteAccess     GROUPNAME [CONTEXTPREFIX] SECURITYMODEL SECURITYLEVEL\n");
    fprintf(stderr, "        createSec2Group  MODEL SECURITYNAME  GROUPNAME\n");
    fprintf(stderr, "        deleteSec2Group  MODEL SECURITYNAME\n");
    fprintf(stderr, "  [-Ce] createView       NAME SUBTREE [MASK]\n");
    fprintf(stderr, "        deleteView       NAME SUBTREE\n");
    fprintf(stderr, "        createAuth       GROUPNAME [CONTEXTPREFIX] SECURITYMODEL SECURITYLEVEL AUTHTYPE CONTEXTMATCH VIEWNAME\n");
    fprintf(stderr, "        deleteAuth       GROUPNAME [CONTEXTPREFIX] SECURITYMODEL SECURITYLEVEL AUTHTYPE\n");
}


void
auth_oid(oid * it, size_t * len, const char *groupName,
           const char *prefix, int model, int level, const char *authtype)
{
    int             i;
    int             itIndex = AUTH_OID_LEN;

    it[itIndex++] = strlen(groupName);
    for (i = 0; i < (int) strlen(groupName); i++)
        it[itIndex++] = groupName[i];

    if (prefix) {
        *len += strlen(prefix);
        it[itIndex++] = strlen(prefix);
        for (i = 0; i < (int) strlen(prefix); i++)
            it[itIndex++] = prefix[i];
    } else
        it[itIndex++] = 0;

    it[itIndex++] = model;
    it[itIndex++] = level;

    it[itIndex++] = strlen(authtype);
    for (i = 0; i < (int) strlen(authtype); i++)
        it[itIndex++] = authtype[i];

    *len = itIndex;
}

void
access_oid(oid * it, size_t * len, const char *groupName,
           const char *prefix, int model, int level)
{
    int             i;

    int             itIndex = ACCESS_OID_LEN;

    *len = itIndex + 4 + +strlen(groupName);

    it[itIndex++] = strlen(groupName);
    for (i = 0; i < (int) strlen(groupName); i++)
        it[itIndex++] = groupName[i];

    if (prefix) {
        *len += strlen(prefix);
        it[itIndex++] = strlen(prefix);
        for (i = 0; i < (int) strlen(prefix); i++)
            it[itIndex++] = prefix[i];
    } else
        it[itIndex++] = 0;

    it[itIndex++] = model;
    it[itIndex++] = level;
}


void
sec2group_oid(oid * it, size_t * len, int model, const char *name)
{
    int             i;

    int             itIndex = SEC2GROUP_OID_LEN;

    *len = itIndex + 2 + strlen(name);

    it[itIndex++] = model;

    it[itIndex++] = strlen(name);
    for (i = 0; i < (int) strlen(name); i++)
        it[itIndex++] = name[i];
}

void
view_oid(oid * it, size_t * len, const char *viewName, char *viewSubtree)
{
    int             i;
    oid             c_oid[SPRINT_MAX_LEN];
    size_t          c_oid_length = SPRINT_MAX_LEN;

    int             itIndex = VIEW_OID_LEN;

    if (!snmp_parse_oid(viewSubtree, c_oid, &c_oid_length)) {
        printf("Error parsing subtree (%s)\n", viewSubtree);
        exit(1);
    }

    *len = itIndex + 2 + strlen(viewName) + c_oid_length;

    it[itIndex++] = strlen(viewName);
    for (i = 0; i < (int) strlen(viewName); i++)
        it[itIndex++] = viewName[i];

    
    it[itIndex++] = c_oid_length;
    for (i = 0; i < (int) c_oid_length; i++)
        it[itIndex++] = c_oid[i];

    /*
     * sprint_objid(c_oid, it, *len); 
     */
}

static void
optProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
            case 'e':
                viewTreeFamilyType = 2;
                break;

            default:
                fprintf(stderr,
                        "Unknown flag passed to -C: %c\n", optarg[-1]);
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
#ifdef notused
    netsnmp_variable_list *vars;
#endif

    int             arg;
#ifdef notused
    int             count;
    int             current_name = 0;
    int             current_type = 0;
    int             current_value = 0;
    char           *names[128];
    char            types[128];
    char           *values[128];
    oid             name[MAX_OID_LEN];
#endif
    size_t          name_length;
    int             status;
    int             exitval = 0;
    int             command = 0;
    long            longvar;
    int             secModel, secLevel, contextMatch;
    unsigned int    val, i = 0;
    char           *mask, *groupName, *prefix, *authtype;
    u_char          viewMask[VACMSTRINGLEN];
    char           *st;


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


    SOCK_STARTUP;

    /*
     * open an SNMP session 
     */
    /*
     * Note:  this wil obtain the engineID needed below 
     */
    ss = snmp_open(&session);
    if (ss == NULL) {
        /*
         * diagnose snmp_open errors with the input netsnmp_session pointer 
         */
        snmp_sess_perror("snmpvacm", &session);
        exit(1);
    }

    /*
     * create PDU for SET request and add object names and values to request 
     */
    pdu = snmp_pdu_create(SNMP_MSG_SET);

    if (arg >= argc) {
        fprintf(stderr, "Please specify a operation to perform.\n");
        usage();
        exit(1);
    }

    if (strcmp(argv[arg], CMD_DELETEVIEW_NAME) == 0)
        /*
         * deleteView: delete a view
         *
         * deleteView NAME SUBTREE
         *
         */
    {
        if (++arg + 2 != argc) {
            fprintf(stderr, "You must specify the view to delete\n");
            usage();
            exit(1);
        }

        command = CMD_DELETEVIEW;
        name_length = VIEW_OID_LEN;
        view_oid(vacmViewTreeFamilyStatus, &name_length, argv[arg],
                 argv[arg + 1]);
        longvar = RS_DESTROY;
        snmp_pdu_add_variable(pdu, vacmViewTreeFamilyStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
    } else if (strcmp(argv[arg], CMD_CREATEVIEW_NAME) == 0)
        /*
         * createView: create a view
         *
         * createView NAME SUBTREE MASK
         *
         */
    {
        if (++arg + 2 > argc) {
            fprintf(stderr, "You must specify name, subtree and mask\n");
            usage();
            exit(1);
        }
        command = CMD_CREATEVIEW;
        name_length = VIEW_OID_LEN;
        view_oid(vacmViewTreeFamilyStatus, &name_length, argv[arg],
                 argv[arg + 1]);
        longvar = RS_CREATEANDGO;
        snmp_pdu_add_variable(pdu, vacmViewTreeFamilyStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
        /*
         * Mask
         */
        if (arg + 3 == argc) {
            mask = argv[arg + 2];
            for (mask = strtok_r(mask, ".:", &st); mask; mask = strtok_r(NULL, ".:", &st)) {
                if (i >= sizeof(viewMask)) {
                    printf("MASK too long\n");
                    exit(1);
                }
                if (sscanf(mask, "%x", &val) == 0) {
                    printf("invalid MASK\n");
                    exit(1);
                }
                viewMask[i] = val;
                i++;
            }
	} else {
            for (i=0 ; i < (name_length+7)/8; i++)
                viewMask[i] = (u_char)0xff;
        }
        view_oid(vacmViewTreeFamilyMask, &name_length, argv[arg],
                 argv[arg + 1]);
        snmp_pdu_add_variable(pdu, vacmViewTreeFamilyMask, name_length,
                              ASN_OCTET_STR, viewMask, i);

        view_oid(vacmViewTreeFamilyType, &name_length, argv[arg],
                 argv[arg + 1]);
        snmp_pdu_add_variable(pdu, vacmViewTreeFamilyType, name_length,
                              ASN_INTEGER, (u_char *) & viewTreeFamilyType,
                              sizeof(viewTreeFamilyType));

    } else if (strcmp(argv[arg], CMD_DELETESEC2GROUP_NAME) == 0)
        /*
         * deleteSec2Group: delete security2group
         *
         * deleteSec2Group  MODEL SECURITYNAME
         *
         */
    {
        if (++arg + 2 != argc) {
            fprintf(stderr, "You must specify the sec2group to delete\n");
            usage();
            exit(1);
        }

        command = CMD_DELETESEC2GROUP;
        name_length = SEC2GROUP_OID_LEN;
        if (sscanf(argv[arg], "%d", &secModel) == 0) {
            printf("invalid security model\n");
            usage();
            exit(1);
        }
        sec2group_oid(vacmSec2GroupStatus, &name_length, secModel,
                      argv[arg + 1]);
        longvar = RS_DESTROY;
        snmp_pdu_add_variable(pdu, vacmSec2GroupStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
    } else if (strcmp(argv[arg], CMD_CREATESEC2GROUP_NAME) == 0)
        /*
         * createSec2Group: create a security2group
         *
         * createSec2Group  MODEL SECURITYNAME GROUPNAME
         *
         */
    {
        if (++arg + 3 != argc) {
            fprintf(stderr,
                    "You must specify model, security name and group name\n");
            usage();
            exit(1);
        }

        command = CMD_CREATESEC2GROUP;
        name_length = SEC2GROUP_OID_LEN;
        if (sscanf(argv[arg], "%d", &secModel) == 0) {
            printf("invalid security model\n");
            usage();
            exit(1);
        }
        sec2group_oid(vacmSec2GroupStatus, &name_length, secModel,
                      argv[arg + 1]);
        longvar = RS_CREATEANDGO;
        snmp_pdu_add_variable(pdu, vacmSec2GroupStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
        sec2group_oid(vacmGroupName, &name_length, secModel,
                      argv[arg + 1]);
        snmp_pdu_add_variable(pdu, vacmGroupName, name_length,
                              ASN_OCTET_STR, (u_char *) argv[arg + 2],
                              strlen(argv[arg + 2]));
    } else if (strcmp(argv[arg], CMD_DELETEACCESS_NAME) == 0)
        /*
         * deleteAccess: delete access entry
         *
         * deleteAccess  GROUPNAME [CONTEXTPREFIX] SECURITYMODEL SECURITYLEVEL
         *
         */
    {
        if (++arg + 3 > argc) {
            fprintf(stderr,
                    "You must specify the access entry to delete\n");
            usage();
            exit(1);
        }

        command = CMD_DELETEACCESS;
        name_length = ACCESS_OID_LEN;
        groupName = argv[arg];
        if (arg + 4 == argc)
            prefix = argv[++arg];
        else
            prefix = NULL;

        if (sscanf(argv[arg + 1], "%d", &secModel) == 0) {
            printf("invalid security model\n");
            usage();
            exit(1);
        }
        if (sscanf(argv[arg + 2], "%d", &secLevel) == 0) {
            printf("invalid security level\n");
            usage();
            exit(1);
        }
        access_oid(vacmAccessStatus, &name_length, groupName, prefix,
                   secModel, secLevel);
        longvar = RS_DESTROY;
        snmp_pdu_add_variable(pdu, vacmAccessStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
    } else if (strcmp(argv[arg], CMD_CREATEACCESS_NAME) == 0)
        /*
         * createAccess: create access entry
         *
         * createAccess  GROUPNAME [CONTEXTPREFIX] SECURITYMODEL SECURITYLEVEL CONTEXTMATCH READVIEWNAME WRITEVIEWNAME NOTIFYVIEWNAME
         *
         */
    {
        if (++arg + 7 > argc) {
            fprintf(stderr,
                    "You must specify the access entry to create\n");
            usage();
            exit(1);
        }

        command = CMD_CREATEACCESS;
        name_length = ACCESS_OID_LEN;
        groupName = argv[arg];
        if (arg + 8 == argc)
            prefix = argv[++arg];
        else
            prefix = NULL;

        if (sscanf(argv[arg + 1], "%d", &secModel) == 0) {
            printf("invalid security model\n");
            usage();
            exit(1);
        }
        if (sscanf(argv[arg + 2], "%d", &secLevel) == 0) {
            printf("invalid security level\n");
            usage();
            exit(1);
        }
        access_oid(vacmAccessStatus, &name_length, groupName, prefix,
                   secModel, secLevel);
        longvar = RS_CREATEANDGO;
        snmp_pdu_add_variable(pdu, vacmAccessStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));

        access_oid(vacmAccessContextMatch, &name_length, groupName, prefix,
                   secModel, secLevel);
        if (sscanf(argv[arg + 3], "%d", &contextMatch) == 0) {
            printf("invalid contextMatch\n");
            usage();
            exit(1);
        }
        snmp_pdu_add_variable(pdu, vacmAccessContextMatch, name_length,
                              ASN_INTEGER, (u_char *) & contextMatch,
                              sizeof(contextMatch));

        access_oid(vacmAccessReadViewName, &name_length, groupName, prefix,
                   secModel, secLevel);
        snmp_pdu_add_variable(pdu, vacmAccessReadViewName, name_length,
                              ASN_OCTET_STR, (u_char *) argv[arg + 4],
                              strlen(argv[arg + 4]));

        access_oid(vacmAccessWriteViewName, &name_length, groupName,
                   prefix, secModel, secLevel);
        snmp_pdu_add_variable(pdu, vacmAccessWriteViewName, name_length,
                              ASN_OCTET_STR, (u_char *) argv[arg + 5],
                              strlen(argv[arg + 5]));

        access_oid(vacmAccessNotifyViewName, &name_length, groupName,
                   prefix, secModel, secLevel);
        snmp_pdu_add_variable(pdu, vacmAccessNotifyViewName, name_length,
                              ASN_OCTET_STR, (u_char *) argv[arg + 6],
                              strlen(argv[arg + 6]));
    } else if (strcmp(argv[arg], CMD_DELETEAUTH_NAME) == 0)
        /*
         * deleteAuth: delete authAccess entry
         *
         * deleteAuth  GROUPNAME [CONTEXTPREFIX] SECURITYMODEL SECURITYLEVEL AUTHTYPE
         *
         */
    {
        if (++arg + 4 > argc) {
            fprintf(stderr,
                    "You must specify the authAccess entry to delete\n");
            usage();
            exit(1);
        }

        command = CMD_DELETEAUTH;
        name_length = AUTH_OID_LEN;
        groupName = argv[arg];
        if (arg + 5 == argc)
            prefix = argv[++arg];
        else
            prefix = NULL;

        if (sscanf(argv[arg + 1], "%d", &secModel) == 0) {
            printf("invalid security model\n");
            usage();
            exit(1);
        }
        if (sscanf(argv[arg + 2], "%d", &secLevel) == 0) {
            printf("invalid security level\n");
            usage();
            exit(1);
        }
        authtype = argv[arg+3];
        auth_oid(nsVacmRowStatus, &name_length, groupName, prefix,
                   secModel, secLevel, authtype);
        longvar = RS_DESTROY;
        snmp_pdu_add_variable(pdu, nsVacmRowStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));
    } else if (strcmp(argv[arg], CMD_CREATEAUTH_NAME) == 0)
        /*
         * createAuth: create authAccess entry
         *
         * createAuth  GROUPNAME [CONTEXTPREFIX] SECURITYMODEL SECURITYLEVEL AUTHTYPE CONTEXTMATCH VIEWNAME
         *
         */
    {
        if (++arg + 6 > argc) {
            fprintf(stderr,
                    "You must specify the authAccess entry to create\n");
            usage();
            exit(1);
        }

        command = CMD_CREATEAUTH;
        name_length = AUTH_OID_LEN;
        groupName = argv[arg];
        if (arg + 7 == argc)
            prefix = argv[++arg];
        else
            prefix = NULL;

        if (sscanf(argv[arg + 1], "%d", &secModel) == 0) {
            printf("invalid security model\n");
            usage();
            exit(1);
        }
        if (sscanf(argv[arg + 2], "%d", &secLevel) == 0) {
            printf("invalid security level\n");
            usage();
            exit(1);
        }
        authtype = argv[arg+3];
        auth_oid(nsVacmRowStatus, &name_length, groupName, prefix,
                   secModel, secLevel, authtype);
        longvar = RS_CREATEANDGO;
        snmp_pdu_add_variable(pdu, nsVacmRowStatus, name_length,
                              ASN_INTEGER, (u_char *) & longvar,
                              sizeof(longvar));

        auth_oid(nsVacmContextPfx, &name_length, groupName, prefix,
                   secModel, secLevel, authtype);
        if (sscanf(argv[arg + 4], "%d", &contextMatch) == 0) {
            printf("invalid contextMatch\n");
            usage();
            exit(1);
        }
        snmp_pdu_add_variable(pdu, nsVacmContextPfx, name_length,
                              ASN_INTEGER, (u_char *) & contextMatch,
                              sizeof(contextMatch));

        auth_oid(nsVacmViewName, &name_length, groupName, prefix,
                   secModel, secLevel, authtype);
        snmp_pdu_add_variable(pdu, nsVacmViewName, name_length,
                              ASN_OCTET_STR, (u_char *) argv[arg + 5],
                              strlen(argv[arg + 5]));
    } else {
        printf("Unknown command\n");
        usage();
        exit(1);
    }

    /*
     * do the request 
     */
    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_SUCCESS) {
        if (response) {
            if (response->errstat == SNMP_ERR_NOERROR) {
                fprintf(stderr, "%s\n", successNotes[command - 1]);
            } else {
                fprintf(stderr, "Error in packet.\nReason: %s\n",
                        snmp_errstring(response->errstat));
		if (response->errindex != 0){
		    int count;
		    struct variable_list *vars = response->variables;
		    fprintf(stderr, "Failed object: ");
		    for(count = 1; vars && (count != response->errindex);
			    vars = vars->next_variable, count++)
			;
		    if (vars)
			fprint_objid(stderr, vars->name, vars->name_length);
		    fprintf(stderr, "\n");
		}
                exitval = 2;
            }
        }
    } else if (status == STAT_TIMEOUT) {
        fprintf(stderr, "Timeout: No Response from %s\n",
                session.peername);
        exitval = 1;
    } else {
        snmp_sess_perror("snmpset", ss);
        exitval = 1;
    }

    if (response)
        snmp_free_pdu(response);

    snmp_close(ss);
    SOCK_CLEANUP;
    return exitval;
}
