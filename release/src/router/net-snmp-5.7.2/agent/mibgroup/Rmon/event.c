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
#include <net-snmp/net-snmp-features.h>

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
#include <ctype.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "event.h"

/*
 * Implementation headers 
 */
#include "agutil_api.h"
#include "row_api.h"

netsnmp_feature_require(snprint_objid)

/*
 * File scope definitions section 
 */

/*
 * from MIB compilation 
 */
#define eventEntryFirstIndexBegin       11

#define EVENTINDEX            3
#define EVENTDESCRIPTION      4
#define EVENTTYPE             5
#define EVENTCOMMUNITY        6
#define EVENTLASTTIMESENT     7
#define EVENTOWNER            8
#define EVENTSTATUS           9

#define Leaf_event_index        1
#define Leaf_event_description  2
#define MIN_event_description   0
#define MAX_event_description   127
#define Leaf_event_type         3
#define Leaf_event_community    4
#define MIN_event_community     0
#define MAX_event_community     127
#define Leaf_event_last_time_sent 5
#define Leaf_eventOwner        6
#define Leaf_eventStatus       7


#define LOGEVENTINDEX         3
#define LOGINDEX              4
#define LOGTIME               5
#define LOGDESCRIPTION        6


/*
 * defaults & limitations 
 */

#define MAX_LOG_ENTRIES_PER_CTRL	200

typedef struct data_struct_t {
    struct data_struct_t *next;
    u_long          data_index;
    u_long          log_time;
    char           *log_description;
} DATA_ENTRY_T;

typedef enum {
    EVENT_NONE = 1,
    EVENT_LOG,
    EVENT_TRAP,
    EVENT_LOG_AND_TRAP
} EVENT_TYPE_T;

typedef struct {
    char           *event_description;
    char           *event_community;
    EVENT_TYPE_T    event_type;
    u_long          event_last_time_sent;

    SCROLLER_T      scrlr;
#if 0
    u_long          event_last_logged_index;
    u_long          event_number_of_log_entries;
    DATA_ENTRY_T   *log_list;
    DATA_ENTRY_T   *last_log_ptr;
#endif
} CRTL_ENTRY_T;

/*
 * Main section 
 */

static TABLE_DEFINTION_T EventCtrlTable;
static TABLE_DEFINTION_T *table_ptr = &EventCtrlTable;
static unsigned char zero_octet_string[1];

/*
 * Control Table RowApi Callbacks 
 */

static int
data_destructor(SCROLLER_T * scrlr, void *free_me)
{
    DATA_ENTRY_T   *lptr = free_me;

    if (lptr->log_description)
        AGFREE(lptr->log_description);

    return 0;
}

int
event_Create(RMON_ENTRY_T * eptr)
{                               /* create the body: alloc it and set defaults */
    CRTL_ENTRY_T   *body;

    eptr->body = AGMALLOC(sizeof(CRTL_ENTRY_T));
    if (!eptr->body)
        return -3;
    body = (CRTL_ENTRY_T *) eptr->body;

    /*
     * set defaults 
     */

    body->event_description = NULL;
    body->event_community = AGSTRDUP("public");
    /*
     * ag_trace ("Dbg: created event_community=<%s>", body->event_community); 
     */
    body->event_type = EVENT_NONE;
    ROWDATAAPI_init(&body->scrlr,
                    MAX_LOG_ENTRIES_PER_CTRL,
                    MAX_LOG_ENTRIES_PER_CTRL,
                    sizeof(DATA_ENTRY_T), &data_destructor);


    return 0;
}

int
event_Clone(RMON_ENTRY_T * eptr)
{                               /* copy entry_bod -> clone */
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;
    CRTL_ENTRY_T   *clone = (CRTL_ENTRY_T *) eptr->tmp;

    if (body->event_description)
        clone->event_description = AGSTRDUP(body->event_description);

    if (body->event_community)
        clone->event_community = AGSTRDUP(body->event_community);
    return 0;
}

int
event_Copy(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;
    CRTL_ENTRY_T   *clone = (CRTL_ENTRY_T *) eptr->tmp;

    if (body->event_type != clone->event_type) {
        body->event_type = clone->event_type;
    }

    if (clone->event_description) {
        if (body->event_description)
            AGFREE(body->event_description);
        body->event_description = AGSTRDUP(clone->event_description);
    }

    if (clone->event_community) {
        if (body->event_community)
            AGFREE(body->event_community);
        body->event_community = AGSTRDUP(clone->event_community);
    }

    return 0;
}

