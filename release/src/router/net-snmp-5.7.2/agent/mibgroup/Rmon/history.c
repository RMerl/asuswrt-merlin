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

#if HAVE_STDLIB_H
#include <stdlib.h>
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
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "history.h"

/*
 * Implementation headers 
 */
#include "agutil_api.h"
#include "row_api.h"

/*
 * File scope definitions section 
 */

#define historyControlEntryFirstIndexBegin      11

#define CTRL_INDEX		3
#define CTRL_DATASOURCE		4
#define CTRL_BUCKETSREQUESTED	5
#define CTRL_BUCKETSGRANTED	6
#define CTRL_INTERVAL		7
#define CTRL_OWNER		8
#define CTRL_STATUS		9

#define DATA_INDEX		3
#define DATA_SAMPLEINDEX	4
#define DATA_INTERVALSTART	5
#define DATA_DROPEVENTS		6
#define DATA_OCTETS		7
#define DATA_PKTS		8
#define DATA_BROADCASTPKTS	9
#define DATA_MULTICASTPKTS	10
#define DATA_CRCALIGNERRORS	11
#define DATA_UNDERSIZEPKTS	12
#define DATA_OVERSIZEPKTS	13
#define DATA_FRAGMENTS		14
#define DATA_JABBERS		15
#define DATA_COLLISIONS		16
#define DATA_UTILIZATION	17

/*
 * defaults & limitations 
 */

#define MAX_BUCKETS_IN_CRTL_ENTRY	50
#define HIST_DEF_BUCK_REQ		50
#define HIST_DEF_INTERVAL		1800
static VAR_OID_T DEFAULT_DATA_SOURCE = { 11,    /* ifIndex.1 */
    {1, 3, 6, 1, 2, 1, 2, 2, 1, 1, 1}
};

typedef struct data_struct_t {
    struct data_struct_t *next;
    u_long          data_index;
    u_long          start_interval;
    u_long          utilization;
    ETH_STATS_T     EthData;
} DATA_ENTRY_T;

typedef struct {
    u_long          interval;
    u_long          timer_id;
    VAR_OID_T       data_source;

    u_long          coeff;
    DATA_ENTRY_T    previous_bucket;
    SCROLLER_T      scrlr;

} CRTL_ENTRY_T;

static TABLE_DEFINTION_T HistoryCtrlTable;
static TABLE_DEFINTION_T *table_ptr = &HistoryCtrlTable;

/*
 * Main section 
 */

#  define Leaf_historyControlDataSource                    2
#  define Leaf_historyControlBucketsRequested              3
#  define Leaf_historyControlInterval                      5
#  define Leaf_historyControlOwner                         6
#  define Leaf_historyControlStatus                        7
#  define MIN_historyControlBucketsRequested               1
#  define MAX_historyControlBucketsRequested               65535
#  define MIN_historyControlInterval                       1
#  define MAX_historyControlInterval                       3600

