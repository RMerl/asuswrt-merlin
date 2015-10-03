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

/* This is the state machine that controls the enumeration process, smE.
 * It handles enumeration by both a Topology-Discovery Mapper and by
 * a Quick-Discovery Mapper. T-D mappers are restricted to being one-
 * at-a-time associations. Up to 10 Q-D mappers are allowed to associate
 * with this Responder at the same time. Each association is assigned
 * to one session, so there can be up to 11 sessions active at any time.
 *
 * A session conceptually represents the association of a Mapper with this
 * Responder, throughout the lifetime of that association.
 *
 * Packets flow from the initial packet-receive function to the Session
 * State Machine, smS, which will then forward them to smE_event_distributor()
 * if appropriate. */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "globals.h"

#include "statemachines.h"
#include "bandfuncs.h"
#include "packetio.h"



/***********************  Q U I E S C E N T   S T A T E  ***********************/

static
enum sm_Status
smE_QuiescentHandler( protocol_event_t* evt )
{
    IF_TRACED(TRC_STATE)
        if (evt->evtType != evtBlockTimeout)
        {
            printf("smE (Quiescent): Entered with event %s",smEvent_names[evt->evtType]);
            if (g_this_event.evtType==evtPacketRcvd)
            {
                printf(" (%s)\n",Topo_opcode_names[g_opcode]);
            } else {
                puts("");
            }
        }
    END_TRACE

    switch (evt->evtType)
    {
      case evtDiscoverRcvd:
        if (evt->isNewSession)
        {
            if (g_this_event.ssn->ssn_state == smS_Complete)
            {
                CANCEL(g_block_timer);
                g_smE_state = smE_Wait;
                IF_DEBUG
                    printf("smE (Quiescent): Leaving for Wait state\n");
                END_DEBUG
            } else {
                band_InitStats();
                band_ChooseHelloTime();
                if (g_base_hdr->tbh_tos == ToS_TopologyDiscovery)
                {
                    osl_set_promisc(g_osl, TRUE);
                }
//!!            osl_set_arprx(g_osl, TRUE);
                g_smE_state = smE_Pausing;
                IF_DEBUG
                    printf("smE (Quiescent):  Leaving for Pausing state\n");
                END_DEBUG
            }
        } else {
            /* We are in Quiescent - there should be NO existing sessions, and each should be NEW */
            warn("smE (quiescent): Rcvd Discover from an existing session. Discarded.\n");
        }
        break;

      case evtBlockTimeout:
        CANCEL(g_block_timer);
        warn("smE (Quiescent): Killing a block timeout that should already be dead!\n");
        break;

      case evtPacketRcvd:
      case evtEmitRcvd:
      case evtResetRcvd:

      case evtChargeTimeout:
      case evtEmitTimeout:
      case evtHelloDelayTimeout:
      case evtInactivityTimeout:
      default :
        IF_DEBUG
//*/        printf("smE (Quiescent): Ignored event %s\n",smEvent_names[evt->evtType]);
        END_DEBUG
        break;
    }

    return PROCESSING_COMPLETED;
}


/***********************  P A U S I N G   S T A T E  ***********************/

