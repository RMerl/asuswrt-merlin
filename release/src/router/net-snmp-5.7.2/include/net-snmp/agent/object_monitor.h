/**************************************************************************
 * object_monitor.h
 *
 * Contributed by: Robert Story <rstory@freesnmp.com>
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
#ifndef OBJECT_MONITOR_H
#define OBJECT_MONITOR_H

#ifdef __cplusplus
extern          "C" {
#endif

    /*
     * notification types
     */
    enum {
        /*
         * cooperative notification from the object being watched
         */
        NOTIFICATION_COOPERATIVE = 1,
        /*
         * notification that an object has been set vi SNMP-SET
         */
        NOTIFICATION_SET_REQUEST,
        /*
         * end of current notification types
         */
        NOTIFICATION_END
    };

    /*
     * COOPERATIVE event types
     */
    enum {
        EVENT_ROW_ADD = 1,
        EVENT_ROW_MOD,
        EVENT_ROW_DEL,
        EVENT_COL_MOD,
        EVENT_OBJ_MOD,
        EVENT_END
    };

    /*
     * data structures
     */


    /**
     * callback header
     */
    typedef struct netsnmp_monitor_callback_header_s {

   /** callback type */
        unsigned int    event;

   /** registered oid */
        netsnmp_index   monitored_object;

   /** priority */
        int             priority;

   /** pointer given by watcher at registration */
        void           *watcher_data;

   /** pointer passed from the monitored object */
        void           *object_info;

   /** DO NOT USE, INTERNAL USE ONLY */
        struct netsnmp_monitor_callback_header_s *private;
        int             refs;


    } netsnmp_monitor_callback_header;

    /*
     *
     */
    typedef struct netsnmp_monitor_callback_set_request_s {

    /** header */
        netsnmp_monitor_callback_header hdr;

    /** handler that registered to handle this object */
        netsnmp_mib_handler *handler;

    /** pdu containing the set request */
        netsnmp_pdu    *pdu;

    /** the set request */
        netsnmp_request_info *request;

    } netsnmp_monitor_set_request_data;

    /*
     *
     */
    typedef struct netsnmp_monitor_callback_cooperative_s {

   /** header */
        netsnmp_monitor_callback_header hdr;

    } netsnmp_monitor_callback_cooperative;



    typedef void   
        (netsnmp_object_monitor_callback) (netsnmp_monitor_callback_header
                                           *);




    /**********************************************************************
     * Registration function prototypes
     */

    /*
     * Register a callback for the specified object.
     */
    int             netsnmp_monitor_register(oid * object, size_t oid_len,
                                             int priority,
                                             unsigned int events,
                                             void *watcher_data,
                                             netsnmp_object_monitor_callback
                                             * cb);

    /*
     * Unregister a callback for the specified object.
     */
    int             netsnmp_monitor_unregister(oid * object,
                                               size_t oid_len,
                                               int priority,
                                               void *watcher_data,
                                               netsnmp_object_monitor_callback
                                               * cb);

    /*
     * check to see if a registration exists for an object/event combination
     */
    int             netsnmp_monitor_check_registered(int event, oid * oid,
                                                     int oid_len);


    /**********************************************************************
     * function prototypes
     */

    /*
     * Notifies the object monitor of an event.
     */
    void            netsnmp_notify_monitor(netsnmp_monitor_callback_header
                                           * cbh);




    /**********************************************************************
     * function prototypes
     */

    /*
     * Notifies the object monitor of a cooperative event.
     */
    void            netsnmp_notify_cooperative(int event, oid * object,
                                               size_t len, char oid_steal,
                                               void *object_info);



#ifdef __cplusplus
}
#endif
#endif /** OBJECT_MONITOR_H */
