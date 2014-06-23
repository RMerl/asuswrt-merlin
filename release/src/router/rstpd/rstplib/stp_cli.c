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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "cli.h"
#include "stp_cli.h"
#include "bitmap.h"
#include "uid_stp.h"
#include "stp_in.h"
#include "stp_to.h"

int I_am_a_stupid_hub = 0;

static void
print_bridge_id (UID_BRIDGE_ID_T *bridge_id, unsigned char cr)
{
  printf("%04lX-%02x%02x%02x%02x%02x%02x",
                  (unsigned long) bridge_id->prio,
                  (unsigned char) bridge_id->addr[0],
                  (unsigned char) bridge_id->addr[1],
                  (unsigned char) bridge_id->addr[2],
                  (unsigned char) bridge_id->addr[3],
                  (unsigned char) bridge_id->addr[4],
                  (unsigned char) bridge_id->addr[5]);
  if (cr)
        printf("\n");
}

static char *
stp_state2str (RSTP_PORT_STATE stp_port_state, int detail)
{
  if (detail) {
    switch (stp_port_state) {
      case UID_PORT_DISABLED:   return "Disabled";
      case UID_PORT_DISCARDING: return "Discarding";
      case UID_PORT_LEARNING:   return "Learning";
      case UID_PORT_FORWARDING: return "Forwarding";
      case UID_PORT_NON_STP:    return "NoStp";
      default:                  return "Unknown";
    }
  }

  switch (stp_port_state) {
    case UID_PORT_DISABLED:     return "Dis";
    case UID_PORT_DISCARDING:   return "Blk";
    case UID_PORT_LEARNING:     return "Lrn";
    case UID_PORT_FORWARDING:   return "Fwd";
    case UID_PORT_NON_STP:      return "Non";
    default:                    return "Unk";
  }
}

static void CLI_out_port_id (int port, unsigned char cr)
{
  printf ("%s", STP_OUT_get_port_name (port));
  if (cr)
        printf("\n");
}

static int cli_enable (int argc, char** argv)
{
  UID_STP_CFG_T uid_cfg;
  int rc;

  uid_cfg.field_mask = BR_CFG_STATE;
  uid_cfg.stp_enabled = STP_ENABLED;
  rc = STP_IN_stpm_set_cfg (0, NULL, &uid_cfg);
  if (rc) {
    printf ("can't enable: %s\n", STP_IN_get_error_explanation (rc));
  } else
    I_am_a_stupid_hub = 0;

  return 0;
}

static int cli_disable (int argc, char** argv)
{
  UID_STP_CFG_T uid_cfg;
  int rc;

  uid_cfg.field_mask = BR_CFG_STATE;
  uid_cfg.stp_enabled = STP_DISABLED;
  rc = STP_IN_stpm_set_cfg (0, NULL, &uid_cfg);
  if (rc) {
    printf ("can't disable: %s\n", STP_IN_get_error_explanation (rc));
  } else
    I_am_a_stupid_hub = 1;

  return 0;
}

