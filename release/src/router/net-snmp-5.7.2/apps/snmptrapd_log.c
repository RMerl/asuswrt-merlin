/*
 * snmptrapd_log.c - format SNMP trap information for logging
 *
 */
/*****************************************************************
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
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <ctype.h>
#if !defined(mingw32) && defined(HAVE_SYS_TIME_H)
# include <sys/time.h>
# if TIME_WITH_SYS_TIME
#  include <time.h>
# endif
#else
# include <time.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include "snmptrapd_log.h"


#ifndef BSD4_3
#define BSD4_2
#endif

/*
 * These flags mark undefined values in the options structure 
 */
#define UNDEF_CMD '*'
#define UNDEF_PRECISION -1

/*
 * This structure holds the options for a single format command 
 */
typedef struct {
    char            cmd;        /* the format command itself */
    size_t          width;      /* the field's minimum width */
    int             precision;  /* the field's precision */
    int             left_justify;       /* if true, left justify this field */
    int             alt_format; /* if true, display in alternate format */
    int             leading_zeroes;     /* if true, display with leading zeroes */
} options_type;

char            separator[32];

/*
 * These symbols define the characters that the parser recognizes.
 * The rather odd choice of symbols comes from an attempt to avoid
 * colliding with the ones that printf uses, so that someone could add
 * printf functionality to this code and turn it into a library
 * routine in the future.  
 */
typedef enum {
    CHR_FMT_DELIM = '%',        /* starts a format command */
    CHR_LEFT_JUST = '-',        /* left justify */
    CHR_LEAD_ZERO = '0',        /* use leading zeroes */
    CHR_ALT_FORM = '#',         /* use alternate format */
    CHR_FIELD_SEP = '.',        /* separates width and precision fields */

    /* Date / Time Information */
    CHR_CUR_TIME = 't',         /* current time, Unix format */
    CHR_CUR_YEAR = 'y',         /* current year */
    CHR_CUR_MONTH = 'm',        /* current month */
    CHR_CUR_MDAY = 'l',         /* current day of month */
    CHR_CUR_HOUR = 'h',         /* current hour */
    CHR_CUR_MIN = 'j',          /* current minute */
    CHR_CUR_SEC = 'k',          /* current second */
    CHR_UP_TIME = 'T',          /* uptime, Unix format */
    CHR_UP_YEAR = 'Y',          /* uptime year */
    CHR_UP_MONTH = 'M',         /* uptime month */
    CHR_UP_MDAY = 'L',          /* uptime day of month */
    CHR_UP_HOUR = 'H',          /* uptime hour */
    CHR_UP_MIN = 'J',           /* uptime minute */
    CHR_UP_SEC = 'K',           /* uptime second */

    /* transport information */
    CHR_AGENT_IP = 'a',         /* agent's IP address */
    CHR_AGENT_NAME = 'A',       /* agent's host name if available */

    /* authentication information */
    CHR_SNMP_VERSION = 's',     /* SNMP Version Number */
    CHR_SNMP_SECMOD  = 'S',     /* SNMPv3 Security Model Version Number */
    CHR_SNMP_USER = 'u',        /* SNMPv3 secName or v1/v2c community */
    CHR_TRAP_CONTEXTID = 'E',   /* SNMPv3 context engineID if available */

    /* PDU information */
    CHR_PDU_IP = 'b',           /* PDU's IP address */
    CHR_PDU_NAME = 'B',         /* PDU's host name if available */
    CHR_PDU_ENT = 'N',          /* PDU's enterprise string */
    CHR_PDU_WRAP = 'P',         /* PDU's wrapper info (community, security) */
    CHR_TRAP_NUM = 'w',         /* trap number */
    CHR_TRAP_DESC = 'W',        /* trap's description (textual) */
    CHR_TRAP_STYPE = 'q',       /* trap's subtype */
    CHR_TRAP_VARSEP = 'V',      /* character (or string) to separate variables */
    CHR_TRAP_VARS = 'v'        /* tab-separated list of trap's variables */

} parse_chr_type;

/*
 * These symbols define the states for the parser's state machine 
 */
typedef enum {
    PARSE_NORMAL,               /* looking for next character */
    PARSE_BACKSLASH,            /* saw a backslash */
    PARSE_IN_FORMAT,            /* saw a % sign, in a format command */
    PARSE_GET_WIDTH,            /* getting field width */
    PARSE_GET_PRECISION,        /* getting field precision */
    PARSE_GET_SEPARATOR         /* getting field separator */
} parse_state_type;

/*
 * macros 
 */

#define is_cur_time_cmd(chr) ((((chr) == CHR_CUR_TIME)     \
			       || ((chr) == CHR_CUR_YEAR)  \
			       || ((chr) == CHR_CUR_MONTH) \
			       || ((chr) == CHR_CUR_MDAY)  \
			       || ((chr) == CHR_CUR_HOUR)  \
			       || ((chr) == CHR_CUR_MIN)   \
			       || ((chr) == CHR_CUR_SEC)) ? TRUE : FALSE)
     /*
      * Function:
      *    Returns true if the character is a format command that outputs
      * some field that deals with the current time.
      *
      * Input Parameters:
      *    chr - character to check
      */

#define is_up_time_cmd(chr) ((((chr) == CHR_UP_TIME)     \
			      || ((chr) == CHR_UP_YEAR)  \
			      || ((chr) == CHR_UP_MONTH) \
			      || ((chr) == CHR_UP_MDAY)  \
			      || ((chr) == CHR_UP_HOUR)  \
			      || ((chr) == CHR_UP_MIN)   \
			      || ((chr) == CHR_UP_SEC)) ? TRUE : FALSE)
     /*
      * Function:
      *    Returns true if the character is a format command that outputs
      * some field that deals with up-time.
      *
      * Input Parameters:
      *    chr - character to check
      */

#define is_agent_cmd(chr) ((((chr) == CHR_AGENT_IP) \
			    || ((chr) == CHR_AGENT_NAME)) ? TRUE : FALSE)
     /*
      * Function:
      *    Returns true if the character outputs information about the
      * agent.
      *
      * Input Parameters:
      *    chr - the character to check
      */

