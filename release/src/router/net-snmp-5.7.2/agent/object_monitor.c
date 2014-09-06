/*
 * object_monitor.c
 *
 * $Id$
 *
 * functions and data structures for cooperating code to monitor objects.
 *
 * WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING!
 * WARNING!                                                       WARNING!
 * WARNING!                                                       WARNING!
 * WARNING!         This code is under active development         WARNING!
 * WARNING!         and is subject to change at any time.         WARNING!
 * WARNING!                                                       WARNING!
 * WARNING!                                                       WARNING!
 * WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING!
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_assert.h>

#include "net-snmp/agent/object_monitor.h"

#if ! defined TRUE
#  define TRUE 1
#elsif TRUE != 1
error "TRUE != 1"
#endif
/**************************************************************************
 *
 * Private data structures
 *
 **************************************************************************/
    /*
     * individual callback info for an object
     */
    typedef struct monitor_info_s {

   /** priority for this callback */
    int             priority;

   /** handler that registred to watch this object */
    netsnmp_mib_handler *watcher;

   /** events that the watcher cares about */
    unsigned int    events;

   /** callback function */
    netsnmp_object_monitor_callback *cb;

   /** pointer to data from the watcher */
    void           *watcher_data;

    struct monitor_info_s *next;

} monitor_info;

/*
 * list of watchers for a given object
 */
typedef struct watcher_list_s {

   /** netsnmp_index must be first! */
    netsnmp_index  monitored_object;

    monitor_info   *head;

} watcher_list;

/*
 * temp holder for ordered list of callbacks
 */
typedef struct callback_placeholder_s {

    monitor_info   *mi;
    netsnmp_monitor_callback_header *cbh;

    struct callback_placeholder_s *next;

} callback_placeholder;


/**************************************************************************
 *
 * 
 *
 **************************************************************************/

/*
 * local statics
 */
static char     need_init = 1;
static netsnmp_container *monitored_objects = NULL;
static netsnmp_monitor_callback_header *callback_pending_list;
static callback_placeholder *callback_ready_list;

/*
 * local prototypes
 */
static watcher_list *find_watchers(oid * object, size_t oid_len);
static int      insert_watcher(oid *, size_t, monitor_info *);
static int      check_registered(unsigned int event, oid * o, int o_l,
                                 watcher_list ** pWl, monitor_info ** pMi);
static void     move_pending_to_ready(void);


/**************************************************************************
 *
 * Public functions
 *
 **************************************************************************/

/*
 * 
 */
void
netsnmp_monitor_init(void)
{
    if (!need_init)
        return;

    callback_pending_list = NULL;
    callback_ready_list = NULL;

    monitored_objects = netsnmp_container_get("object_monitor:binary_array");
    if (NULL != monitored_objects)
        need_init = 0;
    monitored_objects->compare = netsnmp_compare_netsnmp_index;
    monitored_objects->ncompare = netsnmp_ncompare_netsnmp_index;
    
    return;
}


/**************************************************************************
 *
 * Registration functions
 *
 **************************************************************************/

/**
 * Register a callback for the specified object.
 *
 * @param object  pointer to the OID of the object to monitor.
 * @param oid_len length of the OID pointed to by object.
 * @param priority the priority to associate with this callback. A
 *                 higher number indicates higher priority. This
 *                 allows multiple callbacks for the same object to
 *                 coordinate the order in which they are called. If
 *                 two callbacks register with the same priority, the
 *                 order is undefined.
 * @param events  the events which the callback is interested in.
 * @param watcher_data pointer to data that will be supplied to the
 *                     callback method when an event occurs.
 * @param cb pointer to the function to be called when an event occurs.
 *
 * NOTE: the combination of the object, priority and watcher_data must
 *       be unique, as they are the parameters for unregistering a
 *       callback.
 *
 * @return SNMPERR_NOERR registration was successful
 * @return SNMPERR_MALLOC memory allocation failed
 * @return SNMPERR_VALUE the combination of the object, priority and
 *                        watcher_data is not unique.
 */
int
netsnmp_monitor_register(oid * object, size_t oid_len, int priority,
                         unsigned int events, void *watcher_data,
                         netsnmp_object_monitor_callback * cb)
{
    monitor_info   *mi;
    int             rc;

    netsnmp_assert(need_init == 0);

    mi = calloc(1, sizeof(monitor_info));
    if (NULL == mi)
        return SNMPERR_MALLOC;

    mi->priority = priority;
    mi->events = events;
    mi->watcher_data = watcher_data;
    mi->cb = cb;

    rc = insert_watcher(object, oid_len, mi);
    if (rc != SNMPERR_SUCCESS)
        free(mi);

    return rc;
}