static int cli_br_get_cfg (int argc, char** argv)
{
  UID_STP_STATE_T uid_state;
  UID_STP_CFG_T   uid_cfg;
  int             rc;

  rc = STP_IN_stpm_get_state (0, &uid_state);
  if (rc) {
    printf ("can't get rstp bridge state: %s\n", STP_IN_get_error_explanation (rc));
    return 0;
  }
  rc = STP_IN_stpm_get_cfg (0, &uid_cfg);
  if (rc) {
    printf ("can't get rstp bridge configuration: %s\n", STP_IN_get_error_explanation (rc));
    return 0;
  }


#if 0
  printf("Interface:       %-7s (tag:%d)    State: ",
    uid_state.vlan_name, (int) uid_state.vlan_id);
#else
  printf("Bridge:          %-7s               State:",
         uid_state.vlan_name);
#endif
  switch (uid_state.stp_enabled) {
    case STP_ENABLED:  printf("enabled\n"); break;
    case STP_DISABLED: printf("disabled\n");break;
    default:           printf("unknown\n"); return 0;
  }

  printf("BridgeId:        "); print_bridge_id (&uid_state.bridge_id, 0);
  printf("     Bridge Proirity: %lu (0x%lX)\n",
    (unsigned long) uid_state.bridge_id.prio, (unsigned long) uid_state.bridge_id.prio);
  if (uid_cfg.force_version < 2)
    printf("Force Version:   stp\n");

  printf("Designated Root: "); print_bridge_id (&uid_state.designated_root, 1);
  if (uid_state.root_port) {
    printf("Root Port:       %04lx (", (unsigned long) uid_state.root_port);
        CLI_out_port_id (uid_state.root_port & 0xfff, False);
    printf("), Root Cost:     %-lu\n", (unsigned long) uid_state.root_path_cost);
  } else {
    printf("Root Port:       none\n");
  }

  if (uid_state.Topo_Change)
    printf ("Topology Change Count: %lu\n", uid_state.Topo_Change_Count);
  else
    printf ("Time Since Topology Change: %lu\n", uid_state.timeSince_Topo_Change);

  printf ("Max Age:         %2d   Bridge Max Age:       %-2d\n",
    (int) uid_state.max_age, (int) uid_cfg.max_age);
  printf ("Hello Time:      %2d   Bridge Hello Time:    %-2d\n",
    (int) uid_state.hello_time, (int) uid_cfg.hello_time);
  printf ("Forward Delay:   %2d   Bridge Forward Delay: %-2d\n",
    (int) uid_state.forward_delay, (int) uid_cfg.forward_delay);
  printf ("Hold Time:       %2d\n", (int) uid_cfg.hold_time);

  return 0;
}