static int
write_historyControl(int action, u_char * var_val, u_char var_val_type,
                     size_t var_val_len, u_char * statP,
                     oid * name, size_t name_len)
{
    long            long_temp;
    int             leaf_id, snmp_status;
    static int      prev_action = COMMIT;
    RMON_ENTRY_T   *hdr;
    CRTL_ENTRY_T   *cloned_body;
    CRTL_ENTRY_T   *body;

    switch (action) {
    case RESERVE1:
    case FREE:
    case UNDO:
    case ACTION:
    case COMMIT:
    default:
        return ROWAPI_do_another_action(name,
                                        historyControlEntryFirstIndexBegin,
                                        action, &prev_action, table_ptr,
                                        sizeof(CRTL_ENTRY_T));
    case RESERVE2:
        /*
         * get values from PDU, check them and save them in the cloned entry 
         */
        long_temp = name[historyControlEntryFirstIndexBegin];
        leaf_id = (int) name[historyControlEntryFirstIndexBegin - 1];
        hdr = ROWAPI_find(table_ptr, long_temp);        /* it MUST be OK */
        cloned_body = (CRTL_ENTRY_T *) hdr->tmp;
        body = (CRTL_ENTRY_T *) hdr->body;
        switch (leaf_id) {
        case Leaf_historyControlDataSource:
            snmp_status = AGUTIL_get_oid_value(var_val, var_val_type,
                                               var_val_len,
                                               &cloned_body->data_source);
            if (SNMP_ERR_NOERROR != snmp_status) {
                ag_trace("can't browse historyControlDataSource");
                return snmp_status;
            }
            if (RMON1_ENTRY_UNDER_CREATION != hdr->status &&
                snmp_oid_compare(cloned_body->data_source.objid,
                                 cloned_body->data_source.length,
                                 body->data_source.objid,
                                 body->data_source.length)) {
                ag_trace
                    ("can't change historyControlDataSource - not Creation");
                return SNMP_ERR_BADVALUE;
            }
            break;
        case Leaf_historyControlBucketsRequested:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               MIN_historyControlBucketsRequested,
                                               MAX_historyControlBucketsRequested,
                                               (long *) &cloned_body->scrlr.
                                               data_requested);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
#if 0
            if (RMON1_ENTRY_UNDER_CREATION != hdr->status &&
                cloned_body->scrlr.data_requested !=
                body->scrlr.data_requested)
                return SNMP_ERR_BADVALUE;
#endif
            break;
        case Leaf_historyControlInterval:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               MIN_historyControlInterval,
                                               MAX_historyControlInterval,
                                               (long *) &cloned_body->interval);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
#if 0
            if (RMON1_ENTRY_UNDER_CREATION != hdr->status &&
                cloned_body->interval != body->interval)
                return SNMP_ERR_BADVALUE;
#endif
            break;
        case Leaf_historyControlOwner:
            if (hdr->new_owner)
                AGFREE(hdr->new_owner);
            hdr->new_owner = AGMALLOC(MAX_OWNERSTRING);;
            if (!hdr->new_owner)
                return SNMP_ERR_TOOBIG;
            snmp_status = AGUTIL_get_string_value(var_val, var_val_type,
                                                  var_val_len,
                                                  MAX_OWNERSTRING,
                                                  1, NULL, hdr->new_owner);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }

            break;
        case Leaf_historyControlStatus:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               RMON1_ENTRY_VALID,
                                               RMON1_ENTRY_INVALID,
                                               &long_temp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            hdr->new_status = long_temp;
            break;
        default:
            ag_trace("%s:unknown leaf_id=%d\n", table_ptr->name,
                     (int) leaf_id);
            return SNMP_ERR_NOSUCHNAME;
        }                       /* of switch by 'leaf_id' */
        break;

    }                           /* of switch by actions */

    prev_action = action;
    return SNMP_ERR_NOERROR;
}

/*
 * var_historyControlTable():
 */
unsigned char  *
var_historyControlTable(struct variable *vp,
                        oid * name,
                        size_t * length,
                        int exact,
                        size_t * var_len, WriteMethod ** write_method)
{
    static unsigned char zero_octet_string[1];
    static long     long_ret;
    static CRTL_ENTRY_T theEntry;
    RMON_ENTRY_T   *hdr;

    *write_method = write_historyControl;
    hdr = ROWAPI_header_ControlEntry(vp, name, length, exact, var_len,
                                     table_ptr,
                                     &theEntry, sizeof(CRTL_ENTRY_T));
    if (!hdr)
        return NULL;

    *var_len = sizeof(long);    /* default */

    switch (vp->magic) {
    case CTRL_INDEX:
        long_ret = hdr->ctrl_index;
        return (unsigned char *) &long_ret;

    case CTRL_DATASOURCE:
        *var_len = sizeof(oid) * theEntry.data_source.length;
        return (unsigned char *) theEntry.data_source.objid;

    case CTRL_BUCKETSREQUESTED:
        long_ret = theEntry.scrlr.data_requested;
        return (unsigned char *) &long_ret;

    case CTRL_BUCKETSGRANTED:

        long_ret = theEntry.scrlr.data_granted;
        return (unsigned char *) &long_ret;

    case CTRL_INTERVAL:
        long_ret = theEntry.interval;
        return (unsigned char *) &long_ret;

    case CTRL_OWNER:
        if (hdr->owner) {
            *var_len = strlen(hdr->owner);
            return (unsigned char *) hdr->owner;
        } else {
            *var_len = 0;
            return zero_octet_string;
        }

    case CTRL_STATUS:
        long_ret = hdr->status;
        return (unsigned char *) &long_ret;

    default:
        ag_trace("HistoryControlTable: unknown vp->magic=%d",
                 (int) vp->magic);
        ERROR_MSG("");
    }
    return NULL;
}

