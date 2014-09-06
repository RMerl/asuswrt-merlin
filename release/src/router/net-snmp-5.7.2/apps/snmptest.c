/*
 * snmptest.c - send snmp requests to a network entity.
 *
 * Usage: snmptest -v 1 [-q] hostname community [objectID]
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

#include <net-snmp/net-snmp-includes.h>

int             command = SNMP_MSG_GET;

int             input_variable(netsnmp_variable_list *);

void
usage(void)
{
    fprintf(stderr, "USAGE: snmptest ");
    snmp_parse_args_usage(stderr);
    fprintf(stderr, "\n\n");
    snmp_parse_args_descriptions(stderr);
}

int
main(int argc, char *argv[])
{
    netsnmp_session session, *ss;
    netsnmp_pdu    *pdu = NULL, *response, *copy = NULL;
    netsnmp_variable_list *vars, *vp;
    netsnmp_transport *transport = NULL;
    int             ret;
    int             status, count;
    char            input[128];
    int             varcount, nonRepeaters = -1, maxRepetitions;

    /*
     * get the common command line arguments 
     */
    switch (snmp_parse_args(argc, argv, &session, NULL, NULL)) {
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
    ss = snmp_open(&session);
    if (ss == NULL) {
        /*
         * diagnose snmp_open errors with the input netsnmp_session pointer 
         */
        snmp_sess_perror("snmptest", &session);
        SOCK_CLEANUP;
        exit(1);
    }

    varcount = 0;
    for(;;) {
        vars = NULL;
        for (ret = 1; ret != 0;) {
            vp = (netsnmp_variable_list *)
                malloc(sizeof(netsnmp_variable_list));
            memset(vp, 0, sizeof(netsnmp_variable_list));

            while ((ret = input_variable(vp)) == -1);
            if (ret == 1) {
                varcount++;
                /*
                 * add it to the list 
                 */
                if (vars == NULL) {
                    /*
                     * if first variable 
                     */
                    pdu = snmp_pdu_create(command);
                    pdu->variables = vp;
                } else {
                    vars->next_variable = vp;
                }
                vars = vp;
            } else {
                /*
                 * free the last (unused) variable 
                 */
                if (vp->name)
                    free((char *) vp->name);
                if (vp->val.string)
                    free((char *) vp->val.string);
                free((char *) vp);

                if (command == SNMP_MSG_GETBULK) {
                    if (nonRepeaters == -1) {
                        nonRepeaters = varcount;
                        ret = -1;       /* so we collect more variables */
                        printf("Now input the repeating variables\n");
                    } else {
                        printf("What repeat count? ");
                        fflush(stdout);
                        if (!fgets(input, sizeof(input), stdin)) {
                            printf("Quitting,  Goodbye\n");
                            SOCK_CLEANUP;
                            exit(0);
                        }
                        maxRepetitions = atoi(input);
                        pdu->non_repeaters = nonRepeaters;
                        pdu->max_repetitions = maxRepetitions;
                    }
                }
            }
            if (varcount == 0 && ret == 0) {
                if (!copy) {
                    printf("No PDU to send.\n");
                    ret = -1;
                } else {
                    pdu = snmp_clone_pdu(copy);
                    printf("Resending last PDU.\n");
                }
            }
        }
        copy = snmp_clone_pdu(pdu);
        if (command == SNMP_MSG_TRAP2) {
            /*
             * No response needed 
             */
            if (!snmp_send(ss, pdu)) {
                snmp_free_pdu(pdu);
                snmp_sess_perror("snmptest", ss);
            }
        } else {
            status = snmp_synch_response(ss, pdu, &response);
            if (status == STAT_SUCCESS) {
                if (command == SNMP_MSG_INFORM &&
                    response->errstat == SNMP_ERR_NOERROR) {
                    printf("Inform Acknowledged\n");
                } else {
                    switch (response->command) {
                    case SNMP_MSG_GET:
                        printf("Received Get Request ");
                        break;
                    case SNMP_MSG_GETNEXT:
                        printf("Received Getnext Request ");
                        break;
                    case SNMP_MSG_RESPONSE:
                        printf("Received Get Response ");
                        break;
#ifndef NETSNMP_NO_WRITE_SUPPORT
                    case SNMP_MSG_SET:
                        printf("Received Set Request ");
                        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
                    case SNMP_MSG_TRAP:
                        printf("Received Trap Request ");
                        break;
                    case SNMP_MSG_GETBULK:
                        printf("Received Bulk Request ");
                        break;
                    case SNMP_MSG_INFORM:
                        printf("Received Inform Request ");
                        break;
                    case SNMP_MSG_TRAP2:
                        printf("Received SNMPv2 Trap Request ");
                        break;
                    }
                    transport = snmp_sess_transport(snmp_sess_pointer(ss));
                    if (transport != NULL && transport->f_fmtaddr != NULL) {
                        char *addr_string = transport->f_fmtaddr(transport,
                                                                 response->
                                                                 transport_data,
                                                                 response->
                                                                 transport_data_length);
                        if (addr_string != NULL) {
                            printf("from %s\n", addr_string);
                            free(addr_string);
                        }
                    } else {
                        printf("from <UNKNOWN>\n");
                    }
                    printf
                        ("requestid 0x%lX errstat 0x%lX errindex 0x%lX\n",
                         response->reqid, response->errstat,
                         response->errindex);
                    if (response->errstat == SNMP_ERR_NOERROR) {
                        for (vars = response->variables; vars;
                             vars = vars->next_variable)
                            print_variable(vars->name, vars->name_length,
                                           vars);
                    } else {
                        printf("Error in packet.\nReason: %s\n",
                               snmp_errstring(response->errstat));
                        if (response->errindex != 0) {
                            for (count = 1, vars = response->variables;
                                 vars && count != response->errindex;
                                 vars = vars->next_variable, count++);
                            if (vars) {
                                printf("Failed object: ");
                                print_objid(vars->name, vars->name_length);
                            }
                            printf("\n");
                        }
                    }
                }
            } else if (status == STAT_TIMEOUT) {
                printf("Timeout: No Response from %s\n", session.peername);
            } else {            /* status == STAT_ERROR */
                snmp_sess_perror("snmptest", ss);
            }

            if (response)
                snmp_free_pdu(response);
        }
        varcount = 0;
        nonRepeaters = -1;
    }
    /* NOTREACHED */
}