/**
 * Unregister a callback for the specified object.
 *
 * @param object  pointer to the OID of the object to monitor.
 * @param oid_len length of the OID pointed to by object.
 * @param priority the priority to associate with this callback.
 * @param wd pointer to data that was supplied when the
 *                     callback was registered.
 * @param cb pointer to the function to be called when an event occurs.
 */
int
netsnmp_monitor_unregister(oid * object, size_t oid_len, int priority,
                           void *wd, netsnmp_object_monitor_callback * cb)
{
    monitor_info   *mi, *last;

    watcher_list   *wl = find_watchers(object, oid_len);
    if (NULL == wl)
        return SNMPERR_GENERR;

    last = NULL;
    mi = wl->head;
    while (mi) {
        if ((mi->cb == cb) && (mi->priority == priority) &&
            (mi->watcher_data == wd))
            break;
        last = mi;
        mi = mi->next;
    }

    if (NULL == mi)
        return SNMPERR_GENERR;

    if (NULL == last)
        wl->head = mi->next;
    else
        last->next = mi->next;

    if (NULL == wl->head) {
        CONTAINER_REMOVE(monitored_objects, wl);
        free(wl->monitored_object.oids);
        free(wl);
    }

    free(mi);

    return SNMPERR_SUCCESS;
}

/**************************************************************************
 *
 * object monitor functions
 *
 **************************************************************************/

/**
 * Notifies the object monitor of an event.
 *
 * The object monitor funtions will save the callback information
 * until all varbinds in the current PDU have been processed and
 * a response has been sent. At that time, the object monitor will
 * determine if there are any watchers monitoring for the event.
 *
 * NOTE: the actual type of the callback structure may vary. The
 *       object monitor functions require only that the start of
 *       the structure match the netsnmp_monitor_callback_header
 *       structure. It is up to the watcher and monitored objects
 *       to agree on the format of other data.
 *
 * @param cbh pointer to a callback header.
 */
void
netsnmp_notify_monitor(netsnmp_monitor_callback_header * cbh)
{

    netsnmp_assert(need_init == 0);

    /*
     * put processing of until response has been sent
     */
    cbh->private = callback_pending_list;
    callback_pending_list = cbh;

    return;
}

/**
 * check to see if a registration exists for an object/event combination
 *
 * @param event the event type to check for
 * @param o     the oid to check for
 * @param o_l   the length of the oid
 *
 * @returns TRUE(1) if a callback is registerd
 * @returns FALSE(0) if no callback is registered
 */
int
netsnmp_monitor_check_registered(int event, oid * o, int o_l)
{
    return check_registered(event, o, o_l, NULL, NULL);
}

/**
 * Process all pending callbacks
 *
 * NOTE: this method is not in the public header, as it should only be
 *       called in one place, in the agent.
 */
void
netsnmp_monitor_process_callbacks(void)
{
    netsnmp_assert(need_init == 0);
    netsnmp_assert(NULL == callback_ready_list);

    if (NULL == callback_pending_list) {
        DEBUGMSGT(("object_monitor", "No callbacks to process"));
        return;
    }

    DEBUGMSG(("object_monitor", "Checking for registered " "callbacks."));

    /*
     * move an pending notification which has a registered watcher to the
     * ready list. Free any other notifications.
     */
    move_pending_to_ready();

    /*
     * call callbacks
     */
    while (callback_ready_list) {

        /*
         * pop off the first item
         */
        callback_placeholder *current_cbr;
        current_cbr = callback_ready_list;
        callback_ready_list = current_cbr->next;

        /*
         * setup, then call callback
         */
        current_cbr->cbh->watcher_data = current_cbr->mi->watcher_data;
        current_cbr->cbh->priority = current_cbr->mi->priority;
        (*current_cbr->mi->cb) (current_cbr->cbh);

        /*
         * release memory (don't free current_cbr->mi)
         */
        if (--(current_cbr->cbh->refs) == 0) {
            free(current_cbr->cbh->monitored_object.oids);
            free(current_cbr->cbh);
        }
        free(current_cbr);

        /*
         * check for any new pending notifications
         */
        move_pending_to_ready();

    }

    netsnmp_assert(callback_ready_list == NULL);
    netsnmp_assert(callback_pending_list = NULL);

    return;
}

/**************************************************************************
 *
 * COOPERATIVE helpers
 *
 **************************************************************************/
/**
 * Notifies the object monitor of a cooperative event.
 *
 * This convenience function will build a
 * ::netsnmp_monitor_callback_header and call
 * netsnmp_notify_monitor().
 *
 * @param event the event type
 * @param  o pointer to the oid of the object sending the event
 * @param o_len    the lenght of the oid
 * @param o_steal set to true if the function may keep the pointer
 *                  to the memory (and free it later). set to false
 *                  to make the function allocate and copy the oid.
 * @param object_info pointer to data supplied by the object for
 *                    the callback. This pointer must remain valid,
 *                    will be provided to each callback registered
 *                    for the object (i.e. it will not be copied or
 *                    freed).
 */
