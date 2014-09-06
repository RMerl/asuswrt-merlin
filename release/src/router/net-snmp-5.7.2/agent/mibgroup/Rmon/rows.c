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
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "agutil_api.h"
#include "rows.h"
#include "row_api.h"

#define MAX_CREATION_TIME	60

/*
 * *************************** 
 */
/*
 * static file scope functions 
 */
/*
 * *************************** 
 */

static void
rowapi_delete(RMON_ENTRY_T * eold)
{
    register RMON_ENTRY_T *eptr;
    register RMON_ENTRY_T *prev = NULL;
    TABLE_DEFINTION_T *table_ptr;

    table_ptr = (TABLE_DEFINTION_T *) eold->table_ptr;

    /*
     * delete timout scheduling 
     */
    snmp_alarm_unregister(eold->timer_id);
    ag_trace("Entry %ld in %s has been deleted",
             eold->ctrl_index, table_ptr->name);

    /*
     * It it was valid entry => deactivate it 
     */
    if (RMON1_ENTRY_VALID == eold->status) {
        if (table_ptr->ClbkDeactivate)
            table_ptr->ClbkDeactivate(eold);
    }

    /*
     * delete it in users's sence 
     */
    if (table_ptr->ClbkDelete)
        table_ptr->ClbkDelete((RMON_ENTRY_T *) eold->body);

    if (eold->body) {
        AGFREE(eold->body);
    }

    if (eold->owner)
        AGFREE(eold->owner);

    /*
     * delete it from the list in table 
     */

    table_ptr->current_number_of_entries--;

    for (eptr = table_ptr->first; eptr; eptr = eptr->next) {
        if (eptr == eold)
            break;
        prev = eptr;
    }

    if (prev)
        prev->next = eold->next;
    else
        table_ptr->first = eold->next;

    AGFREE(eold);
}

static void
rowapi_too_long_creation_callback(unsigned int clientreg, void *clientarg)
{
    RMON_ENTRY_T   *eptr;
    TABLE_DEFINTION_T *table_ptr;

    eptr = (RMON_ENTRY_T *) clientarg;
    table_ptr = (TABLE_DEFINTION_T *) eptr->table_ptr;
    if (RMON1_ENTRY_VALID != eptr->status) {
        ag_trace("row #%d in %s was under creation more then %ld sec.",
                 eptr->ctrl_index, table_ptr->name,
                 (long) MAX_CREATION_TIME);
        rowapi_delete(eptr);
    } else {
        snmp_alarm_unregister(eptr->timer_id);
    }
}

static int
rowapi_deactivate(TABLE_DEFINTION_T * table_ptr, RMON_ENTRY_T * eptr)
{
    if (RMON1_ENTRY_UNDER_CREATION == eptr->status) {
        /*
         * nothing to do 
         */
        return SNMP_ERR_NOERROR;
    }

    if (table_ptr->ClbkDeactivate)
        table_ptr->ClbkDeactivate(eptr);
    eptr->status = RMON1_ENTRY_UNDER_CREATION;
    eptr->timer_id = snmp_alarm_register(MAX_CREATION_TIME, 0,
                                         rowapi_too_long_creation_callback,
                                         eptr);
    ag_trace("Entry %ld in %s has been deactivated",
             eptr->ctrl_index, table_ptr->name);

    return SNMP_ERR_NOERROR;
}

static int
rowapi_activate(TABLE_DEFINTION_T * table_ptr, RMON_ENTRY_T * eptr)
{
    RMON1_ENTRY_STATUS_T prev_status = eptr->status;

    eptr->status = RMON1_ENTRY_VALID;

    if (table_ptr->ClbkActivate) {
        if (0 != table_ptr->ClbkActivate(eptr)) {
            ag_trace("Can't activate entry #%ld in %s",
                     eptr->ctrl_index, table_ptr->name);
            eptr->status = prev_status;
            return SNMP_ERR_BADVALUE;
        }
    }

    snmp_alarm_unregister(eptr->timer_id);
    eptr->timer_id = 0;
    ag_trace("Entry %ld in %s has been activated",
             eptr->ctrl_index, table_ptr->name);
    return SNMP_ERR_NOERROR;
}

/*
 * creates an entry, locats it in proper sorted order by index
 * Row is initialized to zero,
 * except: 'next', 'table_ptr', 'index',
 * 'timer_id' & 'status'=(RMON1_ENTRY_UNDER_CREATION)
 * Calls (if need) ClbkCreate.
 * Schedules for timeout under entry creation (id of this
 * scheduling is saved in 'timer_id').
 * Returns 0: OK,
 -1:max. number exedes;
 -2:malloc failed;
 -3:ClbkCreate failed */
