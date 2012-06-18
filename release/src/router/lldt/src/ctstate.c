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
            DEBUG({printf("find_session returning session %d @ %X\n",i,(uint)ssn);})
            return ssn;
        }
    }
    DEBUG({puts("find_session returning NULL");})
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


void
dump_packet(uint8_t *buf)
{
    topo_ether_header_t*	ehdr;
    topo_base_header_t*		bhdr;
    topo_discover_header_t*	dhdr;
    topo_hello_header_t*	hhdr;
    topo_qltlv_header_t*	qlhdr;
    topo_queryresp_header_t*	qrhdr;
    topo_flat_header_t*         fhdr;

    uint16_t	seqNum;
    uint16_t	genNum;
    uint16_t	numSta;
    uint16_t	tripleCnt;
    uint16_t	pktCnt;
    uint32_t	byteCnt;

    ehdr = (topo_ether_header_t*)(buf);
    bhdr = (topo_base_header_t*)(ehdr + 1);
    dhdr = (topo_discover_header_t*)(bhdr + 1);
    hhdr = (topo_hello_header_t*)(bhdr + 1);
    qlhdr = (topo_qltlv_header_t*)(bhdr + 1);
    qrhdr = (topo_queryresp_header_t*)(bhdr + 1);
    fhdr  = (topo_flat_header_t*)(bhdr + 1);

    /* Print Ether-hdr */
    printf("==================\nEthernet-header:\n  src=" ETHERADDR_FMT " dst=" ETHERADDR_FMT " E-type=0x%0X\n\n",
		ETHERADDR_PRINT(&ehdr->eh_src), ETHERADDR_PRINT(&ehdr->eh_dst), ehdr->eh_ethertype);

    /* Print the Base hdr */
    seqNum = ntohs(bhdr->tbh_seqnum);
    printf("Base-header:\n  version=%d  ToS=%s  Opcode=%s  SeqNum=%d\n", bhdr->tbh_version, Lld2_tos_names[bhdr->tbh_tos],
           Topo_opcode_names[bhdr->tbh_opcode], seqNum);

    printf("  real-src=" ETHERADDR_FMT " real-dst=" ETHERADDR_FMT "\n\n",
		ETHERADDR_PRINT(&bhdr->tbh_realsrc), ETHERADDR_PRINT(&bhdr->tbh_realdst));
 
    switch (bhdr->tbh_opcode)
    {
      case Opcode_Hello:
        genNum = ntohs(hhdr->hh_gen);
        printf("Hello:   generation-number=0x%0X\n", genNum);
        printf("  mapper current MAC =" ETHERADDR_FMT "   mapper apparent MAC =" ETHERADDR_FMT "\n\n",
		ETHERADDR_PRINT(&hhdr->hh_curmapraddr), ETHERADDR_PRINT(&hhdr->hh_aprmapraddr));
        break;

      case Opcode_Discover:
        genNum = ntohs(dhdr->mh_gen);
        numSta = ntohs(dhdr->mh_numstations);
        printf("Discover:   generation-number=0x%0X   num-stations=%d\n", genNum, numSta);
        break;

      case Opcode_QueryResp:
        tripleCnt = ntohs(qrhdr->qr_numdescs);
        printf("QueryResp:  num-triples=%d   %smore to send\n", tripleCnt & 0x7FFF, (tripleCnt & 0x8000) ? "":"no ");
        break;

      case Opcode_Flat:
        pktCnt  = ntohs(fhdr->fh_ctc_packets);
        byteCnt = ntohl(fhdr->fh_ctc_bytes);
        printf("Flat:  ctc-bytes=%d, ctc-packets=%d\n", byteCnt, pktCnt);
        break;

      default:
        break;
    }
}



/*****************************************************************************
 *
 * This code processes the current packet-event (in g_this_event) by locating
 * (and possibly creating) the session with which it must be associated,
 * then passing it to the 3 state machines, smS, smE, and smT, in that order.
 *
 *****************************************************************************/ 
extern	etheraddr_t	uutMAC;

uint
state_process_packet()
{
    session_t           *this_session;
    enum sm_Status       smStatus;

#ifdef  __DEBUG__
    IF_TRACED((TRC_STATE|TRC_PACKET))
        printf("state_process_packet: Entered with event %s",smEvent_names[g_this_event.evtType]);
        if (g_this_event.evtType==evtPacketRcvd)
        {
            printf(" (%s)\n",Topo_opcode_names[g_opcode]);
        } else {
            puts("");
        }
    END_TRACE
#endif

    if (g_opcode == Opcode_Hello  &&  ETHERADDR_IS_ZERO(&uutMAC))
    {
        memcpy(&uutMAC, &g_base_hdr->tbh_realsrc, sizeof(etheraddr_t));
    }
    dump_packet(g_rxbuf);

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

#ifdef  __DEBUG__
    IF_TRACED(TRC_STATE)
        if (g_this_event.evtType!=evtBlockTimeout)
            printf("state_process_timeout: Entered with event %s\n",smEvent_names[g_this_event.evtType]);
    END_TRACE
#endif

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
