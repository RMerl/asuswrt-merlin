/*
 * snmpdelta.c - Monitor deltas of integer valued SNMP variables
 *
 */
/**********************************************************************
 *
 *           Copyright 1996 by Carnegie Mellon University
 * 
 *                       All Rights Reserved
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of CMU not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * 
 * CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * 
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

#define MAX_ARGS 256
#define NETSNMP_DS_APP_DONT_FIX_PDUS 0

const char     *SumFile = "Sum";

/*
 * Information about the handled variables 
 */
struct varInfo {
    char           *name;
    oid            *info_oid;
    int             type;
    size_t          oidlen;
    char            descriptor[64];
    u_int           value;
    struct counter64 c64value;
    float           max;
    time_t          time;
    int             peak_count;
    float           peak;
    float           peak_average;
    int             spoiled;
};

struct varInfo  varinfo[MAX_ARGS];
int             current_name = 0;
int             period = 1;
int             deltat = 0, timestamp = 0, fileout = 0, dosum =
    0, printmax = 0;
int             keepSeconds = 0, peaks = 0;
int             tableForm = 0;
int             varbindsPerPacket = 60;

void            processFileArgs(char *fileName);

void
usage(void)
{
    fprintf(stderr,
            "Usage: snmpdelta [-Cf] [-CF commandFile] [-Cl] [-CL SumFileName]\n\t[-Cs] [-Ck] [-Ct] [-CS] [-Cv vars/pkt] [-Cp period]\n\t[-CP peaks] ");
    snmp_parse_args_usage(stderr);
    fprintf(stderr, " oid [oid ...]\n");
    snmp_parse_args_descriptions(stderr);
    fprintf(stderr, "snmpdelta specific options\n");
    fprintf(stderr, "  -Cf\t\tDon't fix errors and retry the request.\n");
    fprintf(stderr, "  -Cl\t\twrite configuration to file\n");
    fprintf(stderr, "  -CF config\tload configuration from file\n");
    fprintf(stderr, "  -Cp period\tspecifies the poll period\n");
    fprintf(stderr, "  -CP peaks\treporting period in poll periods\n");
    fprintf(stderr, "  -Cv vars/pkt\tnumber of variables per packet\n");
    fprintf(stderr, "  -Ck\t\tkeep seconds in output time\n");
    fprintf(stderr, "  -Cm\t\tshow max values\n");
    fprintf(stderr, "  -CS\t\tlog to a sum file\n");
    fprintf(stderr, "  -Cs\t\tshow timestamps\n");
    fprintf(stderr, "  -Ct\t\tget timing from agent\n");
    fprintf(stderr, "  -CT\t\tprint output in tabular form\n");
    fprintf(stderr, "  -CL sumfile\tspecifies the sum file name\n");
}

static void
optProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch ((opt = *optarg++)) {
            case 'f':
                netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID,
					  NETSNMP_DS_APP_DONT_FIX_PDUS);
                break;
            case 'p':
                period = atoi(argv[optind++]);
                break;
            case 'P':
                peaks = atoi(argv[optind++]);
                break;
            case 'v':
                varbindsPerPacket = atoi(argv[optind++]);
                break;
            case 't':
                deltat = 1;
                break;
            case 's':
                timestamp = 1;
                break;
            case 'S':
                dosum = 1;
                break;
            case 'm':
                printmax = 1;
                break;
            case 'F':
                processFileArgs(argv[optind++]);
                break;
            case 'l':
                fileout = 1;
                break;
            case 'L':
                SumFile = argv[optind++];
                break;
            case 'k':
                keepSeconds = 1;
                break;
            case 'T':
                tableForm = 1;
                break;
            default:
                fprintf(stderr, "Bad -C options: %c\n", opt);
                exit(1);
            }
        }
        break;
    }
}

int
wait_for_peak_start(int period, int peak)
{
    struct timeval  m_time, *tv = &m_time;
    struct tm       tm;
    time_t          SecondsAtNextHour;
    int             target = 0;
    int             seconds;

    seconds = period * peak;

    /*
     * Find the current time 
     */
    gettimeofday(tv, (struct timezone *) 0);

    /*
     * Create a tm struct from it 
     */
    memcpy(&tm, localtime((time_t *) & tv->tv_sec), sizeof(tm));

    /*
     * Calculate the next hour 
     */
    tm.tm_sec = 0;
    tm.tm_min = 0;
    tm.tm_hour++;
    SecondsAtNextHour = mktime(&tm);

    /*
     * Now figure out the amount of time to sleep 
     */
    target = (int)(SecondsAtNextHour - tv->tv_sec) % seconds;

    return target;
}