int
input_variable(netsnmp_variable_list * vp)
{
    char            buf[256];
    size_t          val_len;
    u_char          ch;

    printf("Variable: ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) {
        printf("Quitting,  Goobye\n");
        SOCK_CLEANUP;
        exit(0);
    }
    val_len = strlen(buf);

    if (val_len == 0 || *buf == '\n') {
        vp->name_length = 0;
        return 0;
    }
    if (buf[val_len - 1] == '\n')
        buf[--val_len] = 0;
    if (*buf == '$') {
        switch (toupper((unsigned char)(buf[1]))) {
        case 'G':
            command = SNMP_MSG_GET;
            printf("Request type is Get Request\n");
            break;
        case 'N':
            command = SNMP_MSG_GETNEXT;
            printf("Request type is Getnext Request\n");
            break;
#ifndef NETSNMP_NO_WRITE_SUPPORT
        case 'S':
            command = SNMP_MSG_SET;
            printf("Request type is Set Request\n");
            break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
        case 'B':
            command = SNMP_MSG_GETBULK;
            printf("Request type is Bulk Request\n");
            printf
                ("Enter a blank line to terminate the list of non-repeaters\n");
            printf("and to begin the repeating variables\n");
            break;
        case 'I':
            command = SNMP_MSG_INFORM;
            printf("Request type is Inform Request\n");
            printf("(Are you sending to the right port?)\n");
            break;
        case 'T':
            command = SNMP_MSG_TRAP2;
            printf("Request type is SNMPv2 Trap Request\n");
            printf("(Are you sending to the right port?)\n");
            break;
        case 'D':
            if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                                       NETSNMP_DS_LIB_DUMP_PACKET)) {
                netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                       NETSNMP_DS_LIB_DUMP_PACKET, 0);
                printf("Turned packet dump off\n");
            } else {
                netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                       NETSNMP_DS_LIB_DUMP_PACKET, 1);
                printf("Turned packet dump on\n");
            }
            break;
        case 'Q':
            switch ((toupper((unsigned char)(buf[2])))) {
            case '\n':
            case 0:
                printf("Quitting,  Goodbye\n");
                SOCK_CLEANUP;
                exit(0);
                break;
            case 'P':
                if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                                           NETSNMP_DS_LIB_QUICK_PRINT)) {
                    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                           NETSNMP_DS_LIB_QUICK_PRINT, 0);
                    printf("Turned quick printing off\n");
                } else {
                    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                           NETSNMP_DS_LIB_QUICK_PRINT, 1);
                    printf("Turned quick printing on\n");
                }
                break;
            }
            break;
        default:
            printf("Bad command\n");
        }
        return -1;
    }
    {
	oid     name[MAX_OID_LEN];
	vp->name_length = MAX_OID_LEN;
	if (!snmp_parse_oid(buf, name, &vp->name_length)) {
	    snmp_perror(buf);
	    return -1;
	}
	vp->name = snmp_duplicate_objid(name, vp->name_length);
    }

    if (command == SNMP_MSG_INFORM
        || command == SNMP_MSG_TRAP2
#ifndef NETSNMP_NO_WRITE_SUPPORT
        || command == SNMP_MSG_SET
#endif /* NETSNMP_NO_WRITE_SUPPORT */
        ) {
        printf("Type [i|u|s|x|d|n|o|t|a]: ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("Quitting,  Goodbye\n");
            SOCK_CLEANUP;
            exit(0);
        }
        ch = *buf;
        switch (ch) {
        case 'i':
            vp->type = ASN_INTEGER;
            break;
        case 'u':
            vp->type = ASN_UNSIGNED;
            break;
        case 's':
            vp->type = ASN_OCTET_STR;
            break;
        case 'x':
            vp->type = ASN_OCTET_STR;
            break;
        case 'd':
            vp->type = ASN_OCTET_STR;
            break;
        case 'n':
            vp->type = ASN_NULL;
            break;
        case 'o':
            vp->type = ASN_OBJECT_ID;
            break;
        case 't':
            vp->type = ASN_TIMETICKS;
            break;
        case 'a':
            vp->type = ASN_IPADDRESS;
            break;
        default:
            printf
                ("bad type \"%c\", use \"i\", \"u\", \"s\", \"x\", \"d\", \"n\", \"o\", \"t\", or \"a\".\n",
                 *buf);
            return -1;
        }
      getValue:
        printf("Value: ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("Quitting,  Goodbye\n");
            SOCK_CLEANUP;
            exit(0);
        }
        switch (vp->type) {
        case ASN_INTEGER:
            vp->val.integer = (long *) malloc(sizeof(long));
            *(vp->val.integer) = atoi(buf);
            vp->val_len = sizeof(long);
            break;
        case ASN_UNSIGNED:
            vp->val.integer = (long *) malloc(sizeof(long));
            *(vp->val.integer) = strtoul(buf, NULL, 0);
            vp->val_len = sizeof(long);
            break;
        case ASN_OCTET_STR:
            if (ch == 'd') {
                size_t          buf_len = 256;
                val_len = 0;
                if ((vp->val.string = (u_char *) malloc(buf_len)) == NULL) {
                    printf("malloc failure\n");
                    goto getValue;
                }
                if (!snmp_decimal_to_binary(&(vp->val.string), &buf_len,
                                            &val_len, 1, buf)) {
                    printf("Bad value or no sub-identifier > 255\n");
                    free(vp->val.string);
                    goto getValue;
                }
                vp->val_len = val_len;
            } else if (ch == 's') {
                /*
                 * -1 to omit trailing newline  
                 */
                vp->val.string = (u_char *) malloc(strlen(buf) - 1);
                if (vp->val.string == NULL) {
                    printf("malloc failure\n");
                    goto getValue;
                }
                memcpy(vp->val.string, buf, strlen(buf) - 1);
                vp->val.string[sizeof(vp->val.string)-1] = 0;
                vp->val_len = strlen(buf) - 1;
            } else if (ch == 'x') {
                size_t          buf_len = 256;
                val_len = 0;
                if ((vp->val.string = (u_char *) malloc(buf_len)) == NULL) {
                    printf("malloc failure\n");
                    goto getValue;
                }
                if (!snmp_hex_to_binary(&(vp->val.string), &buf_len,
                                        &val_len, 1, buf)) {
                    printf("Bad value (need pairs of hex digits)\n");
                    free(vp->val.string);
                    goto getValue;
                }
                vp->val_len = val_len;
            }
            break;
        case ASN_NULL:
            vp->val_len = 0;
            vp->val.string = NULL;
            break;
        case ASN_OBJECT_ID:
            if ('\n' == buf[strlen(buf) - 1])
                buf[strlen(buf) - 1] = '\0';
	    else {
		oid value[MAX_OID_LEN];
		vp->val_len = MAX_OID_LEN;
		if (0 == read_objid(buf, value, &vp->val_len)) {
		    printf("Unrecognised OID value\n");
		    goto getValue;
		}
		vp->val.objid = snmp_duplicate_objid(value, vp->val_len);
		vp->val_len *= sizeof(oid);
	    }
            break;
        case ASN_TIMETICKS:
            vp->val.integer = (long *) malloc(sizeof(long));
            *(vp->val.integer) = atoi(buf);
            vp->val_len = sizeof(long);
            break;
        case ASN_IPADDRESS:
            vp->val.integer = (long *) malloc(sizeof(long));
            *(vp->val.integer) = inet_addr(buf);
            vp->val_len = sizeof(long);
            break;
        default:
            printf("Internal error\n");
            break;
        }
    } else {                    /* some form of get message */
        vp->type = ASN_NULL;
        vp->val_len = 0;
    }
    return 1;
}