#define is_pdu_ip_cmd(chr) ((((chr) == CHR_PDU_IP)   \
			  || ((chr) == CHR_PDU_NAME)) ? TRUE : FALSE)

     /*
      * Function:
      *    Returns true if the character outputs information about the SNMP
      *      authentication information
      * Input Parameters:
      *    chr - the character to check
      */

#define is_auth_cmd(chr) ((((chr) == CHR_SNMP_VERSION       \
                            || (chr) == CHR_SNMP_SECMOD     \
                            || (chr) == CHR_SNMP_USER)) ? TRUE : FALSE)

     /*
      * Function:
      *    Returns true if the character outputs information about the PDU's
      * host name or IP address.
      *
      * Input Parameters:
      *    chr - the character to check
      */

#define is_trap_cmd(chr) ((((chr) == CHR_TRAP_NUM)      \
			   || ((chr) == CHR_TRAP_DESC)  \
			   || ((chr) == CHR_TRAP_STYPE) \
			   || ((chr) == CHR_TRAP_VARS)) ? TRUE : FALSE)

     /*
      * Function:
      *    Returns true if the character outputs information about the trap.
      *
      * Input Parameters:
      *    chr - the character to check
      */

#define is_fmt_cmd(chr) ((is_cur_time_cmd (chr)     \
			  || is_up_time_cmd (chr)   \
			  || is_auth_cmd (chr)   \
			  || is_agent_cmd (chr)     \
			  || is_pdu_ip_cmd (chr)    \
                          || ((chr) == CHR_PDU_ENT) \
                          || ((chr) == CHR_TRAP_CONTEXTID) \
                          || ((chr) == CHR_PDU_WRAP) \
			  || is_trap_cmd (chr)) ? TRUE : FALSE)
     /*
      * Function:
      *    Returns true if the character is a format command.
      * 
      * Input Parameters:
      *    chr - character to check
      */

#define is_numeric_cmd(chr) ((is_cur_time_cmd(chr)   \
			      || is_up_time_cmd(chr) \
			      || (chr) == CHR_TRAP_NUM) ? TRUE : FALSE)
     /*
      * Function:
      *    Returns true if this is a numeric format command.
      *
      * Input Parameters:
      *    chr - character to check
      */

#define reference(var) ((var) == (var))

     /*
      * Function:
      *    Some compiler options will tell the compiler to be picky and
      * warn you if you pass a parameter to a function but don't use it.
      * This macro lets you reference a parameter so that the compiler won't
      * generate the warning. It has no other effect.
      *
      * Input Parameters:
      *    var - the parameter to reference
      */

/*
 * prototypes 
 */
extern const char *trap_description(int trap);

static void
init_options(options_type * options)

     /*
      * Function:
      *    Initialize a structure that contains the option settings for
      * a format command.
      *
      * Input Parameters:
      *    options - points to the structure to initialize
      */
{
    /*
     * initialize the structure's fields 
     */
    options->cmd = '*';
    options->width = 0;
    options->precision = UNDEF_PRECISION;
    options->left_justify = FALSE;
    options->alt_format = FALSE;
    options->leading_zeroes = FALSE;
    return;
}


static int
realloc_output_temp_bfr(u_char ** buf, size_t * buf_len, size_t * out_len,
                        int allow_realloc,
                        u_char ** temp_buf, options_type * options)

     /*
      * Function:
      *    Append the contents of the temporary buffer to the specified
      * buffer using the correct justification, leading zeroes, width,
      * precision, and other characteristics specified in the options
      * structure.
      *
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    temp_buf - pointer to string to append onto output buffer.  THIS
      *               STRING IS free()d BY THIS FUNCTION.
      *    options  - what options to use when appending string
      */
{
    size_t          temp_len;   /* length of temporary buffer */
    size_t          temp_to_write;      /* # of chars to write from temp bfr */
    size_t          char_to_write;      /* # of other chars to write */
    size_t          zeroes_to_write;    /* fill to precision with zeroes for numbers */

    if (temp_buf == NULL || *temp_buf == NULL) {
        return 1;
    }

    /*
     * Figure out how many characters are in the temporary buffer now,
     * and how many of them we'll write.
     */
    temp_len = strlen((char *) *temp_buf);
    temp_to_write = temp_len;

    if (options->precision != UNDEF_PRECISION &&
        temp_to_write > (size_t)options->precision) {
        temp_to_write = options->precision;
    }

    /*
     * Handle leading characters.  
     */
    if ((!options->left_justify) && (temp_to_write < options->width)) {
        zeroes_to_write = options->precision - temp_to_write;
        if (!is_numeric_cmd(options->cmd)) {
            zeroes_to_write = 0;
        }

        for (char_to_write = options->width - temp_to_write;
             char_to_write > 0; char_to_write--) {
            if ((*out_len + 1) >= *buf_len) {
                if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                    *(*buf + *out_len) = '\0';
                    free(*temp_buf);
                    return 0;
                }
            }
            if (options->leading_zeroes || zeroes_to_write-- > 0) {
                *(*buf + *out_len) = '0';
            } else {
                *(*buf + *out_len) = ' ';
            }
            (*out_len)++;
        }
    }

    /*
     * Truncate the temporary buffer and append its contents.  
     */
    *(*temp_buf + temp_to_write) = '\0';
    if (!snmp_strcat(buf, buf_len, out_len, allow_realloc, *temp_buf)) {
        free(*temp_buf);
        return 0;
    }

    /*
     * Handle trailing characters.  
     */
    if ((options->left_justify) && (temp_to_write < options->width)) {
        for (char_to_write = options->width - temp_to_write;
             char_to_write > 0; char_to_write--) {
            if ((*out_len + 1) >= *buf_len) {
                if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                    *(*buf + *out_len) = '\0';
                    free(*temp_buf);
                    return 0;
                }
            }
            *(*buf + *out_len) = '0';
            (*out_len)++;
        }
    }

    /*
     * Slap on a trailing \0 for good measure.  
     */

    *(*buf + *out_len) = '\0';
    free(*temp_buf);
    *temp_buf = NULL;
    return 1;
}


