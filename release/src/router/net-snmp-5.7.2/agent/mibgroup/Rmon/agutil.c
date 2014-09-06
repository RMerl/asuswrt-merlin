/**************************************************************
 * Copyright (C) 2001 Alex Rozin, Optical Access 
 *
 *                     All Rights Reserved
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 * 
 * ALEX ROZIN DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * ALEX ROZIN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 ******************************************************************/

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "agutil.h"
#include "agutil_api.h"

#ifndef OPTICALL_ACESS          /* in OpticalAccess case : find them in ~agent/mibgroup/nbase directory */
void
ag_trace(const char *format, ...)
{
#define AG_MAX_MSG_LEN  120
    char            msg[AG_MAX_MSG_LEN];
    va_list         args;

    /*
     * create msg 
     */
    va_start(args, format);
    vsnprintf(msg, AG_MAX_MSG_LEN - 1, format, args);

    snmp_log(LOG_INFO, "%s\n", msg);
    va_end(args);
}

int
AGUTIL_advance_index_name(struct variable *vp, oid * name,
                          size_t * length, int exact)
{
    int             result;

    if (exact)
        return 0;

    if (*length <= vp->namelen) {
        result =
            snmp_oid_compare(name, *length, vp->name, (int) vp->namelen);
        memcpy((char *) name, (char *) vp->name,
               ((int) vp->namelen) * sizeof(oid));
        *length = vp->namelen;
    } else {
        /*
         * If the name is given with indexes - compare only the oids. 
         */
        result =
            snmp_oid_compare(name, (int) vp->namelen, vp->name,
                             (int) vp->namelen);
        /*
         * If it's not the same oid - change name to the new oid 
         */
        if (result < 0) {
            memcpy((char *) name, (char *) vp->name,
                   ((int) vp->namelen) * sizeof(oid));
            *length = vp->namelen;
        }
    }

    if (result > 0) {
        ag_trace("*length=%d result=%d !!!", (int) *length, (int) result);
        return -1;
    }
    return 0;
}

/*********************************************************************
 * Function: AGUTIL_get_int_value
 *
 * Description:
 *   Check/Get long value from the PDU..
 *   The parameters 'min_value' & 'max_value' allow to check the diaposon
 *   of the value. If (max_value <= min_value) we avoid this checking.
 *
 * Returns:
 *   SNMP_ERR_NOERROR
 *   SNMP_ERR_WRONGTYPE
 *   SNMP_ERR_WRONGLENGTH
 *   SNMP_ERR_WRONGENCODING
 *   SNMP_ERR_BADVALUE
 *********************************************************************/
int
AGUTIL_get_int_value(u_char * var_val, u_char var_val_type,
                     size_t var_val_len, long min_value, long max_value,
                     long *long_tmp)
{
    if (var_val_type != ASN_INTEGER && var_val_type != ASN_TIMETICKS) {
        ag_trace("not ASN_INTEGER 0x%lx", (long) var_val_type);
        return SNMP_ERR_WRONGTYPE;
    }

    if (var_val_len > sizeof(long)) {
        ag_trace("wrong len=%d", (int) var_val_len);
        return SNMP_ERR_WRONGLENGTH;
    }

    *long_tmp = *((long *) var_val);

    if (max_value > min_value) {
        if (*long_tmp < min_value) {
            ag_trace("%ld=long_tmp < min=%ld", (long) *long_tmp,
                     (long) min_value);
            return SNMP_ERR_BADVALUE;
        }

        if (*long_tmp > max_value) {
            ag_trace("%ld=long_tmp > max=%ld", (long) *long_tmp,
                     (long) max_value);
            return SNMP_ERR_BADVALUE;
        }
    }

    return SNMP_ERR_NOERROR;
}


/*********************************************************************
 * Function: AGUTIL_get_string_value
 *
 * Description:
 *   Check/Get 'DisplayString' value from the PDU..
 *
 * Returns:
 *   SNMP_ERR_NOERROR
 *   SNMP_ERR_WRONGTYPE
 *   SNMP_ERR_WRONGLENGTH
 *   SNMP_ERR_WRONGENCODING
 *   SNMP_ERR_BADVALUE
 *********************************************************************/
