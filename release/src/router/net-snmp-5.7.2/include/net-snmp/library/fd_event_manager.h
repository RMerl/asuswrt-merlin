/**************************************************************************
 * UNIT: File Descriptor (FD) Event Manager
 *
 * OVERVIEW: This unit contains functions to register a FD with the FD
 *           event manager for callbacks when activity is received on that
 *           FD.  Notification of read, write, and exception activity can 
 *           all be registered for individually.  Once a registered FD is
 *           closed by the user, the FD must be unregistered.  To use
 *           the FD Event manager you need to make calls to 
 *           netsnmp_external_event_info() and 
 *           netsnmp_dispatch_external_events() in your event loop to receive
 *           callbacks for registered events.  See snmpd.c and snmptrapd.c 
 *           for examples.
 *
 * LIMITATIONS:
 **************************************************************************/
#ifndef FD_EVENT_MANAGER_H
#define FD_EVENT_MANAGER_H

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef __cplusplus
extern          "C" {
#endif

#define NUM_EXTERNAL_FDS 32
#define FD_REGISTERED_OK                 0
#define FD_REGISTRATION_FAILED          -2
#define FD_UNREGISTERED_OK               0
#define FD_NO_SUCH_REGISTRATION         -1

/* Since the inception of netsnmp_external_event_info and 
 * netsnmp_dispatch_external_events, there is no longer a need for the data 
 * below to be globally visible.  We will leave it global for now for 
 * compatibility purposes. */
extern int      external_readfd[NUM_EXTERNAL_FDS],   external_readfdlen;
extern int      external_writefd[NUM_EXTERNAL_FDS],  external_writefdlen;
extern int      external_exceptfd[NUM_EXTERNAL_FDS], external_exceptfdlen;

extern void     (*external_readfdfunc[NUM_EXTERNAL_FDS])   (int, void *);
extern void     (*external_writefdfunc[NUM_EXTERNAL_FDS])  (int, void *);
extern void     (*external_exceptfdfunc[NUM_EXTERNAL_FDS]) (int, void *);

extern void    *external_readfd_data[NUM_EXTERNAL_FDS];
extern void    *external_writefd_data[NUM_EXTERNAL_FDS];
extern void    *external_exceptfd_data[NUM_EXTERNAL_FDS];

/* Here are the key functions of this unit.  Use register_xfd to register
 * a callback to be called when there is x activity on the register fd.  
 * x can be read, write, or except (for exception).  When registering,
 * you can pass in a pointer to some data that you have allocated that
 * you would like to have back when the callback is called. */
int             register_readfd(int, void (*func)(int, void *),   void *);
int             register_writefd(int, void (*func)(int, void *),  void *);
int             register_exceptfd(int, void (*func)(int, void *), void *);

/* Unregisters a given fd for events */
int             unregister_readfd(int);
int             unregister_writefd(int);
int             unregister_exceptfd(int);

/*
 * External Event Info
 *
 * Description:
 *   Call this function to add an external event fds to your read, write, 
 *   exception fds that your application already has.  When this function
 *   returns, your fd_sets will be ready for select().  It returns the
 *   biggest fd in the fd_sets so far.
 *
 * Input Parameters: None
 *
 * Output Parameters: None
 *
 * In/Out Parameters: 
 *   numfds - The biggest fd so far.  On exit to this function, numfds
 *            could of changed since we pass out the new biggest fd.
 *   readfds - Set of read FDs that we are monitoring.  This function
 *             can modify this set to have more FDs that we are monitoring.
 *   writefds - Set of write FDs that we are monitoring.  This function
 *             can modify this set to have more FDs that we are monitoring.
 *   exceptfds - Set of exception FDs that we are monitoring.  This function
 *             can modify this set to have more FDs that we are monitoring.
 *
 * Return Value: None
 *
 * Side Effects: None
 */
NETSNMP_IMPORT
void netsnmp_external_event_info(int *numfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);

NETSNMP_IMPORT
void netsnmp_external_event_info2(int *numfds,
                                  netsnmp_large_fd_set *readfds,
                                  netsnmp_large_fd_set *writefds,
                                  netsnmp_large_fd_set *exceptfds);

/*
 * Dispatch External Events
 *
 * Description:
 *   Call this function after select returns with pending events.  If any of
 *   them were NETSNMP external events, the registered callback will be called.
 *   The corresponding fd_set will have the FD cleared after the event is
 *   dispatched.
 *
 * Input Parameters: None
 *
 * Output Parameters: None
 *
 * In/Out Parameters: 
 *   count - Number of FDs that have activity.  In this function, we decrement
 *           count as we dispatch an event.
 *   readfds - Set of read FDs that have activity
 *   writefds - Set of write FDs that have activity
 *   exceptfds - Set of exception FDs that have activity 
 *
 * Return Value: None
 *
 * Side Effects: None
 */
NETSNMP_IMPORT
void netsnmp_dispatch_external_events(int *count, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);
NETSNMP_IMPORT
void netsnmp_dispatch_external_events2(int *count,
                                       netsnmp_large_fd_set *readfds,
                                       netsnmp_large_fd_set *writefds,
                                       netsnmp_large_fd_set *exceptfds);
#ifdef __cplusplus
}
#endif
#endif
