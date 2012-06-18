/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#ifndef	STATEMACHINES_H
#define STATEMACHINES_H

#include "lld2d_types.h"

/* This is the entry point for state machine processing. All
 * packet events, timer events, and calculated events begin
 * their processing at these entries. They call the smS, smE,
 * and smT in an appropriate order, unless an smX returns a cutoff. */

extern uint state_process_packet(void);
extern uint state_process_timeout(void);

/* State machine return codes */

/* State machines can loop, internally (only smT does), in which case they keep looping until KEEP_GOING is no
 * longer returned. Otherwise, they can finish normally, (PROCESSING_COMPLETED) or indicate an error condition
 * that forbids any further processing in any state machine (PROCESSING_ABORTED). */
enum sm_Status {
    PROCESSING_COMPLETED,	/* returned when no further passes thru the state machine are req'd for this event */
    KEEP_GOING,			/* requests another pass through the state machine for further processing          */
    PROCESSING_ABORTED		/* no further passes, & this one failed - further state machines are not invoked   */
};

/* These are the entry points for the three state machines, smS, smE, and smT */

extern enum sm_Status   smS_process_event( protocol_event_t *evt );
extern enum sm_Status   smE_process_event( protocol_event_t *evt );
extern enum sm_Status   smT_process_event( protocol_event_t *evt );

/* These are utility routines, timer routines, etc, associated with the state machine processing */
extern void restart_inactivity_timer(uint32_t timeout);
extern bool_t set_emit_timer(void);

extern void state_block_timeout(void *cookie);
extern void state_charge_timeout(void *cookie);
extern void state_emit_timeout(void *cookie);
extern void state_hello_delay_timeout(void *cookie);
extern void state_inactivity_timeout(void *cookie);

/* These are pseudo-event recognizers. They detect pseudo-events like
 * "table has only complete sessions", which need to be separate from
 * normal timeouts and packet receipts, for one reason or another. */

extern bool_t OnlyCompleteSessions(void);
extern bool_t SessionTableIsEmpty(void);

#endif /*** STATEMACHINES_H ***/