static void
show_rstp_port (BITMAP_T* ports_bitmap, int detail)
{
  UID_STP_STATE_T      uid_state;
  UID_STP_PORT_STATE_T uid_port;
  UID_STP_PORT_CFG_T   uid_cfg;
  int                  port_index;
  int         rc;
  
  rc = STP_IN_stpm_get_state (0, &uid_state);
  if (rc) {
    printf ("can't get rstp bridge state: %s\n", STP_IN_get_error_explanation (rc));
  } else if (! detail) {
    printf (" BridgeId: "); print_bridge_id (&uid_state.bridge_id, 0);
    printf ("  RootId: "); print_bridge_id (&uid_state.designated_root, 1);
  }

  for (port_index = 0; port_index <= NUMBER_OF_PORTS; port_index++) {
    if (! BitmapGetBit(ports_bitmap, port_index - 1)) continue;
    uid_port.port_no = port_index;
    rc = STP_IN_port_get_state (0, &uid_port);
    if (rc) {
      printf ("can't get rstp port state: %s\n", STP_IN_get_error_explanation (rc));
      continue;
    }
    memset (&uid_cfg, 0, sizeof (UID_STP_PORT_CFG_T));
    rc = STP_IN_port_get_cfg (0, uid_port.port_no, &uid_cfg);
    if (rc) {
      printf ("can't get rstp port config: %s\n", STP_IN_get_error_explanation (rc));
      continue;
    }

    if (detail) {
      printf("Stp Port "); CLI_out_port_id (port_index, False);
#if 0
      printf(": PortId: %04lx in vlan '%s' with tag %d:\n",
        (unsigned long) uid_port.port_id, uid_state.vlan_name, (int) uid_state.vlan_id);
#else
      printf(": PortId: %04lx in Bridge '%s':\n",
        (unsigned long) uid_port.port_id, uid_state.vlan_name);
#endif
      printf ("Priority:          %-d\n", (int) (uid_port.port_id >> 8));
      printf ("State:             %-16s", stp_state2str (uid_port.state, 1));
      printf ("       Uptime: %-9lu\n", uid_port.uptime);
      printf ("PortPathCost:      admin: ");
      if (ADMIN_PORT_PATH_COST_AUTO == uid_cfg.admin_port_path_cost)
        printf ("%-9s", "Auto");
      else
        printf ("%-9lu", uid_cfg.admin_port_path_cost);
      printf ("       oper: %-9lu\n", uid_port.oper_port_path_cost);

      printf ("Point2Point:       admin: ");
      switch (uid_cfg.admin_point2point) {
        case P2P_FORCE_TRUE:
          printf ("%-9s", "ForceYes");
          break;
        case P2P_FORCE_FALSE:
          printf ("%-9s", "ForceNo");
          break;
        case P2P_AUTO:
          printf ("%-9s", "Auto");
          break;
      }
      printf ("       oper: %-9s\n", uid_port.oper_point2point ? "Yes" : "No");
      printf ("Edge:              admin: %-9s       oper: %-9s\n",
              uid_cfg.admin_edge ? "Y" : "N",
              uid_port.oper_edge ? "Y" : "N");
      printf ("Partner:                                  oper: %-9s\n",
              uid_port.oper_stp_neigb ? "Slow" : "Rapid");
        
      if (' ' != uid_port.role) {
        if ('-' != uid_port.role) {
          printf("PathCost:          %-lu\n", (unsigned long) (uid_port.path_cost));
          printf("Designated Root:   "); print_bridge_id (&uid_port.designated_root, 1);
          printf("Designated Cost:   %-ld\n", (unsigned long) uid_port.designated_cost);
          printf("Designated Bridge: "); print_bridge_id (&uid_port.designated_bridge, 1);
          printf("Designated Port:   %-4lx\n\r", (unsigned long) uid_port.designated_port);
        }
        printf("Role:              ");
        switch (uid_port.role) {
          case 'A': printf("Alternate\n"); break;
          case 'B': printf("Backup\n"); break;
          case 'R': printf("Root\n"); break;
          case 'D': printf("Designated\n"); break;
          case '-': printf("NonStp\n"); break;
          default:  printf("Unknown(%c)\n", uid_port.role); break;
        }

        if ('R' == uid_port.role || 'D' == uid_port.role) {
          /* printf("Tc:                %c  ", uid_port.tc ? 'Y' : 'n'); */
          printf("TcAck:             %c  ",
               uid_port.top_change_ack ?  'Y' : 'N');
          printf("TcWhile:       %3d\n", (int) uid_port.tcWhile);
        }
      }

      if (UID_PORT_DISABLED == uid_port.state || '-' == uid_port.role) {
#if 0
        printf("helloWhen:       %3d  ", (int) uid_port.helloWhen);
        printf("lnkWhile:      %3d\n", (int) uid_port.lnkWhile);
        printf("fdWhile:         %3d\n", (int) uid_port.fdWhile);
#endif
      } else if ('-' != uid_port.role) {
        printf("fdWhile:         %3d  ", (int) uid_port.fdWhile);
        printf("rcvdInfoWhile: %3d\n", (int) uid_port.rcvdInfoWhile);
        printf("rbWhile:         %3d  ", (int) uid_port.rbWhile);
        printf("rrWhile:       %3d\n", (int) uid_port.rrWhile);
#if 0
        printf("mdelayWhile:     %3d  ", (int) uid_port.mdelayWhile);
        printf("lnkWhile:      %3d\n", (int) uid_port.lnkWhile);
        printf("helloWhen:       %3d  ", (int) uid_port.helloWhen);
        printf("txCount:       %3d\n", (int) uid_port.txCount);
#endif
      }

      printf("RSTP BPDU rx:      %lu\n", (unsigned long) uid_port.rx_rstp_bpdu_cnt);
      printf("CONFIG BPDU rx:    %lu\n", (unsigned long) uid_port.rx_cfg_bpdu_cnt);
      printf("TCN BPDU rx:       %lu\n", (unsigned long) uid_port.rx_tcn_bpdu_cnt);
    } else {
      printf("%c%c%c  ",
        (uid_port.oper_point2point) ? ' ' : '*',
        (uid_port.oper_edge) ?        'E' : ' ',
        (uid_port.oper_stp_neigb) ?   's' : ' ');
      CLI_out_port_id (port_index, False);
      printf(" %04lx %3s ", (unsigned long) uid_port.port_id,
               stp_state2str (uid_port.state, 0));
      printf (" ");
      print_bridge_id (&uid_port.designated_root, 0);
      printf(" ");
      print_bridge_id (&uid_port.designated_bridge, 0);
      printf(" %4lx %c", (unsigned long) uid_port.designated_port,  uid_port.role);
      printf ("\n");
    }
  }
}

