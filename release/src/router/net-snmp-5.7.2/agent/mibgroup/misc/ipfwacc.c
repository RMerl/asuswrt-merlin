/****************************************************************************
 * Module for ucd-snmpd reading IP Firewall accounting rules.               *
 * It reads "/proc/net/ip_acct". If the file has a wrong format it silently *
 * returns erroneous data but doesn't do anything harmfull. Based (on the   *
 * output of) mib2c, wombat.c, proc.c and the Linux kernel.                 *
 * Author: Cristian.Estan@net.utcluj.ro                                     *
 ***************************************************************************/

#include <net-snmp/net-snmp-config.h>

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

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "util_funcs/header_simple_table.h"
#include "ipfwacc.h"

/*
 * According to the 2.0.33 Linux kernel, assuming we use ipv4 any line from
 * * "/proc/net/ip_acct should fit into
 * * 8+1+8+2+8+1+8+1+16+1+8+1+4+1+2+1+2+1+20+20+10*(1+5)+2+2+2+2=182
 * * characters+ newline.
 */
#define IPFWRULELEN 200

#define IP_FW_F_ALL     0x0000  /* This is a universal packet firewall */
#define IP_FW_F_TCP     0x0001  /* This is a TCP packet firewall      */
#define IP_FW_F_UDP     0x0002  /* This is a UDP packet firewall      */
#define IP_FW_F_ICMP    0x0003  /* This is a ICMP packet firewall     */
#define IP_FW_F_KIND    0x0003  /* Mask to isolate firewall kind      */
#define IP_FW_F_SRNG    0x0008  /* The first two src ports are a min  *
                                 * and max range (stored in host byte *
                                 * order).                            */
#define IP_FW_F_DRNG    0x0010  /* The first two dst ports are a min  *
                                 * and max range (stored in host byte *
                                 * order).                            *
                                 * (ports[0] <= port <= ports[1])     */
#define IP_FW_F_BIDIR   0x0040  /* For bidirectional firewalls        */
#define IP_FW_F_ACCTIN  0x1000  /* Account incoming packets only.     */
#define IP_FW_F_ACCTOUT 0x2000  /* Account outgoing packets only.     */

static unsigned char rule[IPFWRULELEN]; /*Buffer for reading a line from
                                         * /proc/net/ip_acct. Care has been taken
                                         * not to read beyond the end of this 
                                         * buffer, even if rules are in an 
                                         * unexpected format
                                         */

/*
 * This function reads the rule with the given number into the buffer. It
 * * returns the number of rule read or 0 if the number is invalid or other
 * * problems occur. If the argument is 0 it returns the number of accounting
 * * rules. No caching of rules is done.
 */