static int
realloc_handle_time_fmt(u_char ** buf, size_t * buf_len, size_t * out_len,
                        int allow_realloc,
                        options_type * options, netsnmp_pdu *pdu)

     /*
      * Function:
      *    Handle a format command that deals with the current or up-time.
      * Append the correct time information to the buffer subject to the
      * buffer's length limit.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    options - options governing how to write the field
      *    pdu     - information about this trap
      */
{
    time_t          time_val;   /* the time value to output */
    unsigned long   time_ul;    /* u_long time/timeticks */
    struct tm      *parsed_time;        /* parsed version of current time */
    char           *safe_bfr = NULL;
    char            fmt_cmd = options->cmd;     /* the format command to use */

    if ((safe_bfr = (char *) calloc(30, 1)) == NULL) {
        return 0;
    }

    /*
     * Get the time field to output.  
     */
    if (is_up_time_cmd(fmt_cmd)) {
        time_ul = pdu->time;
    } else {
        /*
         * Note: a time_t is a signed long.  
         */
        time(&time_val);
        time_ul = (unsigned long) time_val;
    }

    /*
     * Handle output in Unix time format.  
     */
    if (fmt_cmd == CHR_CUR_TIME) {
        sprintf(safe_bfr, "%lu", time_ul);
    } else if (fmt_cmd == CHR_UP_TIME && !options->alt_format) {
        sprintf(safe_bfr, "%lu", time_ul);
    } else if (fmt_cmd == CHR_UP_TIME) {
        unsigned int    centisecs, seconds, minutes, hours, days;

        centisecs = time_ul % 100;
        time_ul /= 100;
        days = time_ul / (60 * 60 * 24);
        time_ul %= (60 * 60 * 24);

        hours = time_ul / (60 * 60);
        time_ul %= (60 * 60);

        minutes = time_ul / 60;
        seconds = time_ul % 60;

        switch (days) {
        case 0:
            sprintf(safe_bfr, "%u:%02u:%02u.%02u",
                    hours, minutes, seconds, centisecs);
            break;
        case 1:
            sprintf(safe_bfr, "1 day, %u:%02u:%02u.%02u",
                    hours, minutes, seconds, centisecs);
            break;
        default:
            sprintf(safe_bfr, "%u days, %u:%02u:%02u.%02u",
                    days, hours, minutes, seconds, centisecs);
        }
    } else {
        /*
         * Handle other time fields.  
         */

        if (options->alt_format) {
            parsed_time = gmtime(&time_val);
        } else {
            parsed_time = localtime(&time_val);
        }

        switch (fmt_cmd) {

            /*
             * Output year. The year field is unusual: if there's a restriction 
             * on precision, we want to truncate from the left of the number,
             * not the right, so someone printing the year 1972 with 2 digit 
             * precision gets "72" not "19".
             */
        case CHR_CUR_YEAR:
        case CHR_UP_YEAR:
            sprintf(safe_bfr, "%d", parsed_time->tm_year + 1900);
            break;

            /*
             * output month 
             */
        case CHR_CUR_MONTH:
        case CHR_UP_MONTH:
            sprintf(safe_bfr, "%d", parsed_time->tm_mon + 1);
            break;

            /*
             * output day of month 
             */
        case CHR_CUR_MDAY:
        case CHR_UP_MDAY:
            sprintf(safe_bfr, "%d", parsed_time->tm_mday);
            break;

            /*
             * output hour 
             */
        case CHR_CUR_HOUR:
        case CHR_UP_HOUR:
            sprintf(safe_bfr, "%d", parsed_time->tm_hour);
            break;

            /*
             * output minute 
             */
        case CHR_CUR_MIN:
        case CHR_UP_MIN:
            sprintf(safe_bfr, "%d", parsed_time->tm_min);
            break;

            /*
             * output second 
             */
        case CHR_CUR_SEC:
        case CHR_UP_SEC:
            sprintf(safe_bfr, "%d", parsed_time->tm_sec);
            break;

            /*
             * unknown format command - just output the character 
             */
        default:
            sprintf(safe_bfr, "%c", fmt_cmd);
        }
    }

    /*
     * Output with correct justification, leading zeroes, etc.  
     */
    return realloc_output_temp_bfr(buf, buf_len, out_len, allow_realloc,
                                   (u_char **) & safe_bfr, options);
}


