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

/* This is the session-manager state machine (smS) event distributor */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "globals.h"

#include "statemachines.h"
#include "packetio.h"

static bool_t
is_acking_me(void)
{
    etheraddr_t             *p  = (etheraddr_t*)(g_discover_hdr + 1);
    etheraddr_t          *limit = (etheraddr_t*)(((char*)g_ethernet_hdr) + g_rcvd_pkt_len);

    uint16_t                gen = ntohs(g_discover_hdr->mh_gen);
    uint16_t        numstations = ntohs(g_discover_hdr->mh_numstations);

    bool_t               acking = FALSE;
    int  i;

    if (g_this_event.isAckingMe == TRUE)  return TRUE;

    IF_TRACED(TRC_PACKET)
	dbgprintf("gen=%d, numsta=%d, stations=[", gen, numstations);
    END_TRACE

    /* parse seenlist, and decide if we are acked in this frame */

    for (i=0; p+1 <= limit && i<numstations; i++, p++)
    {
        IF_TRACED(TRC_PACKET)
	    dbgprintf(ETHERADDR_FMT " ", ETHERADDR_PRINT(p));
        END_TRACE

	if (ETHERADDR_EQUALS(p, &g_hwaddr))
	{
	    acking = TRUE;
	    /* we could break out early, but we also want to test the
	     * numstations count is accurate so we loop over all the
	     * acked addresses */
	}
    }
    IF_TRACED(TRC_PACKET)
	dbgprintf("]\n");
    END_TRACE

    if (i != numstations)
	warn("rx_discover: truncated frame: ended at station %d, "
	     "but numstations claimed %d\n", i, numstations);

    return acking;
}


static bool_t
is_conflicting_ssn( session_t *ssn )
{
    if ((g_topo_session) && (g_topo_session->ssn_is_valid==TRUE) && /* if there already IS a topo-session, */
        (ssn != g_topo_session) &&			 /* and this is not the same session,   */
        (ssn->ssn_TypeOfSvc == ToS_TopologyDiscovery) && /* and this IS a topo-ssn Discover,    */
                                                         /* and the real-addresses don't match, */
        !(ETHERADDR_EQUALS(&ssn->ssn_mapper_real, &g_topo_session->ssn_mapper_real)))
    { /* then this is a conflicting Discover */
        return TRUE;
    }
    return FALSE;
}

/*====================== S T A T E   H A N D L E R S ======================*/


/***********************  N A S C E N T   S T A T E  ***********************/

static
enum sm_Status
smS_NascentHandler  ( protocol_event_t *evt )
{
    IF_TRACED(TRC_STATE)
        printf("smS (Nascent): Entered with event %s",smEvent_names[evt->evtType]);
        if (g_this_event.evtType==evtPacketRcvd)
        {
            printf(" (%s)\n",Topo_opcode_names[g_opcode]);
        } else {
            puts("");
        }
    END_TRACE

    switch (evt->evtType)
    {
      case evtDiscoverRcvd:
        /* The first Topo ssn is the definitive one */
/**/    DEBUG({printf("session's ToS is %s %d\n",Lld2_tos_names[evt->ssn->ssn_TypeOfSvc],evt->ssn->ssn_TypeOfSvc);})
        if ((evt->ssn->ssn_TypeOfSvc == ToS_TopologyDiscovery) &&
            ((g_topo_session == NULL) || (g_topo_session->ssn_is_valid==FALSE)))
        {
/**/        DEBUG({printf("noting new TOPO session @ %X\n",(uint)evt->ssn);})
            g_topo_session = evt->ssn;
        }
        /* check for a conflicting TOPO Discover (rogue mapper) */
        if (is_conflicting_ssn( evt->ssn ))
        {
            evt->ssn->ssn_state = smS_Temporary;
        } else {
/**/        DEBUG({puts("not conflicting....");})
            if (is_acking_me())
            {
/**/            DEBUG({puts("is acking me....");})
                evt->isAckingMe = TRUE;
                IF_TRACED(TRC_STATE)
                    printf("smS[%X] (Nascent): Leaving for Complete.\n", (uint)evt->ssn);
                END_TRACE
                evt->ssn->ssn_state = smS_Complete;
                restart_inactivity_timer((evt->ssn->ssn_TypeOfSvc == ToS_TopologyDiscovery) ? TOPO_CMD_ACTIVITYTIMEOUT : TOPO_GENERAL_ACTIVITYTIMEOUT);
            } else {
/**/            DEBUG({puts("is NOT acking me....");})
                evt->isAckingMe = FALSE;
                IF_TRACED(TRC_STATE)
                    printf("smS[%X] (Nascent): Leaving for Pending.\n", (uint)evt->ssn);
                END_TRACE
                evt->ssn->ssn_state = smS_Pending;
                restart_inactivity_timer(TOPO_HELLO_ACTIVITYTIMEOUT);
            }
        }
        break;

      case evtPacketRcvd:
      case evtEmitRcvd:
      case evtResetRcvd:

      case evtBlockTimeout:
      case evtChargeTimeout:
      case evtEmitTimeout:
      case evtHelloDelayTimeout:
      case evtInactivityTimeout:
      default :
        IF_TRACED(TRC_STATE)
            printf("smS: Nascent handler ignored event %s\n",smEvent_names[evt->evtType]);
        END_TRACE
        break;
    }

    return PROCESSING_COMPLETED;
}

