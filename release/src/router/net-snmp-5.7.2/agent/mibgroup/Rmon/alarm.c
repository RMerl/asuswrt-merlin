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
#if HAVE_UNISTD_H
#include <unistd.h>
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

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "alarm.h"
    /*
     * Implementation headers 
     */
#include "agutil_api.h"
#include "row_api.h"
    /*
     * File scope definitions section 
     */
    /*
     * from MIB compilation 
     */
#define alarmEntryFirstIndexBegin       11
#define MMM_MAX				0xFFFFFFFFl
#define IDalarmIndex                    1
#define IDalarmInterval                 2
#define IDalarmVariable                 3
#define IDalarmSampleType               4
#define IDalarmValue                    5
#define IDalarmStartupAlarm             6
#define IDalarmRisingThreshold          7
#define IDalarmFallingThreshold         8
#define IDalarmRisingEventIndex         9
#define IDalarmFallingEventIndex        10
#define IDalarmOwner                    11
#define IDalarmStatus                   12
#define MIN_alarmEventIndex             0
#define MAX_alarmEventIndex             65535
     typedef enum {
         SAMPLE_TYPE_ABSOLUTE =
             1,
         SAMPLE_TYPE_DELTE
     } SAMPLE_TYPE_T;

     typedef enum {
         ALARM_NOTHING =
             0,
         ALARM_RISING,
         ALARM_FALLING,
         ALARM_BOTH
     } ALARM_TYPE_T;

     typedef struct {
         u_long
             interval;
         u_long
             timer_id;
         VAR_OID_T
             var_name;
         SAMPLE_TYPE_T
             sample_type;
         ALARM_TYPE_T
             startup_type;      /* RISING | FALLING | BOTH */

         u_long
             rising_threshold;
         u_long
             falling_threshold;
         u_long
             rising_event_index;
         u_long
             falling_event_index;

         u_long
             last_abs_value;
         u_long
             value;
         ALARM_TYPE_T
             prev_alarm;        /* NOTHING | RISING | FALLING */
     } CRTL_ENTRY_T;

/*
 * Main section 
 */

     static TABLE_DEFINTION_T
         AlarmCtrlTable;
     static TABLE_DEFINTION_T *
         table_ptr = &
         AlarmCtrlTable;

#if 0                           /* KUKU */
     static u_long
         kuku_sum =
         0,
         kuku_cnt =
         0;
#endif

/*
 * find & enjoy it in event.c 
 */
     extern int
     event_api_send_alarm(u_char is_rising,
                          u_long alarm_index,
                          u_long event_index,
                          oid * alarmed_var,
                          size_t alarmed_var_length,
                          u_long sample_type,
                          u_long value,
                          u_long the_threshold, char *alarm_descr);

static int
fetch_var_val(oid * name, size_t namelen, u_long * new_value)
{
    netsnmp_subtree *tree_ptr;
    size_t          var_len;
    WriteMethod    *write_method;
    struct variable called_var;
    register struct variable *s_var_ptr = NULL;
    register u_char *access;


    tree_ptr = netsnmp_subtree_find(name, namelen, NULL, "");
    if (!tree_ptr) {
        ag_trace("tree_ptr is NULL");
        return SNMP_ERR_NOSUCHNAME;
    }

    
    memcpy(called_var.name, tree_ptr->name_a,
           tree_ptr->namelen * sizeof(oid));
 
    if (tree_ptr->reginfo && 
        tree_ptr->reginfo->handler && 
        tree_ptr->reginfo->handler->next && 
        tree_ptr->reginfo->handler->next->myvoid) {
        s_var_ptr = (struct variable *)tree_ptr->reginfo->handler->next->myvoid;
    }

    if (s_var_ptr) {
        if (s_var_ptr->namelen) {
                called_var.namelen = 
                                   tree_ptr->namelen;
                called_var.type = s_var_ptr->type;
                called_var.magic = s_var_ptr->magic;
                called_var.acl = s_var_ptr->acl;
                called_var.findVar = s_var_ptr->findVar;
                access =    
                    (*(s_var_ptr->findVar)) (&called_var, name, &namelen,
                                             1, &var_len, &write_method);

                if (access
                    && snmp_oid_compare(name, namelen, tree_ptr->end_a,
                                        tree_ptr->end_len) > 0) {
                    memcpy(name, tree_ptr->end_a, tree_ptr->end_len);
                    access = 0;
                    ag_trace("access := 0");
                }

                if (access) {

                    /*
                     * check 'var_len' ? 
                     */

                    /*
                     * check type 
                     */
                    switch (called_var.type) {
                    case ASN_INTEGER:
                    case ASN_COUNTER:
                    case ASN_TIMETICKS:
                    case ASN_GAUGE:
                    case ASN_COUNTER64:
                        break;
                    default:
                        ag_trace("invalid type: %d",
                                 (int) called_var.type);
                        return SNMP_ERR_GENERR;
                    }
                    *new_value = *(u_long *) access;
                    return SNMP_ERR_NOERROR;
                }
            }
        }

    return SNMP_ERR_NOSUCHNAME;
}