void
netsnmp_notify_cooperative(int event, oid * o, size_t o_len, char o_steal,
                           void *object_info)
{
    netsnmp_monitor_callback_cooperative *cbh;

    netsnmp_assert(need_init == 0);

    cbh = SNMP_MALLOC_TYPEDEF(netsnmp_monitor_callback_cooperative);
    if (NULL == cbh) {
        snmp_log(LOG_ERR, "could not allocate memory for "
                 "cooperative callback");
        return;
    }

    cbh->hdr.event = event;
    cbh->hdr.object_info = object_info;
    cbh->hdr.monitored_object.len = o_len;

    if (o_steal) {
        cbh->hdr.monitored_object.oids = o;
    } else {
        cbh->hdr.monitored_object.oids = snmp_duplicate_objid(o, o_len);
    }

    netsnmp_notify_monitor((netsnmp_monitor_callback_header *) cbh);
}

/** @cond */
/*************************************************************************
 *************************************************************************
 *************************************************************************
 * WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING!
 * WARNING!                                                       WARNING!
 * WARNING!                                                       WARNING!
 * WARNING!         This code is under active development         WARNING!
 * WARNING!         and is subject to change at any time.         WARNING!
 * WARNING!                                                       WARNING!
 * WARNING!                                                       WARNING!
 * WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING! WARNING!
 *************************************************************************
 *************************************************************************
 *************************************************************************
 */
static watcher_list *
find_watchers(oid * object, size_t oid_len)
{
    netsnmp_index oah;

    oah.oids = object;
    oah.len = oid_len;

    return (watcher_list *)CONTAINER_FIND(monitored_objects, &oah);
}

static int
insert_watcher(oid * object, size_t oid_len, monitor_info * mi)
{
    watcher_list   *wl = find_watchers(object, oid_len);
    int             rc = SNMPERR_SUCCESS;

    if (NULL != wl) {

        monitor_info   *last, *current;

        netsnmp_assert(wl->head != NULL);

        last = NULL;
        current = wl->head;
        while (current) {
            if (mi->priority == current->priority) {
                /*
                 * check for duplicate
                 */
                if (mi->watcher_data == current->watcher_data)
                    return SNMPERR_VALUE; /** duplicate! */
            } else if (mi->priority > current->priority) {
                break;
            }
            last = current;
            current = current->next;
        }
        if (NULL == last) {
            mi->next = wl->head;
            wl->head = mi;
        } else {
            mi->next = last->next;
            last->next = mi;
        }
    } else {

        /*
         * first watcher for this oid; set up list
         */
        wl = SNMP_MALLOC_TYPEDEF(watcher_list);
        if (NULL == wl)
            return SNMPERR_MALLOC;

        /*
         * copy index oid
         */
        wl->monitored_object.len = oid_len;
        wl->monitored_object.oids = malloc(sizeof(oid) * oid_len);
        if (NULL == wl->monitored_object.oids) {
            free(wl);
            return SNMPERR_MALLOC;
        }
        memcpy(wl->monitored_object.oids, object, sizeof(oid) * oid_len);

        /*
         * add watcher, and insert into array
         */
        wl->head = mi;
        mi->next = NULL;
        rc = CONTAINER_INSERT(monitored_objects, wl);
        if (rc) {
            free(wl->monitored_object.oids);
            free(wl);
            return rc;
        }
    }
    return rc;
}

/**
 * @internal
 * check to see if a registration exists for an object/event combination
 *
 * @param event the event type to check for
 * @param o     the oid to check for
 * @param o_l   the length of the oid
 * @param pWl   if a pointer to a watcher_list pointer is supplied,
 *              upon return the watcher list pointer will be set to
 *              the watcher list for the object, or NULL if there are
 *              no watchers for the object.
 * @param pMi   if a pointer to a monitor_info pointer is supplied,
 *              upon return the pointer will be set to the first
 *              monitor_info object for the specified event.
 *
 * @returns TRUE(1) if a callback is registerd
 * @returns FALSE(0) if no callback is registered
 */
static int
check_registered(unsigned int event, oid * o, int o_l,
                 watcher_list ** pWl, monitor_info ** pMi)
{
    watcher_list   *wl;
    monitor_info   *mi;

    netsnmp_assert(need_init == 0);

    /*
     * check to see if anyone has registered for callbacks
     * for the object.
     */
    wl = find_watchers(o, o_l);
    if (pWl)
        *pWl = wl;
    if (NULL == wl)
        return 0;

    /*
     * check if any watchers are watching for this specific event
     */
    for (mi = wl->head; mi; mi = mi->next) {

        if (mi->events & event) {
            if (pMi)
                *pMi = mi;
            return TRUE;
        }
    }

    return 0;
}

