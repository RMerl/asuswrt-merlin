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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <readline/readline.h>

#include "cli.h"
#include "uid.h"
#include "stp_cli.h"

#include "base.h"
#include "bitmap.h"
#include "uid_stp.h"
#include "stp_in.h"

long        my_pid = 0;
BITMAP_T    enabled_ports;
UID_SOCKET_T    uid_socket;

int bridge_tx_bpdu (int port_index, unsigned char *bpdu, size_t bpdu_len)
{
  UID_MSG_T msg;

  msg.header.sender_pid = my_pid;
  msg.header.cmd_type = UID_BPDU;
  msg.header.source_port = port_index;
  msg.header.body_len = bpdu_len;
  memcpy (&msg.body, bpdu, bpdu_len);
  UiD_SocketSendto (&uid_socket, &msg, sizeof (UID_MSG_T));
  return 0;
}

int bridge_start (void)
{
  BITMAP_T  ports;
  UID_MSG_T msg;
  UID_STP_CFG_T uid_cfg;
  register int  iii;

  //rl_callback_handler_install (get_prompt (), rl_read_cli);

  if (0 != UiD_SocketInit (&uid_socket, UID_REPL_PATH, UID_BIND_AS_CLIENT)) {
    printf ("FATAL: can't init the connection\n");
    exit (-3);
  }

  /* send HANDSHAKE */
  msg.header.sender_pid = my_pid;
  msg.header.cmd_type = UID_CNTRL;
  msg.body.cntrl.cmd = UID_BRIDGE_HANDSHAKE;
  msg.body.cntrl.param1 = NUMBER_OF_PORTS;
  iii = UiD_SocketSendto (&uid_socket, &msg, sizeof (UID_MSG_T));
  if (iii < 0) {
    printf ("can't send HANDSHAKE: %s\n", strerror(errno));
    printf ("May be 'mngr' is not alive ? :(\n");
    return (-4);
  }
  
  stp_cli_init ();

  STP_IN_init (NUMBER_OF_PORTS);
  BitmapClear(&enabled_ports);
  BitmapClear(&ports);
  for (iii = 1; iii <= NUMBER_OF_PORTS; iii++) {
    BitmapSetBit(&ports, iii - 1);
  }
  
  uid_cfg.field_mask = BR_CFG_STATE;
  uid_cfg.stp_enabled = STP_ENABLED;
  snprintf (uid_cfg.vlan_name, NAME_LEN - 1, "B%ld", (long) my_pid);
  iii = STP_IN_stpm_set_cfg (0, &ports, &uid_cfg);
  if (STP_OK != iii) {
    printf ("FATAL: can't enable:%s\n",
               STP_IN_get_error_explanation (iii));
    return (-1);
  }
  return 0;
}

void bridge_shutdown (void)
{
  UID_MSG_T msg;
  int       rc;

  /* send SHUTDOWN */
  msg.header.sender_pid = my_pid;
  msg.header.cmd_type = UID_CNTRL;
  msg.body.cntrl.cmd = UID_BRIDGE_SHUTDOWN;
  UiD_SocketSendto (&uid_socket, &msg, sizeof (UID_MSG_T));

  rc = STP_IN_stpm_delete (0);
  if (STP_OK != rc) {
    printf ("FATAL: can't delete:%s\n",
               STP_IN_get_error_explanation (rc));
    exit (1);
  }
}

char *get_prompt (void)
{
  static char prompt[MAX_CLI_PROMT];
  snprintf (prompt, MAX_CLI_PROMT - 1, "%s B%ld > ", UT_sprint_time_stamp(), my_pid);
  return prompt;
}

int bridge_control (int port_index,
                    UID_CNTRL_BODY_T* cntrl)
{
  switch (cntrl->cmd) {
    case UID_PORT_CONNECT:
      printf ("connected port p%02d\n", port_index);
      BitmapSetBit(&enabled_ports, port_index - 1);
      STP_IN_enable_port (port_index, True);
      break;
    case UID_PORT_DISCONNECT:
      printf ("disconnected port p%02d\n", port_index);
      BitmapClearBit(&enabled_ports, port_index - 1);
      STP_IN_enable_port (port_index, False);
      break;
    case UID_BRIDGE_SHUTDOWN:
      printf ("shutdown from manager :(\n");
      return 1;
    default:
      printf ("Unknown control command <%d> for port %d\n",
              cntrl->cmd, port_index);
  }
  return 0;
}