static int cli_pr_get_cfg (int argc, char** argv)
{
  BITMAP_T        ports_bitmap;
  int             port_index;
  char        detail;

  if ('a' == argv[1][0]) {
    BitmapSetAllBits(&ports_bitmap);
    detail = 0;
  } else {
    port_index = strtoul(argv[1], 0, 10);
    BitmapClear(&ports_bitmap);
    BitmapSetBit(&ports_bitmap, port_index - 1);
    detail = 1;
  }

  show_rstp_port (&ports_bitmap, detail);

  return 0;
}

static void
set_bridge_cfg_value (unsigned long value, unsigned long val_mask)
{
  UID_STP_CFG_T uid_cfg;
  char*         val_name;
  int           rc;

  uid_cfg.field_mask = val_mask;
  switch (val_mask) {
    case BR_CFG_STATE:
      uid_cfg.stp_enabled = value;
      val_name = "state";
      break;
    case BR_CFG_PRIO:
      uid_cfg.bridge_priority = value;
      val_name = "priority";
      break;
    case BR_CFG_AGE:
      uid_cfg.max_age = value;
      val_name = "max_age";
      break;
    case BR_CFG_HELLO:
      uid_cfg.hello_time = value;
      val_name = "hello_time";
      break;
    case BR_CFG_DELAY:
      uid_cfg.forward_delay = value;
      val_name = "forward_delay";
      break;
    case BR_CFG_FORCE_VER:
      uid_cfg.force_version = value;
      val_name = "force_version";
      break;
    case BR_CFG_AGE_MODE:
    case BR_CFG_AGE_TIME:
    default: printf ("Invalid value mask 0X%lx\n", val_mask);  return;
      break;
  }

  rc = STP_IN_stpm_set_cfg (0, NULL, &uid_cfg);

  if (0 != rc) {
    printf ("Can't change rstp bridge %s:%s", val_name, STP_IN_get_error_explanation (rc));
  } else {
    printf ("Changed rstp bridge %s\n", val_name);
  }
}

static int cli_br_prio (int argc, char** argv)
{
  long      br_prio = 32768L;

  if (strlen (argv[1]) > 2 &&
      (! strncmp (argv[1], "0x", 2) || ! strncmp (argv[1], "0X", 2))) {
    br_prio = strtoul(argv[1] + 2, 0, 16);
  } else {
    br_prio = strtoul(argv[1], 0, 10);
  }

  if (! br_prio) {
    printf ("Warning: newPriority=0, are you sure ?\n");
  }

  set_bridge_cfg_value (br_prio, BR_CFG_PRIO);

  return 0;
}

static int cli_br_maxage (int argc, char** argv)
{
  long      value = 20L;

  value = strtoul(argv[1], 0, 10);
  set_bridge_cfg_value (value, BR_CFG_AGE);
  return 0;
}

static int cli_br_fdelay (int argc, char** argv)
{
  long      value = 15L;

  value = strtoul(argv[1], 0, 10);
  set_bridge_cfg_value (value, BR_CFG_DELAY);
  return 0;
}