static void
alarm_check_var(unsigned int clientreg, void *clientarg)
{
    RMON_ENTRY_T   *hdr_ptr;
    CRTL_ENTRY_T   *body;
    u_long          new_value;
    int             ierr;

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
        snmp_alarm_unregister(body->timer_id);
        return;
    }

    ierr = fetch_var_val(body->var_name.objid,
                         body->var_name.length, &new_value);
    if (SNMP_ERR_NOERROR != ierr) {
        ag_trace("Err: Can't fetch var_name");
        return;
    }

    body->value = (SAMPLE_TYPE_ABSOLUTE == body->sample_type) ?
        new_value : new_value - body->last_abs_value;
    body->last_abs_value = new_value;
    /*
     * ag_trace ("fetched value=%ld check %ld", (long) new_value, (long) body->value); 
     */
#if 0                           /* KUKU */
    kuku_sum += body->value;
    kuku_cnt++;
#endif

    if (ALARM_RISING != body->prev_alarm &&
        body->value >= body->rising_threshold &&
        SNMP_ERR_NOERROR == event_api_send_alarm(1, hdr_ptr->ctrl_index,
                                                 body->rising_event_index,
                                                 body->var_name.objid,
                                                 body->var_name.length,
                                                 ALARM_RISING, body->value,
                                                 body->rising_threshold,
                                                 "Rising"))
        body->prev_alarm = ALARM_RISING;
    else if (ALARM_FALLING != body->prev_alarm &&
             body->value <= body->falling_threshold &&
             SNMP_ERR_NOERROR == event_api_send_alarm(0,
                                                      hdr_ptr->ctrl_index,
                                                      body->
                                                      falling_event_index,
                                                      body->var_name.objid,
                                                      body->var_name.
                                                      length, ALARM_RISING,
                                                      body->value,
                                                      body->
                                                      falling_threshold,
                                                      "Falling"))
        body->prev_alarm = ALARM_FALLING;
}

/*
 * Control Table RowApi Callbacks 
 */

int
alarm_Create(RMON_ENTRY_T * eptr)
{                               /* create the body: alloc it and set defaults */
    CRTL_ENTRY_T   *body;
    static VAR_OID_T DEFAULT_VAR = { 12,        /* etherStatsPkts.1 */
        {1, 3, 6, 1, 2, 1, 16, 1, 1, 1, 5, 1}
    };


    eptr->body = AGMALLOC(sizeof(CRTL_ENTRY_T));
    if (!eptr->body)
        return -3;
    body = (CRTL_ENTRY_T *) eptr->body;

    /*
     * set defaults 
     */
    body->interval = 1;
    memcpy(&body->var_name, &DEFAULT_VAR, sizeof(VAR_OID_T));
    body->sample_type = SAMPLE_TYPE_ABSOLUTE;
    body->startup_type = ALARM_BOTH;
    body->rising_threshold = MMM_MAX;
    body->falling_threshold = 0;
    body->rising_event_index = body->falling_event_index = 0;

    body->prev_alarm = ALARM_NOTHING;

    return 0;
}

int
alarm_Validate(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;

    if (body->rising_threshold <= body->falling_threshold) {
        ag_trace("alarm_Validate failed: %lu must be > %lu",
                 body->rising_threshold, body->falling_threshold);
        return SNMP_ERR_BADVALUE;
    }

    return 0;
}

int
alarm_Activate(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;
    int             ierr;

#if 0                           /* KUKU */
    kuku_sum = 0;
    kuku_cnt = 0;
#endif
    ierr = fetch_var_val(body->var_name.objid,
                         body->var_name.length, &body->last_abs_value);
    if (SNMP_ERR_NOERROR != ierr) {
        ag_trace("Can't fetch var_name");
        return ierr;
    }

    if (SAMPLE_TYPE_ABSOLUTE != body->sample_type) {
        /*
         * check startup alarm 
         */
        if (ALARM_RISING == body->startup_type ||
            ALARM_BOTH == body->startup_type) {
            if (body->last_abs_value >= body->rising_threshold) {
                event_api_send_alarm(1, eptr->ctrl_index,
                                     body->rising_event_index,
                                     body->var_name.objid,
                                     body->var_name.length,
                                     ALARM_RISING, body->value,
                                     body->rising_threshold,
                                     "Startup Rising");
            }
        }

        if (ALARM_FALLING == body->startup_type ||
            ALARM_BOTH == body->startup_type) {
            if (body->last_abs_value <= body->falling_threshold) {
                event_api_send_alarm(0, eptr->ctrl_index,
                                     body->falling_event_index,
                                     body->var_name.objid,
                                     body->var_name.length,
                                     ALARM_RISING, body->value,
                                     body->falling_threshold,
                                     "Startup Falling");
            }
        }

    }

    body->timer_id = snmp_alarm_register(body->interval, SA_REPEAT,
                                         alarm_check_var, eptr);
    return 0;
}