int
event_Delete(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr;

    if (body->event_description)
        AGFREE(body->event_description);

    if (body->event_community)
        AGFREE(body->event_community);

    return 0;
}

int
event_Activate(RMON_ENTRY_T * eptr)
{                               /* init logTable */
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;

    ROWDATAAPI_set_size(&body->scrlr,
                        body->scrlr.data_requested,
                        (u_char)(RMON1_ENTRY_VALID == eptr->status) );

    return 0;
}

int
event_Deactivate(RMON_ENTRY_T * eptr)
{                               /* free logTable */
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;

    /*
     * free data list 
     */
    ROWDATAAPI_descructor(&body->scrlr);

    return 0;
}

static int
write_eventControl(int action, u_char * var_val, u_char var_val_type,
                   size_t var_val_len, u_char * statP,
                   oid * name, size_t name_len)
{
    long            long_temp;
    char           *char_temp;
    int             leaf_id, snmp_status;
    static int      prev_action = COMMIT;
    RMON_ENTRY_T   *hdr;
    CRTL_ENTRY_T   *cloned_body;

    switch (action) {
    case RESERVE1:
    case FREE:
    case UNDO:
    case ACTION:
    case COMMIT:
    default:
        return ROWAPI_do_another_action(name, eventEntryFirstIndexBegin,
                                        action, &prev_action,
                                        table_ptr, sizeof(CRTL_ENTRY_T));

    case RESERVE2:
        /*
         * get values from PDU, check them and save them in the cloned entry 
         */
        long_temp = name[eventEntryFirstIndexBegin];
        leaf_id = (int) name[eventEntryFirstIndexBegin - 1];
        hdr = ROWAPI_find(table_ptr, long_temp);        /* it MUST be OK */
        cloned_body = (CRTL_ENTRY_T *) hdr->tmp;
        switch (leaf_id) {
        case Leaf_event_index:
            return SNMP_ERR_NOTWRITABLE;
        case Leaf_event_description:
            char_temp = AGMALLOC(1 + MAX_event_description);
            if (!char_temp)
                return SNMP_ERR_TOOBIG;
            snmp_status = AGUTIL_get_string_value(var_val, var_val_type,
                                                  var_val_len,
                                                  MAX_event_description,
                                                  1, NULL, char_temp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                AGFREE(char_temp);
                return snmp_status;
            }

            if (cloned_body->event_description)
                AGFREE(cloned_body->event_description);

            cloned_body->event_description = AGSTRDUP(char_temp);
            /*
             * ag_trace ("rx: event_description=<%s>", cloned_body->event_description); 
             */
            AGFREE(char_temp);

            break;
        case Leaf_event_type:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               EVENT_NONE,
                                               EVENT_LOG_AND_TRAP,
                                               &long_temp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            cloned_body->event_type = long_temp;
            break;
        case Leaf_event_last_time_sent:
            return SNMP_ERR_NOTWRITABLE;
        case Leaf_event_community:
            char_temp = AGMALLOC(1 + MAX_event_community);
            if (!char_temp)
                return SNMP_ERR_TOOBIG;
            snmp_status = AGUTIL_get_string_value(var_val, var_val_type,
                                                  var_val_len,
                                                  MAX_event_community,
                                                  1, NULL, char_temp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                AGFREE(char_temp);
                return snmp_status;
            }

            if (cloned_body->event_community)
                AGFREE(cloned_body->event_community);

            cloned_body->event_community = AGSTRDUP(char_temp);
            AGFREE(char_temp);

            break;
        case Leaf_eventOwner:
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
        case Leaf_eventStatus:
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

unsigned char  *
var_eventTable(struct variable *vp,
               oid * name,
               size_t * length,
               int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static CRTL_ENTRY_T theEntry;
    RMON_ENTRY_T   *hdr;

    *write_method = write_eventControl;
    hdr = ROWAPI_header_ControlEntry(vp, name, length, exact, var_len,
                                     table_ptr,
                                     &theEntry, sizeof(CRTL_ENTRY_T));
    if (!hdr)
        return NULL;

    *var_len = sizeof(long);    /* default */

    switch (vp->magic) {
    case EVENTINDEX:
        long_ret = hdr->ctrl_index;
        return (unsigned char *) &long_ret;
    case EVENTDESCRIPTION:
        if (theEntry.event_description) {
            *var_len = strlen(theEntry.event_description);
            return (unsigned char *) theEntry.event_description;
        } else {
            *var_len = 0;
            return zero_octet_string;
        }
    case EVENTTYPE:
        long_ret = theEntry.event_type;
        return (unsigned char *) &long_ret;
    case EVENTCOMMUNITY:
        if (theEntry.event_community) {
            *var_len = strlen(theEntry.event_community);
            return (unsigned char *) theEntry.event_community;
        } else {
            *var_len = 0;
            return zero_octet_string;
        }
    case EVENTLASTTIMESENT:
        long_ret = theEntry.event_last_time_sent;
        return (unsigned char *) &long_ret;
    case EVENTOWNER:
        if (hdr->owner) {
            *var_len = strlen(hdr->owner);
            return (unsigned char *) hdr->owner;
        } else {
            *var_len = 0;
            return zero_octet_string;
        }
    case EVENTSTATUS:
        long_ret = hdr->status;
        return (unsigned char *) &long_ret;
    default:
        ag_trace("EventControlTable: unknown vp->magic=%d",
                 (int) vp->magic);
        ERROR_MSG("");
    }
    return NULL;
}

static SCROLLER_T *
event_extract_scroller(void *v_body)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) v_body;
    return &body->scrlr;
}

unsigned char  *
var_logTable(struct variable *vp,
             oid * name,
             size_t * length,
             int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static DATA_ENTRY_T theEntry;
    RMON_ENTRY_T   *hdr;

    *write_method = NULL;
    hdr = ROWDATAAPI_header_DataEntry(vp, name, length, exact, var_len,
                                      table_ptr,
                                      &event_extract_scroller,
                                      sizeof(DATA_ENTRY_T), &theEntry);
    if (!hdr)
        return NULL;

    *var_len = sizeof(long);    /* default */

    switch (vp->magic) {
    case LOGEVENTINDEX:
        long_ret = hdr->ctrl_index;
        return (unsigned char *) &long_ret;
    case LOGINDEX:
        long_ret = theEntry.data_index;
        return (unsigned char *) &long_ret;
    case LOGTIME:
        long_ret = theEntry.log_time;
        return (unsigned char *) &long_ret;
    case LOGDESCRIPTION:
        if (theEntry.log_description) {
            *var_len = strlen(theEntry.log_description);
            return (unsigned char *) theEntry.log_description;
        } else {
            *var_len = 0;
            return zero_octet_string;
        }
    default:
        ERROR_MSG("");
    }

    return NULL;
}

/*
 * External API section 
 */

static char    *
create_explanaition(CRTL_ENTRY_T * evptr, u_char is_rising,
                    u_long alarm_index, u_long event_index,
                    oid * alarmed_var,
                    size_t alarmed_var_length,
                    u_long value, u_long the_threshold,
                    u_long sample_type, char *alarm_descr)
{
#define UNEQ_LENGTH	(1 + 11 + 4 + 11 + 1 + 20)
    char            expl[UNEQ_LENGTH];
    static char     c_oid[SPRINT_MAX_LEN];
    size_t          sz;
    char           *descr;
    register char  *pch;
    register char  *tmp;


    snprint_objid(c_oid, sizeof(c_oid)-1, alarmed_var, alarmed_var_length);
    c_oid[sizeof(c_oid)-1] = '\0';
    for (pch = c_oid;;) {
        tmp = strchr(pch, '.');
        if (!tmp)
            break;
        if (isdigit(tmp[1]) || '"' == tmp[1])
            break;
        pch = tmp + 1;
    }

    snprintf(expl, UNEQ_LENGTH, "=%ld %s= %ld :%ld, %ld",
             (unsigned long) value,
             is_rising ? ">" : "<",
             (unsigned long) the_threshold,
             (long) alarm_index, (long) event_index);
    sz = 3 + strlen(expl) + strlen(pch);
    if (alarm_descr)
        sz += strlen(alarm_descr);

    descr = AGMALLOC(sz);
    if (!descr) {
        ag_trace("Can't allocate event description");
        return NULL;
    }

    if (alarm_descr) {
        strcpy(descr, alarm_descr);
        strcat(descr, ":");
    } else
        *descr = '\0';

    strcat(descr, pch);
    strcat(descr, expl);
    return descr;
}

static void
event_send_trap(CRTL_ENTRY_T * evptr, u_char is_rising,
                u_int alarm_index,
                u_int value, u_int the_threshold,
                oid * alarmed_var, size_t alarmed_var_length,
                u_int sample_type)
{
    static oid      objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    static oid      rmon1_trap_oid[] = { 1, 3, 6, 1, 2, 1, 16, 0, 0 };
    static oid      alarm_index_oid[] =
        { 1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 1, 0 };
    static oid      alarmed_var_oid[] =
        { 1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 3, 0 };
    static oid      sample_type_oid[] =
        { 1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 4, 0 };
    static oid      value_oid[] = { 1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 5, 0 };
    static oid      threshold_oid[] = { 1, 3, 6, 1, 2, 1, 16, 3, 1, 1, 7, 0 };     /* rising case */
    netsnmp_variable_list *var_list = NULL;

    /*
     * set the last 'oid' : risingAlarm or fallingAlarm 
     */
    if (is_rising) {
        rmon1_trap_oid[8] = 1;
        threshold_oid[10] = 7;
    } else {
        rmon1_trap_oid[8] = 2;
        threshold_oid[10] = 8;
    }
    alarm_index_oid[11] = alarm_index;
    alarmed_var_oid[11] = alarm_index;
    sample_type_oid[11] = alarm_index;
    value_oid[11]       = alarm_index;
    threshold_oid[11]   = alarm_index;

    /*
     * build the var list 
     */
    snmp_varlist_add_variable(&var_list, objid_snmptrap,
                              OID_LENGTH(objid_snmptrap),
                              ASN_OBJECT_ID, (u_char *) rmon1_trap_oid,
                              sizeof(rmon1_trap_oid));
    snmp_varlist_add_variable(&var_list, alarm_index_oid,
                              OID_LENGTH(alarm_index_oid),
                              ASN_INTEGER, (u_char *) &alarm_index,
                              sizeof(u_int));
    snmp_varlist_add_variable(&var_list, alarmed_var_oid,
                              OID_LENGTH(alarmed_var_oid),
                              ASN_OBJECT_ID, (u_char *) alarmed_var,
                              alarmed_var_length * sizeof(oid));
    snmp_varlist_add_variable(&var_list, sample_type_oid,
                              OID_LENGTH(sample_type_oid),
                              ASN_INTEGER, (u_char *) &sample_type,
                              sizeof(u_int));
    snmp_varlist_add_variable(&var_list, value_oid,
                              OID_LENGTH(value_oid),
                              ASN_INTEGER, (u_char *) &value,
                              sizeof(u_int));
    snmp_varlist_add_variable(&var_list, threshold_oid,
                              OID_LENGTH(threshold_oid),
                              ASN_INTEGER, (u_char *) &the_threshold,
                              sizeof(u_int));

    send_v2trap(var_list);
    ag_trace("rmon trap has been sent");
    snmp_free_varbind(var_list);
}


static void
event_save_log(CRTL_ENTRY_T * body, char *event_descr)
{
    register DATA_ENTRY_T *lptr;

    lptr = ROWDATAAPI_locate_new_data(&body->scrlr);
    if (!lptr) {
        ag_trace("Err: event_save_log:cannot locate ?");
        return;
    }

    lptr->log_time = body->event_last_time_sent;
    if (lptr->log_description)
        AGFREE(lptr->log_description);
    lptr->log_description = AGSTRDUP(event_descr);
    lptr->data_index = ROWDATAAPI_get_total_number(&body->scrlr);

    /*
     * ag_trace ("log has been saved, data_index=%d", (int) lptr->data_index); 
     */
}

int
event_api_send_alarm(u_char is_rising,
                     u_long alarm_index,
                     u_long event_index,
                     oid * alarmed_var,
                     size_t alarmed_var_length,
                     u_long sample_type,
                     u_long value, u_long the_threshold, char *alarm_descr)
{
    RMON_ENTRY_T   *eptr;
    CRTL_ENTRY_T   *evptr;

    if (!event_index)
        return SNMP_ERR_NOERROR;

#if 0
    ag_trace("event_api_send_alarm(%d,%d,%d,'%s')",
             (int) is_rising, (int) alarm_index, (int) event_index,
             alarm_descr);
#endif
    eptr = ROWAPI_find(table_ptr, event_index);
    if (!eptr) {
        /*
         * ag_trace ("event cannot find entry %ld", event_index); 
         */
        return SNMP_ERR_NOSUCHNAME;
    }

    evptr = (CRTL_ENTRY_T *) eptr->body;
    evptr->event_last_time_sent = AGUTIL_sys_up_time();


    if (EVENT_TRAP == evptr->event_type
        || EVENT_LOG_AND_TRAP == evptr->event_type) {
        event_send_trap(evptr, is_rising, alarm_index, value,
                        the_threshold, alarmed_var, alarmed_var_length,
                        sample_type);
    }

    if (EVENT_LOG == evptr->event_type
        || EVENT_LOG_AND_TRAP == evptr->event_type) {
        register char  *explain;

        explain = create_explanaition(evptr, is_rising,
                                      alarm_index, event_index,
                                      alarmed_var, alarmed_var_length,
                                      value, the_threshold,
                                      sample_type, alarm_descr);
        /*
         * if (explain) ag_trace ("Dbg:'%s'", explain); 
         */
        event_save_log(evptr, explain);
        if (explain)
            AGFREE(explain);
    }

    return SNMP_ERR_NOERROR;
}

#if 1                           /* debug, but may be used for init. TBD: may be token snmpd.conf ? */
int
add_event_entry(int ctrl_index,
                char *event_description,
                EVENT_TYPE_T event_type, char *event_community)
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

    if (event_description) {
        if (body->event_description)
            AGFREE(body->event_description);
        body->event_description = AGSTRDUP(event_description);
    }

    if (event_community) {
        if (body->event_community)
            AGFREE(body->event_community);
        body->event_community = AGSTRDUP(event_community);
    }

    body->event_type = event_type;

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

oid             eventTable_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 16, 9, 1 };
oid             logTable_variables_oid[] = { 1, 3, 6, 1, 2, 1, 16, 9, 2 };