static int cli_br_fvers (int argc, char** argv)
{
  long      value = 2L;

  switch (argv[1][0]) {
      case '0':
      case '1':
      case 'f':
      case 'F':
        value = 0L;
        printf ("Accepted 'force_slow'\n");
        break;
      case '2':
      case 'r':
      case 'R':
        printf ("Accepted 'rapid'\n");
        value = 2L;
        break;
      default:
        printf ("Invalid argument '%s'\n", argv[1]);
        return 0;
  }
  
  set_bridge_cfg_value (value, BR_CFG_FORCE_VER);
  return 0;
}

static void
set_rstp_port_cfg_value (int port_index,
                         unsigned long value,
                         unsigned long val_mask)
{
  UID_STP_PORT_CFG_T uid_cfg;
  int           rc, detail;
  char          *val_name;

  if (port_index > 0) {
    BitmapClear(&uid_cfg.port_bmp);
    BitmapSetBit(&uid_cfg.port_bmp, port_index - 1);
    detail = 1;
  } else {
    BitmapSetAllBits(&uid_cfg.port_bmp);
    detail = 0;
  }

  uid_cfg.field_mask = val_mask;
  switch (val_mask) {
    case PT_CFG_MCHECK:
      val_name = "mcheck";
      break;
    case PT_CFG_COST:
      uid_cfg.admin_port_path_cost = value;
      val_name = "path cost";
      break;
    case PT_CFG_PRIO:
      uid_cfg.port_priority = value;
      val_name = "priority";
      break;
    case PT_CFG_P2P:
      uid_cfg.admin_point2point = (ADMIN_P2P_T) value;
      val_name = "p2p flag";
      break;
    case PT_CFG_EDGE:
      uid_cfg.admin_edge = value;
      val_name = "adminEdge";
      break;
    case PT_CFG_NON_STP:
      uid_cfg.admin_non_stp = value;
      val_name = "adminNonStp";
      break;
#ifdef STP_DBG
    case PT_CFG_DBG_SKIP_TX:
      uid_cfg.skip_tx = value;
      val_name = "skip tx";
      break;
    case PT_CFG_DBG_SKIP_RX:
      uid_cfg.skip_rx = value;
      val_name = "skip rx";
      break;
#endif
    case PT_CFG_STATE:
    default:
      printf ("Invalid value mask 0X%lx\n", val_mask);
      return;
  }

  rc = STP_IN_set_port_cfg (0, &uid_cfg);
  if (0 != rc) {
    printf ("can't change rstp port[s] %s: %s\n",
           val_name, STP_IN_get_error_explanation (rc));
  } else {
    printf ("changed rstp port[s] %s\n", val_name);
  }

  /* show_rstp_port (&uid_cfg.port_bmp, 0); */
}

static int cli_prt_prio (int argc, char** argv)
{
  int port_index = 0;
  unsigned long value = 128;

  if ('a' != argv[1][0])
    port_index = strtoul(argv[1], 0, 10);

  value = strtoul(argv[2], 0, 10);
  set_rstp_port_cfg_value (port_index, value, PT_CFG_PRIO);
  return 0;
}

static int cli_prt_pcost (int argc, char** argv)
{
  int port_index = 0;
  unsigned long value = 0;

  if ('a' != argv[1][0])
    port_index = strtoul(argv[1], 0, 10);

  value = strtoul(argv[2], 0, 10);
  set_rstp_port_cfg_value (port_index, value, PT_CFG_COST);
  return 0;
}

static int cli_prt_mcheck (int argc, char** argv)
{
  int port_index = 0;

  if ('a' != argv[1][0])
    port_index = strtoul(argv[1], 0, 10);
  set_rstp_port_cfg_value (port_index, 0, PT_CFG_MCHECK);
  return 0;
}

static int get_bool_arg (int narg, int argc, char** argv,
                         unsigned long* value)
{
  switch (argv[narg][0]) {
    case 'y':
    case 'Y':
      *value = 1;
      break;
    case 'n':
    case 'N':
      *value = 0;
      break;
    default:
      printf ("Invalid Bollean parameter '%s'\n", argv[narg]);
      return -1;
  }
  return 0;
}