static int
realloc_handle_ip_fmt(u_char ** buf, size_t * buf_len, size_t * out_len,
                      int allow_realloc,
                      options_type * options, netsnmp_pdu *pdu,
                      netsnmp_transport *transport)

     /*
      * Function:
      *     Handle a format command that deals with an IP address 
      * or host name.  Append the information to the buffer subject to
      * the buffer's length limit.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    options   - options governing how to write the field
      *    pdu       - information about this trap 
      *    transport - the transport descriptor
      */
{
    struct in_addr *agent_inaddr = (struct in_addr *) pdu->agent_addr;
    struct hostent *host = NULL;       /* corresponding host name */
    char            fmt_cmd = options->cmd;     /* what we're formatting */
    u_char         *temp_buf = NULL;
    size_t          temp_buf_len = 64, temp_out_len = 0;
    char           *tstr;
    unsigned int    oflags;

    if ((temp_buf = (u_char*)calloc(temp_buf_len, 1)) == NULL) {
        return 0;
    }

    /*
     * Decide exactly what to output.  
     */
    switch (fmt_cmd) {
    case CHR_AGENT_IP:
        /*
         * Write a numerical address.  
         */
        if (!snmp_strcat(&temp_buf, &temp_buf_len, &temp_out_len, 1,
                         (u_char *)inet_ntoa(*agent_inaddr))) {
            if (temp_buf != NULL) {
                free(temp_buf);
            }
            return 0;
        }
        break;

    case CHR_AGENT_NAME:
        /*
         * Try to resolve the agent_addr field as a hostname; fall back
         * to numerical address.  
         */
        if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
                                    NETSNMP_DS_APP_NUMERIC_IP)) {
            host = netsnmp_gethostbyaddr((char *) pdu->agent_addr, 4, AF_INET);
        }
        if (host != NULL) {
            if (!snmp_strcat(&temp_buf, &temp_buf_len, &temp_out_len, 1,
                             (const u_char *)host->h_name)) {
                if (temp_buf != NULL) {
                    free(temp_buf);
                }
                return 0;
            }
        } else {
            if (!snmp_strcat(&temp_buf, &temp_buf_len, &temp_out_len, 1,
                             (u_char *)inet_ntoa(*agent_inaddr))) {
                if (temp_buf != NULL) {
                    free(temp_buf);
                }
                return 0;
            }
        }
        break;

    case CHR_PDU_IP:
        /*
         * Write the numerical transport information.  
         */
        if (transport != NULL && transport->f_fmtaddr != NULL) {
            oflags = transport->flags;
            transport->flags &= ~NETSNMP_TRANSPORT_FLAG_HOSTNAME;
            tstr = transport->f_fmtaddr(transport, pdu->transport_data,
                                        pdu->transport_data_length);
            transport->flags = oflags;
          
            if (!tstr) goto noip;
            if (!snmp_strcat(&temp_buf, &temp_buf_len, &temp_out_len,
                             1, (u_char *)tstr)) {
                SNMP_FREE(temp_buf);
                SNMP_FREE(tstr);
                return 0;
            }
            SNMP_FREE(tstr);
        } else {
noip:
            if (!snmp_strcat(&temp_buf, &temp_buf_len, &temp_out_len, 1,
                             (const u_char*)"<UNKNOWN>")) {
                SNMP_FREE(temp_buf);
                return 0;
            }
        }
        break;

    case CHR_PDU_NAME:
        /*
         * Try to convert the numerical transport information
         *  into a hostname.  Or rather, have the transport-specific
         *  address formatting routine do this.
         * Otherwise falls back to the numeric address format.
         */
        if (transport != NULL && transport->f_fmtaddr != NULL) {
            oflags = transport->flags;
            if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
                                        NETSNMP_DS_APP_NUMERIC_IP))
                transport->flags |= NETSNMP_TRANSPORT_FLAG_HOSTNAME;
            tstr = transport->f_fmtaddr(transport, pdu->transport_data,
                                        pdu->transport_data_length);
            transport->flags = oflags;
          
            if (!tstr) goto nohost;
            if (!snmp_strcat(&temp_buf, &temp_buf_len, &temp_out_len,
                             1, (u_char *)tstr)) {
                SNMP_FREE(temp_buf);
                SNMP_FREE(tstr);
                return 0;
            }
            SNMP_FREE(tstr);
        } else {
nohost:
            if (!snmp_strcat(&temp_buf, &temp_buf_len, &temp_out_len, 1,
                             (const u_char*)"<UNKNOWN>")) {
                SNMP_FREE(temp_buf);
                return 0;
            }
        }
        break;

        /*
         * Don't know how to handle this command - write the character itself.  
         */
    default:
        temp_buf[0] = fmt_cmd;
    }

    /*
     * Output with correct justification, leading zeroes, etc.  
     */
    return realloc_output_temp_bfr(buf, buf_len, out_len, allow_realloc,
                                   &temp_buf, options);
}


static int
realloc_handle_ent_fmt(u_char ** buf, size_t * buf_len, size_t * out_len,
                       int allow_realloc,
                       options_type * options, netsnmp_pdu *pdu)

     /*
      * Function:
      *     Handle a format command that deals with OID strings. 
      * Append the information to the buffer subject to the
      * buffer's length limit.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    options - options governing how to write the field
      *    pdu     - information about this trap 
      */
{
    char            fmt_cmd = options->cmd;     /* what we're formatting */
    u_char         *temp_buf = NULL;
    size_t          temp_buf_len = 64, temp_out_len = 0;

    if ((temp_buf = (u_char *) calloc(temp_buf_len, 1)) == NULL) {
        return 0;
    }

    /*
     * Decide exactly what to output.  
     */
    switch (fmt_cmd) {
    case CHR_PDU_ENT:
        /*
         * Write the enterprise oid.  
         */
        if (!sprint_realloc_objid
            (&temp_buf, &temp_buf_len, &temp_out_len, 1, pdu->enterprise,
             pdu->enterprise_length)) {
            free(temp_buf);
            return 0;
        }
        break;

    case CHR_TRAP_CONTEXTID:
        /*
         * Write the context oid.  
         */
        if (!sprint_realloc_hexstring
            (&temp_buf, &temp_buf_len, &temp_out_len, 1, pdu->contextEngineID,
             pdu->contextEngineIDLen)) {
            free(temp_buf);
            return 0;
        }
        break;

        /*
         * Don't know how to handle this command - write the character itself.  
         */
    default:
        temp_buf[0] = fmt_cmd;
    }

    /*
     * Output with correct justification, leading zeroes, etc.  
     */
    return realloc_output_temp_bfr(buf, buf_len, out_len, allow_realloc,
                                   &temp_buf, options);
}