struct variable2 eventTable_variables[] = {
    /*
     * magic number        , variable type, ro/rw , callback fn  ,           L, oidsuffix 
     */
    {EVENTINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_eventTable, 2, {1, 1}},
    {EVENTDESCRIPTION, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_eventTable, 2, {1, 2}},
    {EVENTTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_eventTable, 2, {1, 3}},
    {EVENTCOMMUNITY, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_eventTable, 2, {1, 4}},
    {EVENTLASTTIMESENT, ASN_TIMETICKS, NETSNMP_OLDAPI_RONLY,
     var_eventTable, 2, {1, 5}},
    {EVENTOWNER, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_eventTable, 2, {1, 6}},
    {EVENTSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_eventTable, 2, {1, 7}}
};

struct variable2 logTable_variables[] = {
    /*
     * magic number        , variable type, ro/rw , callback fn  ,           L, oidsuffix 
     */
    {LOGEVENTINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_logTable, 2, {1, 1}},
    {LOGINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_logTable, 2, {1, 2}},
    {LOGTIME, ASN_TIMETICKS, NETSNMP_OLDAPI_RONLY,
     var_logTable, 2, {1, 3}},
    {LOGDESCRIPTION, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_logTable, 2, {1, 4}}

};

void
init_event(void)
{
    REGISTER_MIB("eventTable", eventTable_variables, variable2,
                 eventTable_variables_oid);
    REGISTER_MIB("logTable", logTable_variables, variable2,
                 logTable_variables_oid);

    ROWAPI_init_table(&EventCtrlTable, "Event", 0, &event_Create, &event_Clone, &event_Delete, NULL,    /* &event_Validate, */
                      &event_Activate, &event_Deactivate, &event_Copy);
#if 0
    add_event_entry(3, "Alarm", EVENT_LOG_AND_TRAP, NULL);
    /*
     * add_event_entry (5, ">=", EVENT_LOG_AND_TRAP, NULL); 
     */
#endif
}