/*
 * history row management control callbacks 
 */

static void
compute_delta(ETH_STATS_T * delta,
              ETH_STATS_T * newval, ETH_STATS_T * prevval)
{
#define CNT_DIF(X) delta->X = newval->X - prevval->X

    CNT_DIF(octets);
    CNT_DIF(packets);
    CNT_DIF(bcast_pkts);
    CNT_DIF(mcast_pkts);
    CNT_DIF(crc_align);
    CNT_DIF(undersize);
    CNT_DIF(oversize);
    CNT_DIF(fragments);
    CNT_DIF(jabbers);
    CNT_DIF(collisions);
}

static void
history_get_backet(unsigned int clientreg, void *clientarg)
{
    RMON_ENTRY_T   *hdr_ptr;
    CRTL_ENTRY_T   *body;
    DATA_ENTRY_T   *bptr;
    ETH_STATS_T     newSample;

    /*
     * ag_trace ("history_get_backet: timer_id=%d", (int) clientreg); 
     */
    hdr_ptr = (RMON_ENTRY_T *) clientarg;
    if (!hdr_ptr) {
        ag_trace
            ("Err: history_get_backet: hdr_ptr=NULL ? (Inserted in shock)");
        return;
    }

    body = (CRTL_ENTRY_T *) hdr_ptr->body;
    if (!body) {
        ag_trace
            ("Err: history_get_backet: body=NULL ? (Inserted in shock)");
        return;
    }

    if (RMON1_ENTRY_VALID != hdr_ptr->status) {
        ag_trace("Err: history_get_backet when entry %d is not valid ?!!",
                 (int) hdr_ptr->ctrl_index);
        /*
         * snmp_alarm_print_list (); 
         */
        snmp_alarm_unregister(body->timer_id);
        ag_trace("Err: unregistered %ld", (long) body->timer_id);
        return;
    }

    SYSTEM_get_eth_statistics(&body->data_source, &newSample);

    bptr = ROWDATAAPI_locate_new_data(&body->scrlr);
    if (!bptr) {
        ag_trace
            ("Err: history_get_backet for %d: empty bucket's list !??\n",
             (int) hdr_ptr->ctrl_index);
        return;
    }

    bptr->data_index = ROWDATAAPI_get_total_number(&body->scrlr);

    bptr->start_interval = body->previous_bucket.start_interval;

    compute_delta(&bptr->EthData, &newSample,
                  &body->previous_bucket.EthData);

    bptr->utilization =
        bptr->EthData.octets * 8 + bptr->EthData.packets * (96 + 64);
    bptr->utilization /= body->coeff;

    /*
     * update previous_bucket 
     */
    body->previous_bucket.start_interval = AGUTIL_sys_up_time();
    memcpy(&body->previous_bucket.EthData, &newSample,
           sizeof(ETH_STATS_T));
}

/*
 * Control Table RowApi Callbacks 
 */

int
history_Create(RMON_ENTRY_T * eptr)
{                               /* create the body: alloc it and set defaults */
    CRTL_ENTRY_T   *body;

    eptr->body = AGMALLOC(sizeof(CRTL_ENTRY_T));
    if (!eptr->body)
        return -3;
    body = (CRTL_ENTRY_T *) eptr->body;

    /*
     * set defaults 
     */
    body->interval = HIST_DEF_INTERVAL;
    body->timer_id = 0;
    memcpy(&body->data_source, &DEFAULT_DATA_SOURCE, sizeof(VAR_OID_T));

    ROWDATAAPI_init(&body->scrlr, HIST_DEF_BUCK_REQ,
                    MAX_BUCKETS_IN_CRTL_ENTRY, sizeof(DATA_ENTRY_T), NULL);

    return 0;
}

