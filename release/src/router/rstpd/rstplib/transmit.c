/************************************************************************ 
 * RSTP library - Rapid Spanning Tree (802.1t, 802.1w) 
 * Copyright (C) 2001-2003 Optical Access 
 * Author: Alex Rozin 
 * 
 * This file is part of RSTP library. 
 * 
 * RSTP library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published by the 
 * Free Software Foundation; version 2.1 
 * 
 * RSTP library is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser 
 * General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License 
 * along with RSTP library; see the file COPYING.  If not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
 * 02111-1307, USA. 
 **********************************************************************/

/* Port Transmit state machine : 17.27 */
  
#include "base.h"
#include "stpm.h"
#include "stp_to.h" /* for STP_OUT_get_port_mac & STP_OUT_tx_bpdu */

#define BPDU_LEN8023_OFF    12

#define STATES {        \
  CHOOSE(TRANSMIT_INIT),    \
  CHOOSE(TRANSMIT_PERIODIC),    \
  CHOOSE(IDLE),         \
  CHOOSE(TRANSMIT_CONFIG),  \
  CHOOSE(TRANSMIT_TCN),     \
  CHOOSE(TRANSMIT_RSTP),    \
}

#define GET_STATE_NAME STP_transmit_get_state_name
#include "choose.h"

#define MIN_FRAME_LENGTH    64


typedef struct tx_tcn_bpdu_t {
  MAC_HEADER_T  mac;
  ETH_HEADER_T  eth;
  BPDU_HEADER_T hdr;
} TCN_BPDU_T;

typedef struct tx_stp_bpdu_t {
  MAC_HEADER_T  mac;
  ETH_HEADER_T  eth;
  BPDU_HEADER_T hdr;
  BPDU_BODY_T   body;
} CONFIG_BPDU_T;

typedef struct tx_rstp_bpdu_t {
  MAC_HEADER_T  mac;
  ETH_HEADER_T  eth;
  BPDU_HEADER_T hdr;
  BPDU_BODY_T   body;
  unsigned char ver_1_length[2];
} RSTP_BPDU_T;


static RSTP_BPDU_T bpdu_packet  = {
  {/* MAC_HEADER_T */
    {0x01, 0x80, 0xc2, 0x00, 0x00, 0x00},   /* dst_mac */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}    /* src_mac */
  },
  { /* ETH_HEADER_T */
    {0x00, 0x00},               /* len8023 */
    BPDU_L_SAP, BPDU_L_SAP, LLC_UI      /* dsap, ssap, llc */
  },
  {/* BPDU_HEADER_T */
    {0x00, 0x00},               /* protocol */
    BPDU_VERSION_ID, 0x00           /* version, bpdu_type */
  },
  {
    0x00,                   /*  flags; */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  /*  root_id[8]; */
    {0x00,0x00,0x00,0x00},          /*  root_path_cost[4]; */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  /*  bridge_id[8]; */
    {0x00,0x00},                /*  port_id[2]; */
    {0x00,0x00},                /*  message_age[2]; */
    {0x00,0x00},                /*  max_age[2]; */
    {0x00,0x00},                /*  hello_time[2]; */
    {0x00,0x00},                /*  forward_delay[2]; */
  },
   {0x00,0x00},                 /*  ver_1_length[2]; */
};

static size_t
build_bpdu_header (int port_index,
                   unsigned char bpdu_type,
                   unsigned short pkt_len)
{
  unsigned short len8023;

  STP_OUT_get_port_mac (port_index, bpdu_packet.mac.src_mac);

  bpdu_packet.hdr.bpdu_type = bpdu_type;
  bpdu_packet.hdr.version = (BPDU_RSTP == bpdu_type) ?
                            BPDU_VERSION_RAPID_ID    :
                            BPDU_VERSION_ID;

  /* NOTE: I suppose, that sizeof(unsigned short)=2 ! */
  len8023 = htons ((unsigned short) (pkt_len + 3));
  memcpy (&bpdu_packet.eth.len8023, &len8023, 2); 

#ifdef ORIG
  if (pkt_len < MIN_FRAME_LENGTH) pkt_len = MIN_FRAME_LENGTH;
#else
  /* Don't do this. LLC puts in 802.3 length based on what we transmit */
#endif
  return pkt_len;
}