static
enum sm_Status
smE_PausingHandler( protocol_event_t* evt )
{
    IF_TRACED(TRC_STATE)
        if (evt->evtType != evtBlockTimeout)
        {
            printf("smE[%p%s] (Pausing): Entered with event %s", evt->ssn,
               (evt->ssn == g_topo_session)?"-T":"", smEvent_names[evt->evtType]);
            if (g_this_event.evtType==evtPacketRcvd)
            {
                printf(" (%s)\n",Topo_opcode_names[g_opcode]);
            } else {
                puts("");
            }
        }
    END_TRACE

    switch (evt->evtType)
    {
      case evtBlockTimeout:
        band_UpdateStats();
        band_ChooseHelloTime();
        break;

      case evtHelloDelayTimeout:
        packetio_tx_hello();
        if (OnlyCompleteSessions()==TRUE)
        {
            CANCEL(g_block_timer);
	    g_smE_state = smE_Wait;
	    IF_DEBUG
              printf("smE (Pausing): Leaving for Wait state\n");
	    END_DEBUG
	}
        break;

      case evtDiscoverRcvd:
        /* This handles arcs for "Discover", "new session", and
         * "table has only complete sessions" (because an acking-
         * Discover is the only thing that can push the session to
         * the smS_Complete state).
         *
         * This makes "table has only complete sessions" a perfect
         * example of a pseudo-event that is re-calculated everywhere.
         */

        /* Discover: r++, n++, Nmb = n */
        band_IncreaseLoad(TRUE);
        /* new session: ResetNi */
        if (evt->isNewSession)
            band_ResetNi();
        /* Does "table has only complete sessions" event fire? */
        if (OnlyCompleteSessions()==TRUE)
        {
            CANCEL(g_hello_timer);
            CANCEL(g_block_timer);
            g_smE_state = smE_Wait;
            IF_DEBUG
                printf("smE (Pausing): Leaving for Wait state (Only-completed-sessions arc)\n");
            END_DEBUG
        }
        break;

      case evtPacketRcvd:
        /* if (rcvd-Hello) r++, n++ */
        if (g_opcode == Opcode_Hello)
            band_IncreaseLoad(FALSE);
        break;

      case evtInactivityTimeout:
      case evtResetRcvd:
        /* This handles arc for "session table is empty" */
        if (SessionTableIsEmpty()==TRUE)
        {
            CANCEL(g_block_timer);
            CANCEL(g_hello_timer);
            IF_DEBUG
                printf("smE (Pausing): Killing the Block- and Hello-timers after getting reset.\n");
                printf("smE (Pausing): Leaving for Quiescent state\n");
            END_DEBUG
            g_smE_state = smE_Quiescent;
        }
        break;

      case evtEmitRcvd:
      case evtChargeTimeout:
      case evtEmitTimeout:
      default :
        IF_DEBUG
//*/        printf("smE: Pausing state got ignored event %s\n",smEvent_names[evt->evtType]);
        END_DEBUG
        break;
    }

    return PROCESSING_COMPLETED;
}


/***********************  W A I T   S T A T E  ***********************/

static
enum sm_Status
smE_WaitHandler( protocol_event_t* evt )
{
    IF_TRACED(TRC_STATE)
        if (evt->evtType != evtBlockTimeout)
        {
            printf("smE (Wait): Entered with event %s",smEvent_names[evt->evtType]);
            if (g_this_event.evtType==evtPacketRcvd)
            {
                printf(" (%s)\n",Topo_opcode_names[g_opcode]);
            } else {
                puts("");
            }
        }
    END_TRACE

    switch (evt->evtType)
    {
      case evtDiscoverRcvd:
        /* This handles arcs for "new session in complete state", and
         * "new session not in complete state" */

        if (evt->isNewSession)
        {
            if (evt->ssn->ssn_state!=smS_Complete)
            {
                band_InitStats();
                band_ChooseHelloTime();
                g_smE_state = smE_Pausing;
                IF_DEBUG
                    printf("smE (Wait): Leaving for Pausing state\n");
                END_DEBUG
            } else {
                /* just stay here */
            }
        }
        break;

      case evtInactivityTimeout:
      case evtResetRcvd:
        /* This handles arc for "session table is empty" */
        if (SessionTableIsEmpty()==TRUE)
        {
            g_smE_state = smE_Quiescent;
            IF_DEBUG
                printf("smE (Wait): Leaving for Quiescent state\n");
            END_DEBUG
        }
        break;


      case evtPacketRcvd:
      case evtEmitRcvd:
      case evtBlockTimeout:
      case evtChargeTimeout:
      case evtEmitTimeout:
      case evtHelloDelayTimeout:
      default :
        IF_DEBUG
//*/            printf("smE (Wait): Ignored event %s\n",smEvent_names[evt->evtType]);
        END_DEBUG
        break;
    }

    return PROCESSING_COMPLETED;
}


/***********************                             ***********************/
/***********************  S T A T E   M A C H I N E  ***********************/
/***********************                             ***********************/


/* smE_process_event() - process an incoming event (rcvd pkt or timeout)
 *                       and distribute to the current state's handler	*/

enum sm_Status
smE_process_event( protocol_event_t* evt )
{
    IF_DEBUG
//*/        printf("smE_process_event: Entered with event %s\n",smEvent_names[evt->evtType]);
    END_DEBUG
    switch (g_smE_state)
    {
      case smE_Quiescent:
        return smE_QuiescentHandler( evt );

      case smE_Pausing:
        return smE_PausingHandler( evt );

      case smE_Wait:
        return smE_WaitHandler( evt );

      default:
        return PROCESSING_ABORTED;	/* Stop! smE in unknown state! */
    }
}
