/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#ifndef CTL_SOCKET_H
#define CTL_SOCKET_H

#include <sys/types.h>
#include <string.h>

#include "ctl_functions.h"

struct ctl_msg_hdr {
	int cmd;
	int lin;
	int lout;
	int res;
};

#define set_socket_address(sa, string) \
 do {\
  (sa)->sun_family = AF_UNIX; \
  memset((sa)->sun_path, 0, sizeof((sa)->sun_path)); \
  strcpy((sa)->sun_path + 1, (string)); \
 } while (0)

#define RSTP_SERVER_SOCK_NAME ".rstp_server"

/* COMMANDS */

#if 0
int CTL_enable_bridge_rstp(int br_index, int enable);
#endif
#define CMD_CODE_enable_bridge_rstp  101
#define enable_bridge_rstp_ARGS (int br_index, int enable)
struct enable_bridge_rstp_IN {
	int br_index;
	int enable;
};
struct enable_bridge_rstp_OUT {
};
#define enable_bridge_rstp_COPY_IN \
  ({ in->br_index = br_index; in->enable = enable; })
#define enable_bridge_rstp_COPY_OUT ({ (void)0; })
#define enable_bridge_rstp_CALL (in->br_index, in->enable)

#if 0
int CTL_get_bridge_state(int br_index,
			 UID_STP_CFG_T * cfg, UID_STP_STATE_T * state);
#endif
#define CMD_CODE_get_bridge_state  102
#define get_bridge_state_ARGS (int br_index, UID_STP_CFG_T *cfg, UID_STP_STATE_T *state)
struct get_bridge_state_IN {
	int br_index;
};
struct get_bridge_state_OUT {
	UID_STP_CFG_T cfg;
	UID_STP_STATE_T state;
};
#define get_bridge_state_COPY_IN \
  ({ in->br_index = br_index; })
#define get_bridge_state_COPY_OUT ({ *cfg = out->cfg; *state = out->state; })
#define get_bridge_state_CALL (in->br_index, &out->cfg, &out->state)

#if 0
int CTL_set_bridge_config(int br_index, UID_STP_CFG_T * cfg);
#endif
#define CMD_CODE_set_bridge_config  103
#define set_bridge_config_ARGS (int br_index, UID_STP_CFG_T *cfg)
struct set_bridge_config_IN {
	int br_index;
	UID_STP_CFG_T cfg;
};
struct set_bridge_config_OUT {
};
#define set_bridge_config_COPY_IN \
  ({ in->br_index = br_index; in->cfg = *cfg; })
#define set_bridge_config_COPY_OUT ({ (void)0; })
#define set_bridge_config_CALL (in->br_index, &in->cfg)

#if 0
int CTL_get_port_state(int br_index, int port_index,
		       UID_STP_PORT_CFG_T * cfg, UID_STP_PORT_STATE_T * state);
#endif
#define CMD_CODE_get_port_state  104
#define get_port_state_ARGS (int br_index, int port_index, UID_STP_PORT_CFG_T *cfg, UID_STP_PORT_STATE_T *state)
struct get_port_state_IN {
	int br_index;
	int port_index;
};
struct get_port_state_OUT {
	UID_STP_PORT_CFG_T cfg;
	UID_STP_PORT_STATE_T state;
};
#define get_port_state_COPY_IN \
  ({ in->br_index = br_index; in->port_index = port_index; })
#define get_port_state_COPY_OUT ({ *cfg = out->cfg; *state = out->state; })
#define get_port_state_CALL (in->br_index, in->port_index, &out->cfg, &out->state)

#if 0
int CTL_set_port_config(int br_index, int port_index, UID_STP_PORT_CFG_T * cfg);
#endif
#define CMD_CODE_set_port_config  105
#define set_port_config_ARGS (int br_index, int port_index, UID_STP_PORT_CFG_T *cfg)
struct set_port_config_IN {
	int br_index;
	int port_index;
	UID_STP_PORT_CFG_T cfg;
};
struct set_port_config_OUT {
};
#define set_port_config_COPY_IN \
  ({ in->br_index = br_index; in->port_index = port_index; in->cfg = *cfg; })
#define set_port_config_COPY_OUT ({ (void)0; })
#define set_port_config_CALL (in->br_index, in->port_index, &in->cfg)

#if 0
int CTL_set_debug_level(int level);
#endif
#define CMD_CODE_set_debug_level 106
#define set_debug_level_ARGS (int level)
struct set_debug_level_IN {
	int level;
};
struct set_debug_level_OUT {
};
#define set_debug_level_COPY_IN \
  ({ in->level = level; })
#define set_debug_level_COPY_OUT ({ (void)0; })
#define set_debug_level_CALL (in->level)

/* General case part in ctl command server switch */
#define SERVER_MESSAGE_CASE(name) \
case CMD_CODE_ ## name : do { \
  if (0) LOG("CTL command " #name); \
  struct name ## _IN in0, *in = &in0; \
  struct name ## _OUT out0, *out = &out0; \
  if (sizeof(*in) != lin || sizeof(*out) != (outbuf?*lout:0)) { \
  LOG("Bad sizes lin %d != %zd or lout %d != %zd", \
       lin, sizeof(*in), lout?*lout:0, sizeof(*out)); \
    return -1; \
  } \
  memcpy(in, inbuf, lin); \
  int r = CTL_ ## name name ## _CALL; \
  if (r) return r; \
  if (outbuf) memcpy(outbuf, out, *lout); \
  return r; \
} while (0)

/* Wraper for the control functions in the control command client */
#define CLIENT_SIDE_FUNCTION(name) \
int CTL_ ## name name ## _ARGS \
{ \
  struct name ## _IN in0, *in=&in0; \
  struct name ## _OUT out0, *out = &out0; \
  int l = sizeof(*out); \
  name ## _COPY_IN; \
  int res = 0; \
  int r = send_ctl_message(CMD_CODE_ ## name, in, sizeof(*in), out, &l, \
                           &res); \
  if (r || res) LOG("Got return code %d, %d", r, res); \
  if (r) return r; \
  if (res) return res; \
  name ## _COPY_OUT; \
  return r; \
}

#endif
