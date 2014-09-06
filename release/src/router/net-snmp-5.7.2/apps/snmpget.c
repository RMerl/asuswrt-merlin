/*
 * snmpget.c - send snmp GET requests to a network entity.
 *
 */
/***********************************************************************
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

#include <net-snmp/utilities.h>

#include <net-snmp/net-snmp-includes.h>

#define NETSNMP_DS_APP_DONT_FIX_PDUS 0

static void
optProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
            case 'f':
                netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID, 
					  NETSNMP_DS_APP_DONT_FIX_PDUS);
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

void
usage(void)
{
    fprintf(stderr, "USAGE: snmpget ");
    snmp_parse_args_usage(stderr);
    fprintf(stderr, " OID [OID]...\n\n");
    snmp_parse_args_descriptions(stderr);
    fprintf(stderr,
            "  -C APPOPTS\t\tSet various application specific behaviours:\n");
    fprintf(stderr,
            "\t\t\t  f:  do not fix errors and retry the request\n");
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
    int             current_name = 0;
    char           *names[SNMP_MAX_CMDLINE_OIDS];
    oid             name[MAX_OID_LEN];
    size_t          name_length;
    int             status;
    int             failures = 0;
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

    if (arg >= argc) {
        fprintf(stderr, "Missing object name\n");
        usage();
        exit(1);
    }
    if ((argc - arg) > SNMP_MAX_CMDLINE_OIDS) {
        fprintf(stderr, "Too many object identifiers specified. ");
        fprintf(stderr, "Only %d allowed in one request.\n", SNMP_MAX_CMDLINE_OIDS);
        usage();
        exit(1);
    }

    /*
     * get the object names 
     */
    for (; arg < argc; arg++)
        names[current_name++] = argv[arg];

    SOCK_STARTUP;


    /*
     * Open an SNMP session.
     */
    ss = snmp_open(&session);
    if (ss == NULL) {
        /*
         * diagnose snmp_open errors with the input netsnmp_session pointer 
         */
        snmp_sess_perror("snmpget", &session);
        SOCK_CLEANUP;
        exit(1);
    }


    /*
     * Create PDU for GET request and add object names to request.
     */
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    for (count = 0; count < current_name; count++) {
        name_length = MAX_OID_LEN;
        if (!snmp_parse_oid(names[count], name, &name_length)) {
            snmp_perror(names[count]);
            failures++;
        } else
            snmp_add_null_var(pdu, name, name_length);
    }
    if (failures) {
        snmp_close(ss);
        SOCK_CLEANUP;
        exit(1);
    }


    /*
     * Perform the request.
     *
     * If the Get Request fails, note the OID that caused the error,
     * "fix" the PDU (removing the error-prone OID) and retry.
     */
  retry:
    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_SUCCESS) {
        if (response->errstat == SNMP_ERR_NOERROR) {
            for (vars = response->variables; vars;
                 vars = vars->next_variable)
                print_variable(vars->name, vars->name_length, vars);

        } else {
            fprintf(stderr, "Error in packet\nReason: %s\n",
                    snmp_errstring(response->errstat));

            if (response->errindex != 0) {
                fprintf(stderr, "Failed object: ");
                for (count = 1, vars = response->variables;
                     vars && count != response->errindex;
                     vars = vars->next_variable, count++)
                    /*EMPTY*/;
                if (vars) {
                    fprint_objid(stderr, vars->name, vars->name_length);
		}
                fprintf(stderr, "\n");
            }
            exitval = 2;

            /*
             * retry if the errored variable was successfully removed 
             */
            if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
					NETSNMP_DS_APP_DONT_FIX_PDUS)) {
                pdu = snmp_fix_pdu(response, SNMP_MSG_GET);
                snmp_free_pdu(response);
                response = NULL;
                if (pdu != NULL) {
                    goto retry;
		}
            }
        }                       /* endif -- SNMP_ERR_NOERROR */

    } else if (status == STAT_TIMEOUT) {
        fprintf(stderr, "Timeout: No Response from %s.\n",
                session.peername);
        exitval = 1;

    } else {                    /* status == STAT_ERROR */
        snmp_sess_perror("snmpget", ss);
        exitval = 1;

    }                           /* endif -- STAT_SUCCESS */


    if (response)
        snmp_free_pdu(response);
    snmp_close(ss);
    SOCK_CLEANUP;
    return exitval;

}                               /* end main() */