int
AGUTIL_get_string_value(u_char * var_val, u_char var_val_type,
                        size_t var_val_len, size_t buffer_max_size,
                        u_char should_zero_limited,
                        size_t * buffer_actual_size, char *buffer)
{
    if (var_val_type != ASN_OCTET_STR) {
        ag_trace("not ASN_OCTET_STR 0x%lx", (long) var_val_type);
        return SNMP_ERR_WRONGTYPE;
    }

    if (var_val_len > buffer_max_size) {
        ag_trace("wrong len=%d > %d", (int) var_val_len, buffer_max_size);
        return SNMP_ERR_WRONGLENGTH;
    }

    if (buffer_actual_size)
        *buffer_actual_size = var_val_len;

    memcpy(buffer, var_val, var_val_len);
    if (should_zero_limited) {
        buffer[var_val_len] = 0;
        if (buffer_actual_size)
            *buffer_actual_size += 1;
    }

    return SNMP_ERR_NOERROR;
}

#endif

int
AGUTIL_get_oid_value(u_char * var_val, u_char var_val_type,
                     size_t var_val_len, VAR_OID_T * data_source_ptr)
{
    register int    iii;
    register oid   *oid_var;

    if (var_val_len > MAX_OID_LEN) {
        ag_trace("wrong len=%d > %d", (int) var_val_len, MAX_OID_LEN);
        return SNMP_ERR_WRONGLENGTH;
    }

    var_val_len /= sizeof(oid);
    data_source_ptr->length = var_val_len;
    oid_var = (oid *) var_val;
    for (iii = 0; iii < (int)data_source_ptr->length; iii++)
        data_source_ptr->objid[iii] = oid_var[iii];

    return SNMP_ERR_NOERROR;
}

u_long
AGUTIL_sys_up_time(void)
{
    return netsnmp_get_agent_runtime();
}

/*
 * NOTE: this function is a template for system dependent
 * implementation. Actually it (in debug purposes) returns
 * random (but likely) data */
void
SYSTEM_get_eth_statistics(VAR_OID_T * data_source, ETH_STATS_T * where)
{
#if OPTICALL_ACESS
    where->ifIndex = data_source->objid[data_source->length - 1];
    agent_get_Rmon_ethernet_statistics(where->ifIndex, 1,       /* exact */
                                       where);
#else                           /* OPTICALL_ACESS */
    static ETH_STATS_T prev = { -1, -1 };
    static time_t   ifLastRead = 0;
    time_t          curr_time;
    u_char          need_to_read;
    u_long          rc;

    where->ifIndex = data_source->objid[data_source->length - 1];
    need_to_read = (where->ifIndex != prev.ifIndex);
    if (!need_to_read) {
        curr_time = time(NULL);
        need_to_read = (curr_time - ifLastRead > 1);
    }

    if (need_to_read) {
        rc = (u_long) (1.0 +
                       ((double) rand() / (double) RAND_MAX) * 100.0);
        ifLastRead = time(NULL);
        prev.ifIndex = where->ifIndex;
    } else
        rc = 0;

    memcpy(where, &prev, sizeof(ETH_STATS_T));
    where->octets += rc * 100 * 200;
    where->packets += rc * 100;
    where->bcast_pkts += rc * 2;
    where->mcast_pkts += rc * 3;
    where->crc_align += rc;
    where->undersize += 0;
    where->oversize += 0;
    where->fragments += rc / 2;
    where->jabbers += 0;
    where->collisions += rc / 4;

    where->pkts_64 += rc * 10;
    where->pkts_65_127 += rc * 50;
    where->pkts_128_255 += rc * 20;
    where->pkts_256_511 += rc * 10;
    where->pkts_512_1023 += rc * 15;
    where->pkts_1024_1518 += rc * 5;

    need_to_read = prev.ifIndex;
    memcpy(&prev, where, sizeof(ETH_STATS_T));
    prev.ifIndex = need_to_read;
#endif                          /* OPTICALL_ACESS */
}

#if 0                           /* for memory debug */
static u_long   dbg_mem_cnt = 0;

void           *
dbg_f_AGMALLOC(size_t size)
{
    dbg_mem_cnt++;
    return malloc(size);
}

void
dbg_f_AGFREE(void *ptr)
{
    dbg_mem_cnt--;
    free(ptr);
}

char           *
dbg_f_AGSTRDUP(const char *s)
{
    dbg_mem_cnt++;
    return strdup(s);
}

void
dbg_f_AG_MEM_REPORT(void)
{
    ag_trace("dbg_mem_cnt=%ld", (long) dbg_mem_cnt);
}

#endif

void
init_agutil(void)
{
}