int
ROWAPI_new(TABLE_DEFINTION_T * table_ptr, u_long ctrl_index)
{
    register RMON_ENTRY_T *eptr;
    register RMON_ENTRY_T *prev = NULL;
    register RMON_ENTRY_T *enew;

    /*
     * check on 'max.number' 
     */
    if (table_ptr->max_number_of_entries > 0 &&
        table_ptr->current_number_of_entries >=
        table_ptr->max_number_of_entries)
        return -1;

    /*
     * allocate memory for the header 
     */
    enew = (RMON_ENTRY_T *) AGMALLOC(sizeof(RMON_ENTRY_T));
    if (!enew)
        return -2;

    /*
     * init the header 
     */
    memset(enew, 0, sizeof(RMON_ENTRY_T));
    enew->ctrl_index = ctrl_index;
    enew->table_ptr = (void *) table_ptr;
    enew->status = RMON1_ENTRY_UNDER_CREATION;
    enew->only_just_created = 1;

    /*
     * create the body: alloc it and set defaults 
     */
    if (table_ptr->ClbkCreate) {
        if (0 != table_ptr->ClbkCreate(enew)) {
            AGFREE(enew);
            return -3;
        }
    }

    table_ptr->current_number_of_entries++;

    /*
     * find the place : before 'eptr' and after 'prev' 
     */
    for (eptr = table_ptr->first; eptr; eptr = eptr->next) {
        if (ctrl_index < eptr->ctrl_index)
            break;
        prev = eptr;
    }

    /*
     * insert it 
     */
    enew->next = eptr;
    if (prev)
        prev->next = enew;
    else
        table_ptr->first = enew;

    enew->timer_id = snmp_alarm_register(MAX_CREATION_TIME, 0,
                                         rowapi_too_long_creation_callback,
                                         enew);
    ag_trace("Entry %ld in %s has been created",
             enew->ctrl_index, table_ptr->name);
    return 0;
}

/*
 * ****************************** 
 */
/*
 * external usage (API) functions 
 */
/*
 * ****************************** 
 */

void
ROWAPI_init_table(TABLE_DEFINTION_T * table_ptr,
                  const char *name,
                  u_long max_number_of_entries,
                  ENTRY_CALLBACK_T * ClbkCreate,
                  ENTRY_CALLBACK_T * ClbkClone,
                  ENTRY_CALLBACK_T * ClbkDelete,
                  ENTRY_CALLBACK_T * ClbkValidate,
                  ENTRY_CALLBACK_T * ClbkActivate,
                  ENTRY_CALLBACK_T * ClbkDeactivate,
                  ENTRY_CALLBACK_T * ClbkCopy)
{
    table_ptr->name = name;
    if (!table_ptr->name)
        table_ptr->name = NETSNMP_REMOVE_CONST(char*,"Unknown");

    table_ptr->max_number_of_entries = max_number_of_entries;
    table_ptr->ClbkCreate = ClbkCreate;
    table_ptr->ClbkClone = ClbkClone;
    table_ptr->ClbkDelete = ClbkDelete;
    table_ptr->ClbkValidate = ClbkValidate;
    table_ptr->ClbkActivate = ClbkActivate;
    table_ptr->ClbkDeactivate = ClbkDeactivate;
    table_ptr->ClbkCopy = ClbkCopy;

    table_ptr->first = NULL;
    table_ptr->current_number_of_entries = 0;
}

void
ROWAPI_delete_clone(TABLE_DEFINTION_T * table_ptr, u_long ctrl_index)
{
    register RMON_ENTRY_T *eptr;

    eptr = ROWAPI_find(table_ptr, ctrl_index);
    if (eptr) {
        if (eptr->new_owner)
            AGFREE(eptr->new_owner);

        if (eptr->tmp) {
            if (table_ptr->ClbkDelete)
                table_ptr->ClbkDelete((RMON_ENTRY_T *) eptr->tmp);
            AGFREE(eptr->tmp);
        }

        if (eptr->only_just_created) {
            rowapi_delete(eptr);
        }
    }
}