void
print_log(char *file, char *message)
{
    FILE           *fp;

    fp = fopen(file, "a");
    if (fp == NULL) {
        fprintf(stderr, "Couldn't open %s\n", file);
        return;
    }
    fprintf(fp, "%s\n", message);
    fclose(fp);
}

void
sprint_descriptor(char *buffer, struct varInfo *vip)
{
    char           *buf = NULL, *cp = NULL;
    size_t          buf_len = 0, out_len = 0;

    if (!sprint_realloc_objid((u_char **)&buf, &buf_len, &out_len, 1,
                              vip->info_oid, vip->oidlen)) {
        if (buf != NULL) {
            free(buf);
        }
        return;
    }

    for (cp = buf; *cp; cp++);
    while (cp >= buf) {
        if (isalpha((unsigned char)(*cp)))
            break;
        cp--;
    }
    while (cp >= buf) {
        if (*cp == '.')
            break;
        cp--;
    }
    cp++;
    if (cp < buf)
        cp = buf;
    strcpy(buffer, cp);

    if (buf != NULL) {
        free(buf);
    }
}

void
processFileArgs(char *fileName)
{
    FILE           *fp;
    char            buf[260] = { 0 }, *cp;
    int             blank, linenumber = 0;

    fp = fopen(fileName, "r");
    if (fp == NULL)
        return;
    while (fgets(buf, sizeof(buf), fp)) {
        linenumber++;
        if (strlen(buf) > (sizeof(buf) - 2)) {
            fprintf(stderr, "Line too long on line %d of %s\n",
                    linenumber, fileName);
            exit(1);
        }
        if (buf[0] == '#')
            continue;
        blank = TRUE;
        for (cp = buf; *cp; cp++)
            if (!isspace((unsigned char)(*cp))) {
                blank = FALSE;
                break;
            }
        if (blank)
            continue;
        buf[strlen(buf) - 1] = 0;
	if (current_name >= MAX_ARGS) {
	    fprintf(stderr, "Too many variables read at line %d of %s (max %d)\n",
	    	linenumber, fileName, MAX_ARGS);
	    exit(1);
	}
        varinfo[current_name++].name = strdup(buf);
    }
    fclose(fp);
    return;
}

void
wait_for_period(int period)
{
#ifdef WIN32
    Sleep(period * 1000);
#else                   /* WIN32 */
    struct timeval  m_time, *tv = &m_time;
    struct tm       tm;
    int             count;
    static int      target = 0;
    time_t          nexthour;

    gettimeofday(tv, (struct timezone *) 0);

    if (target) {
        target += period;
    } else {
        memcpy(&tm, localtime((time_t *) & tv->tv_sec), sizeof(tm));
        tm.tm_sec = 0;
        tm.tm_min = 0;
        tm.tm_hour++;
        nexthour = mktime(&tm);

        target = (nexthour - tv->tv_sec) % period;
        if (target == 0)
            target = period;
        target += tv->tv_sec;
    }

    tv->tv_sec = target - tv->tv_sec;
    if (tv->tv_usec != 0) {
        tv->tv_sec--;
        tv->tv_usec = 1000000 - tv->tv_usec;
    }
    if (tv->tv_sec < 0) {
        /*
         * ran out of time, schedule immediately 
         */
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }
    count = 1;
    while (count != 0) {
        count = select(0, NULL, NULL, NULL, tv);
        switch (count) {
        case 0:
            break;
        case -1:
            /*
             * FALLTHRU 
             */
        default:
            snmp_log_perror("select");
            break;
        }
    }
#endif                   /* WIN32 */
}

oid             sysUpTimeOid[9] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
size_t          sysUpTimeLen = 9;