static int cli_prt_edge (int argc, char** argv)
{
  int port_index = 0;
  unsigned long value = 1;

  if ('a' != argv[1][0])
    port_index = strtoul(argv[1], 0, 10);

  if (0 != get_bool_arg (2, argc, argv, &value))
    return 0;

  set_rstp_port_cfg_value (port_index, value, PT_CFG_EDGE);
  return 0;
}

static int cli_prt_non_stp (int argc, char** argv)
{
  int port_index = 0;
  unsigned long value = 0;

  if ('a' != argv[1][0])
    port_index = strtoul(argv[1], 0, 10);

  if (0 != get_bool_arg (2, argc, argv, &value))
    return 0;

  set_rstp_port_cfg_value (port_index, value, PT_CFG_NON_STP);
  return 0;
}

static int cli_prt_p2p (int argc, char** argv)
{
  int port_index = 0;
  unsigned long value = P2P_FORCE_TRUE;

  if ('a' != argv[1][0])
    port_index = strtoul(argv[1], 0, 10);

  switch (argv[2][0]) {
      case 'y':
      case 'Y':
        value = P2P_FORCE_TRUE;
        break;
      case 'n':
      case 'N':
        value = P2P_FORCE_FALSE;
        break;
      case 'a':
      case 'A':
        value = P2P_AUTO;
        break;
      default:
        printf ("Invalid parameter '%s'\n", argv[2]);
        return 0;
  }

  set_rstp_port_cfg_value (port_index, (ADMIN_P2P_T) value, PT_CFG_P2P);
  return 0;
}

#ifdef STP_DBG
static int cli_trace (int argc, char** argv)
{
  BITMAP_T ports_bitmap;
  int port_index;

  if ('a' == argv[1][0]) {
    BitmapSetAllBits(&ports_bitmap);
  } else {
    port_index = strtoul(argv[1], 0, 10);
    BitmapClear(&ports_bitmap);
    BitmapSetBit(&ports_bitmap, port_index - 1);
  }

  STP_IN_dbg_set_port_trace (argv[2],
                             argv[3][0] != 'n' && argv[3][0] != 'N',
                             0, &ports_bitmap,
                             1);
  return 0;
}

/****
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  PARAM_ENUM("receive or/and transmit")
    PARAM_ENUM_SEL("rx", "receive")
    PARAM_ENUM_SEL("tx", "transmit")
    PARAM_ENUM_DEFAULT("all")
  PARAM_NUMBER("number of BPDU to skip", 0, 10000, "1")
****/
static int cli_skip (int argc, char** argv)
{
  int port_index = 0, to_skip;

  if ('a' != argv[1][0])
    port_index = strtoul(argv[1], 0, 10);

  to_skip = atoi (argv[3]);

  if ('a' == argv[2][0] || 'r' == argv[2][0]) {
    set_rstp_port_cfg_value (port_index, to_skip, PT_CFG_DBG_SKIP_RX);
  }

  if ('a' == argv[2][0] || 't' == argv[2][0]) {
    set_rstp_port_cfg_value (port_index, to_skip, PT_CFG_DBG_SKIP_TX);
  }
  return 0;
}

static int cli_sleep (int argc, char** argv)
{
  int delay = atoi (argv[1]);
  sleep (delay);
  return 0;
}

#endif