int
history_Validate(RMON_ENTRY_T * eptr)
{
    /*
     * T.B.D. (system dependent) check valid inteface in body->data_source; 
     */
    return 0;
}

int
history_Activate(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;

    body->coeff = 100000L * (long) body->interval;

    ROWDATAAPI_set_size(&body->scrlr,
                        body->scrlr.data_requested,
                        (u_char)(RMON1_ENTRY_VALID == eptr->status) );

    SYSTEM_get_eth_statistics(&body->data_source,
                              &body->previous_bucket.EthData);
    body->previous_bucket.start_interval = AGUTIL_sys_up_time();

    body->scrlr.current_data_ptr = body->scrlr.first_data_ptr;
    /*
     * ag_trace ("Dbg:   registered in history_Activate"); 
     */
    body->timer_id = snmp_alarm_register(body->interval, SA_REPEAT,
                                         history_get_backet, eptr);
    return 0;
}

int
history_Deactivate(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;

    snmp_alarm_unregister(body->timer_id);
    /*
     * ag_trace ("Dbg: unregistered in history_Deactivate timer_id=%d",
     * (int) body->timer_id); 
     */

    /*
     * free data list 
     */
    ROWDATAAPI_descructor(&body->scrlr);

    return 0;
}

int
history_Copy(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;
    CRTL_ENTRY_T   *clone = (CRTL_ENTRY_T *) eptr->tmp;

    if (body->scrlr.data_requested != clone->scrlr.data_requested) {
        ROWDATAAPI_set_size(&body->scrlr, clone->scrlr.data_requested,
                            (u_char)(RMON1_ENTRY_VALID == eptr->status) );
    }

    if (body->interval != clone->interval) {
        if (RMON1_ENTRY_VALID == eptr->status) {
            snmp_alarm_unregister(body->timer_id);
            body->timer_id =
                snmp_alarm_register(clone->interval, SA_REPEAT,
                                    history_get_backet, eptr);
        }

        body->interval = clone->interval;
    }

    if (snmp_oid_compare
        (clone->data_source.objid, clone->data_source.length,
         body->data_source.objid, body->data_source.length)) {
        memcpy(&body->data_source, &clone->data_source, sizeof(VAR_OID_T));
    }

    return 0;
}

static SCROLLER_T *
history_extract_scroller(void *v_body)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) v_body;
    return &body->scrlr;
}

/*
 * var_etherHistoryTable():
 */
unsigned char  *
var_etherHistoryTable(struct variable *vp,
                      oid * name,
                      size_t * length,
                      int exact,
                      size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static DATA_ENTRY_T theBucket;
    RMON_ENTRY_T   *hdr;

    *write_method = NULL;
    hdr = ROWDATAAPI_header_DataEntry(vp, name, length, exact, var_len,
                                      table_ptr,
                                      &history_extract_scroller,
                                      sizeof(DATA_ENTRY_T), &theBucket);
    if (!hdr)
        return NULL;

    *var_len = sizeof(long);    /* default */

    switch (vp->magic) {
    case DATA_INDEX:
        long_ret = hdr->ctrl_index;
        return (unsigned char *) &long_ret;
    case DATA_SAMPLEINDEX:
        long_ret = theBucket.data_index;
        return (unsigned char *) &long_ret;
    case DATA_INTERVALSTART:
        long_ret = 0;
        return (unsigned char *) &theBucket.start_interval;
    case DATA_DROPEVENTS:
        long_ret = 0;
        return (unsigned char *) &long_ret;
    case DATA_OCTETS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.octets;
    case DATA_PKTS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.packets;
    case DATA_BROADCASTPKTS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.bcast_pkts;
    case DATA_MULTICASTPKTS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.mcast_pkts;
    case DATA_CRCALIGNERRORS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.crc_align;
    case DATA_UNDERSIZEPKTS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.undersize;
    case DATA_OVERSIZEPKTS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.oversize;
    case DATA_FRAGMENTS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.fragments;
    case DATA_JABBERS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.jabbers;
    case DATA_COLLISIONS:
        long_ret = 0;
        return (unsigned char *) &theBucket.EthData.collisions;
    case DATA_UTILIZATION:
        long_ret = 0;
        return (unsigned char *) &theBucket.utilization;
    default:
        ag_trace("etherHistoryTable: unknown vp->magic=%d",
                 (int) vp->magic);
        ERROR_MSG("");
    }
    return NULL;
}