/**
 *@internal
 */
inline void
insert_ready(callback_placeholder * new_cbr)
{
    callback_placeholder *current_cbr, *last_cbr;

    /*
     * insert in callback ready list
     */
    last_cbr = NULL;
    current_cbr = callback_ready_list;
    while (current_cbr) {

        if (new_cbr->mi->priority > current_cbr->mi->priority)
            break;

        last_cbr = current_cbr;
        current_cbr = current_cbr->next;
    }
    if (NULL == last_cbr) {
        new_cbr->next = callback_ready_list;
        callback_ready_list = new_cbr;
    } else {
        new_cbr->next = last_cbr->next;
        last_cbr->next = new_cbr;
    }
}

/**
 *@internal
 *
 * move an pending notification which has a registered watcher to the
 * ready list. Free any other notifications.
 */
static void
move_pending_to_ready(void)
{
    /*
     * check to see if anyone has registered for callbacks
     * for each object.
     */
    while (callback_pending_list) {

        watcher_list   *wl;
        monitor_info   *mi;
        netsnmp_monitor_callback_header *cbp;

        /*
         * pop off first item
         */
        cbp = callback_pending_list;
        callback_pending_list = cbp->private; /** next */

        if (0 == check_registered(cbp->event, cbp->monitored_object.oids,
                                  cbp->monitored_object.len, &wl,
                                  &mi)) {

            /*
             * nobody watching, free memory
             */
            free(cbp);
            continue;
        }

        /*
         * Found at least one; check the rest of the list and
         * save callback for processing
         */
        for (; mi; mi = mi->next) {

            callback_placeholder *new_cbr;

            if (0 == (mi->events & cbp->event))
                continue;

            /*
             * create temprory placeholder.
             *
             * I hate to allocate memory here, as I'd like this code to
             * be fast and lean. But I don't have time to think of another
             * solution os this will have to do for now.
             *
             * I need a list of monitor_info (mi) objects for each
             * callback which has registered for the event, and want
             * that list sorted by the priority required by the watcher.
             */
            new_cbr = SNMP_MALLOC_TYPEDEF(callback_placeholder);
            if (NULL == new_cbr) {
                snmp_log(LOG_ERR, "malloc failed, callback dropped.");
                continue;
            }
            new_cbr->cbh = cbp;
            new_cbr->mi = mi;
            ++cbp->refs;

            /*
             * insert in callback ready list
             */
            insert_ready(new_cbr);

        } /** end mi loop */
    } /** end cbp loop */

    netsnmp_assert(callback_pending_list == NULL);
}


#if defined TESTING_OBJECT_MONITOR
/**************************************************************************
 *
 * (untested) TEST CODE
 *
 */
void
dummy_callback(netsnmp_monitor_callback_header * cbh)
{
    printf("Callback received.\n");
}

void
dump_watchers(netsnmp_index *oah, void *)
{
    watcher_list   *wl = (watcher_list *) oah;
    netsnmp_monitor_callback_header *cbh = wl->head;

    printf("Watcher List for OID ");
    print_objid(wl->hdr->oids, wl->hdr->len);
    printf("\n");

    while (cbh) {

        printf("Priority = %d;, Events = %d; Watcher Data = 0x%x\n",
               cbh->priority, cbh->events, cbh->watcher_data);

        cbh = cbh->private;
    }
}

void
main(int argc, char **argv)
{

    oid             object[3] = { 1, 3, 6 };
    int             object_len = 3;
    int             rc;

    /*
     * init
     */
    netsnmp_monitor_init();

    /*
     * insert an object
     */
    rc = netsnmp_monitor_register(object, object_len, 0,
                                  EVENT_ROW_ADD, (void *) 0xdeadbeef,
                                  dummy_callback);
    printf("insert an object: %d\n", rc);

    /*
     * insert same object, new priority
     */
    netsnmp_monitor_register(object, object_len, 10,
                             EVENT_ROW_ADD, (void *) 0xdeadbeef,
                             dummy_callback);
    printf("insert same object, new priority: %d\n", rc);

    /*
     * insert same object, same priority, new data
     */
    netsnmp_monitor_register(object, object_len, 10,
                             EVENT_ROW_ADD, (void *) 0xbeefdead,
                             dummy_callback);
    printf("insert same object, same priority, new data: %d\n", rc);

    /*
     * insert same object, same priority, same data
     */
    netsnmp_monitor_register(object, object_len, 10,
                             EVENT_ROW_ADD, (void *) 0xbeefdead,
                             dummy_callback);
    printf("insert same object, same priority, new data: %d\n", rc);


    /*
     * dump table
     */
    CONTAINER_FOR_EACH(monitored_objects, dump_watchers, NULL);
}
#endif /** defined TESTING_OBJECT_MONITOR */

/** @endcond */


