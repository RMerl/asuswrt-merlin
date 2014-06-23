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

/* This file contains system dependent API
   from the RStp to a operation system (see stp_to.h) */

/* stp_to API for Linux */

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "base.h"
#include "stpm.h"
#include "stp_in.h"
#include "stp_to.h"

extern BITMAP_T        enabled_ports;

/*************
void
stp_trace (const char *format, ...)
{
  #define MAX_MSG_LEN  128
  char     msg[MAX_MSG_LEN];
  va_list  args;

  va_start(args, format);
  vsnprintf (msg, MAX_MSG_LEN-1, format, args);
  printf ("%s\n", msg);
  va_end(args);
  
}
***********/

#ifdef STRONGLY_SPEC_802_1W
int
STP_OUT_set_learning (int port_index, int vlan_id, int enable)
{
  return STP_OK;
}

int
STP_OUT_set_forwarding (int port_index, int vlan_id, int enable)
{
  return STP_OK;
}
#else
/* 
 * In many kinds of hardware the state of ports may
 * be changed with another method
 */
int
STP_OUT_set_port_state (IN int port_index, IN int vlan_id,
            IN RSTP_PORT_STATE state)
{
  return STP_OK;
  //return AR_INT_STP_set_port_state (port_index, vlan_id, state);
}
#endif


void
STP_OUT_get_port_mac (int port_index, unsigned char *mac)
{
  static long pid = -1;
  static unsigned char mac_beg[] = {'\0', '\0', '\0', '\0', '\0', '\0'};

  if (pid < 0) {
    pid = getpid ();
    memcpy (mac_beg + 1, &pid, 4);
  }
  memcpy (mac, mac_beg, 5);
  mac[5] = port_index;
  //memcpy (mac, STP_MAIN_get_port_mac (port_index), 6);
}

int             /* 1- Up, 0- Down */
STP_OUT_get_port_link_status (int port_index)
{
  if (BitmapGetBit (&enabled_ports, (port_index - 1))) return 1;
  return 0;
}

int
STP_OUT_flush_lt (IN int port_index, IN int vlan_id, LT_FLASH_TYPE_T type, char* reason)
{
/****
  stp_trace("clearFDB (%d, %s, '%s')",
        port_index, 
        (type == LT_FLASH_ALL_PORTS_EXCLUDE_THIS) ? "Exclude" : "Only", 
        reason);
****/

  return STP_OK;
}

int
STP_OUT_set_hardware_mode (int vlan_id, UID_STP_MODE_T mode)
{
  return STP_OK;
  //return AR_INT_STP_set_mode (vlan_id, mode);
}


int
STP_OUT_tx_bpdu (int port_index, int vlan_id,
         unsigned char *bpdu, size_t bpdu_len)
{
extern int bridge_tx_bpdu (int port_index, unsigned char *bpdu, size_t bpdu_len);
  return bridge_tx_bpdu (port_index, bpdu, bpdu_len);
}

const char *
STP_OUT_get_port_name (IN int port_index)
{
  static char tmp[4];
  sprintf (tmp, "p%02d", (int) port_index);
  return tmp;
  //return port2str (port_index, &sys_config);
}

unsigned long
STP_OUT_get_deafult_port_path_cost (IN unsigned int portNo)
{
  return 20000;
}

unsigned long STP_OUT_get_port_oper_speed (unsigned int portNo)
{
  if (portNo <= 2)
    return 1000000L;
  else
    return 1000L;
}

int             /* 1- Full, 0- Half */
STP_OUT_get_duplex (IN int port_index)
{
  return 1;
}

int
STP_OUT_get_init_stpm_cfg (IN int vlan_id,
                           INOUT UID_STP_CFG_T* cfg)
{
  cfg->bridge_priority =        DEF_BR_PRIO;
  cfg->max_age =                DEF_BR_MAXAGE;
  cfg->hello_time =             DEF_BR_HELLOT;
  cfg->forward_delay =          DEF_BR_FWDELAY;
  cfg->force_version =          NORMAL_RSTP;

  return STP_OK;
}
  

int
STP_OUT_get_init_port_cfg (IN int vlan_id,
                           IN int port_index,
                           INOUT UID_STP_PORT_CFG_T* cfg)
{
  cfg->port_priority =                  DEF_PORT_PRIO;
  cfg->admin_non_stp =                  DEF_ADMIN_NON_STP;
  cfg->admin_edge =                     DEF_ADMIN_EDGE;
  cfg->admin_port_path_cost =           ADMIN_PORT_PATH_COST_AUTO;
  cfg->admin_point2point =              DEF_P2P;

  return STP_OK;
}



