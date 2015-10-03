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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "globals.h"

#include "statemachines.h"
#include "bandfuncs.h"
#include "packetio.h"

/****************************
 * Timeout handlers - 
 * these are the time-based entry points, which also clock the state machine.
 */

void
state_block_timeout(void *cookie)
{
    g_block_timer = NULL;
    g_this_event.evtType = evtBlockTimeout;
    state_process_timeout();
}

void
state_charge_timeout(void *cookie)
{
    g_charge_timer     = NULL;
    g_ctc_packets      = 0;
    g_ctc_bytes        = 0;
    g_this_event.evtType  = evtChargeTimeout;
    g_this_event.ssn      = (session_t*) cookie;
    state_process_timeout();
}

void
state_emit_timeout(void *cookie)
{
    g_emit_timer       = NULL;
    g_this_event.evtType  = evtEmitTimeout;

    /* Probes are forced to associate with the mapping session, if there is one. */
    if (g_topo_session != NULL  &&  g_topo_session->ssn_is_valid)
    {
        g_this_event.ssn = g_topo_session;
    }

    state_process_timeout();
}

void
state_hello_delay_timeout(void *cookie)
{
    g_hello_timer      = NULL;
    g_this_event.evtType  = evtHelloDelayTimeout;
    g_this_event.ssn      = (session_t*) cookie;
    state_process_timeout();
}

void
state_inactivity_timeout(void *cookie)
{
    g_this_event.evtType  = evtInactivityTimeout;
    g_this_event.ssn      = (session_t*) cookie;
    g_this_event.ssn->ssn_InactivityTimer = NULL;
    state_process_timeout();
}


/* This function locates an existing session that is associated with the passed-in address */
static session_t *
find_session(etheraddr_t *this_addr)
{
    session_t   *ssn = &g_sessions[0];
    int i;

    for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
    {
        if ( (ssn->ssn_is_valid) && (ETHERADDR_EQUALS(&ssn->ssn_mapper_real, this_addr)) )
        {
//*/        DEBUG({printf("find_session returning session %d @ %X\n",i,(uint)ssn);})
            return ssn;
        }
    }
//*/DEBUG({puts("find_session returning NULL");})
    return NULL;
}


static session_t *
new_session()
{
    session_t   *ssn = &g_sessions[0];
    int i;

    for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
    {
        if ( !(ssn->ssn_is_valid) )
        {
            ssn->ssn_is_valid = TRUE;
	    ssn->ssn_count = BAND_TXC;
            ssn->ssn_XID = 0;
            memset(&ssn->ssn_mapper_real,0,sizeof(etheraddr_t));
            memset(&ssn->ssn_mapper_current,0,sizeof(etheraddr_t));
            ssn->ssn_use_broadcast = TRUE;
            ssn->ssn_TypeOfSvc = ToS_Unknown;
            ssn->ssn_InactivityTimer = NULL;
            return ssn;
        }
    }
    return NULL;
}


/*****************************************************************************
 *
 * This code processes the current packet-event (in g_this_event) by locating
 * (and possibly creating) the session with which it must be associated,
 * then passing it to the 3 state machines, smS, smE, and smT, in that order.
 *
 *****************************************************************************/ 

uint
state_process_packet()
{
    session_t           *this_session;
    enum sm_Status       smStatus;

    IF_TRACED((TRC_STATE|TRC_PACKET))
        printf("state_process_packet: Entered with event %s",smEvent_names[g_this_event.evtType]);
        if (g_this_event.evtType==evtPacketRcvd)
        {
            printf(" (%s)\n",Topo_opcode_names[g_opcode]);
        } else {
            puts("");
        }
    END_TRACE

    g_this_event.isInternalEvt = FALSE;	// It's a real event, not internally generated

    /* First, look this RealSrc up in the session table, to
     * locate any association with an established session.
     *
     * If there is no matching session, create a new one, iff the
     * packet is a valid Discover of either topo- or quick- TOS ... */

    g_this_event.isNewSession = FALSE;

    if ((this_session = find_session(&g_base_hdr->tbh_realsrc)) == NULL)
    {
        /* Not found: Check for a Discovery packet (validated in packetio.c) */
        if (g_opcode == Opcode_Discover)
        {
            /* Create a new session for this association */
            if ((this_session = new_session()) == NULL)
            {
                /* No room in the table: drop the packet and whine. */
                warn("state_process_packet: no room to create new session. Packet dropped.\n");
                return UINT_MAX;
            }
            g_this_event.isNewSession = TRUE;

            /* Fill in the newly valid session table entry with info from the packet */
            this_session->ssn_XID            = g_sequencenum;
            this_session->ssn_mapper_real    = g_base_hdr->tbh_realsrc;
            this_session->ssn_mapper_current = g_ethernet_hdr->eh_src;
            this_session->ssn_TypeOfSvc      = g_base_hdr->tbh_tos;
            IF_TRACED(TRC_STATE)
                printf("New Session:\n\tXID = %X\n\treal address: " ETHERADDR_FMT \
                       "\n",this_session->ssn_XID, \
                       ETHERADDR_PRINT(&this_session->ssn_mapper_real) );

                printf("\tcurrent address: " ETHERADDR_FMT "\n\tToS: %s\n",
                       ETHERADDR_PRINT(&this_session->ssn_mapper_current),
                       Lld2_tos_names[this_session->ssn_TypeOfSvc] );
            END_TRACE
            g_this_event.ssn = this_session;

        }   /*** end of if (g_opcode == Opcode_Discover) ***/

        /* Probes are forced to associate with the mapping session, if there is one. */
        if (g_opcode == Opcode_Probe)
        {
             if (g_topo_session != NULL  &&  g_topo_session->ssn_is_valid)
             {
                this_session = g_topo_session;
             }
        }

    }   /*** endo of if (find_session()==NULL) ***/

    /* We have associated whatever session that we can with this packet - pass to state machines */
    g_this_event.ssn = this_session;

    smStatus = smS_process_event( &g_this_event );

    if (smStatus != PROCESSING_ABORTED)
    {
        smStatus = smE_process_event( &g_this_event );
    }

    if (smStatus != PROCESSING_ABORTED)
    {
        smStatus = smT_process_event( &g_this_event );
    }

    /* Remove any "new-session" marking */
    g_this_event.isNewSession = FALSE;

    IF_TRACED(TRC_PACKET)
        printf("state_process_packet: Leaving - done with event %s\n",smEvent_names[g_this_event.evtType]);
    END_TRACE
    return 0;	/* Success! */
}