int
main(int argc, char *argv[])
{
    netsnmp_session session, *ss;
    netsnmp_pdu    *pdu, *response;
    netsnmp_variable_list *vars;
    int             arg;
    char           *gateway;

    int             count;
    struct varInfo *vip;
    u_int           value = 0;
    struct counter64 c64value;
    float           printvalue;
    time_t          last_time = 0;
    time_t          this_time;
    time_t          delta_time;
    int             sum;        /* what the heck is this for, its never used? */
    char            filename[128] = { 0 };
    struct timeval  tv;
    struct tm       tm;
    char            timestring[64] = { 0 }, valueStr[64] = {
    0}, maxStr[64] = {
    0};
    char            outstr[256] = { 0 }, peakStr[64] = {
    0};
    int             status;
    int             begin, end, last_end;
    int             print = 1;
    int             exit_code = 0;

    switch (arg = snmp_parse_args(argc, argv, &session, "C:", &optProc)) {
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

    gateway = session.peername;

    for (; optind < argc; optind++) {
	if (current_name >= MAX_ARGS) {
	    fprintf(stderr, "%s: Too many variables specified (max %d)\n",
	    	argv[optind], MAX_ARGS);
	    exit(1);
	}
        varinfo[current_name++].name = argv[optind];
    }

    if (current_name == 0) {
        usage();
        exit(1);
    }

    if (dosum) {
	if (current_name >= MAX_ARGS) {
	    fprintf(stderr, "Too many variables specified (max %d)\n",
	    	MAX_ARGS);
	    exit(1);
	}
        varinfo[current_name++].name = NULL;
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
        snmp_sess_perror("snmpdelta", &session);
        SOCK_CLEANUP;
        exit(1);
    }

    if (tableForm && timestamp) {
        printf("%s", gateway);
    }
    for (count = 0; count < current_name; count++) {
        vip = varinfo + count;
        if (vip->name) {
            vip->oidlen = MAX_OID_LEN;
            vip->info_oid = (oid *) malloc(sizeof(oid) * vip->oidlen);
            if (snmp_parse_oid(vip->name, vip->info_oid, &vip->oidlen) ==
                NULL) {
                snmp_perror(vip->name);
                SOCK_CLEANUP;
                exit(1);
            }
            sprint_descriptor(vip->descriptor, vip);
            if (tableForm)
                printf("\t%s", vip->descriptor);
        } else {
            vip->oidlen = 0;
            strlcpy(vip->descriptor, SumFile, sizeof(vip->descriptor));
        }
        vip->value = 0;
        zeroU64(&vip->c64value);
        vip->time = 0;
        vip->max = 0;
        if (peaks) {
            vip->peak_count = -1;
            vip->peak = 0;
            vip->peak_average = 0;
        }
    }

    wait_for_period(period);

    end = current_name;
    sum = 0;
    while (1) {
        pdu = snmp_pdu_create(SNMP_MSG_GET);

        if (deltat)
            snmp_add_null_var(pdu, sysUpTimeOid, sysUpTimeLen);

        if (end == current_name)
            count = 0;
        else
            count = end;
        begin = count;
        for (; count < current_name
             && count < begin + varbindsPerPacket - deltat; count++) {
            if (varinfo[count].oidlen)
                snmp_add_null_var(pdu, varinfo[count].info_oid,
                                  varinfo[count].oidlen);
        }
        last_end = end;
        end = count;

      retry:
        status = snmp_synch_response(ss, pdu, &response);
        if (status == STAT_SUCCESS) {
            if (response->errstat == SNMP_ERR_NOERROR) {
                if (timestamp) {
                    gettimeofday(&tv, (struct timezone *) 0);
                    memcpy(&tm, localtime((time_t *) & tv.tv_sec),
                           sizeof(tm));
                    if (((period % 60)
                         && (!peaks || ((period * peaks) % 60)))
                        || keepSeconds)
                        sprintf(timestring, " [%02d:%02d:%02d %d/%d]",
                                tm.tm_hour, tm.tm_min, tm.tm_sec,
                                tm.tm_mon + 1, tm.tm_mday);
                    else
                        sprintf(timestring, " [%02d:%02d %d/%d]",
                                tm.tm_hour, tm.tm_min,
                                tm.tm_mon + 1, tm.tm_mday);
                }

                vars = response->variables;
                if (deltat) {
                    if (!vars || !vars->val.integer) {
                        fprintf(stderr, "Missing variable in reply\n");
                        continue;
                    } else {
                        this_time = *(vars->val.integer);
                    }
                    vars = vars->next_variable;
                } else {
                    this_time = 1;
                }

                for (count = begin; count < end; count++) {
                    vip = varinfo + count;

                    if (vip->oidlen) {
                        if (!vars || !vars->val.integer) {
                            fprintf(stderr, "Missing variable in reply\n");
                            break;
                        }
                        vip->type = vars->type;
                        if (vars->type == ASN_COUNTER64) {
                            u64Subtract(vars->val.counter64,
                                        &vip->c64value, &c64value);
                            memcpy(&vip->c64value, vars->val.counter64,
                                   sizeof(struct counter64));
                        } else {
                            value = *(vars->val.integer) - vip->value;
                            vip->value = *(vars->val.integer);
                        }
                        vars = vars->next_variable;
                    } else {
                        value = sum;
                        sum = 0;
                    }
                    delta_time = this_time - vip->time;
                    if (delta_time <= 0)
                        delta_time = 100;
                    last_time = vip->time;
                    vip->time = this_time;
                    if (last_time == 0)
                        continue;

                    if (vip->oidlen && vip->type != ASN_COUNTER64) {
                        sum += value;
                    }

                    if (tableForm) {
                        if (count == begin) {
                            sprintf(outstr, "%s", timestring + 1);
                        } else {
                            outstr[0] = '\0';
                        }
                    } else {
                        sprintf(outstr, "%s %s", timestring,
                                vip->descriptor);
                    }

                    if (deltat || tableForm) {
                        if (vip->type == ASN_COUNTER64) {
                            fprintf(stderr,
                                    "time delta and table form not supported for counter64s\n");
                            exit(1);
                        } else {
                            printvalue =
                                ((float) value * 100) / delta_time;
                            if (tableForm)
                                sprintf(valueStr, "\t%.2f", printvalue);
                            else
                                sprintf(valueStr, " /sec: %.2f",
                                        printvalue);
                        }
                    } else {
                        printvalue = (float) value;
                        sprintf(valueStr, " /%d sec: ", period);
                        if (vip->type == ASN_COUNTER64)
                            printU64(valueStr + strlen(valueStr),
                                     &c64value);
                        else
                            sprintf(valueStr + strlen(valueStr), "%u",
                                    value);
                    }

                    if (!peaks) {
                        strcat(outstr, valueStr);
                    } else {
                        print = 0;
                        if (vip->peak_count == -1) {
                            if (wait_for_peak_start(period, peaks) == 0)
                                vip->peak_count = 0;
                        } else {
                            vip->peak_average += printvalue;
                            if (vip->peak < printvalue)
                                vip->peak = printvalue;
                            if (++vip->peak_count == peaks) {
                                if (deltat)
                                    sprintf(peakStr,
                                            " /sec: %.2f	(%d sec Peak: %.2f)",
                                            vip->peak_average /
                                            vip->peak_count, period,
                                            vip->peak);
                                else
                                    sprintf(peakStr,
                                            " /%d sec: %.0f	(%d sec Peak: %.0f)",
                                            period,
                                            vip->peak_average /
                                            vip->peak_count, period,
                                            vip->peak);
                                vip->peak_average = 0;
                                vip->peak = 0;
                                vip->peak_count = 0;
                                print = 1;
                                strcat(outstr, peakStr);
                            }
                        }
                    }

                    if (printmax) {
                        if (printvalue > vip->max) {
                            vip->max = printvalue;
                        }
                        if (deltat)
                            sprintf(maxStr, "	(Max: %.2f)", vip->max);
                        else
                            sprintf(maxStr, "	(Max: %.0f)", vip->max);
                        strcat(outstr, maxStr);
                    }

                    if (print) {
                        if (fileout) {
                            sprintf(filename, "%s-%s", gateway,
                                    vip->descriptor);
                            print_log(filename, outstr + 1);
                        } else {
                            if (tableForm)
                                printf("%s", outstr);
                            else
                                printf("%s\n", outstr + 1);
                            fflush(stdout);
                        }
                    }
                }
                if (end == last_end && tableForm)
                    printf("\n");
            } else {
                if (response->errstat == SNMP_ERR_TOOBIG) {
                    if (response->errindex <= varbindsPerPacket
                        && response->errindex > 0) {
                        varbindsPerPacket = response->errindex - 1;
                    } else {
                        if (varbindsPerPacket > 30)
                            varbindsPerPacket -= 5;
                        else
                            varbindsPerPacket--;
                    }
                    if (varbindsPerPacket <= 0) {
                        exit_code = 5;
                        break;
                    }
                    end = last_end;
                    continue;
                } else if (response->errindex != 0) {
                    fprintf(stderr, "Failed object: ");
                    for (count = 1, vars = response->variables;
                         vars && count != response->errindex;
                         vars = vars->next_variable, count++);
                    if (vars)
                        fprint_objid(stderr, vars->name,
                                     vars->name_length);
                    fprintf(stderr, "\n");
                    /*
                     * Don't exit when OIDs from file are not found on agent
                     * exit_code = 1;
                     * break;
                     */
                } else {
                    fprintf(stderr, "Error in packet: %s\n",
                            snmp_errstring(response->errstat));
                    exit_code = 1;
                    break;
                }

                /*
                 * retry if the errored variable was successfully removed 
                 */
                if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
					    NETSNMP_DS_APP_DONT_FIX_PDUS)) {
                    pdu = snmp_fix_pdu(response, SNMP_MSG_GET);
                    snmp_free_pdu(response);
                    response = NULL;
                    if (pdu != NULL)
                        goto retry;
                }
            }

        } else if (status == STAT_TIMEOUT) {
            fprintf(stderr, "Timeout: No Response from %s\n", gateway);
            response = NULL;
            exit_code = 1;
            break;
        } else {                /* status == STAT_ERROR */
            snmp_sess_perror("snmpdelta", ss);
            response = NULL;
            exit_code = 1;
            break;
        }

        if (response)
            snmp_free_pdu(response);
        if (end == current_name) {
            wait_for_period(period);
        }
    }
    snmp_close(ss);
    SOCK_CLEANUP;
    return (exit_code);
}