RMON_ENTRY_T   *
ROWAPI_get_clone(TABLE_DEFINTION_T * table_ptr,
                 u_long ctrl_index, size_t body_size)
{
    register RMON_ENTRY_T *eptr;

    if (ctrl_index < 1 || ctrl_index > 0xFFFFu) {
        ag_trace("%s: index %ld out of range (1..65535)",
                 table_ptr->name, (long) ctrl_index);
        return NULL;
    }

    /*
     * get it 
     */
    eptr = ROWAPI_find(table_ptr, ctrl_index);

    if (!eptr) {                /* try to create */
        if (0 != ROWAPI_new(table_ptr, ctrl_index)) {
            return NULL;
        }

        /*
         * get it 
         */
        eptr = ROWAPI_find(table_ptr, ctrl_index);
        if (!eptr)              /* it is unbelievable, but ... :( */
            return NULL;
    }

    eptr->new_status = eptr->status;

    eptr->tmp = AGMALLOC(body_size);
    if (!eptr->tmp) {
        if (eptr->only_just_created)
            rowapi_delete(eptr);
        return NULL;
    }

    memcpy(eptr->tmp, eptr->body, body_size);
    if (table_ptr->ClbkClone)
        table_ptr->ClbkClone(eptr);

    if (eptr->new_owner)
        AGFREE(eptr->new_owner);
    return eptr->tmp;
}

RMON_ENTRY_T   *
ROWAPI_first(TABLE_DEFINTION_T * table_ptr)
{
    return table_ptr->first;
}

/*
 * returns an entry with the smallest index
 * which index > prev_index
 */
RMON_ENTRY_T   *
ROWAPI_next(TABLE_DEFINTION_T * table_ptr, u_long prev_index)
{
    register RMON_ENTRY_T *eptr;

    for (eptr = table_ptr->first; eptr; eptr = eptr->next)
        if (eptr->ctrl_index > prev_index)
            return eptr;

    return NULL;
}

RMON_ENTRY_T   *
ROWAPI_find(TABLE_DEFINTION_T * table_ptr, u_long ctrl_index)
{
    register RMON_ENTRY_T *eptr;

    for (eptr = table_ptr->first; eptr; eptr = eptr->next) {
        if (eptr->ctrl_index == ctrl_index)
            return eptr;
        if (eptr->ctrl_index > ctrl_index)
            break;
    }

    return NULL;
}

int
ROWAPI_action_check(TABLE_DEFINTION_T * table_ptr, u_long ctrl_index)
{
    register RMON_ENTRY_T *eptr;

    eptr = ROWAPI_find(table_ptr, ctrl_index);
    if (!eptr) {
        ag_trace("Smth wrong ?");
        return SNMP_ERR_GENERR;
    }

    /*
     * test owner string 
     */
    if (RMON1_ENTRY_UNDER_CREATION != eptr->status) {
        /*
         * Only the same value is allowed 
         */
        if (eptr->new_owner &&
            (!eptr->owner
             || strncmp(eptr->new_owner, eptr->owner, MAX_OWNERSTRING))) {
            ag_trace("invalid owner string in ROWAPI_action_check");
            ag_trace("eptr->new_owner=%p eptr->owner=%p", eptr->new_owner,
                     eptr->owner);
            return SNMP_ERR_BADVALUE;
        }
    }

    switch (eptr->new_status) { /* this status we want to set */
    case RMON1_ENTRY_CREATE_REQUEST:
        if (RMON1_ENTRY_UNDER_CREATION != eptr->status)
            return SNMP_ERR_BADVALUE;
        break;
    case RMON1_ENTRY_INVALID:
        break;
    case RMON1_ENTRY_VALID:
        if (RMON1_ENTRY_VALID == eptr->status) {
            break;              /* nothing to do */
        }
        if (RMON1_ENTRY_UNDER_CREATION != eptr->status) {
            ag_trace("Validate %s: entry %ld has wrong status %d",
                     table_ptr->name, (long) ctrl_index,
                     (int) eptr->status);
            return SNMP_ERR_BADVALUE;
        }

        /*
         * Our MIB understanding extension: we permit to set
         * VALID when entry doesn't exit, in this case PDU has to have
         * the nessessary & valid set of non-default values 
         */
        if (table_ptr->ClbkValidate) {
            return table_ptr->ClbkValidate(eptr);
        }
        break;
    case RMON1_ENTRY_UNDER_CREATION:
        /*
         * Our MIB understanding extension: we permit to travel from 
         * VALID to 'UNDER_CREATION' state 
         */
        break;
    }

    return SNMP_ERR_NOERROR;
}