/******************************************************************************
 *
 * This code processes the current timeout-event (in g_this_event). Any session
 * associated with this event (only happens with activity-timeouts) is already
 * noted in the GLOBAL g_this_event (g_this_event).
 *
 ******************************************************************************/ 

uint
state_process_timeout()
{
    enum sm_Status         smStatus;

    IF_TRACED(TRC_STATE)
        if (g_this_event.evtType!=evtBlockTimeout)
            printf("state_process_timeout: Entered with event %s\n",smEvent_names[g_this_event.evtType]);
    END_TRACE

    g_rcvd_pkt_len = 0;

    /* Finish initializing the protocol-event */

    /* g_this_event.evtType    = <set in individual timeout handler> */
    /* g_this_event.ssn        = <set in individual timeout handler> */
    g_this_event.isNewSession  = FALSE;
    g_this_event.isAckingMe    = FALSE;
    g_this_event.isInternalEvt = FALSE;	// It's a real event, not internally generated
    g_this_event.numDescrs     = 0;

    /* pass Hello-Delay timer events to each existing session's smS state-machine */
    if (g_this_event.evtType == evtHelloDelayTimeout)
    {
        session_t     *ssn = &g_sessions[0];
        int            i;

        for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
        {
            if (ssn->ssn_is_valid)
            {
                g_this_event.ssn = ssn;
                smS_process_event( &g_this_event );
            }
        }
        g_this_event.ssn = NULL;
    }

    /* Pass per-session activity timeouts to the associated session.  The remaining
     * timeouts, for charge and emit, need only be processed by smE & smT...        */
    if (g_this_event.evtType == evtInactivityTimeout)
    {
        smS_process_event( &g_this_event );
    }

    /* send all the timeouts to smE and then smT */

    smStatus = smE_process_event( &g_this_event );

    if (smStatus != PROCESSING_ABORTED)
    {
        smT_process_event( &g_this_event );
    }

    return 0;	/* Success! */
}


/****************************
 * Helper functions -
 * actions performed as part of state processing.
 */

/* Restart the inactivity timer for the session associated with the current event */
void
restart_inactivity_timer(uint32_t timeout)
{
    struct timeval now;

    if (g_this_event.ssn == NULL  ||  g_this_event.ssn->ssn_is_valid != TRUE)   return;

    gettimeofday(&now, NULL);
    timeval_add_ms(&now, timeout);
    CANCEL(g_this_event.ssn->ssn_InactivityTimer);
    g_this_event.ssn->ssn_InactivityTimer = event_add(&now, state_inactivity_timeout, g_this_event.ssn);
}


/* Searches session table - returns TRUE if all valid sessions are in smS_Complete state */

bool_t
OnlyCompleteSessions(void)
{
    session_t   *ssn = &g_sessions[0];
    int i;

    for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
    {
        if (ssn->ssn_is_valid && ssn->ssn_state != smS_Complete)
        {
            return FALSE;
        }
    }
    return TRUE;
}


/* Searches session table - returns TRUE if all sessions are invalid */

bool_t
SessionTableIsEmpty(void)
{
    session_t   *ssn = &g_sessions[0];
    int i;

    for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
    {
        if (ssn->ssn_is_valid)
        {
            return FALSE;
        }
    }
    return TRUE;
}

bool_t
set_emit_timer(void)
{
    topo_emitee_desc_t *ed;

    assert(g_emit_remaining > 0);
    assert(g_emit_timer == NULL);

    /* get the next emitee_desc and schedule a callback when it is due
     * to be transmitted */
    ed = g_emitdesc;
    if (ed->ed_pause != 0)
    {
	struct timeval now;
	gettimeofday(&now, NULL);
	timeval_add_ms(&now, ed->ed_pause);
	g_emit_timer = event_add(&now, state_emit_timeout, NULL);
        return TRUE;
    } else {
	/* no pause; return PAUSE=FALSE */
	return FALSE;
    }
}
