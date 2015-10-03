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

#ifndef LLD2D_TYPES_H
#define LLD2D_TYPES_H

typedef struct {
    uint8_t a[6];
} __attribute__ ((packed)) etheraddr_t;

typedef uint32_t ipv4addr_t;

typedef struct in6_addr ipv6addr_t;

/* our own (hopefully portable) 2-byte char type */
typedef uint16_t ucs2char_t;


/* Process-level event management structure for io & timers */

/* Events invoke functions of this type: */
typedef void (*event_fn_t)(void *state);
/* which get called once, at their firing time. */

struct event_st {
    struct timeval ev_firetime;
    event_fn_t     ev_function;
    void          *ev_state;
    struct event_st *ev_next;
};

/* Process-level events are encapsulated by this opaque type: */
typedef struct event_st event_t;


typedef enum {
    smS_Nascent,
    smS_Pending,
    smS_Temporary,
    smS_Complete
} smS_state_t;


typedef struct {
    bool_t		ssn_is_valid;		/* empty entries in the session table are "invalid" */
    smS_state_t		ssn_state;
    uint                ssn_count;
    uint16_t		ssn_XID;		/* seq number from the last Discover we are associated with */
    etheraddr_t		ssn_mapper_real;	/* mapper we are associated with (Discover BH:RealSrc) */
    etheraddr_t		ssn_mapper_current;	/* mapper we are associated with (curpkt->eh: ethersrc) */
    bool_t		ssn_use_broadcast;	/* TRUE if broadcast need to reach mapper (always, for now) */
    uint8_t		ssn_TypeOfSvc;		/* Discover BH:ToS */
    struct event_st    *ssn_InactivityTimer;
} session_t;


typedef enum {
    smE_Quiescent,
    smE_Pausing,
    smE_Wait
} smE_state_t;


typedef enum {
    smT_Quiescent,
    smT_Command,
    smT_Emit
} smT_state_t;

#include "protocol.h"	// must precede smevent.h, below

/* State machine events, generated from Process-level events */
#include "smevent.h"

/* Process-level-event handler routines */
#include "event.h"

#include "util.h"

#define NUM_SEES 1024	/* how many recvee_desc_t we keep */

#endif	/*** LLD2D_TYPES_H ***/
