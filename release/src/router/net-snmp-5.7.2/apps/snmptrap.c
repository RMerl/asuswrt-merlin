/*
 * snmptrap.c - send snmp traps to a network entity.
 *
 */
/******************************************************************
	Copyright 1989, 1991, 1992 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
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
# include <netinet/in.h>
#endif
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
#include <stdio.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <net-snmp/net-snmp-includes.h>

oid             objid_enterprise[] = { 1, 3, 6, 1, 4, 1, 3, 1, 1 };
oid             objid_sysdescr[] = { 1, 3, 6, 1, 2, 1, 1, 1, 0 };
oid             objid_sysuptime[] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
oid             objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
int             inform = 0;

void
usage(void)
{
    fprintf(stderr, "USAGE: %s ", inform ? "snmpinform" : "snmptrap");
    snmp_parse_args_usage(stderr);
    fprintf(stderr, " TRAP-PARAMETERS\n\n");
    snmp_parse_args_descriptions(stderr);
    fprintf(stderr,
            "  -C APPOPTS\t\tSet various application specific behaviour:\n");
    fprintf(stderr, "\t\t\t  i:  send an INFORM instead of a TRAP\n");
    fprintf(stderr,
            "\n  -v 1 TRAP-PARAMETERS:\n\t enterprise-oid agent trap-type specific-type uptime [OID TYPE VALUE]...\n");
    fprintf(stderr, "  or\n");
    fprintf(stderr,
            "  -v 2 TRAP-PARAMETERS:\n\t uptime trapoid [OID TYPE VALUE] ...\n");
}

int
snmp_input(int operation,
           netsnmp_session * session,
           int reqid, netsnmp_pdu *pdu, void *magic)
{
    return 1;
}

static void
optProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
            case 'i':
                inform = 1;
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
    netsnmp_pdu    *pdu, *response;
    oid             name[MAX_OID_LEN];
    size_t          name_length;
    int             arg;
    int             status;
    char           *trap = NULL;
    char           *prognam;
    int             exitval = 0;
#ifndef NETSNMP_DISABLE_SNMPV1
    char           *specific = NULL, *description = NULL, *agent = NULL;
    in_addr_t      *pdu_in_addr_t;
#endif

    prognam = strrchr(argv[0], '/');
    if (prognam)
        prognam++;
    else
        prognam = argv[0];

    putenv(strdup("POSIXLY_CORRECT=1"));

    if (strcmp(prognam, "snmpinform") == 0)
        inform = 1;
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

    session.callback = snmp_input;
    session.callback_magic = NULL;

    /*
     * setup the local engineID which may be for either or both of the
     * contextEngineID and/or the securityEngineID.
     */
    setup_engineID(NULL, NULL);

    /* if we don't have a contextEngineID set via command line
       arguments, use our internal engineID as the context. */
    if (session.contextEngineIDLen == 0 ||
        session.contextEngineID == NULL) {
        session.contextEngineID =
            snmpv3_generate_engineID(&session.contextEngineIDLen);
    }

    if (session.version == SNMP_VERSION_3 && !inform) {
        /*
         * for traps, we use ourselves as the authoritative engine
         * which is really stupid since command line apps don't have a
         * notion of a persistent engine.  Hence, our boots and time
         * values are probably always really wacked with respect to what
         * a manager would like to see.
         * 
         * The following should be enough to:
         * 
         * 1) prevent the library from doing discovery for engineid & time.
         * 2) use our engineid instead of the remote engineid for
         * authoritative & privacy related operations.
         * 3) The remote engine must be configured with users for our engineID.
         * 
         * -- Wes 
         */

        /*
         * pick our own engineID 
         */
        if (session.securityEngineIDLen == 0 ||
            session.securityEngineID == NULL) {
            session.securityEngineID =
                snmpv3_generate_engineID(&session.securityEngineIDLen);
        }

        /*
         * set boots and time, which will cause problems if this
         * machine ever reboots and a remote trap receiver has cached our
         * boots and time...  I'll cause a not-in-time-window report to
         * be sent back to this machine. 
         */
        if (session.engineBoots == 0)
            session.engineBoots = 1;
        if (session.engineTime == 0)    /* not really correct, */
            session.engineTime = get_uptime();  /* but it'll work. Sort of. */
    }

    ss = snmp_add(&session,
                  netsnmp_transport_open_client("snmptrap", session.peername),
                  NULL, NULL);
    if (ss == NULL) {
        /*
         * diagnose netsnmp_transport_open_client and snmp_add errors with
         * the input netsnmp_session pointer
         */
        snmp_sess_perror("snmptrap", &session);
        SOCK_CLEANUP;
        exit(1);
    }