#if 1                           /* debug, but may be used for init. TBD: may be token snmpd.conf ? */
int
add_hist_entry(int ctrl_index, int ifIndex,
               u_long interval, u_long requested)
{
    register RMON_ENTRY_T *eptr;
    register CRTL_ENTRY_T *body;
    int             ierr;

    ierr = ROWAPI_new(table_ptr, ctrl_index);
    if (ierr) {
        ag_trace("ROWAPI_new failed with %d", ierr);
        return ierr;
    }

    eptr = ROWAPI_find(table_ptr, ctrl_index);
    if (!eptr) {
        ag_trace("ROWAPI_find failed");
        return -4;
    }

    body = (CRTL_ENTRY_T *) eptr->body;

    /*
     * set parameters 
     */

    body->data_source.objid[body->data_source.length - 1] = ifIndex;
    body->interval = interval;
    body->scrlr.data_requested = requested;

    eptr->new_status = RMON1_ENTRY_VALID;
    ierr = ROWAPI_commit(table_ptr, ctrl_index);
    if (ierr) {
        ag_trace("ROWAPI_commit failed with %d", ierr);
    }

    return ierr;

}

#endif

/*
 * Registration & Initializatio section 
 */

oid             historyControlTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 16, 2, 1 };

struct variable2 historyControlTable_variables[] = {
    /*
     * magic number        , variable type, ro/rw , callback fn  ,           L, oidsuffix 
     */
    {CTRL_INDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_historyControlTable, 2, {1, 1}},
    {CTRL_DATASOURCE, ASN_OBJECT_ID, NETSNMP_OLDAPI_RWRITE,
     var_historyControlTable, 2, {1, 2}},
    {CTRL_BUCKETSREQUESTED, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_historyControlTable, 2, {1, 3}},
    {CTRL_BUCKETSGRANTED, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_historyControlTable, 2, {1, 4}},
    {CTRL_INTERVAL, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_historyControlTable, 2, {1, 5}},
    {CTRL_OWNER, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_historyControlTable, 2, {1, 6}},
    {CTRL_STATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_historyControlTable, 2, {1, 7}},

};

oid             etherHistoryTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 16, 2, 2 };

struct variable2 etherHistoryTable_variables[] = {
    /*
     * magic number     , variable type , ro/rw , callback fn  ,        L, oidsuffix 
     */
    {DATA_INDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 1}},
    {DATA_SAMPLEINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 2}},
    {DATA_INTERVALSTART, ASN_TIMETICKS, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 3}},
    {DATA_DROPEVENTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 4}},
    {DATA_OCTETS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 5}},
    {DATA_PKTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 6}},
    {DATA_BROADCASTPKTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 7}},
    {DATA_MULTICASTPKTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 8}},
    {DATA_CRCALIGNERRORS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 9}},
    {DATA_UNDERSIZEPKTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 10}},
    {DATA_OVERSIZEPKTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 11}},
    {DATA_FRAGMENTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 12}},
    {DATA_JABBERS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 13}},
    {DATA_COLLISIONS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 14}},
    {DATA_UTILIZATION, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_etherHistoryTable, 2, {1, 15}},

};

void
init_history(void)
{
    REGISTER_MIB("historyControlTable", historyControlTable_variables,
                 variable2, historyControlTable_variables_oid);
    REGISTER_MIB("etherHistoryTable", etherHistoryTable_variables,
                 variable2, etherHistoryTable_variables_oid);

    ROWAPI_init_table(&HistoryCtrlTable, "History", 0, &history_Create, NULL,   /* &history_Clone, */
                      NULL,     /* &history_Delete, */
                      &history_Validate,
                      &history_Activate,
                      &history_Deactivate, &history_Copy);

    /*
     * add_hist_entry (2, 3, 4, 2); 
     */
}