/***********************  P E N D I N G   S T A T E  ***********************/

static
enum sm_Status
smS_PendingHandler  ( protocol_event_t *evt )
{
    IF_TRACED(TRC_STATE)
        printf("smS[%p%s] (Pending): Entered with event %s", evt->ssn,
               (evt->ssn == g_topo_session)?"-T":"", smEvent_names[evt->evtType]);
        if (g_this_event.evtType==evtPacketRcvd)
        {
            printf(" (%s)\n",Topo_opcode_names[g_opcode]);
        } else {
            puts("");
        }
    END_TRACE

     switch (evt->evtType)
    {
      case evtDiscoverRcvd:
        /* Normally, we are entered with isAckingMe initialized to FALSE,
         * but the faked events from other state machines will all have it
         * set to TRUE already, which will be recognized in is_acking_me() */

        /* The changed-XID arcs must also be handled here */

        if (is_acking_me())
        {
//*/        DEBUG({puts("is acking me....");})
            evt->isAckingMe = TRUE;

            /* Discover-acking-changed-XID arc says we must reset the smT if this
             * event is associated with the topo-session. Then we process it as
             * the Discover, afterwards. */
            if (g_sequencenum  &&  evt->ssn->ssn_XID != g_sequencenum  &&  g_topo_session == evt->ssn)
            {
                bool_t  isActuallyInternal = g_this_event.isInternalEvt;

                IF_TRACED(TRC_PACKET)
                    printf("smS (Pending): Discover-ack, new XID (%d) != old XID (%d); resetting topo session....\n",
                           g_sequencenum, evt->ssn->ssn_XID);
                END_TRACE
                g_this_event.evtType = evtResetRcvd;
                g_this_event.isInternalEvt = TRUE;
                smT_process_event( &g_this_event );
                g_this_event.evtType = evtDiscoverRcvd;
                g_this_event.isInternalEvt = isActuallyInternal;
                IF_TRACED(TRC_PACKET)
                    printf("smS (Pending): After topo reset, re-creating the topo-session ptr....\n");
                END_TRACE
                g_topo_session = evt->ssn;
                /* Regardless of whether this is acking or not, we must accept the new XID, unless it's zero */
                evt->ssn->ssn_XID = g_sequencenum;
            }

            IF_TRACED(TRC_STATE)
                printf("smS[%X] (Pending): Leaving for Complete.\n", (uint)evt->ssn);
            END_TRACE
            evt->ssn->ssn_state = smS_Complete;
            restart_inactivity_timer((evt->ssn->ssn_TypeOfSvc == ToS_TopologyDiscovery) ? TOPO_CMD_ACTIVITYTIMEOUT : TOPO_GENERAL_ACTIVITYTIMEOUT);
        } else {
            /* Discover-noack-changed-XID arc says we must reset the smT if this
             * event is associated with the topo-session. Then we process it as
             * the Discover, afterwards. */
            if (g_sequencenum  &&  evt->ssn->ssn_XID != g_sequencenum  &&  g_topo_session == evt->ssn)
            {
                bool_t  isActuallyInternal = g_this_event.isInternalEvt;

                IF_TRACED(TRC_PACKET)
                    printf("smS (Pending): Discover-noack, new XID (%d) != old XID (%d); resetting topo session....\n",
                           g_sequencenum, evt->ssn->ssn_XID);
                END_TRACE
                g_this_event.evtType = evtResetRcvd;
                g_this_event.isInternalEvt = TRUE;
                smT_process_event( &g_this_event );
                g_this_event.evtType = evtDiscoverRcvd;
                g_this_event.isInternalEvt = isActuallyInternal;
                IF_TRACED(TRC_PACKET)
                    printf("smS (Pending): After topo reset, re-creating the topo-session ptr....\n");
                END_TRACE
                g_topo_session = evt->ssn;
                /* Regardless of whether this is acking or not, we must accept the new XID, unless it's zero */
                evt->ssn->ssn_XID = g_sequencenum;
            }
            restart_inactivity_timer(TOPO_HELLO_ACTIVITYTIMEOUT);
        }
        break;

      case evtInactivityTimeout:
      case evtResetRcvd:
        /* is this session the topology session? */
        if (evt->ssn == g_topo_session  &&  g_topo_session->ssn_is_valid == TRUE)
        {
            /* Must reset the Temporary sessions, as well, then. */
            session_t   *ssn = &g_sessions[0];
            int i;
            for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
            {
                if ( (ssn->ssn_is_valid) && (ssn->ssn_state==smS_Temporary) )
                {
                    IF_TRACED(TRC_STATE)
                        printf("smS[%X] (Temporary): Leaving for Nascent.\n", (uint)ssn);
                    END_TRACE
                    ssn->ssn_state = smS_Nascent;
                    CANCEL(ssn->ssn_InactivityTimer);
                    ssn->ssn_is_valid = FALSE;
                }
            }
        }
        /* In any case, reset (invalidate) this session */
        IF_TRACED(TRC_STATE)
            printf("smS[%X] (Pending): Leaving for Nascent.\n", (uint)evt->ssn);
        END_TRACE
        evt->ssn->ssn_state = smS_Nascent;
        CANCEL(evt->ssn->ssn_InactivityTimer);
        evt->ssn->ssn_is_valid = FALSE;
        packetio_invalidate_retxbuf();
        break;

      case evtHelloDelayTimeout:
	if (--(evt->ssn->ssn_count) == 0)
	{
            evt->ssn->ssn_state = smS_Complete;
            IF_TRACED(TRC_STATE)
              printf("smS[%X] (Pending): Leaving for Complete.\n", (uint)evt->ssn);
            END_TRACE
	}
	break;

      case evtPacketRcvd:
      case evtEmitRcvd:
      case evtBlockTimeout:
      case evtChargeTimeout:
      case evtEmitTimeout:
      default :
        IF_TRACED(TRC_STATE)
            printf("smS (Pending): Ignored event %s\n", smEvent_names[evt->evtType]);
        END_TRACE
        break;
    }

    return PROCESSING_COMPLETED;
}