static int
txTcn (STATE_MACH_T* this)
{ /* 17.19.17 (page 68) & 9.3.2 (page 25) */
  register size_t       pkt_len;
  register int          port_index, vlan_id;

#ifdef STP_DBG
  if (this->owner.port->skip_tx > 0) {
    if (1 == this->owner.port->skip_tx)
      stp_trace ("port %s stop tx skipping",
                 this->owner.port->port_name);
    this->owner.port->skip_tx--;
    return STP_Nothing_To_Do;
  }
#endif

  if (this->owner.port->admin_non_stp) return 1;
  port_index = this->owner.port->port_index;
  vlan_id = this->owner.port->owner->vlan_id;

  pkt_len = build_bpdu_header (port_index,
                               BPDU_TOPO_CHANGE_TYPE,
                               sizeof (BPDU_HEADER_T));

#ifdef STP_DBG
  if (this->debug)
    stp_trace ("port %s txTcn", this->owner.port->port_name);
#endif
  return STP_OUT_tx_bpdu (port_index, vlan_id,
                          (unsigned char *) &bpdu_packet,
                          pkt_len);
}

static void
build_config_bpdu (PORT_T* port, Bool set_topo_ack_flag)
{
  bpdu_packet.body.flags = 0;
  if (port->tcWhile) {
#ifdef STP_DBG
    if (port->topoch->debug)
      stp_trace ("tcWhile=%d =>tx TOLPLOGY_CHANGE_BIT to port %s",
                 (int) port->tcWhile, port->port_name);
#endif
    bpdu_packet.body.flags |= TOLPLOGY_CHANGE_BIT;
  }

  if (set_topo_ack_flag && port->tcAck) {
    bpdu_packet.body.flags |= TOLPLOGY_CHANGE_ACK_BIT;
  }

  STP_VECT_set_vector (&port->portPrio, &bpdu_packet.body);
  STP_set_times (&port->portTimes, &bpdu_packet.body);
}

static int
txConfig (STATE_MACH_T* this)
{/* 17.19.15 (page 67) & 9.3.1 (page 23) */
  register size_t   pkt_len;
  register PORT_T*  port = NULL;
  register int      port_index, vlan_id;

#ifdef STP_DBG
  if (this->owner.port->skip_tx > 0) {
    if (1 == this->owner.port->skip_tx)
      stp_trace ("port %s stop tx skipping",
                 this->owner.port->port_name);
    this->owner.port->skip_tx--;
    return STP_Nothing_To_Do;
  }
#endif

  port = this->owner.port;
  if (port->admin_non_stp) return 1;
  port_index = port->port_index;
  vlan_id = port->owner->vlan_id;
  
  pkt_len = build_bpdu_header (port->port_index,
                               BPDU_CONFIG_TYPE,
                               sizeof (BPDU_HEADER_T) + sizeof (BPDU_BODY_T));
  build_config_bpdu (port, True);
 
#ifdef STP_DBG
  if (this->debug)
    stp_trace ("port %s txConfig flags=0X%lx",
        port->port_name,
        (unsigned long) bpdu_packet.body.flags);
#endif
  return STP_OUT_tx_bpdu (port_index, vlan_id,
                          (unsigned char *) &bpdu_packet,
                          pkt_len);
}