int
ROWAPI_commit(TABLE_DEFINTION_T * table_ptr, u_long ctrl_index)
{
    register RMON_ENTRY_T *eptr;

    eptr = ROWAPI_find(table_ptr, ctrl_index);
    if (!eptr) {
        ag_trace("Smth wrong ?");
        return SNMP_ERR_GENERR;
    }

    eptr->only_just_created = 0;

    switch (eptr->new_status) { /* this status we want to set */
    case RMON1_ENTRY_CREATE_REQUEST:   /* copy tmp => eprt */
        if (eptr->new_owner) {
            if (eptr->owner)
                AGFREE(eptr->owner);
            eptr->owner = AGSTRDUP(eptr->new_owner);
        }

        if (table_ptr->ClbkCopy && eptr->tmp)
            table_ptr->ClbkCopy(eptr);
        break;
    case RMON1_ENTRY_INVALID:
        ROWAPI_delete_clone(table_ptr, ctrl_index);
        rowapi_delete(eptr);
#if 0                           /* for debug */
        dbg_f_AG_MEM_REPORT();
#endif
        break;
    case RMON1_ENTRY_VALID:    /* copy tmp => eprt and activate */
        /*
         * Our MIB understanding extension: we permit to set
         * VALID when entry doesn't exit, in this case PDU has to have
         * the nessessary & valid set of non-default values 
         */
        if (eptr->new_owner) {
            if (eptr->owner)
                AGFREE(eptr->owner);
            eptr->owner = AGSTRDUP(eptr->new_owner);
        }
        if (table_ptr->ClbkCopy && eptr->tmp)
            table_ptr->ClbkCopy(eptr);
        if (RMON1_ENTRY_VALID != eptr->status) {
            rowapi_activate(table_ptr, eptr);
        }
        break;
    case RMON1_ENTRY_UNDER_CREATION:   /* deactivate (if need) and copy tmp => eprt */
        /*
         * Our MIB understanding extension: we permit to travel from
         * VALID to 'UNDER_CREATION' state 
         */
        rowapi_deactivate(table_ptr, eptr);
        if (eptr->new_owner) {
            if (eptr->owner)
                AGFREE(eptr->owner);
            eptr->owner = AGSTRDUP(eptr->new_owner);
        }
        if (table_ptr->ClbkCopy && eptr->tmp)
            table_ptr->ClbkCopy(eptr);
        break;
    }

    ROWAPI_delete_clone(table_ptr, ctrl_index);
    return SNMP_ERR_NOERROR;
}

RMON_ENTRY_T   *
ROWAPI_header_ControlEntry(struct variable * vp, oid * name,
                           size_t * length, int exact,
                           size_t * var_len,
                           TABLE_DEFINTION_T * table_ptr,
                           void *entry_ptr, size_t entry_size)
{
    long            ctrl_index;
    RMON_ENTRY_T   *hdr = NULL;

    if (0 != AGUTIL_advance_index_name(vp, name, length, exact)) {
        ag_trace("cannot advance_index_name");
        return NULL;
    }

    ctrl_index = vp->namelen >= *length ? 0 : name[vp->namelen];

    if (exact) {
        if (ctrl_index)
            hdr = ROWAPI_find(table_ptr, ctrl_index);
    } else {
        if (ctrl_index)
            hdr = ROWAPI_next(table_ptr, ctrl_index);
        else
            hdr = ROWAPI_first(table_ptr);

        if (hdr) {              /* set new index */
            name[vp->namelen] = hdr->ctrl_index;
            *length = vp->namelen + 1;
        }
    }

    if (hdr)
        memcpy(entry_ptr, hdr->body, entry_size);
    return hdr;
}

int
ROWAPI_do_another_action(oid * name, int tbl_first_index_begin,
                         int action, int *prev_action,
                         TABLE_DEFINTION_T * table_ptr, size_t entry_size)
{
    long            long_temp;
    RMON_ENTRY_T   *tmp;

    if (action == *prev_action)
        return SNMP_ERR_NOERROR;        /* I want to process it only once ! */
    *prev_action = action;

    long_temp = name[tbl_first_index_begin];

    switch (action) {
    case RESERVE1:
        tmp = ROWAPI_get_clone(table_ptr, long_temp, entry_size);
        if (!tmp) {
            ag_trace("RESERVE1: cannot get clone\n");
            return SNMP_ERR_TOOBIG;
        }
        break;

    case FREE:                 /* if RESERVEx failed: release any resources that have been allocated */
    case UNDO:                 /* if ACTION failed: release any resources that have been allocated */
        ROWAPI_delete_clone(table_ptr, long_temp);
        break;

    case ACTION:
        long_temp = ROWAPI_action_check(table_ptr, long_temp);
        if (0 != long_temp)
            return long_temp;
        break;

    case COMMIT:
        long_temp = ROWAPI_commit(table_ptr, long_temp);
        if (0 != long_temp)     /* it MUST NOT be */
            return long_temp;
        break;
    default:
        ag_trace("Unknown action %d", (int) action);
        return SNMP_ERR_GENERR;
    }                           /* of switch by actions */

    return SNMP_ERR_NOERROR;
}