/***********************  T E M P O R A R Y   S T A T E  ***********************/

static
enum sm_Status
smS_TemporaryHandler( protocol_event_t *evt )
{
    IF_TRACED(TRC_STATE)
        printf("smS (Temporary): Entered with event %s",smEvent_names[evt->evtType]);
        if (g_this_event.evtType==evtPacketRcvd)
        {
            printf(" (%s)\n",Topo_opcode_names[g_opcode]);
        } else {
            puts("");
        }
    END_TRACE

    switch (evt->evtType)
    {
      case evtInactivityTimeout:
      case evtResetRcvd:
      case evtHelloDelayTimeout:
        IF_TRACED(TRC_STATE)
            printf("smS[%X] (Temporary): Leaving for Nascent.\n", (uint)evt->ssn);
        END_TRACE
        evt->ssn->ssn_state = smS_Nascent;
        CANCEL(evt->ssn->ssn_InactivityTimer);
        evt->ssn->ssn_is_valid = FALSE;
        break;

      case evtPacketRcvd:
      case evtEmitRcvd:
      case evtDiscoverRcvd:

      case evtBlockTimeout:
      case evtChargeTimeout:
      case evtEmitTimeout:
      default:
        IF_TRACED(TRC_STATE)
            printf("smS (Temporary): Ignored event %s\n",smEvent_names[evt->evtType]);
        END_TRACE
        break;
    }

    return PROCESSING_COMPLETED;
}