static int
realloc_handle_trap_fmt(u_char ** buf, size_t * buf_len, size_t * out_len,
                        int allow_realloc,
                        options_type * options, netsnmp_pdu *pdu)

     /*
      * Function:
      *     Handle a format command that deals with the trap itself. 
      * Append the information to the buffer subject to the buffer's 
      * length limit.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    options - options governing how to write the field
      *    pdu     - information about this trap 
      */
{
    netsnmp_variable_list *vars;        /* variables assoc with trap */
    char            fmt_cmd = options->cmd;     /* what we're outputting */
    u_char         *temp_buf = NULL;
    size_t          tbuf_len = 64, tout_len = 0;
    const char           *sep = separator;
    const char           *default_sep = "\t";
    const char           *default_alt_sep = ", ";

    if ((temp_buf = (u_char *) calloc(tbuf_len, 1)) == NULL) {
        return 0;
    }

    /*
     * Decide exactly what to output.  
     */
    switch (fmt_cmd) {
    case CHR_TRAP_NUM:
        /*
         * Write the trap's number.  
         */
        tout_len = sprintf((char*)temp_buf, "%ld", pdu->trap_type);
        break;

    case CHR_TRAP_DESC:
        /*
         * Write the trap's description.  
         */
        tout_len =
            sprintf((char*)temp_buf, "%s", trap_description(pdu->trap_type));
        break;

    case CHR_TRAP_STYPE:
        /*
         * Write the trap's subtype.  
         */
        if (pdu->trap_type != SNMP_TRAP_ENTERPRISESPECIFIC) {
            tout_len = sprintf((char*)temp_buf, "%ld", pdu->specific_type);
        } else {
            /*
             * Get object ID for the trap.  
             */
            size_t          obuf_len = 64, oout_len = 0, trap_oid_len = 0;
            oid             trap_oid[MAX_OID_LEN + 2] = { 0 };
            u_char         *obuf = NULL;
            char           *ptr = NULL;

            if ((obuf = (u_char *) calloc(obuf_len, 1)) == NULL) {
                free(temp_buf);
                return 0;
            }

            trap_oid_len = pdu->enterprise_length;
            memcpy(trap_oid, pdu->enterprise, trap_oid_len * sizeof(oid));
            if (trap_oid[trap_oid_len - 1] != 0) {
                trap_oid[trap_oid_len] = 0;
                trap_oid_len++;
            }
            trap_oid[trap_oid_len] = pdu->specific_type;
            trap_oid_len++;

            /*
             * Find the element after the last dot.  
             */
            if (!sprint_realloc_objid(&obuf, &obuf_len, &oout_len, 1,
                                      trap_oid, trap_oid_len)) {
                if (obuf != NULL) {
                    free(obuf);
                }
                free(temp_buf);
		return 0;
            }

            ptr = strrchr((char *) obuf, '.');
            if (ptr != NULL) {
                if (!snmp_strcat
                    (&temp_buf, &tbuf_len, &tout_len, 1, (u_char *) ptr)) {
                    free(obuf);
                    if (temp_buf != NULL) {
                        free(temp_buf);
                    }
                    return 0;
                }
                free(obuf);
            } else {
                free(temp_buf);
                temp_buf = obuf;
                tbuf_len = obuf_len;
                tout_len = oout_len;
            }
        }
        break;

    case CHR_TRAP_VARS:
        /*
         * Write the trap's variables.  
         */
        if (!sep || !*sep)
            sep = (options->alt_format ? default_alt_sep : default_sep);
        for (vars = pdu->variables; vars != NULL;
             vars = vars->next_variable) {
            /*
             * Print a separator between variables,
             *   (plus beforehand if the alt format is used)
             */
            if (options->alt_format ||
                vars != pdu->variables ) {
                if (!snmp_strcat(&temp_buf, &tbuf_len, &tout_len, 1, (const u_char *)sep)) {
                    if (temp_buf != NULL) {
                        free(temp_buf);
                    }
                    return 0;
                }
            }
            if (!sprint_realloc_variable
                (&temp_buf, &tbuf_len, &tout_len, 1, vars->name,
                 vars->name_length, vars)) {
                if (temp_buf != NULL) {
                    free(temp_buf);
                }
                return 0;
            }
        }
        break;

    default:
        /*
         * Don't know how to handle this command - write the character itself.  
         */
        temp_buf[0] = fmt_cmd;
    }

    /*
     * Output with correct justification, leading zeroes, etc.  
     */
    return realloc_output_temp_bfr(buf, buf_len, out_len, allow_realloc,
                                   &temp_buf, options);
}

static int
realloc_handle_auth_fmt(u_char ** buf, size_t * buf_len, size_t * out_len,
                        int allow_realloc,
                        options_type * options, netsnmp_pdu *pdu)
     /*
      * Function:
      *     Handle a format command that deals with authentication
      * information.
      * Append the information to the buffer subject to the buffer's 
      * length limit.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    options - options governing how to write the field
      *    pdu     - information about this trap 
      */
{
    char            fmt_cmd = options->cmd;     /* what we're outputting */
    u_char         *temp_buf = NULL;
    size_t          tbuf_len = 64;
    unsigned int    i;

    if ((temp_buf = (u_char*)calloc(tbuf_len, 1)) == NULL) {
        return 0;
    }

    switch (fmt_cmd) {

    case CHR_SNMP_VERSION:
        snprintf((char*)temp_buf, tbuf_len, "%ld", pdu->version);
        break;

    case CHR_SNMP_SECMOD:
        snprintf((char*)temp_buf, tbuf_len, "%d", pdu->securityModel);
        break;

    case CHR_SNMP_USER:
        switch ( pdu->version ) {
#ifndef NETSNMP_DISABLE_SNMPV1
        case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
        case SNMP_VERSION_2c:
#endif
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
            while ((*out_len + pdu->community_len + 1) >= *buf_len) {
                if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                    if (temp_buf)
                        free(temp_buf);
                    return 0;
                }
            }

            for (i = 0; i < pdu->community_len; i++) {
                if (isprint(pdu->community[i])) {
                    *(*buf + *out_len) = pdu->community[i];
                } else {
                    *(*buf + *out_len) = '.';
                }
                (*out_len)++;
            }
            *(*buf + *out_len) = '\0';
            break;
#endif
        default:
            snprintf((char*)temp_buf, tbuf_len, "%s", pdu->securityName);
        }
        break;

    default:
        /*
         * Don't know how to handle this command - write the character itself.  
         */
        temp_buf[0] = fmt_cmd;
    }

    /*
     * Output with correct justification, leading zeroes, etc.  
     */
    return realloc_output_temp_bfr(buf, buf_len, out_len, allow_realloc,
                                   &temp_buf, options);
}

static int
realloc_handle_wrap_fmt(u_char ** buf, size_t * buf_len, size_t * out_len,
                        int allow_realloc, netsnmp_pdu *pdu)
{
    size_t          i = 0;

    switch (pdu->command) {
    case SNMP_MSG_TRAP:
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) "TRAP")) {
            return 0;
        }
        break;
    case SNMP_MSG_TRAP2:
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) "TRAP2")) {
            return 0;
        }
        break;
    case SNMP_MSG_INFORM:
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) "INFORM")) {
            return 0;
        }
        break;
    }

    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) ", SNMP v1")) {
            return 0;
        }
        break;
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) ", SNMP v2c")) {
            return 0;
        }
        break;
#endif
    case SNMP_VERSION_3:
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) ", SNMP v3")) {
            return 0;
        }
        break;
    }

    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