/*
 * data tables API section 
 */

int
ROWDATAAPI_init(SCROLLER_T * scrlr,
                u_long data_requested,
                u_long max_number_of_entries,
                size_t data_size,
                int (*data_destructor) (struct data_scroller *, void *))
{
    scrlr->data_granted = 0;
    scrlr->data_created = 0;
    scrlr->data_total_number = 0;
    scrlr->first_data_ptr =
        scrlr->last_data_ptr = scrlr->current_data_ptr = NULL;

    scrlr->max_number_of_entries = max_number_of_entries;
    scrlr->data_size = data_size;

    scrlr->data_destructor = data_destructor;

    ROWDATAAPI_set_size(scrlr, data_requested, 0);

    return 0;
}

static int
delete_data_entry(SCROLLER_T * scrlr, void *delete_me)
{
    NEXTED_PTR_T   *data_ptr = delete_me;
    register NEXTED_PTR_T *tmp;

    if (data_ptr == scrlr->first_data_ptr) {
        scrlr->first_data_ptr = data_ptr->next;
        if (data_ptr == scrlr->last_data_ptr)
            scrlr->last_data_ptr = NULL;
    } else {                    /* not first */
        for (tmp = scrlr->first_data_ptr; tmp; tmp = tmp->next) {
            if (tmp->next == data_ptr) {
                if (data_ptr == scrlr->last_data_ptr)
                    scrlr->last_data_ptr = tmp;
                tmp->next = data_ptr->next;
                break;
            }
        }                       /* for */
    }                           /* not first */

    if (data_ptr == scrlr->current_data_ptr)
        scrlr->current_data_ptr = data_ptr->next;

    if (scrlr->data_destructor)
        scrlr->data_destructor(scrlr, data_ptr);
    AGFREE(data_ptr);
    scrlr->data_created--;
    scrlr->data_stored--;

    return 0;
}

static void
realloc_number_of_data(SCROLLER_T * scrlr, long dlong)
{
    void           *bptr;       /* DATA_ENTRY_T */
    NEXTED_PTR_T   *prev = NULL;
    void           *first = NULL;

    if (dlong > 0) {
        for (; dlong; dlong--, prev = bptr, scrlr->data_created++) {
            bptr = AGMALLOC(scrlr->data_size);
            if (!bptr) {
                ag_trace("Err: no memory for data");
                break;
            }
            memset(bptr, 0, scrlr->data_size);
            if (prev)
                prev->next = bptr;
            else
                first = bptr;
        }                       /* of loop by malloc bucket */

        if (!scrlr->current_data_ptr)
            scrlr->current_data_ptr = first;
        if (scrlr->last_data_ptr) {
            scrlr->last_data_ptr->next = first;
        } else
            scrlr->first_data_ptr = first;

        scrlr->last_data_ptr = bptr;

    } else {
        for (; dlong && scrlr->data_created > 0; dlong++) {
            if (scrlr->current_data_ptr)
                delete_data_entry(scrlr, scrlr->current_data_ptr);
            else
                delete_data_entry(scrlr, scrlr->first_data_ptr);
        }
    }
}

void
ROWDATAAPI_set_size(SCROLLER_T * scrlr,
                    u_long data_requested, u_char do_allocation)
{
    long            dlong;

    scrlr->data_requested = data_requested;
    scrlr->data_granted = (data_requested < scrlr->max_number_of_entries) ?
        data_requested : scrlr->max_number_of_entries;
    if (do_allocation) {
        dlong = (long) scrlr->data_granted - (long) scrlr->data_created;
        realloc_number_of_data(scrlr, dlong);
    }
}

void
ROWDATAAPI_descructor(SCROLLER_T * scrlr)
{
    register NEXTED_PTR_T *bptr;
    register void  *next;

    for (bptr = scrlr->first_data_ptr; bptr; bptr = next) {
        next = bptr->next;
        if (scrlr->data_destructor)
            scrlr->data_destructor(scrlr, bptr);
        AGFREE(bptr);
    }
    scrlr->data_created = 0;
    scrlr->data_granted = 0;
    scrlr->first_data_ptr =
        scrlr->last_data_ptr = scrlr->current_data_ptr = NULL;
}

