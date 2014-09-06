#ifndef NET_SNMP_SESSION_API_H
#define NET_SNMP_SESSION_API_H

    /**
     *  Library API routines concerned with specifying and using SNMP "sessions"
     *    including sending and receiving requests.
     */

#include <net-snmp/types.h>

#ifdef __cplusplus
extern          "C" {
#endif

    NETSNMP_IMPORT
    void            snmp_sess_init(netsnmp_session *);

    /*
     * netsnmp_session *snmp_open(session)
     *      netsnmp_session *session;
     *
     * Sets up the session with the snmp_session information provided
     * by the user.  Then opens and binds the necessary UDP port.
     * A handle to the created session is returned (this is different than
     * the pointer passed to snmp_open()).  On any error, NULL is returned
     * and snmp_errno is set to the appropriate error code.
     */
    NETSNMP_IMPORT
    netsnmp_session *snmp_open(netsnmp_session *);

    /*
     * int snmp_close(session)
     *     netsnmp_session *session;
     *
     * Close the input session.  Frees all data allocated for the session,
     * dequeues any pending requests, and closes any sockets allocated for
     * the session.  Returns 0 on error, 1 otherwise.
     *
     * snmp_close_sessions() does the same thing for all open sessions
     */
    NETSNMP_IMPORT
    int             snmp_close(netsnmp_session *);
    NETSNMP_IMPORT
    int             snmp_close_sessions(void);


    /*
     * int snmp_send(session, pdu)
     *     netsnmp_session *session;
     *     netsnmp_pdu      *pdu;
     *
     * Sends the input pdu on the session after calling snmp_build to create
     * a serialized packet.  If necessary, set some of the pdu data from the
     * session defaults.  Add a request corresponding to this pdu to the list
     * of outstanding requests on this session, then send the pdu.
     * Returns the request id of the generated packet if applicable, otherwise 1.
     * On any error, 0 is returned.
     * The pdu is freed by snmp_send() unless a failure occured.
     */
    NETSNMP_IMPORT
    int             snmp_send(netsnmp_session *, netsnmp_pdu *);

    /*
     * int snmp_async_send(session, pdu, callback, cb_data)
     *     netsnmp_session *session;
     *     netsnmp_pdu      *pdu;
     *     netsnmp_callback callback;
     *     void   *cb_data;
     *
     * Sends the input pdu on the session after calling snmp_build to create
     * a serialized packet.  If necessary, set some of the pdu data from the
     * session defaults.  Add a request corresponding to this pdu to the list
     * of outstanding requests on this session and store callback and data,
     * then send the pdu.
     * Returns the request id of the generated packet if applicable, otherwise 1.
     * On any error, 0 is returned.
     * The pdu is freed by snmp_send() unless a failure occured.
     */
    NETSNMP_IMPORT
    int             snmp_async_send(netsnmp_session *, netsnmp_pdu *,
                                    netsnmp_callback, void *);


    /*
     * void snmp_read(fdset)
     *     fd_set  *fdset;
     *
     * Checks to see if any of the fd's set in the fdset belong to
     * snmp.  Each socket with it's fd set has a packet read from it
     * and snmp_parse is called on the packet received.  The resulting pdu
     * is passed to the callback routine for that session.  If the callback
     * routine returns successfully, the pdu and it's request are deleted.
     */
    NETSNMP_IMPORT
    void            snmp_read(fd_set *);

    /*
     * snmp_read2() is similar to snmp_read(), but accepts a pointer to a
     * large file descriptor set instead of a pointer to a regular file
     * descriptor set.
     */
    NETSNMP_IMPORT
    void            snmp_read2(netsnmp_large_fd_set *);


    NETSNMP_IMPORT
    int             snmp_synch_response(netsnmp_session *, netsnmp_pdu *,
                                        netsnmp_pdu **);

    /*
     * int snmp_select_info(numfds, fdset, timeout, block)
     * int *numfds;
     * fd_set   *fdset;
     * struct timeval *timeout;
     * int *block;
     *
     * Returns info about what snmp requires from a select statement.
     * numfds is the number of fds in the list that are significant.
     * All file descriptors opened for SNMP are OR'd into the fdset.
     * If activity occurs on any of these file descriptors, snmp_read
     * should be called with that file descriptor set.
     *
     * The timeout is the latest time that SNMP can wait for a timeout.  The
     * select should be done with the minimum time between timeout and any other
     * timeouts necessary.  This should be checked upon each invocation of select.
     * If a timeout is received, snmp_timeout should be called to check if the
     * timeout was for SNMP.  (snmp_timeout is idempotent)
     *
     * Block is 1 if the select is requested to block indefinitely, rather than
     * time out.  If block is input as 1, the timeout value will be treated as
     * undefined, but it must be available for setting in snmp_select_info.  On
     * return, if block is true, the value of timeout will be undefined.
     *
     * snmp_select_info returns the number of open sockets.  (i.e. The number
     * of sessions open)
     */
    NETSNMP_IMPORT
    int             snmp_select_info(int *, fd_set *, struct timeval *,
                                     int *);

    /*
     * snmp_select_info2() is similar to snmp_select_info(), but accepts a
     * pointer to a large file descriptor set instead of a pointer to a
     * regular file descriptor set.
     */
    NETSNMP_IMPORT
    int             snmp_select_info2(int *, netsnmp_large_fd_set *,
                                      struct timeval *, int *);

#define NETSNMP_SELECT_NOFLAGS  0x00
#define NETSNMP_SELECT_NOALARMS 0x01
    NETSNMP_IMPORT
    int             snmp_sess_select_info_flags(void *, int *, fd_set *,
                                                struct timeval *, int *, int);
    int             snmp_sess_select_info2_flags(void *, int *,
                                                 netsnmp_large_fd_set *,
                                                 struct timeval *, int *, int);

    /*
     * void snmp_timeout();
     *
     * snmp_timeout should be called whenever the timeout from snmp_select_info
     * expires, but it is idempotent, so snmp_timeout can be polled (probably a
     * cpu expensive proposition).  snmp_timeout checks to see if any of the
     * sessions have an outstanding request that has timed out.  If it finds one
     * (or more), and that pdu has more retries available, a new packet is formed
     * from the pdu and is resent.  If there are no more retries available, the
     * callback for the session is used to alert the user of the timeout.
     */

    NETSNMP_IMPORT
    void            snmp_timeout(void);

    /*
     * single session API.
     *
     * These functions perform similar actions as snmp_XX functions,
     * but operate on a single session only.
     *
     * Synopsis:
     
     void * sessp;
     netsnmp_session session, *ss;
     netsnmp_pdu *pdu, *response;
     
     snmp_sess_init(&session);
     session.retries = ...
     session.remote_port = ...
     sessp = snmp_sess_open(&session);
     ss = snmp_sess_session(sessp);
     if (ss == NULL)
     exit(1);
     ...
     if (ss->community) free(ss->community);
     ss->community = strdup(gateway);
     ss->community_len = strlen(gateway);
     ...
     snmp_sess_synch_response(sessp, pdu, &response);
     ...
     snmp_sess_close(sessp);
     
     * See also:
     * snmp_sess_synch_response, in snmp_client.h.
     
     * Notes:
     *  1. Invoke snmp_sess_session after snmp_sess_open.
     *  2. snmp_sess_session return value is an opaque pointer.
     *  3. Do NOT free memory returned by snmp_sess_session.
     *  4. Replace snmp_send(ss,pdu) with snmp_sess_send(sessp,pdu)
     */

    NETSNMP_IMPORT
    void           *snmp_sess_open(netsnmp_session *);
    NETSNMP_IMPORT
    void           *snmp_sess_pointer(netsnmp_session *);
    NETSNMP_IMPORT
    netsnmp_session *snmp_sess_session(void *);
    NETSNMP_IMPORT
    netsnmp_session *snmp_sess_session_lookup(void *);


    /*
     * use return value from snmp_sess_open as void * parameter 
     */

    NETSNMP_IMPORT
    int             snmp_sess_send(void *, netsnmp_pdu *);
    NETSNMP_IMPORT
    int             snmp_sess_async_send(void *, netsnmp_pdu *,
                                         netsnmp_callback, void *);
    NETSNMP_IMPORT
    int             snmp_sess_select_info(void *, int *, fd_set *,
                                          struct timeval *, int *);
    NETSNMP_IMPORT
    int             snmp_sess_select_info2(void *, int *,
					   netsnmp_large_fd_set *,
                                           struct timeval *, int *);
    /*
     * Returns 0 if success, -1 if fail.
     */
    NETSNMP_IMPORT
    int             snmp_sess_read(void *, fd_set *);
    /*
     * Similar to snmp_sess_read(), but accepts a pointer to a large file
     * descriptor set instead of a pointer to a file descriptor set.
     */
    NETSNMP_IMPORT
    int             snmp_sess_read2(void *,
                                    netsnmp_large_fd_set *);
    NETSNMP_IMPORT
    void            snmp_sess_timeout(void *);
    NETSNMP_IMPORT
    int             snmp_sess_close(void *);

    NETSNMP_IMPORT
    int             snmp_sess_synch_response(void *, netsnmp_pdu *,
                                             netsnmp_pdu **);

#ifdef __cplusplus
}
#endif


    /*
     *    Having extracted the main ("public API") calls relevant
     *  to this area of the Net-SNMP project, the next step is to
     *  identify the related "public internal API" routines.
     *
     *    In due course, these should probably be gathered
     *  together into a companion 'library/session_api.h' header file.
     *  [Or some suitable name]
     *
     *    But for the time being, the expectation is that the
     *  traditional headers that provided the above definitions
     *  will probably also cover the relevant internal API calls.
     *  Hence they are listed here:
     */

#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_client.h>
#include <net-snmp/library/asn1.h>
#include <net-snmp/library/callback.h>

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/snmp_service.h>
#include <net-snmp/library/snmpCallbackDomain.h>
#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN
#include <net-snmp/library/snmpUnixDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_UDP_DOMAIN
#include <net-snmp/library/snmpUDPDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_TCP_DOMAIN
#include <net-snmp/library/snmpTCPDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_UDPIPV6_DOMAIN
#include <net-snmp/library/snmpUDPIPv6Domain.h>
#endif
#ifdef NETSNMP_TRANSPORT_TCPIPV6_DOMAIN
#include <net-snmp/library/snmpTCPIPv6Domain.h>
#endif
#ifdef NETSNMP_TRANSPORT_IPX_DOMAIN
#include <net-snmp/library/snmpIPXDomain.h>
#endif
#ifdef NETSNMP_TRANSPORT_AAL5PVC_DOMAIN
#include <net-snmp/library/snmpAAL5PVCDomain.h>
#endif

#include <net-snmp/library/ucd_compat.h>

#endif                          /* NET_SNMP_SESSION_API_H */