static int
readrule(unsigned int number)
{
    int             i;
    FILE           *f = fopen("/proc/net/ip_acct", "rt");

    if (!f)
        return 0;
    /*
     * get rid of "IP accounting rules" line
     */
    if (!fgets((char *) rule, sizeof(rule), f)) {
        fclose(f);
        return 0;
    }
    for (i = 1; i != number; i++)
        if (!fgets((char *) rule, sizeof(rule), f)) {
            fclose(f);
            return (number ? 0 : (i - 1));
        }
    if (!fgets((char *) rule, sizeof(rule), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return i;
}

static unsigned long ret_val;   /* Used by var_ipfwacc to return ulongs */

/*
 * This function converts the hexadecimal representation of an IP address from
 * * the rule buffer to an unsigned long. The result is stored in the ret_val
 * * variable. The parameter indicates the position where the address starts. It
 * * only works with uppercase letters and assumes input is correct. Had to use
 * * this because stol returns a signed long. 
 */

static inline void
atoip(int pos)
{
    int             i;

    ret_val = 0;
    for (i = 0; i < 32; i += 8) {
        unsigned long   value = (((rule[pos]) >= '0' && rule[pos] <= '9') ?
                                 rule[pos] - '0' : rule[pos] - 'A' + 10);
        pos++;
        value = (value << 4) + (((rule[pos]) >= '0' && rule[pos] <= '9') ?
                                rule[pos] - '0' : rule[pos] - 'A' + 10);
        pos++;
        ret_val |= (value << i);
    }
}

/*
 * This function parses the flags field from the line in the buffer 
 */

static unsigned long int
getflags(void)
{
    unsigned long int flags;
    int             i = 37;     /* position in the rule */

    /*
     * skipping via name 
     */
    while (rule[i] != ' ' && i < IPFWRULELEN - 12)
        i++;
    /*
     * skipping via address 
     */
    i += 10;
    for (flags = 0; rule[i] != ' ' && i < IPFWRULELEN - 1; i++) {
        int             value = (((rule[i]) >= '0' && rule[i] <= '9') ?
                                 rule[i] - '0' : rule[i] - 'A' + 10);
        flags = (flags << 4) + value;
    }
    return flags;
}

/*
 * This function reads into ret_val a field from the rule buffer. The field
 * * is a base 10 long integer and the parameter skip tells us how many fields
 * * to skip after the "via addrress" field (including the flag field)
 */

static void
getnumeric(int skip)
{
    int             i = 37;     /* position in the rule */

    /*
     * skipping via name 
     */
    while (rule[i] != ' ' && i < IPFWRULELEN - 12)
        i++;
    /*
     * skipping via address 
     */
    i += 10;
    while (skip > 0) {
        skip--;
        /*
         * skipping field, than subsequent spaces 
         */
        while (rule[i] != ' ' && i < IPFWRULELEN - 2)
            i++;
        while (rule[i] == ' ' && i < IPFWRULELEN - 1)
            i++;
    }
    for (ret_val = 0; rule[i] != ' ' && i < IPFWRULELEN - 1; i++)
        ret_val = ret_val * 10 + rule[i] - '0';
}

/*
 * this variable defines function callbacks and type return information 
 * for the ipfwaccounting mib 
 */

struct variable2 ipfwacc_variables[] = {
    {IPFWACCINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCINDEX}},
    {IPFWACCSRCADDR, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCSRCADDR}},
    {IPFWACCSRCNM, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCSRCNM}},
    {IPFWACCDSTADDR, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCDSTADDR}},
    {IPFWACCDSTNM, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCDSTNM}},
    {IPFWACCVIANAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCVIANAME}},
    {IPFWACCVIAADDR, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCVIAADDR}},
    {IPFWACCPROTO, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPROTO}},
    {IPFWACCBIDIR, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCBIDIR}},
    {IPFWACCDIR, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCDIR}},
    {IPFWACCBYTES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCBYTES}},
    {IPFWACCPACKETS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPACKETS}},
    {IPFWACCNSRCPRTS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCNSRCPRTS}},
    {IPFWACCNDSTPRTS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCNDSTPRTS}},
    {IPFWACCSRCISRNG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCSRCISRNG}},
    {IPFWACCDSTISRNG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCDSTISRNG}},
    {IPFWACCPORT1, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT1}},
    {IPFWACCPORT2, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT2}},
    {IPFWACCPORT3, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT3}},
    {IPFWACCPORT4, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT4}},
    {IPFWACCPORT5, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT5}},
    {IPFWACCPORT6, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT6}},
    {IPFWACCPORT7, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT7}},
    {IPFWACCPORT8, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT8}},
    {IPFWACCPORT9, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT9}},
    {IPFWACCPORT10, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ipfwacc, 1, {IPFWACCPORT10}}
};

oid             ipfwacc_variables_oid[] =
    { 1, 3, 6, 1, 4, 1, 2021, 13, 1, 1, 1 };

void
init_ipfwacc(void)
{
    REGISTER_MIB("misc/ipfwacc", ipfwacc_variables, variable2,
                 ipfwacc_variables_oid);
}