#endif
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) ", community ")) {
            return 0;
        }

        while ((*out_len + pdu->community_len + 1) >= *buf_len) {
            if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                return 0;
            }
        }

        for (i = 0; i < pdu->community_len; i++) {
            if (isprint(pdu->community[i])) {
                *(*buf + *out_len) = pdu->community[i];
            } else {
                *(*buf + *out_len) = '.';
            }
            (*out_len)++;
        }
        *(*buf + *out_len) = '\0';
        break;
#endif
    case SNMP_VERSION_3:
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) ", user ")) {
            return 0;
        }

        while ((*out_len + pdu->securityNameLen + 1) >= *buf_len) {
            if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                return 0;
            }
        }

        for (i = 0; i < pdu->securityNameLen; i++) {
            if (isprint((unsigned char)(pdu->securityName[i]))) {
                *(*buf + *out_len) = pdu->securityName[i];
            } else {
                *(*buf + *out_len) = '.';
            }
            (*out_len)++;
        }
        *(*buf + *out_len) = '\0';

        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) ", context ")) {
            return 0;
        }

        while ((*out_len + pdu->contextNameLen + 1) >= *buf_len) {
            if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                return 0;
            }
        }

        for (i = 0; i < pdu->contextNameLen; i++) {
            if (isprint((unsigned char)(pdu->contextName[i]))) {
                *(*buf + *out_len) = pdu->contextName[i];
            } else {
                *(*buf + *out_len) = '.';
            }
            (*out_len)++;
        }
        *(*buf + *out_len) = '\0';
    }
    return 1;
}


static int
realloc_dispatch_format_cmd(u_char ** buf, size_t * buf_len,
                            size_t * out_len, int allow_realloc,
                            options_type * options, netsnmp_pdu *pdu,
                            netsnmp_transport *transport)

     /*
      * Function:
      *     Dispatch a format command to the appropriate command handler.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    options   - options governing how to write the field
      *    pdu       - information about this trap
      *    transport - the transport descriptor
      */
{
    char            fmt_cmd = options->cmd;     /* for speed */

    /*
     * choose the appropriate command handler 
     */

    if (is_cur_time_cmd(fmt_cmd) || is_up_time_cmd(fmt_cmd)) {
        return realloc_handle_time_fmt(buf, buf_len, out_len,
                                       allow_realloc, options, pdu);
    } else if (is_agent_cmd(fmt_cmd) || is_pdu_ip_cmd(fmt_cmd)) {
        return realloc_handle_ip_fmt(buf, buf_len, out_len, allow_realloc,
                                     options, pdu, transport);
    } else if (is_trap_cmd(fmt_cmd)) {
        return realloc_handle_trap_fmt(buf, buf_len, out_len,
                                       allow_realloc, options, pdu);
    } else if (is_auth_cmd(fmt_cmd)) {
        return realloc_handle_auth_fmt(buf, buf_len, out_len,
                                       allow_realloc, options, pdu);
    } else if (fmt_cmd == CHR_PDU_ENT || fmt_cmd == CHR_TRAP_CONTEXTID) {
        return realloc_handle_ent_fmt(buf, buf_len, out_len, allow_realloc,
                                      options, pdu);
    } else if (fmt_cmd == CHR_PDU_WRAP) {
        return realloc_handle_wrap_fmt(buf, buf_len, out_len,
                                       allow_realloc, pdu);
    } else {
        /*
         * unknown format command - just output the character 
         */
        char            fmt_cmd_string[2] = { 0, 0 };
        fmt_cmd_string[0] = fmt_cmd;

        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) fmt_cmd_string);
    }
}


static int
realloc_handle_backslash(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc, char fmt_cmd)

     /*
      * Function:
      *     Handle a character following a backslash. Append the resulting 
      * character to the buffer subject to the buffer's length limit.
      *     This routine currently isn't sophisticated enough to handle
      * \nnn or \xhh formats.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    fmt_cmd - the character after the backslash
      */
{
    char            temp_bfr[3];        /* for bulding temporary strings */

    /*
     * select the proper output character(s) 
     */
    switch (fmt_cmd) {
    case 'a':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\a");
    case 'b':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\b");
    case 'f':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\f");
    case 'n':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\n");
    case 'r':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\r");
    case 't':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\t");
    case 'v':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\v");
    case '\\':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\\");
    case '?':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "?");
    case '%':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "%");
    case '\'':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\'");
    case '"':
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) "\"");
    default:
        sprintf(temp_bfr, "\\%c", fmt_cmd);
        return snmp_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *) temp_bfr);
    }
}