/***********************  C O M P L E T E   S T A T E  ***********************/

static
enum sm_Status
smS_CompleteHandler ( protocol_event_t *evt )
{
    IF_TRACED(TRC_STATE)
        printf("smS (Complete): Entered with event %s",smEvent_names[evt->evtType]);
        if (g_this_event.evtType==evtPacketRcvd)
        {
            printf(" (%s)\n",Topo_opcode_names[g_opcode]);
        } else {
            puts("");
        }
    END_TRACE

    switch (evt->evtType)
    {
      case evtInactivityTimeout:
      case evtResetRcvd:
        /* is this session the topology session? */
        if (evt->ssn == g_topo_session  &&  g_topo_session->ssn_is_valid == TRUE)
        {
            /* Must reset the Temporary sessions, as well, then. */
            session_t   *ssn = &g_sessions[0];
            int i;
            for (i=0; i < MAX_NUM_SESSIONS; ssn++, i++)
            {
                if ( (ssn->ssn_is_valid) && (ssn->ssn_state==smS_Temporary) )
                {
                    IF_TRACED(TRC_STATE)
                        printf("smS[%X] (Temporary): Leaving for Nascent.\n", (uint)ssn);
                    END_TRACE
                    ssn->ssn_state = smS_Nascent;
                    CANCEL(ssn->ssn_InactivityTimer);
                    ssn->ssn_is_valid = FALSE;
                }
            }
            osl_set_promisc(g_osl, FALSE);
//!!            osl_set_arprx(FALSE, ts);
        }
        /* In any case, reset (invalidate) this session */
        IF_TRACED(TRC_STATE)
            printf("smS[%X] (Complete): Leaving for Nascent.\n", (uint)evt->ssn);
        END_TRACE
        evt->ssn->ssn_state = smS_Nascent;
        CANCEL(evt->ssn->ssn_InactivityTimer);
        evt->ssn->ssn_is_valid = FALSE;
        packetio_invalidate_retxbuf();
        break;

      case evtDiscoverRcvd:
        if (is_acking_me())
        {
//*/        DEBUG({puts("is acking me....");})

            evt->isAckingMe = TRUE;

            /* Discover-acking-changed-XID arc says we must reset the smT if this
             * event is associated with the topo-session. Then we process it as
             * the Discover, afterwards. */
            if (g_sequencenum  &&  evt->ssn->ssn_XID != g_sequencenum  &&  g_topo_session == evt->ssn)
            {
                bool_t  isActuallyInternal = g_this_event.isInternalEvt;

                IF_TRACED(TRC_PACKET)
                    printf("smS (Complete): Discover-ack, new XID (%d) != old XID (%d); resetting topo session....\n",
                           g_sequencenum, evt->ssn->ssn_XID);
                END_TRACE
                g_this_event.evtType = evtResetRcvd;
                g_this_event.isInternalEvt = TRUE;
                smT_process_event( &g_this_event );
                g_this_event.evtType = evtDiscoverRcvd;
                g_this_event.isInternalEvt = isActuallyInternal;
                IF_TRACED(TRC_PACKET)
                    printf("smS (Complete): After topo reset, re-creating the topo-session ptr....\n");
                END_TRACE
                g_topo_session = evt->ssn;
                /* Regardless of whether this is acking or not, we must accept the new XID, unless it's zero */
                evt->ssn->ssn_XID = g_sequencenum;
            }
            restart_inactivity_timer((evt->ssn->ssn_TypeOfSvc == ToS_TopologyDiscovery) ? TOPO_CMD_ACTIVITYTIMEOUT : TOPO_GENERAL_ACTIVITYTIMEOUT);
        } else {
            /* Discover-noack-changed-XID arc says we must reset the smT if this
             * event is associated with the topo-session. Then we process it as
             * the Discover, afterwards. */
            if (g_sequencenum  &&  evt->ssn->ssn_XID != g_sequencenum  &&  g_topo_session == evt->ssn)
            {
                bool_t  isActuallyInternal = g_this_event.isInternalEvt;

                IF_TRACED(TRC_PACKET)
                    printf("smS (Complete): Discover-noack, new XID (%d) != old XID (%d); resetting topo session....\n",
                           g_sequencenum, evt->ssn->ssn_XID);
                END_TRACE
                g_this_event.evtType = evtResetRcvd;
                g_this_event.isInternalEvt = TRUE;
                smT_process_event( &g_this_event );
                g_this_event.evtType = evtDiscoverRcvd;
                g_this_event.isInternalEvt = isActuallyInternal;
                IF_TRACED(TRC_PACKET)
                    printf("smS (Complete): After topo reset, re-creating the topo-session ptr....\n");
                END_TRACE
                g_topo_session = evt->ssn;
                /* Regardless of whether this is acking or not, we must accept the new XID, unless it's zero */
                evt->ssn->ssn_XID = g_sequencenum;
                IF_TRACED(TRC_STATE)
                    printf("smS[%X] (Complete): Leaving for Pending (No-ack, changed XID).\n", (uint)evt->ssn);
                END_TRACE
                evt->ssn->ssn_state = smS_Pending;
                restart_inactivity_timer(TOPO_HELLO_ACTIVITYTIMEOUT);
            } else {
                restart_inactivity_timer((evt->ssn->ssn_TypeOfSvc == ToS_TopologyDiscovery) ? TOPO_CMD_ACTIVITYTIMEOUT : TOPO_GENERAL_ACTIVITYTIMEOUT);
            }
        }
        break;

      case evtEmitRcvd:
      case evtPacketRcvd:
        restart_inactivity_timer((evt->ssn->ssn_TypeOfSvc == ToS_TopologyDiscovery) ? TOPO_CMD_ACTIVITYTIMEOUT : TOPO_GENERAL_ACTIVITYTIMEOUT);
        break;

      case evtBlockTimeout:
      case evtChargeTimeout:
      case evtEmitTimeout:
      case evtHelloDelayTimeout:
      default :
        IF_TRACED(TRC_STATE)
//*/            printf("smS (Complete): Ignored event %s\n",smEvent_names[evt->evtType]);
        END_TRACE
        break;
    }

    return PROCESSING_COMPLETED;
}


/***********************                             ***********************/
/***********************  S T A T E   M A C H I N E  ***********************/
/***********************                             ***********************/


enum sm_Status
smS_process_event( protocol_event_t *evt )
{
    IF_TRACED(TRC_STATE)
//*/        printf("smS_process_event: Entered with event %s\n",smEvent_names[evt->evtType]);
    END_TRACE

    if (evt->ssn == NULL)
    {
        IF_TRACED(TRC_STATE)
            printf("smS (no-state): Not dispatching - no session associated with event %s",
                   smEvent_names[evt->evtType]);
            if (g_this_event.evtType==evtPacketRcvd)
            {
                printf(" (%s)\n",Topo_opcode_names[g_opcode]);
            } else {
                puts("");
            }
        END_TRACE
        return PROCESSING_COMPLETED;
    }
    switch (evt->ssn->ssn_state)
    {
      case smS_Nascent:
        return smS_NascentHandler( evt );

      case smS_Pending:
        return smS_PendingHandler( evt );

      case smS_Temporary:
        return smS_TemporaryHandler( evt );

      case smS_Complete:
        return smS_CompleteHandler( evt );

      default:
        return PROCESSING_ABORTED;	/* Stop! smS in unknown state! */
    }
}