#ifndef NETSNMP_DISABLE_SNMPV1
    if (session.version == SNMP_VERSION_1) {
        if (inform) {
            fprintf(stderr, "Cannot send INFORM as SNMPv1 PDU\n");
            SOCK_CLEANUP;
            exit(1);
        }
        pdu = snmp_pdu_create(SNMP_MSG_TRAP);
        if ( !pdu ) {
            fprintf(stderr, "Failed to create trap PDU\n");
            SOCK_CLEANUP;
            exit(1);
        }
        pdu_in_addr_t = (in_addr_t *) pdu->agent_addr;
        if (arg == argc) {
            fprintf(stderr, "No enterprise oid\n");
            usage();
            SOCK_CLEANUP;
            exit(1);
        }
        if (argv[arg][0] == 0) {
            pdu->enterprise = (oid *) malloc(sizeof(objid_enterprise));
            memcpy(pdu->enterprise, objid_enterprise,
                   sizeof(objid_enterprise));
            pdu->enterprise_length =
                sizeof(objid_enterprise) / sizeof(oid);
        } else {
            name_length = MAX_OID_LEN;
            if (!snmp_parse_oid(argv[arg], name, &name_length)) {
                snmp_perror(argv[arg]);
                usage();
                SOCK_CLEANUP;
                exit(1);
            }
            pdu->enterprise = (oid *) malloc(name_length * sizeof(oid));
            memcpy(pdu->enterprise, name, name_length * sizeof(oid));
            pdu->enterprise_length = name_length;
        }
        if (++arg >= argc) {
            fprintf(stderr, "Missing agent parameter\n");
            usage();
            SOCK_CLEANUP;
            exit(1);
        }
        agent = argv[arg];
        if (agent != NULL && strlen(agent) != 0) {
            int ret = netsnmp_gethostbyname_v4(agent, pdu_in_addr_t);
            if (ret < 0) {
                fprintf(stderr, "unknown host: %s\n", agent);
                exit(1);
            }
        } else {
            *pdu_in_addr_t = get_myaddr();
        }
        if (++arg == argc) {
            fprintf(stderr, "Missing generic-trap parameter\n");
            usage();
            SOCK_CLEANUP;
            exit(1);
        }
        trap = argv[arg];
        pdu->trap_type = atoi(trap);
        if (++arg == argc) {
            fprintf(stderr, "Missing specific-trap parameter\n");
            usage();
            SOCK_CLEANUP;
            exit(1);
        }
        specific = argv[arg];
        pdu->specific_type = atoi(specific);
        if (++arg == argc) {
            fprintf(stderr, "Missing uptime parameter\n");
            usage();
            SOCK_CLEANUP;
            exit(1);
        }
        description = argv[arg];
        if (description == NULL || *description == 0)
            pdu->time = get_uptime();
        else
            pdu->time = atol(description);
    } else
#endif
    {
        long            sysuptime;
        char            csysuptime[20];

        pdu = snmp_pdu_create(inform ? SNMP_MSG_INFORM : SNMP_MSG_TRAP2);
        if ( !pdu ) {
            fprintf(stderr, "Failed to create notification PDU\n");
            SOCK_CLEANUP;
            exit(1);
        }
        if (arg == argc) {
            fprintf(stderr, "Missing up-time parameter\n");
            usage();
            SOCK_CLEANUP;
            exit(1);
        }
        trap = argv[arg];
        if (*trap == 0) {
            sysuptime = get_uptime();
            sprintf(csysuptime, "%ld", sysuptime);
            trap = csysuptime;
        }
        snmp_add_var(pdu, objid_sysuptime,
                     sizeof(objid_sysuptime) / sizeof(oid), 't', trap);
        if (++arg == argc) {
            fprintf(stderr, "Missing trap-oid parameter\n");
            usage();
            SOCK_CLEANUP;
            exit(1);
        }
        if (snmp_add_var
            (pdu, objid_snmptrap, sizeof(objid_snmptrap) / sizeof(oid),
             'o', argv[arg]) != 0) {
            snmp_perror(argv[arg]);
            SOCK_CLEANUP;
            exit(1);
        }
    }
    arg++;

    while (arg < argc) {
        arg += 3;
        if (arg > argc) {
            fprintf(stderr, "%s: Missing type/value for variable\n",
                    argv[arg - 3]);
            SOCK_CLEANUP;
            exit(1);
        }
        name_length = MAX_OID_LEN;
        if (!snmp_parse_oid(argv[arg - 3], name, &name_length)) {
            snmp_perror(argv[arg - 3]);
            SOCK_CLEANUP;
            exit(1);
        }
        if (snmp_add_var
            (pdu, name, name_length, argv[arg - 2][0],
             argv[arg - 1]) != 0) {
            snmp_perror(argv[arg - 3]);
            SOCK_CLEANUP;
            exit(1);
        }
    }

    if (inform)
        status = snmp_synch_response(ss, pdu, &response);
    else
        status = snmp_send(ss, pdu) == 0;
    if (status) {
        snmp_sess_perror(inform ? "snmpinform" : "snmptrap", ss);
        if (!inform)
            snmp_free_pdu(pdu);
        exitval = 1;
    } else if (inform)
        snmp_free_pdu(response);

    snmp_close(ss);
    snmp_shutdown("snmpapp");
    SOCK_CLEANUP;
    return exitval;
}