int
realloc_format_plain_trap(u_char ** buf, size_t * buf_len,
                          size_t * out_len, int allow_realloc,
                          netsnmp_pdu *pdu, netsnmp_transport *transport)

     /*
      * Function:
      *    Format the trap information in the default way and put the results
      * into the buffer, truncating at the buffer's length limit. This
      * routine returns 1 if the output was completed successfully or
      * 0 if it is truncated due to a memory allocation failure.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    pdu       - the pdu information
      *    transport - the transport descriptor
      */
{
    time_t          now;        /* the current time */
    struct tm      *now_parsed; /* time in struct format */
    char            safe_bfr[200];      /* holds other strings */
    struct in_addr *agent_inaddr = (struct in_addr *) pdu->agent_addr;
    struct hostent *host = NULL;       /* host name */
    netsnmp_variable_list *vars;        /* variables assoc with trap */

    if (buf == NULL) {
        return 0;
    }

    /*
     * Print the current time. Since we don't know how long the buffer is,
     * and snprintf isn't yet standard, build the timestamp in a separate
     * buffer of guaranteed length and then copy it to the output buffer.
     */
    time(&now);
    now_parsed = localtime(&now);
    sprintf(safe_bfr, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d ",
            now_parsed->tm_year + 1900, now_parsed->tm_mon + 1,
            now_parsed->tm_mday, now_parsed->tm_hour,
            now_parsed->tm_min, now_parsed->tm_sec);
    if (!snmp_strcat
        (buf, buf_len, out_len, allow_realloc,
         (const u_char *) safe_bfr)) {
        return 0;
    }

    /*
     * Get info about the sender.  
     */
    if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
                                NETSNMP_DS_APP_NUMERIC_IP)) {
        host = netsnmp_gethostbyaddr((char *) pdu->agent_addr, 4, AF_INET);
    }
    if (host != (struct hostent *) NULL) {
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) host->h_name)) {
            return 0;
        }
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) " [")) {
            return 0;
        }
        if (!snmp_strcat(buf, buf_len, out_len, allow_realloc,
                         (const u_char *) inet_ntoa(*agent_inaddr))) {
            return 0;
        }
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) "] ")) {
            return 0;
        }
    } else {
        if (!snmp_strcat(buf, buf_len, out_len, allow_realloc,
                         (const u_char *) inet_ntoa(*agent_inaddr))) {
            return 0;
        }
    }

    /*
     * Append PDU transport info.  
     */
    if (transport != NULL && transport->f_fmtaddr != NULL) {
        char           *tstr =
            transport->f_fmtaddr(transport, pdu->transport_data,
                                 pdu->transport_data_length);
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) "(via ")) {
            if (tstr != NULL) {
                free(tstr);
            }
            return 0;
        }
        if (!snmp_strcat(buf, buf_len, out_len, allow_realloc, (u_char *)tstr)) {
            if (tstr != NULL) {
                free(tstr);
            }
            return 0;
        }
        if (tstr != NULL) {
            free(tstr);
        }
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) ") ")) {
            return 0;
        }
    }

    /*
     * Add security wrapper information.  
     */
    if (!realloc_handle_wrap_fmt
        (buf, buf_len, out_len, allow_realloc, pdu)) {
        return 0;
    }

    if (!snmp_strcat
        (buf, buf_len, out_len, allow_realloc, (const u_char *) "\n\t")) {
        return 0;
    }

    /*
     * Add enterprise information.  
     */
    if (!sprint_realloc_objid(buf, buf_len, out_len, allow_realloc,
                              pdu->enterprise, pdu->enterprise_length)) {
        return 0;
    }

    if (!snmp_strcat
        (buf, buf_len, out_len, allow_realloc, (const u_char *) " ")) {
        return 0;
    }
    if (!snmp_strcat(buf, buf_len, out_len, allow_realloc,
                     (const u_char *)trap_description(pdu->trap_type))) {
        return 0;
    }
    if (!snmp_strcat
        (buf, buf_len, out_len, allow_realloc,
         (const u_char *) " Trap (")) {
        return 0;
    }

    /*
     * Handle enterprise specific traps.  
     */
    if (pdu->trap_type == SNMP_TRAP_ENTERPRISESPECIFIC) {
        size_t          obuf_len = 64, oout_len = 0, trap_oid_len = 0;
        oid             trap_oid[MAX_OID_LEN + 2] = { 0 };
        char           *ent_spec_code = NULL;
        u_char         *obuf = NULL;

        if ((obuf = (u_char *) calloc(obuf_len, 1)) == NULL) {
            return 0;
        }

        /*
         * Get object ID for the trap.  
         */
        trap_oid_len = pdu->enterprise_length;
        memcpy(trap_oid, pdu->enterprise, trap_oid_len * sizeof(oid));
        if (trap_oid[trap_oid_len - 1] != 0) {
            trap_oid[trap_oid_len] = 0;
            trap_oid_len++;
        }
        trap_oid[trap_oid_len] = pdu->specific_type;
        trap_oid_len++;

        /*
         * Find the element after the last dot.  
         */
        if (!sprint_realloc_objid(&obuf, &obuf_len, &oout_len, 1,
                                  trap_oid, trap_oid_len)) {
            if (obuf != NULL) {
                free(obuf);
            }
            return 0;
        }
        ent_spec_code = strrchr((char *) obuf, '.');
        if (ent_spec_code != NULL) {
            ent_spec_code++;
        } else {
            ent_spec_code = (char *) obuf;
        }

        /*
         * Print trap info.  
         */
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) ent_spec_code)) {
            free(obuf);
            return 0;
        }
        free(obuf);
    } else {
        /*
         * Handle traps that aren't enterprise specific.  
         */
        sprintf(safe_bfr, "%ld", pdu->specific_type);
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) safe_bfr)) {
            return 0;
        }
    }

    /*
     * Finish the line.  
     */
    if (!snmp_strcat
        (buf, buf_len, out_len, allow_realloc,
         (const u_char *) ") Uptime: ")) {
        return 0;
    }
    if (!snmp_strcat(buf, buf_len, out_len, allow_realloc,
                     (const u_char *) uptime_string(pdu->time,
                                                    safe_bfr))) {
        return 0;
    }
    if (!snmp_strcat
        (buf, buf_len, out_len, allow_realloc, (const u_char *) "\n")) {
        return 0;
    }

    /*
     * Finally, output the PDU variables. 
     */
    for (vars = pdu->variables; vars != NULL; vars = vars->next_variable) {
        if (!snmp_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) "\t")) {
            return 0;
        }
        if (!sprint_realloc_variable(buf, buf_len, out_len, allow_realloc,
                                     vars->name, vars->name_length,
                                     vars)) {
            return 0;
        }
    }
    if (!snmp_strcat
        (buf, buf_len, out_len, allow_realloc, (const u_char *) "\n")) {
        return 0;
    }

    /*
     * String is already null-terminated.  That's all folks!  
     */
    return 1;
}