static int
txRstp (STATE_MACH_T* this)
{/* 17.19.16 (page 68) & 9.3.3 (page 25) */
  register size_t       pkt_len;
  register PORT_T*      port = NULL;
  register int          port_index, vlan_id;
  unsigned char         role;

#ifdef STP_DBG
  if (this->owner.port->skip_tx > 0) {
    if (1 == this->owner.port->skip_tx)
      stp_trace ("port %s stop tx skipping",
                 this->owner.port->port_name);
    else
      stp_trace ("port %s skip tx %d",
                 this->owner.port->port_name, this->owner.port->skip_tx);

    this->owner.port->skip_tx--;
    return STP_Nothing_To_Do;
  }
#endif

  port = this->owner.port;
  if (port->admin_non_stp) return 1;
  port_index = port->port_index;
  vlan_id = port->owner->vlan_id;

  pkt_len = build_bpdu_header (port->port_index,
                               BPDU_RSTP,
                               sizeof (BPDU_HEADER_T) + sizeof (BPDU_BODY_T) + 1);
  build_config_bpdu (port, False);

  switch (port->selectedRole) {
    default:
    case DisabledPort:
      role = RSTP_PORT_ROLE_UNKN;
      break;
    case AlternatePort:
      role = RSTP_PORT_ROLE_ALTBACK;
      break;
    case BackupPort:
      role = RSTP_PORT_ROLE_ALTBACK;
      break;
    case RootPort:
      role = RSTP_PORT_ROLE_ROOT;
      break;
    case DesignatedPort:
      role = RSTP_PORT_ROLE_DESGN;
      break;
  }

  bpdu_packet.body.flags |= (role << PORT_ROLE_OFFS);
#ifndef ORIG
  if (port->forwarding)
    bpdu_packet.body.flags |= FORWARD_BIT;
  if (port->learning)
    bpdu_packet.body.flags |= LEARN_BIT;
#endif
  if (port->synced) {
#if 0 /* def STP_DBG */
    if (port->roletrns->debug)
      stp_trace ("tx AGREEMENT_BIT to port %s", port->port_name);
#endif
    bpdu_packet.body.flags |= AGREEMENT_BIT;
  }

  if (port->proposing) {
#if 0 /* def STP_DBG */
    if (port->roletrns->debug)
      stp_trace ("tx PROPOSAL_BIT to port %s", port->port_name);
#endif
    bpdu_packet.body.flags |= PROPOSAL_BIT;
  }

#ifdef STP_DBG
  if (this->debug)
    stp_trace ("port %s txRstp flags=0X%lx",
        port->port_name,
        (unsigned long) bpdu_packet.body.flags);
#endif
   
  return STP_OUT_tx_bpdu (port_index, vlan_id,
                          (unsigned char *) &bpdu_packet,
                          pkt_len);
}

void
STP_transmit_enter_state (STATE_MACH_T* this)
{
  register PORT_T*     port = this->owner.port;

  switch (this->State) {
    case BEGIN:
    case TRANSMIT_INIT:
      port->newInfo = False;
      port->helloWhen = 0;
      port->txCount = 0;
      break;
    case TRANSMIT_PERIODIC:
      port->newInfo = port->newInfo ||
                            ((port->role == DesignatedPort) ||
                             ((port->role == RootPort) && port->tcWhile));
      port->helloWhen = port->owner->rootTimes.HelloTime;
      break;
    case IDLE:
      break;
    case TRANSMIT_CONFIG:
      port->newInfo = False;
      txConfig (this);
      port->txCount++;
      port->tcAck = False;
      break;
    case TRANSMIT_TCN:
      port->newInfo = False;
      txTcn (this);
      port->txCount++;
      break;
    case TRANSMIT_RSTP:
      port->newInfo = False;
      txRstp (this);
      port->txCount++;
      port->tcAck = False;
      break;
  };
}
  
Bool
STP_transmit_check_conditions (STATE_MACH_T* this)
{
  register PORT_T*     port = this->owner.port;

  if (BEGIN == this->State) return STP_hop_2_state (this, TRANSMIT_INIT);

  switch (this->State) {
    case TRANSMIT_INIT:
      return STP_hop_2_state (this, IDLE);
    case TRANSMIT_PERIODIC:
      return STP_hop_2_state (this, IDLE);
    case IDLE:
      if (!port->helloWhen) return STP_hop_2_state (this, TRANSMIT_PERIODIC);
      if (!port->sendRSTP && port->newInfo &&
          (port->txCount < TxHoldCount) &&
          (port->role == DesignatedPort) &&
          port->helloWhen)
        return STP_hop_2_state (this, TRANSMIT_CONFIG);
      if (!port->sendRSTP && port->newInfo &&
          (port->txCount < TxHoldCount) &&
          (port->role == RootPort) &&
          port->helloWhen)
        return STP_hop_2_state (this, TRANSMIT_TCN);
      if (port->sendRSTP && port->newInfo &&
          (port->txCount < TxHoldCount) &&
          ((port->role == RootPort) ||
           (port->role == DesignatedPort)))
        return STP_hop_2_state (this, TRANSMIT_RSTP);
      break;
    case TRANSMIT_CONFIG:
      return STP_hop_2_state (this, IDLE);
    case TRANSMIT_TCN:
      return STP_hop_2_state (this, IDLE);
    case TRANSMIT_RSTP:
      return STP_hop_2_state (this, IDLE);
  };
  return False;
}