int bridge_rx_bpdu (UID_MSG_T* msg, size_t msgsize)
{
  register int port_index;

  if (I_am_a_stupid_hub) { // flooding
    msg->header.sender_pid = my_pid;
    for (port_index = 1; port_index <= NUMBER_OF_PORTS; port_index++) {
      if (BitmapGetBit (&enabled_ports, (port_index - 1)) &&
          msg->header.destination_port != port_index) {
        msg->header.source_port = port_index;
        UiD_SocketSendto (&uid_socket, msg, msgsize);
      }
    }
  } else {
    STP_IN_rx_bpdu (0, msg->header.destination_port,
                    (BPDU_T*) (msg->body.bpdu + sizeof (MAC_HEADER_T)),
                    msg->header.body_len - sizeof (MAC_HEADER_T));
  }
  return 0;
}

char read_uid (UID_SOCKET_T* uid_sock)
{
  char buff[MAX_UID_MSG_SIZE];
  UID_MSG_T* msg;
  size_t msgsize;
  int rc;

  msgsize = UiD_SocketRecvfrom (uid_sock, buff, MAX_UID_MSG_SIZE, 0);
  if (msgsize <= 0) {
    printf ("Something wrong in UIF ?\n");
    return 0;
  }
  
  msg = (UID_MSG_T*) buff;
  switch (msg->header.cmd_type) {
    case UID_CNTRL: 
      rc =  bridge_control (msg->header.destination_port,
                            &msg->body.cntrl);
      break;
    case UID_BPDU:
      rc =  bridge_rx_bpdu (msg, msgsize);
      break;
    default:
      printf ("Unknown message type %d\n", (int) msg->header.cmd_type);
      rc = 0;
  }
  
  return rc;
}

char shutdown_flag = 0;

int main_loop (void)
{
  fd_set        readfds;
  struct timeval    tv;
  struct timeval    now, earliest;
  int           rc, numfds, sock, kkk;

  
  sock = GET_FILE_DESCRIPTOR(&uid_socket);

  gettimeofday (&earliest, NULL);
  earliest.tv_sec++;

  do {
    numfds = -1;
    FD_ZERO(&readfds);

    kkk = 0; /* stdin for commands */
    FD_SET(kkk, &readfds);
    if (kkk > numfds) numfds = kkk;

    FD_SET(sock, &readfds);
    if (sock > numfds) numfds = sock;

    if (numfds < 0)
      numfds = 0;
    else
      numfds++;

    gettimeofday (&now, 0);
    tv.tv_usec = 0;
    tv.tv_sec = 0;

    if (now.tv_sec < earliest.tv_sec) { /* we must wait more than 1 sec. */
      tv.tv_sec = 1;
      tv.tv_usec = 0;
    } else if (now.tv_sec == earliest.tv_sec) {
      if (now.tv_usec < earliest.tv_usec) {
        if (earliest.tv_usec < now.tv_usec)
          tv.tv_usec = 0;
        else
          tv.tv_usec = earliest.tv_usec - now.tv_usec;
      }
    }

    //printf ("wait %ld-%ld\n", (long) tv.tv_sec, (long) tv.tv_usec);
    rc = select (numfds, &readfds, NULL, NULL, &tv);
    if (rc < 0) {             // Error
      if (EINTR == errno) continue; // don't break 
      printf ("FATAL_MODE:select failed: %s\n", strerror(errno));
      return -2;
    }

    if (! rc) {        // Timeout expired
      STP_IN_one_second ();
      gettimeofday (&earliest, NULL);
      //printf ("tick %ld-%ld\n", (long) earliest.tv_sec - 1005042800L, (long) earliest.tv_usec);

      earliest.tv_sec++;
      continue;
    }

    if (FD_ISSET(0, &readfds)) {
      rl_callback_read_char ();
    }

    if (FD_ISSET(sock, &readfds)) {
      shutdown_flag |= read_uid (&uid_socket);
    }

  } while(! shutdown_flag);
  return 0;
}

int main (int argc, char** argv)
{
  rl_init ();
  
  my_pid = getpid();
  printf ("my pid: %ld\n", my_pid); 

  if (0 == bridge_start ()) {
    main_loop ();
  }

  bridge_shutdown ();

  rl_shutdown ();
 
  return 0;
}

char* sprint_time_stump (void)
{
  return UT_sprint_time_stamp();
}

