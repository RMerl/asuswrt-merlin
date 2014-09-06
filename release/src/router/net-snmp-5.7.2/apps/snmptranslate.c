/*
 * snmptranslate.c - report or translate info about oid from mibs
 *
 * Update: 1998-07-17 <jhy@gsu.edu>
 * Added support for dumping alternate oid reports (-t and -T options).
 * Added more detailed (useful?) usage info.
 */
/************************************************************************
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
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <net-snmp/utilities.h>
#include <net-snmp/config_api.h>
#include <net-snmp/output_api.h>
#include <net-snmp/mib_api.h>

int             show_all_matched_objects(FILE *, const char *, oid *,
                                         size_t *, int, int);

void
usage(void)
{
    fprintf(stderr, "USAGE: snmptranslate [OPTIONS] OID [OID]...\n\n");
    fprintf(stderr, "  Version:  %s\n", netsnmp_get_version());
    fprintf(stderr, "  Web:      http://www.net-snmp.org/\n");
    fprintf(stderr,
            "  Email:    net-snmp-coders@lists.sourceforge.net\n\nOPTIONS:\n");

    fprintf(stderr, "  -h\t\t\tdisplay this help message\n");
    fprintf(stderr, "  -V\t\t\tdisplay package version number\n");
    fprintf(stderr,
            "  -m MIB[:...]\t\tload given list of MIBs (ALL loads everything)\n");
    fprintf(stderr,
            "  -M DIR[:...]\t\tlook in given list of directories for MIBs\n");
    fprintf(stderr,
            "  -D[TOKEN[,...]]\tturn on debugging output for the specified TOKENs\n\t\t\t   (ALL gives extremely verbose debugging output)\n");
    fprintf(stderr, "  -w WIDTH\t\tset width of tree and detail output\n");
    fprintf(stderr,
            "  -T TRANSOPTS\t\tSet various options controlling report produced:\n");
    fprintf(stderr,
            "\t\t\t  B:  print all matching objects for a regex search\n");
    fprintf(stderr, "\t\t\t  d:  print full details of the given OID\n");
    fprintf(stderr, "\t\t\t  p:  print tree format symbol table\n");
    fprintf(stderr, "\t\t\t  a:  print ASCII format symbol table\n");
    fprintf(stderr, "\t\t\t  l:  enable labeled OID report\n");
    fprintf(stderr, "\t\t\t  o:  enable OID report\n");
    fprintf(stderr, "\t\t\t  s:  enable dotted symbolic report\n");
    fprintf(stderr, "\t\t\t  z:  enable MIB child OID report\n");
    fprintf(stderr,
            "\t\t\t  t:  enable alternate format symbolic suffix report\n");
#ifndef NETSNMP_DISABLE_MIB_LOADING
    fprintf(stderr,
            "  -P MIBOPTS\t\tToggle various defaults controlling mib parsing:\n");
    snmp_mib_toggle_options_usage("\t\t\t  ", stderr);
#endif /* NETSNMP_DISABLE_MIB_LOADING */
    fprintf(stderr,
            "  -O OUTOPTS\t\tToggle various defaults controlling output display:\n");
    snmp_out_toggle_options_usage("\t\t\t  ", stderr);
    fprintf(stderr,
            "  -I INOPTS\t\tToggle various defaults controlling input parsing:\n");
    snmp_in_toggle_options_usage("\t\t\t  ", stderr);
    fprintf(stderr,
            "  -L LOGOPTS\t\tToggle various defaults controlling logging:\n");
    snmp_log_options_usage("\t\t\t  ", stderr);
    exit(1);
}

