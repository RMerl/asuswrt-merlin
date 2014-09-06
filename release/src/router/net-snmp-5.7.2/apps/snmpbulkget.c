/*
 * snmpbulkget.c - send SNMPv2 Bulk requests to a network entity.
 *
 */
/*********************************************************************
	Copyright 1988, 1989, 1991, 1992 by Carnegie Mellon University

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
**********************************************************************/
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
#include <net-snmp/utilities.h>
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
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
#include <ctype.h>
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <net-snmp/net-snmp-includes.h>

oid             objid_mib[] = { 1, 3, 6, 1, 2, 1 };
int             max_repetitions = 10;
int             non_repeaters = 0;
struct nameStruct {
    oid             name[MAX_OID_LEN];
    size_t          name_len;
}              *name, *namep;
int             names;

void
usage(void)
{
    fprintf(stderr, "USAGE: snmpbulkget ");
    snmp_parse_args_usage(stderr);
    fprintf(stderr, " OID [OID]...\n\n");
    snmp_parse_args_descriptions(stderr);
    fprintf(stderr,
            "  -C APPOPTS\t\tSet various application specific behaviours:\n");
    fprintf(stderr, "\t\t\t  n<NUM>:  set non-repeaters to <NUM>\n");
    fprintf(stderr, "\t\t\t  r<NUM>:  set max-repeaters to <NUM>\n");
}

static
    void
optProc(int argc, char *const *argv, int opt)
{
    char           *endptr = NULL;

    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
            case 'n':
            case 'r':
                if (*(optarg - 1) == 'r') {
                    max_repetitions = strtol(optarg, &endptr, 0);
                } else {
                    non_repeaters = strtol(optarg, &endptr, 0);
                }

                if (endptr == optarg) {
                    /*
                     * No number given -- error.  
                     */
                    usage();
                    exit(1);
                } else {
                    optarg = endptr;
                    if (isspace((unsigned char)(*optarg))) {
                        return;
                    }
                }
                break;

            default:
                fprintf(stderr, "Unknown flag passed to -C: %c\n",
                        optarg[-1]);
                exit(1);
            }
        }
    }
}

int
main(int argc, char *argv[])
{
    netsnmp_session session, *ss;
    netsnmp_pdu    *pdu;
    netsnmp_pdu    *response;
    netsnmp_variable_list *vars;
    int             arg;
    int             count;
    int             status;
    int             exitval = 0;

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

    names = argc - arg;
    if (names < non_repeaters) {
        fprintf(stderr, "snmpbulkget: need more objects than <nonrep>\n");
        exit(1);
    }

    namep = name = (struct nameStruct *) calloc(names, sizeof(*name));
    while (arg < argc) {
        namep->name_len = MAX_OID_LEN;
        if (snmp_parse_oid(argv[arg], namep->name, &namep->name_len) ==
            NULL) {
            snmp_perror(argv[arg]);
            exit(1);
        }
        arg++;
        namep++;
    }

    SOCK_STARTUP;

    /*
     * open an SNMP session 
     */
    ss = snmp_open(&session);
    if (ss == NULL) {
        /*
         * diagnose snmp_open errors with the input netsnmp_session pointer 
         */
        snmp_sess_perror("snmpbulkget", &session);
        SOCK_CLEANUP;
        exit(1);
    }

    /*
     * create PDU for GETBULK request and add object name to request 
     */
    pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
    pdu->non_repeaters = non_repeaters;
    pdu->max_repetitions = max_repetitions;     /* fill the packet */
    for (arg = 0; arg < names; arg++)
        snmp_add_null_var(pdu, name[arg].name, name[arg].name_len);

    /*
     * do the request 
     */
    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_SUCCESS) {
        if (response->errstat == SNMP_ERR_NOERROR) {
            /*
             * check resulting variables 
             */
            for (vars = response->variables; vars;
                 vars = vars->next_variable)
                print_variable(vars->name, vars->name_length, vars);
        } else {
            /*
             * error in response, print it 
             */
            if (response->errstat == SNMP_ERR_NOSUCHNAME) {
                printf("End of MIB.\n");
            } else {
                fprintf(stderr, "Error in packet.\nReason: %s\n",
                        snmp_errstring(response->errstat));
                if (response->errindex != 0) {
                    fprintf(stderr, "Failed object: ");
                    for (count = 1, vars = response->variables;
                         vars && (count != response->errindex);
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
        snmp_sess_perror("snmpbulkget", ss);
        exitval = 1;
    }

    if (response)
        snmp_free_pdu(response);

    snmp_close(ss);
    SOCK_CLEANUP;
    return exitval;
}