int
alarm_Deactivate(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;

    snmp_alarm_unregister(body->timer_id);
#if 0                           /* KUKU */
    ag_trace("kuku_sum=%ld kuku_cnt=%ld sp=%ld",
             (long) kuku_sum, (long) kuku_cnt,
             (long) (kuku_sum / kuku_cnt));
#endif
    return 0;
}

int
alarm_Copy(RMON_ENTRY_T * eptr)
{
    CRTL_ENTRY_T   *body = (CRTL_ENTRY_T *) eptr->body;
    CRTL_ENTRY_T   *clone = (CRTL_ENTRY_T *) eptr->tmp;

    if (RMON1_ENTRY_VALID == eptr->status &&
        clone->rising_threshold <= clone->falling_threshold) {
        ag_trace("alarm_Copy failed: invalid thresholds");
        return SNMP_ERR_BADVALUE;
    }

    if (clone->interval != body->interval) {
        if (RMON1_ENTRY_VALID == eptr->status) {
            snmp_alarm_unregister(body->timer_id);
            body->timer_id =
                snmp_alarm_register(clone->interval, SA_REPEAT,
                                    alarm_check_var, eptr);
        }
        body->interval = clone->interval;
    }

    if (snmp_oid_compare(clone->var_name.objid, clone->var_name.length,
                         body->var_name.objid, body->var_name.length)) {
        memcpy(&body->var_name, &clone->var_name, sizeof(VAR_OID_T));
    }

    body->sample_type = clone->sample_type;
    body->startup_type = clone->startup_type;
    body->sample_type = clone->sample_type;
    body->rising_threshold = clone->rising_threshold;
    body->falling_threshold = clone->falling_threshold;
    body->rising_event_index = clone->rising_event_index;
    body->falling_event_index = clone->falling_event_index;
    /*
     * ag_trace ("alarm_Copy: rising_threshold=%lu falling_threshold=%lu",
     * body->rising_threshold, body->falling_threshold); 
     */
    return 0;
}