int
main(int argc, char *argv[])
{
    int             arg;
    char           *current_name = NULL, *cp = NULL;
    oid             name[MAX_OID_LEN];
    size_t          name_length;
    int             description = 0;
    int             print = 0;
    int             find_all = 0;
    int             width = 1000000;

    /*
     * usage: snmptranslate name
     */
    while ((arg = getopt(argc, argv, "Vhm:M:w:D:P:T:O:I:L:")) != EOF) {
        switch (arg) {
        case 'h':
            usage();
            exit(1);

        case 'm':
            setenv("MIBS", optarg, 1);
            break;
        case 'M':
            setenv("MIBDIRS", optarg, 1);
            break;
        case 'D':
            debug_register_tokens(optarg);
            snmp_set_do_debugging(1);
            break;
        case 'V':
            fprintf(stderr, "NET-SNMP version: %s\n",
                    netsnmp_get_version());
            exit(0);
            break;
        case 'w':
	    width = atoi(optarg);
	    if (width <= 0) {
		fprintf(stderr, "Invalid width specification: %s\n", optarg);
		exit (1);
	    }
	    break;
#ifndef NETSNMP_DISABLE_MIB_LOADING
        case 'P':
            cp = snmp_mib_toggle_options(optarg);
            if (cp != NULL) {
                fprintf(stderr, "Unknown parser option to -P: %c.\n", *cp);
                usage();
                exit(1);
            }
            break;
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        case 'O':
            cp = snmp_out_toggle_options(optarg);
            if (cp != NULL) {
                fprintf(stderr, "Unknown OID option to -O: %c.\n", *cp);
                usage();
                exit(1);
            }
            break;
        case 'I':
            cp = snmp_in_toggle_options(optarg);
            if (cp != NULL) {
                fprintf(stderr, "Unknown OID option to -I: %c.\n", *cp);
                usage();
                exit(1);
            }
            break;
        case 'T':
            for (cp = optarg; *cp; cp++) {
                switch (*cp) {
#ifndef NETSNMP_DISABLE_MIB_LOADING
                case 'l':
                    print = 3;
                    print_oid_report_enable_labeledoid();
                    break;
                case 'o':
                    print = 3;
                    print_oid_report_enable_oid();
                    break;
                case 's':
                    print = 3;
                    print_oid_report_enable_symbolic();
                    break;
                case 't':
                    print = 3;
                    print_oid_report_enable_suffix();
                    break;
                case 'z':
                    print = 3;
                    print_oid_report_enable_mibchildoid();
                    break;
#endif /* NETSNMP_DISABLE_MIB_LOADING */
                case 'd':
                    description = 1;
                    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                           NETSNMP_DS_LIB_SAVE_MIB_DESCRS, 1);
                    break;
                case 'B':
                    find_all = 1;
                    break;
                case 'p':
                    print = 1;
                    break;
                case 'a':
                    print = 2;
                    break;
                default:
                    fprintf(stderr, "Invalid -T<lostpad> character: %c\n",
                            *cp);
                    usage();
                    exit(1);
                    break;
                }
            }
            break;
        case 'L':
            if (snmp_log_options(optarg, argc, argv) < 0) {
                return (-1);
            }
            break;
        default:
            fprintf(stderr, "invalid option: -%c\n", arg);
            usage();
            exit(1);
            break;
        }
    }

    init_snmp("snmpapp");
    if (optind < argc)
        current_name = argv[optind];

    if (current_name == NULL) {
        switch (print) {
        default:
            usage();
            exit(1);
#ifndef NETSNMP_DISABLE_MIB_LOADING
        case 1:
            print_mib_tree(stdout, get_tree_head(), width);
            break;
        case 2:
            print_ascii_dump(stdout);
            break;
        case 3:
            print_oid_report(stdout);
            break;
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        }
        exit(0);
    }

    do {
        name_length = MAX_OID_LEN;
        if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
				  NETSNMP_DS_LIB_RANDOM_ACCESS)) {
#ifndef NETSNMP_DISABLE_MIB_LOADING
            if (!get_node(current_name, name, &name_length)) {
#endif /* NETSNMP_DISABLE_MIB_LOADING */
                fprintf(stderr, "Unknown object identifier: %s\n",
                        current_name);
                exit(2);
#ifndef NETSNMP_DISABLE_MIB_LOADING
            }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        } else if (find_all) {
            if (0 == show_all_matched_objects(stdout, current_name,
                                              name, &name_length,
                                              description, width)) {
                fprintf(stderr,
                        "Unable to find a matching object identifier for \"%s\"\n",
                        current_name);
                exit(1);
            }
            exit(0);
        } else if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
					  NETSNMP_DS_LIB_REGEX_ACCESS)) {
#ifndef NETSNMP_DISABLE_MIB_LOADING
            if (0 == get_wild_node(current_name, name, &name_length)) {
#endif /* NETSNMP_DISABLE_MIB_LOADING */
                fprintf(stderr,
                        "Unable to find a matching object identifier for \"%s\"\n",
                        current_name);
                exit(1);
#ifndef NETSNMP_DISABLE_MIB_LOADING
            }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        } else {
            if (!read_objid(current_name, name, &name_length)) {
                snmp_perror(current_name);
                exit(2);
            }
        }


        if (print == 1) {
#ifndef NETSNMP_DISABLE_MIB_LOADING
            struct tree    *tp;
            tp = get_tree(name, name_length, get_tree_head());
            if (tp == NULL) {
#endif /* NETSNMP_DISABLE_MIB_LOADING */
                snmp_log(LOG_ERR,
                        "Unable to find a matching object identifier for \"%s\"\n",
                        current_name);
                exit(1);
#ifndef NETSNMP_DISABLE_MIB_LOADING
            }
            print_mib_tree(stdout, tp, width);
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        } else {
            print_objid(name, name_length);
            if (description) {
#ifndef NETSNMP_DISABLE_MIB_LOADING
                print_description(name, name_length, width);
#endif /* NETSNMP_DISABLE_MIB_LOADING */
            }
        }
        current_name = argv[++optind];
        if (current_name != NULL)
            printf("\n");
    } while (optind < argc);

    return (0);
}

/*
 * Show all object identifiers that match the pattern.
 */

int
show_all_matched_objects(FILE * fp, const char *patmatch, oid * name, size_t * name_length, int f_desc, /* non-zero if descriptions should be shown */
                         int width)
{
    int             result = 0, count = 0;
    size_t          savlen = *name_length;
#ifndef NETSNMP_DISABLE_MIB_LOADING
    clear_tree_flags(get_tree_head());
#endif /* NETSNMP_DISABLE_MIB_LOADING */

    while (1) {
        *name_length = savlen;
#ifndef NETSNMP_DISABLE_MIB_LOADING
        result = get_wild_node(patmatch, name, name_length);
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        if (!result)
            break;
        count++;

        fprint_objid(fp, name, *name_length);
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (f_desc)
            fprint_description(fp, name, *name_length, width);
#endif /* NETSNMP_DISABLE_MIB_LOADING */
    }

    return (count);
}