static CMD_DSCR_T lang[] = {
  THE_COMMAND("enable", "enable rstp")
  THE_FUNC(cli_enable)

  THE_COMMAND("disable", "disable rstp")
  THE_FUNC(cli_disable)

  THE_COMMAND("show bridge", "get bridge config")
  THE_FUNC(cli_br_get_cfg)

  THE_COMMAND("show port", "get port config")
  PARAM_NUMBER("port number on bridge", 1, NUMBER_OF_PORTS, "all")
  THE_FUNC(cli_pr_get_cfg)

  THE_COMMAND("bridge priority", "set bridge priority")
  PARAM_NUMBER("priority", MIN_BR_PRIO, MAX_BR_PRIO, "0x8000")
  THE_FUNC(cli_br_prio)

  THE_COMMAND("bridge maxage", "set bridge maxAge")
  PARAM_NUMBER("maxAge", MIN_BR_MAXAGE, MAX_BR_MAXAGE, "20")
  THE_FUNC(cli_br_maxage)

  THE_COMMAND("bridge fdelay", "set bridge forwardDelay")
  PARAM_NUMBER("forwardDelay", MIN_BR_FWDELAY, MAX_BR_FWDELAY, "15")
  THE_FUNC(cli_br_fdelay)

  THE_COMMAND("bridge forseVersion", "set bridge forseVersion")
  PARAM_BOOL("forseVersion", "forse slow", "regular", "no")
  THE_FUNC(cli_br_fvers)

  THE_COMMAND("port priority", "set port priority")
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  PARAM_NUMBER("priority", MIN_PORT_PRIO, MAX_PORT_PRIO, "128")
  THE_FUNC(cli_prt_prio)

  THE_COMMAND("port pcost", "set port path cost")
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  PARAM_NUMBER("path cost (0- for auto)", 0, 200000000, 0)
  THE_FUNC(cli_prt_pcost)

  THE_COMMAND("port mcheck", "set port mcheck")
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  THE_FUNC(cli_prt_mcheck)

  THE_COMMAND("port edge", "set port adminEdge")
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  PARAM_BOOL("adminEdge", "Edge", "noEdge", "Y")
  THE_FUNC(cli_prt_edge)

  THE_COMMAND("port nonStp", "set port adminNonStp")
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  PARAM_BOOL("adminEdge", "Doesn't participate", "Paricipates", "n")
  THE_FUNC(cli_prt_non_stp)

  THE_COMMAND("port p2p", "set port adminPoit2Point")
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  PARAM_ENUM("adminPoit2Point")
    PARAM_ENUM_SEL("y", "forcePointToPoint")
    PARAM_ENUM_SEL("n", "forcePointToMultiPoint")
    PARAM_ENUM_SEL("a", "autoPointToPoint")
    PARAM_ENUM_DEFAULT("a")
  THE_FUNC(cli_prt_p2p)

#ifdef STP_DBG
  THE_COMMAND("trace", "set port trace")
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  PARAM_ENUM("state machine name")
    PARAM_ENUM_SEL("info", "info")
    PARAM_ENUM_SEL("roletrns", "roletrns")
    PARAM_ENUM_SEL("sttrans", "sttrans")
    PARAM_ENUM_SEL("topoch", "topoch")
    PARAM_ENUM_SEL("migrate", "migrate")
    PARAM_ENUM_SEL("transmit", "transmit")
    PARAM_ENUM_SEL("p2p", "p2p")
    PARAM_ENUM_SEL("edge", "edge")
    PARAM_ENUM_SEL("pcost", "pcost")
    PARAM_ENUM_DEFAULT("all")
  PARAM_BOOL("enable/disable", "trace it", "don't trace it", "n")
  THE_FUNC(cli_trace)

  THE_COMMAND("skip", "skip BPDU processing")
  PARAM_NUMBER("port number", 1, NUMBER_OF_PORTS, "all")
  PARAM_ENUM("receive or/and transmit")
    PARAM_ENUM_SEL("rx", "receive")
    PARAM_ENUM_SEL("tx", "transmit")
    PARAM_ENUM_DEFAULT("all")
  PARAM_NUMBER("number of BPDU to skip", 0, 10000, "1")
  THE_FUNC(cli_skip)

  THE_COMMAND("sleep", "sleep")
  PARAM_NUMBER("delay in sec.", 1, 4000, "4")
  THE_FUNC(cli_sleep)
#endif
  
  END_OF_LANG
};

int stp_cli_init (void)
{
   I_am_a_stupid_hub = 0;
   cli_register_language (lang);
   return 0;
}