static int
write_alarmEntry(int action, u_char * var_val, u_char var_val_type,
                 size_t var_val_len, u_char * statP,
                 oid * name, size_t name_len)
{
    long            long_tmp;
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
        return ROWAPI_do_another_action(name, alarmEntryFirstIndexBegin,
                                        action, &prev_action,
                                        table_ptr, sizeof(CRTL_ENTRY_T));
    case RESERVE2:
        /*
         * get values from PDU, check them and save them in the cloned entry 
         */
        long_tmp = name[alarmEntryFirstIndexBegin];
        leaf_id = (int) name[alarmEntryFirstIndexBegin - 1];
        hdr = ROWAPI_find(table_ptr, long_tmp); /* it MUST be OK */
        cloned_body = (CRTL_ENTRY_T *) hdr->tmp;
        body = (CRTL_ENTRY_T *) hdr->body;
        switch (leaf_id) {
        case IDalarmInterval:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               0, MMM_MAX, &long_tmp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            cloned_body->interval = long_tmp;
            break;
        case IDalarmVariable:
            snmp_status = AGUTIL_get_oid_value(var_val, var_val_type,
                                               var_val_len,
                                               &cloned_body->var_name);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            if (RMON1_ENTRY_UNDER_CREATION != hdr->status &&
                snmp_oid_compare(cloned_body->var_name.objid,
                                 cloned_body->var_name.length,
                                 body->var_name.objid,
                                 body->var_name.length))
                return SNMP_ERR_BADVALUE;
            break;

            break;
        case IDalarmSampleType:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               SAMPLE_TYPE_ABSOLUTE,
                                               SAMPLE_TYPE_DELTE,
                                               &long_tmp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            cloned_body->sample_type = long_tmp;
            break;
        case IDalarmStartupAlarm:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               ALARM_RISING,
                                               ALARM_BOTH, &long_tmp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            cloned_body->startup_type = long_tmp;
            break;
        case IDalarmRisingThreshold:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               0, MMM_MAX, &long_tmp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            cloned_body->rising_threshold = long_tmp;
            break;
        case IDalarmFallingThreshold:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               0, 0xFFFFFFFFl, &long_tmp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            cloned_body->falling_threshold = long_tmp;
            break;
        case IDalarmRisingEventIndex:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type, var_val_len, 0,   /* min. value */
                                               0,       /* max. value */
                                               &long_tmp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            cloned_body->rising_event_index = long_tmp;
            break;
        case IDalarmFallingEventIndex:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type, var_val_len, 0,   /* min. value */
                                               0,       /* max. value */
                                               &long_tmp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            cloned_body->falling_event_index = long_tmp;
            break;
        case IDalarmOwner:
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
        case IDalarmStatus:
            snmp_status = AGUTIL_get_int_value(var_val, var_val_type,
                                               var_val_len,
                                               RMON1_ENTRY_VALID,
                                               RMON1_ENTRY_INVALID,
                                               &long_tmp);
            if (SNMP_ERR_NOERROR != snmp_status) {
                return snmp_status;
            }
            hdr->new_status = long_tmp;
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

u_char         *
var_alarmEntry(struct variable * vp, oid * name, size_t * length,
               int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_return;
    static CRTL_ENTRY_T theEntry;
    RMON_ENTRY_T   *hdr;

    *write_method = write_alarmEntry;
    hdr = ROWAPI_header_ControlEntry(vp, name, length, exact, var_len,
                                     table_ptr,
                                     &theEntry, sizeof(CRTL_ENTRY_T));
    if (!hdr)
        return NULL;

    *var_len = sizeof(long);    /* default */

    switch (vp->magic) {
    case IDalarmIndex:
        long_return = hdr->ctrl_index;
        return (u_char *) & long_return;
    case IDalarmInterval:
        long_return = theEntry.interval;
        return (u_char *) & long_return;
    case IDalarmVariable:
        *var_len = sizeof(oid) * theEntry.var_name.length;
        return (unsigned char *) theEntry.var_name.objid;
        return (u_char *) & long_return;
    case IDalarmSampleType:
        long_return = theEntry.sample_type;
        return (u_char *) & long_return;
    case IDalarmValue:
        long_return = theEntry.value;
        return (u_char *) & long_return;
    case IDalarmStartupAlarm:
        long_return = theEntry.startup_type;
        return (u_char *) & long_return;
    case IDalarmRisingThreshold:
        long_return = theEntry.rising_threshold;
        return (u_char *) & long_return;
    case IDalarmFallingThreshold:
        long_return = theEntry.falling_threshold;
        return (u_char *) & long_return;
    case IDalarmRisingEventIndex:
        long_return = theEntry.rising_event_index;
        return (u_char *) & long_return;
    case IDalarmFallingEventIndex:
        long_return = theEntry.falling_event_index;
        return (u_char *) & long_return;
    case IDalarmOwner:
        if (hdr->owner) {
            *var_len = strlen(hdr->owner);
            return (unsigned char *) hdr->owner;
        } else {
            *var_len = 0;
            return (unsigned char *) "";
        }

    case IDalarmStatus:
        long_return = hdr->status;
        return (u_char *) & long_return;
    default:
        ag_trace("%s: unknown vp->magic=%d", table_ptr->name,
                 (int) vp->magic);
        ERROR_MSG("");
    };                          /* of switch by 'vp->magic' */

    return NULL;
}

/*
 * Registration & Initializatio section 
 */

oid             oidalarmVariablesOid[] = { 1, 3, 6, 1, 2, 1, 16, 3 };

struct variable7 oidalarmVariables[] = {
    {IDalarmIndex, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_alarmEntry, 3, {1, 1, 1}},
    {IDalarmInterval, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 2}},
    {IDalarmVariable, ASN_OBJECT_ID, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 3}},
    {IDalarmSampleType, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 4}},
    {IDalarmValue, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_alarmEntry, 3, {1, 1, 5}},
    {IDalarmStartupAlarm, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 6}},
    {IDalarmRisingThreshold, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 7}},
    {IDalarmFallingThreshold, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 8}},
    {IDalarmRisingEventIndex, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 9}},
    {IDalarmFallingEventIndex, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 10}},
    {IDalarmOwner, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 11}},
    {IDalarmStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_alarmEntry, 3, {1, 1, 12}}
};

void
init_alarm(void)
{
    REGISTER_MIB("alarmTable", oidalarmVariables, variable7,
                 oidalarmVariablesOid);

    ROWAPI_init_table(&AlarmCtrlTable, "Alarm", 0, &alarm_Create, NULL, /* &alarm_Clone, */
                      NULL,     /* &alarm_Delete, */
                      &alarm_Validate,
                      &alarm_Activate, &alarm_Deactivate, &alarm_Copy);
}

/*
 * end of file alarm.c 
 */