u_char         *
var_ipfwacc(struct variable *vp,
            oid * name,
            size_t * length,
            int exact, size_t * var_len, WriteMethod ** write_method)
{
    *write_method = 0;          /* assume it isnt writable for the time being */
    *var_len = sizeof(ret_val); /* assume an integer and change later if not */

    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, readrule(0)))
        return (NULL);

    if (readrule(name[*length - 1])) {
        /*
         * this is where we do the value assignments for the mib results. 
         */
        switch (vp->magic) {
        case IPFWACCINDEX:
            ret_val = name[*length - 1];
            return ((u_char *) (&ret_val));
        case IPFWACCSRCADDR:
            atoip(0);
            return ((u_char *) (&ret_val));
        case IPFWACCSRCNM:
            atoip(9);
            return ((u_char *) (&ret_val));
        case IPFWACCDSTADDR:
            atoip(19);
            return ((u_char *) (&ret_val));
        case IPFWACCDSTNM:
            atoip(28);
            return ((u_char *) (&ret_val));
        case IPFWACCVIANAME:
            {
                int             i = 37; /* position in the rule */
                while (rule[i] != ' ' && i < IPFWRULELEN - 1)
                    i++;
                rule[i] = 0;
                return (rule + 37);
            }
        case IPFWACCVIAADDR:
            {
                int             i = 37; /* position in the rule */
                while (rule[i] != ' ' && i < IPFWRULELEN - 9)
                    i++;
                atoip(i + 1);
                return ((u_char *) (&ret_val));
            }
        case IPFWACCPROTO:
            switch (getflags() & IP_FW_F_KIND) {
            case IP_FW_F_ALL:
                ret_val = 2;
                return ((u_char *) (&ret_val));
            case IP_FW_F_TCP:
                ret_val = 3;
                return ((u_char *) (&ret_val));
            case IP_FW_F_UDP:
                ret_val = 4;
                return ((u_char *) (&ret_val));
            case IP_FW_F_ICMP:
                ret_val = 5;
                return ((u_char *) (&ret_val));
            default:
                ret_val = 1;
                return ((u_char *) (&ret_val));
            }
        case IPFWACCBIDIR:
            ret_val = ((getflags() & IP_FW_F_BIDIR) ? 2 : 1);
            return ((u_char *) (&ret_val));
        case IPFWACCDIR:
            ret_val = (getflags() & (IP_FW_F_ACCTIN | IP_FW_F_ACCTOUT));
            if (ret_val == IP_FW_F_ACCTIN)
                ret_val = 2;
            else if (ret_val == IP_FW_F_ACCTOUT)
                ret_val = 3;
            else
                ret_val = 1;
            return ((u_char *) (&ret_val));
        case IPFWACCBYTES:
            getnumeric(4);
            return ((u_char *) (&ret_val));
        case IPFWACCPACKETS:
            getnumeric(3);
            return ((u_char *) (&ret_val));
        case IPFWACCNSRCPRTS:
            getnumeric(1);
            return ((u_char *) (&ret_val));
        case IPFWACCNDSTPRTS:
            getnumeric(2);
            return ((u_char *) (&ret_val));
        case IPFWACCSRCISRNG:
            ret_val = ((getflags() & IP_FW_F_SRNG) ? 1 : 2);
            return ((u_char *) (&ret_val));
        case IPFWACCDSTISRNG:
            ret_val = ((getflags() & IP_FW_F_DRNG) ? 1 : 2);
            return ((u_char *) (&ret_val));
        case IPFWACCPORT1:
        case IPFWACCPORT2:
        case IPFWACCPORT3:
        case IPFWACCPORT4:
        case IPFWACCPORT5:
        case IPFWACCPORT6:
        case IPFWACCPORT7:
        case IPFWACCPORT8:
        case IPFWACCPORT9:
        case IPFWACCPORT10:
            getnumeric(5 + (vp->magic) - IPFWACCPORT1);
            return ((u_char *) (&ret_val));
        }
    }
    return NULL;
}