void           *
ROWDATAAPI_locate_new_data(SCROLLER_T * scrlr)
{
    register NEXTED_PTR_T *bptr;

    if (!scrlr->current_data_ptr) {     /* there was wrap */
        bptr = scrlr->first_data_ptr;
        if (!bptr) {
            ag_trace("Err: SCROLLER_T:locate_new_data: internal error :(");
            return NULL;
        }
        scrlr->first_data_ptr = bptr->next;
        scrlr->last_data_ptr->next = bptr;
        scrlr->last_data_ptr = (NEXTED_PTR_T *) bptr;
        bptr->next = NULL;
    } else {
        bptr = scrlr->current_data_ptr;
        scrlr->current_data_ptr = bptr->next;
        ++scrlr->data_stored;
    }

    scrlr->data_total_number++;

    return bptr;
}

u_long
ROWDATAAPI_get_total_number(SCROLLER_T * scrlr)
{
    return scrlr->data_total_number;
}

RMON_ENTRY_T   *
ROWDATAAPI_header_DataEntry(struct variable * vp, oid * name,
                            size_t * length, int exact,
                            size_t * var_len,
                            TABLE_DEFINTION_T * table_ptr,
                            SCROLLER_T * (*extract_scroller) (void *body),
                            size_t data_size, void *entry_ptr)
{
    long            ctrl_indx, data_index;
    RMON_ENTRY_T   *hdr = NULL;
    SCROLLER_T     *scrlr;
    NEXTED_PTR_T   *bptr = NULL;
    register u_long iii;

    if (0 != AGUTIL_advance_index_name(vp, name, length, exact)) {
        ag_trace("cannot advance_index_name");
        return NULL;
    }

    ctrl_indx = vp->namelen >= *length ? 0 : name[vp->namelen];
    if (ctrl_indx)
        data_index =
            ((int)(vp->namelen + 1) >= (int)*length) ? 0 : name[vp->namelen + 1];
    else
        data_index = 0;

    if (exact) {
        if (ctrl_indx && data_index) {
            hdr = ROWAPI_find(table_ptr, ctrl_indx);
            if (hdr) {
                scrlr = extract_scroller(hdr->body);
                bptr = scrlr->first_data_ptr;
                for (iii = 0; iii < scrlr->data_stored && bptr;
                     iii++, bptr = bptr->next) {
                    if ((long)bptr->data_index == data_index)
                        break;
                }
                if (!bptr)
                    hdr = NULL;
            }
        }
    } else {
        if (ctrl_indx)
            hdr = ROWAPI_find(table_ptr, ctrl_indx);
        else
            hdr = ROWAPI_first(table_ptr);

        if (hdr) {
            scrlr = extract_scroller(hdr->body);
            /*
             * ag_trace ("get next after (%d %d)", (int) ctrl_indx, (int) data_index); 
             */
            bptr = scrlr->first_data_ptr;
            for (iii = 0; iii < scrlr->data_stored && bptr;
                 iii++, bptr = bptr->next) {
                if (bptr->data_index && (long)bptr->data_index > data_index)
                    break;
            }

            if (bptr && (long)bptr->data_index <= data_index)
                bptr = NULL;

            if (!bptr) {        /* travel to next row */
                /*
                 * ag_trace ("Dbg: travel to next row"); 
                 */
                for (hdr = hdr->next; hdr; hdr = hdr->next) {
                    if (RMON1_ENTRY_VALID != hdr->status)
                        continue;

                    scrlr = extract_scroller(hdr->body);
                    if (scrlr->data_stored <= 0)
                        continue;
                    for (bptr = scrlr->first_data_ptr; bptr;
                         bptr = bptr->next) {
                        if (bptr->data_index)
                            break;
                    }

                    if (bptr)
                        break;
                }
            }
            if (bptr) {         /* set new index */
                /*
                 * ag_trace ("Dbg: So (%d %d)", (int) hdr->index, (int) bptr->data_index); 
                 */
                name[vp->namelen] = hdr->ctrl_index;
                name[vp->namelen + 1] = bptr->data_index;
                *length = vp->namelen + 2;
            } else
                hdr = NULL;
        }
    }

    if (hdr)
        memcpy(entry_ptr, bptr, data_size);
    return hdr;
}

void
init_rows(void)
{
}
