/*
 * snmpwalk.c - send snmp GETNEXT requests to a network entity, walking a
 * subtree.
 *
 */
/**********************************************************************
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
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <net-snmp/net-snmp-includes.h>

#define NETSNMP_DS_WALK_INCLUDE_REQUESTED	        1
#define NETSNMP_DS_WALK_PRINT_STATISTICS	        2
#define NETSNMP_DS_WALK_DONT_CHECK_LEXICOGRAPHIC	3
#define NETSNMP_DS_WALK_TIME_RESULTS     	        4
#define NETSNMP_DS_WALK_DONT_GET_REQUESTED	        5
#define NETSNMP_DS_WALK_TIME_RESULTS_SINGLE	        6

oid             objid_mib[] = { 1, 3, 6, 1, 2, 1 };
int             numprinted = 0;

char           *end_name = NULL;

void
usage(void)
{
    fprintf(stderr, "USAGE: snmpwalk ");
    snmp_parse_args_usage(stderr);
    fprintf(stderr, " [OID]\n\n");
    snmp_parse_args_descriptions(stderr);
    fprintf(stderr,
            "  -C APPOPTS\t\tSet various application specific behaviours:\n");
    fprintf(stderr, "\t\t\t  p:  print the number of variables found\n");
    fprintf(stderr, "\t\t\t  i:  include given OID in the search range\n");
    fprintf(stderr, "\t\t\t  I:  don't include the given OID, even if no results are returned\n");
    fprintf(stderr,
            "\t\t\t  c:  do not check returned OIDs are increasing\n");
    fprintf(stderr,
            "\t\t\t  t:  Display wall-clock time to complete the walk\n");
    fprintf(stderr,
            "\t\t\t  T:  Display wall-clock time to complete each request\n");
    fprintf(stderr, "\t\t\t  E {OID}:  End the walk at the specified OID\n");
}

void
snmp_get_and_print(netsnmp_session * ss, oid * theoid, size_t theoid_len)
{
    netsnmp_pdu    *pdu, *response;
    netsnmp_variable_list *vars;
    int             status;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, theoid, theoid_len);

    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
        for (vars = response->variables; vars; vars = vars->next_variable) {
            numprinted++;
            print_variable(vars->name, vars->name_length, vars);
        }
    }
    if (response) {
        snmp_free_pdu(response);
    }
}

static void
optProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
            case 'i':
                netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID,
					  NETSNMP_DS_WALK_INCLUDE_REQUESTED);
                break;

            case 'I':
                netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID,
					  NETSNMP_DS_WALK_DONT_GET_REQUESTED);
                break;

            case 'p':
                netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID,
					  NETSNMP_DS_WALK_PRINT_STATISTICS);
                break;

            case 'c':
                netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID,
				    NETSNMP_DS_WALK_DONT_CHECK_LEXICOGRAPHIC);
                break;

            case 't':
                netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID,
                                          NETSNMP_DS_WALK_TIME_RESULTS);
                break;

            case 'E':
                end_name = argv[optind++];
                break;

            case 'T':
                netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID,
                                          NETSNMP_DS_WALK_TIME_RESULTS_SINGLE);
                break;
                
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
    netsnmp_pdu    *pdu, *response;
    netsnmp_variable_list *vars;
    int             arg;
    oid             name[MAX_OID_LEN];
    size_t          name_length;
    oid             root[MAX_OID_LEN];
    size_t          rootlen;
    oid             end_oid[MAX_OID_LEN];
    size_t          end_len = 0;
    int             count;
    int             running;
    int             status = STAT_ERROR;
    int             check;
    int             exitval = 0;
    struct timeval  tv1, tv2, tv_a, tv_b;

    netsnmp_ds_register_config(ASN_BOOLEAN, "snmpwalk", "includeRequested",
			       NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_WALK_INCLUDE_REQUESTED);

    netsnmp_ds_register_config(ASN_BOOLEAN, "snmpwalk", "excludeRequested",
			       NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_WALK_DONT_GET_REQUESTED);

    netsnmp_ds_register_config(ASN_BOOLEAN, "snmpwalk", "printStatistics",
			       NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_WALK_PRINT_STATISTICS);

    netsnmp_ds_register_config(ASN_BOOLEAN, "snmpwalk", "dontCheckOrdering",
			       NETSNMP_DS_APPLICATION_ID,
			       NETSNMP_DS_WALK_DONT_CHECK_LEXICOGRAPHIC);

    netsnmp_ds_register_config(ASN_BOOLEAN, "snmpwalk", "timeResults",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_WALK_TIME_RESULTS);

    netsnmp_ds_register_config(ASN_BOOLEAN, "snmpwalk", "timeResultsSingle",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_WALK_TIME_RESULTS_SINGLE);

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

    /*
     * get the initial object and subtree 
     */
    if (arg < argc) {
        /*
         * specified on the command line 
         */
        rootlen = MAX_OID_LEN;
        if (snmp_parse_oid(argv[arg], root, &rootlen) == NULL) {
            snmp_perror(argv[arg]);
            exit(1);
        }
    } else {
        /*
         * use default value 
         */
        memmove(root, objid_mib, sizeof(objid_mib));
        rootlen = sizeof(objid_mib) / sizeof(oid);
    }

    /*
     * If we've been given an explicit end point,
     *  then convert this to an OID, otherwise
     *  move to the next sibling of the start.
     */
    if ( end_name ) {
        end_len = MAX_OID_LEN;
        if (snmp_parse_oid(end_name, end_oid, &end_len) == NULL) {
            snmp_perror(end_name);
            exit(1);
        }
    } else {
        memmove(end_oid, root, rootlen*sizeof(oid));
        end_len = rootlen;
        end_oid[end_len-1]++;
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
        snmp_sess_perror("snmpwalk", &session);
        SOCK_CLEANUP;
        exit(1);
    }

    /*
     * get first object to start walk 
     */
    memmove(name, root, rootlen * sizeof(oid));
    name_length = rootlen;

    running = 1;

    check =
        !netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                        NETSNMP_DS_WALK_DONT_CHECK_LEXICOGRAPHIC);
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_WALK_INCLUDE_REQUESTED)) {
        snmp_get_and_print(ss, root, rootlen);
    }

    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_WALK_TIME_RESULTS))
        netsnmp_get_monotonic_clock(&tv1);
    while (running) {
        /*
         * create PDU for GETNEXT request and add object name to request 
         */
        pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
        snmp_add_null_var(pdu, name, name_length);

        /*
         * do the request 
         */
        if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_WALK_TIME_RESULTS_SINGLE))
            netsnmp_get_monotonic_clock(&tv_a);
        status = snmp_synch_response(ss, pdu, &response);
        if (status == STAT_SUCCESS) {
            if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_WALK_TIME_RESULTS_SINGLE))
                netsnmp_get_monotonic_clock(&tv_b);
            if (response->errstat == SNMP_ERR_NOERROR) {
                /*
                 * check resulting variables 
                 */
                for (vars = response->variables; vars;
                     vars = vars->next_variable) {
                    if (snmp_oid_compare(end_oid, end_len,
                                         vars->name, vars->name_length) <= 0) {
                        /*
                         * not part of this subtree 
                         */
                        running = 0;
                        continue;
                    }
                    numprinted++;
                    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_WALK_TIME_RESULTS_SINGLE))
                        fprintf(stdout, "%f s: ",  
                                (double) (tv_b.tv_usec - tv_a.tv_usec)/1000000 +
                                (double) (tv_b.tv_sec - tv_a.tv_sec));
                    print_variable(vars->name, vars->name_length, vars);
                    if ((vars->type != SNMP_ENDOFMIBVIEW) &&
                        (vars->type != SNMP_NOSUCHOBJECT) &&
                        (vars->type != SNMP_NOSUCHINSTANCE)) {
                        /*
                         * not an exception value 
                         */
                        if (check
                            && snmp_oid_compare(name, name_length,
                                                vars->name,
                                                vars->name_length) >= 0) {
                            fprintf(stderr, "Error: OID not increasing: ");
                            fprint_objid(stderr, name, name_length);
                            fprintf(stderr, " >= ");
                            fprint_objid(stderr, vars->name,
                                         vars->name_length);
                            fprintf(stderr, "\n");
                            running = 0;
                            exitval = 1;
                        }
                        memmove((char *) name, (char *) vars->name,
                                vars->name_length * sizeof(oid));
                        name_length = vars->name_length;
                    } else
                        /*
                         * an exception value, so stop 
                         */
                        running = 0;
                }
            } else {
                /*
                 * error in response, print it 
                 */
                running = 0;
                if (response->errstat == SNMP_ERR_NOSUCHNAME) {
                    printf("End of MIB\n");
                } else {
                    fprintf(stderr, "Error in packet.\nReason: %s\n",
                            snmp_errstring(response->errstat));
                    if (response->errindex != 0) {
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
            running = 0;
            exitval = 1;
        } else {                /* status == STAT_ERROR */
            snmp_sess_perror("snmpwalk", ss);
            running = 0;
            exitval = 1;
        }
        if (response)
            snmp_free_pdu(response);
    }
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_WALK_TIME_RESULTS))
        netsnmp_get_monotonic_clock(&tv2);

    if (numprinted == 0 && status == STAT_SUCCESS) {
        /*
         * no printed successful results, which may mean we were
         * pointed at an only existing instance.  Attempt a GET, just
         * for get measure. 
         */
        if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_WALK_DONT_GET_REQUESTED)) {
            snmp_get_and_print(ss, root, rootlen);
        }
    }
    snmp_close(ss);

    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_WALK_PRINT_STATISTICS)) {
        printf("Variables found: %d\n", numprinted);
    }
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_WALK_TIME_RESULTS)) {
        fprintf (stderr, "Total traversal time = %f seconds\n",
                 (double) (tv2.tv_usec - tv1.tv_usec)/1000000 +
                 (double) (tv2.tv_sec - tv1.tv_sec));
    }

    SOCK_CLEANUP;
    return exitval;
}