int
realloc_format_trap(u_char ** buf, size_t * buf_len, size_t * out_len,
                    int allow_realloc, const char *format_str,
                    netsnmp_pdu *pdu, netsnmp_transport *transport)

     /*
      * Function:
      *    Format the trap information for display in a log. Place the results
      *    in the specified buffer (truncating to the length of the buffer).
      *    Returns the number of characters it put in the buffer.
      *
      * Input Parameters:
      *    buf, buf_len, out_len, allow_realloc - standard relocatable
      *                                           buffer parameters
      *    format_str - specifies how to format the trap info
      *    pdu        - the pdu information
      *    transport  - the transport descriptor
      */
{
    unsigned long   fmt_idx = 0;        /* index into the format string */
    options_type    options;    /* formatting options */
    parse_state_type state = PARSE_NORMAL;      /* state of the parser */
    char            next_chr;   /* for speed */
    int             reset_options = TRUE;       /* reset opts on next NORMAL state */

    if (buf == NULL) {
        return 0;
    }

    memset(separator, 0, sizeof(separator));
    /*
     * Go until we reach the end of the format string:  
     */
    for (fmt_idx = 0; format_str[fmt_idx] != '\0'; fmt_idx++) {
        next_chr = format_str[fmt_idx];
        switch (state) {
        case PARSE_NORMAL:
            /*
             * Looking for next character.  
             */
            if (reset_options) {
                init_options(&options);
                reset_options = FALSE;
            }
            if (next_chr == '\\') {
                state = PARSE_BACKSLASH;
            } else if (next_chr == CHR_FMT_DELIM) {
                state = PARSE_IN_FORMAT;
            } else {
                if ((*out_len + 1) >= *buf_len) {
                    if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                        return 0;
                    }
                }
                *(*buf + *out_len) = next_chr;
                (*out_len)++;
            }
            break;

        case PARSE_GET_SEPARATOR:
            /*
             * Parse the separator character
             * XXX - Possibly need to handle quoted strings ??
             */
	    {   char *sep = separator;
		size_t i, j;
		i = sizeof(separator);
		j = 0;
		memset(separator, 0, i);
		while (j < i && next_chr && next_chr != CHR_FMT_DELIM) {
		    if (next_chr == '\\') {
			/*
			 * Handle backslash interpretation
			 * Print to "separator" string rather than the output buffer
			 *    (a bit of a hack, but it should work!)
			 */
			next_chr = format_str[++fmt_idx];
			if (!realloc_handle_backslash
			    ((u_char **)&sep, &i, &j, 0, next_chr)) {
			    return 0;
			}
		    } else {
			separator[j++] = next_chr;
		    }
		    next_chr = format_str[++fmt_idx];
		}
	    }
            state = PARSE_IN_FORMAT;
            break;

        case PARSE_BACKSLASH:
            /*
             * Found a backslash.  
             */
            if (!realloc_handle_backslash
                (buf, buf_len, out_len, allow_realloc, next_chr)) {
                return 0;
            }
            state = PARSE_NORMAL;
            break;

        case PARSE_IN_FORMAT:
            /*
             * In a format command.  
             */
            reset_options = TRUE;
            if (next_chr == CHR_LEFT_JUST) {
                options.left_justify = TRUE;
            } else if (next_chr == CHR_LEAD_ZERO) {
                options.leading_zeroes = TRUE;
            } else if (next_chr == CHR_ALT_FORM) {
                options.alt_format = TRUE;
            } else if (next_chr == CHR_FIELD_SEP) {
                state = PARSE_GET_PRECISION;
            } else if (next_chr == CHR_TRAP_VARSEP) {
                state = PARSE_GET_SEPARATOR;
            } else if ((next_chr >= '1') && (next_chr <= '9')) {
                options.width =
                    ((unsigned long) next_chr) - ((unsigned long) '0');
                state = PARSE_GET_WIDTH;
            } else if (is_fmt_cmd(next_chr)) {
                options.cmd = next_chr;
                if (!realloc_dispatch_format_cmd
                    (buf, buf_len, out_len, allow_realloc, &options, pdu,
                     transport)) {
                    return 0;
                }
                state = PARSE_NORMAL;
            } else {
                if ((*out_len + 1) >= *buf_len) {
                    if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                        return 0;
                    }
                }
                *(*buf + *out_len) = next_chr;
                (*out_len)++;
                state = PARSE_NORMAL;
            }
            break;

        case PARSE_GET_WIDTH:
            /*
             * Parsing a width field.  
             */
            reset_options = TRUE;
            if (isdigit((unsigned char)(next_chr))) {
                options.width *= 10;
                options.width +=
                    (unsigned long) next_chr - (unsigned long) '0';
            } else if (next_chr == CHR_FIELD_SEP) {
                state = PARSE_GET_PRECISION;
            } else if (is_fmt_cmd(next_chr)) {
                options.cmd = next_chr;
                if (!realloc_dispatch_format_cmd
                    (buf, buf_len, out_len, allow_realloc, &options, pdu,
                     transport)) {
                    return 0;
                }
                state = PARSE_NORMAL;
            } else {
                if ((*out_len + 1) >= *buf_len) {
                    if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                        return 0;
                    }
                }
                *(*buf + *out_len) = next_chr;
                (*out_len)++;
                state = PARSE_NORMAL;
            }
            break;

        case PARSE_GET_PRECISION:
            /*
             * Parsing a precision field.  
             */
            reset_options = TRUE;
            if (isdigit((unsigned char)(next_chr))) {
                if (options.precision == UNDEF_PRECISION) {
                    options.precision =
                        (unsigned long) next_chr - (unsigned long) '0';
                } else {
                    options.precision *= 10;
                    options.precision +=
                        (unsigned long) next_chr - (unsigned long) '0';
                }
            } else if (is_fmt_cmd(next_chr)) {
                options.cmd = next_chr;
                if ((options.precision != UNDEF_PRECISION) &&
                    (options.width < (size_t)options.precision)) {
                    options.width = (size_t)options.precision;
                }
                if (!realloc_dispatch_format_cmd
                    (buf, buf_len, out_len, allow_realloc, &options, pdu,
                     transport)) {
                    return 0;
                }
                state = PARSE_NORMAL;
            } else {
                if ((*out_len + 1) >= *buf_len) {
                    if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                        return 0;
                    }
                }
                *(*buf + *out_len) = next_chr;
                (*out_len)++;
                state = PARSE_NORMAL;
            }
            break;

        default:
            /*
             * Unknown state.  
             */
            reset_options = TRUE;
            if ((*out_len + 1) >= *buf_len) {
                if (!(allow_realloc && snmp_realloc(buf, buf_len))) {
                    return 0;
                }
            }
            *(*buf + *out_len) = next_chr;
            (*out_len)++;
            state = PARSE_NORMAL;
        }
    }

    *(*buf + *out_len) = '\0';
    return 1;
}
