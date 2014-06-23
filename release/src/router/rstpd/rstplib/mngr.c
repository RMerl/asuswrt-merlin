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
#include "bitmap.h"
#include "uid.h"

UID_SOCKET_T    main_sock;

typedef struct port_s {
  int port;
  struct bridge_s *bridge_partner;
  int             port_partner;
} PORT_T;

typedef struct bridge_s {
  long      pid;
  long      number_of_ports;
  PORT_T*   ports;
  UID_SOCKET_T  sock;
  struct bridge_s* next;
} BRIDGE_T;

static BRIDGE_T* br_lst = 0;

int disconnect_port (PORT_T* port, char reset_myself)
{
  UID_MSG_T     msg;
  if (! port->bridge_partner) {
    printf ("can't disconnect from port p%02d: it has not a partner\n",
            port->port_partner);
    return -1;
  }

  printf ("disconnect from port p%02d for bridge B%ld\n",
          port->port_partner, port->bridge_partner->pid);
  
  msg.header.sender_pid = getpid ();
  msg.header.cmd_type = UID_CNTRL;
  msg.body.cntrl.cmd = UID_PORT_DISCONNECT;

  msg.header.destination_port = port->port_partner;
  UiD_SocketSendto (&port->bridge_partner->sock, &msg, sizeof (UID_MSG_T));

  if (reset_myself)
    port->bridge_partner = NULL;
  else {
    port = port->bridge_partner->ports + port->port_partner - 1;
    port->bridge_partner = NULL;
  }

  return 0;
}

int register_bridge (UID_MSG_T* msg, UID_SOCKET_T* socket_4_reply)
{
  register BRIDGE_T* newbr;

  /* check if such bridge has just been registered */
  for (newbr = br_lst; newbr; newbr = newbr->next)
    if (newbr->pid == msg->header.sender_pid) {
      printf ("Sorry, such bridge has just been registered\n");
      /* TBT: may be send him SHUTDOWN ? */
    }

  newbr = (BRIDGE_T*) malloc (sizeof (BRIDGE_T));
  if (! newbr) {
    printf ("Sorry, there is no memory for it :(\n");
    return 0;
  }

  newbr->pid = msg->header.sender_pid;
  newbr->ports = (PORT_T*) calloc (msg->body.cntrl.param1, sizeof (PORT_T));
  if (! newbr->ports) {
    printf ("Sorry, there is no memory for its ports :(\n");
    free (newbr);
    return 0;
  }
  newbr->number_of_ports = msg->body.cntrl.param1;
  memcpy (&newbr->sock, socket_4_reply, sizeof (UID_SOCKET_T));

  /* bind it to the begin of list */
  newbr->next = br_lst;
  br_lst = newbr;

  return 0;
}

int unregister_bridge (UID_MSG_T* msg)
{
  register BRIDGE_T* oldbr;
  register BRIDGE_T* prev = NULL;
  register PORT_T*   port;
  register int      indx;

  /* check if such bridge has just been registered */
  for (oldbr = br_lst; oldbr; oldbr = oldbr->next)
    if (oldbr->pid == msg->header.sender_pid) {
      break;
    } else
      prev = oldbr;

  if (! oldbr) {
    printf ("Sorry, this bridge has not yet been registered ? :(\n");
    return 0;
  }

  /* disconnect all its connceted ports */
  for (indx = 0; indx < oldbr->number_of_ports; indx++) {
    port = oldbr->ports + indx;
    if (port->bridge_partner) {
      disconnect_port (port, 0);
    }
  } 
  
  /* delete from the list & free */
  if (prev)
    prev->next = oldbr->next;
  else
    br_lst = oldbr->next;
    
  free (oldbr->ports);
  free (oldbr);
  return 0;
}

static long scan_br_name (char* param)
{
  if ('B' == param[0])
    return strtoul(param + 1, 0, 10);
  else
    return strtoul(param, 0, 10);
}

static int show_bridge (int argc, char** argv)
{
  long           pid = 0;
  register BRIDGE_T* br = NULL;
  register PORT_T*   port;
  register int       indx, cnt = 0;

  if (argc > 1) {
    pid = scan_br_name (argv[1]);
    printf ("You wanted to see B%ld, didn't you ?\n", pid);
  }

  for (br = br_lst; br; br = br->next) 
    if (! pid || pid == br->pid) {
      printf ("%2d. Bridge B%ld has %ld ports:\n",
              ++cnt,
              (long) br->pid, br->number_of_ports);
      for (indx = 0; indx < br->number_of_ports; indx++) {
        port = br->ports + indx;
        if (port->bridge_partner) {
          printf ("port p%02d ", (int) indx + 1);
          printf ("connected to B%ld port p%02d\n",
                  port->bridge_partner->pid,
                  port->port_partner);
        }
      }
    }
  
  if (! cnt) {
    printf ("There are no such bridges :(\n");
  }
  return 0;
}

static void _link_two_ports (BRIDGE_T* br1, PORT_T* port1,  int indx1,
                             BRIDGE_T* br2, PORT_T* port2,  int indx2)
{
  UID_MSG_T     msg;

  port1->bridge_partner = br2;
  port1->port_partner = indx2;
  port2->bridge_partner = br1;
  port2->port_partner = indx1;

  msg.header.sender_pid = getpid ();
  msg.header.cmd_type = UID_CNTRL;
  msg.body.cntrl.cmd = UID_PORT_CONNECT;

  msg.header.destination_port = indx1;
  UiD_SocketSendto (&br1->sock, &msg, sizeof (UID_MSG_T));
  msg.header.destination_port = indx2;
  UiD_SocketSendto (&br2->sock, &msg, sizeof (UID_MSG_T));
}

static int link_bridges (int argc, char** argv)
{
  long       pid1, pid2;
  int        indx1, indx2;
  BRIDGE_T*  br;
  BRIDGE_T*  br1;
  BRIDGE_T*  br2;
  PORT_T*    port1;
  PORT_T*    port2;

  if (argc < 5) {
    printf ("for this command must be 4 argumenta :(\n");
    return 0;
  }

  pid1 = scan_br_name (argv[1]);
  indx1 = strtoul(argv[2], 0, 10);
  pid2 = scan_br_name (argv[3]);
  indx2 = strtoul(argv[4], 0, 10);
  printf ("connect B%ld port p%02d to B%ld port p%02d\n",
          pid1, indx1, pid2, indx2);

  for (br = br_lst; br; br = br->next) {
     //printf ("check B%ld\n", br->pid);
     if (br->pid == pid1) br1 = br;
     if (br->pid == pid2) br2 = br;
  }

  if (! br1 || ! br2) {
    printf ("Sorry, one of these bridges is absent :(\n");
    return 0;
  }

  if (indx1 > br1->number_of_ports || indx1 < 0) {
    printf ("Sorry, p%02d invalid\n", indx1);
    return 0;
  }

  if (indx2 > br2->number_of_ports || indx2 < 0) {
    printf ("Sorry, p%02d invalid\n", indx2);
    return 0;
  }
  
  port1 = br1->ports + indx1 - 1;
  port2 = br2->ports + indx2 - 1;

  if (port1->bridge_partner)
    printf ("port p%02d is connected\n", indx1);
  if (port2->bridge_partner)
    printf ("port p%02d is connected\n", indx2);
  if (port1->bridge_partner || port2->bridge_partner)
    return 0;

  _link_two_ports (br1, port1, indx1,
                   br2, port2, indx2);
  return 0;
}

static int unlink_port (int argc, char** argv)
{
  long pid1;
  int indx1;
  BRIDGE_T*     br;
  BRIDGE_T*     br1;
  BRIDGE_T*     br2;
  PORT_T*       port1;
  PORT_T*       port2;

  if (argc < 3) {
    printf ("for this command must be 2 argumenta :(\n");
    return 0;
  }

  pid1 = scan_br_name (argv[1]);
  indx1 = strtoul(argv[2], 0, 10);


  for (br = br_lst; br; br = br->next) {
     //printf ("check B%ld\n", br->pid);
     if (br->pid == pid1) br1 = br;
  }

  if (! br1) {
    printf ("Sorry, the bridge B%ldis absent :(\n", pid1);
    return 0;
  }

  if (indx1 > br1->number_of_ports || indx1 < 0) {
    printf ("Sorry, port p%02d invalid\n", indx1);
    return 0;
  }

  port1 = br1->ports + indx1 - 1;

  if (! port1->bridge_partner) {
    printf ("port p%02d is disconnected\n", indx1);
    return 0;
  }

  br2 = port1->bridge_partner;
  port2 = br2->ports + port1->port_partner - 1;
  disconnect_port (port1, 1);
  disconnect_port (port2, 1);

  return 0;
}

static int link_ring (int argc, char** argv)
{
  BRIDGE_T*     br1;
  BRIDGE_T*     br2;
  PORT_T*       port1;
  PORT_T*       port2;
  register int indx;

  /* unlink all */
  for (br1 = br_lst; br1; br1 = br1->next) {
    /* disconnect all its connceted ports */
    for (indx = 0; indx < br1->number_of_ports; indx++) {
      port1 = br1->ports + indx;
      if (port1->bridge_partner) {
        printf ("disconnect B%ld ", br1->pid);
        printf ("port p%02d (with B%ld-p%02d)\n",
                  indx + 1,
                  port1->bridge_partner->pid,
                  port1->port_partner);
        br2 = port1->bridge_partner;
        port2 = br2->ports + port1->port_partner - 1;
        disconnect_port (port1, 1);
        disconnect_port (port2, 1);
      }
    } 
  }

  /* buid ring */
  for (br1 = br_lst; br1; br1 = br1->next) {
    br2 = br1->next;
    if (! br2)  br2 = br_lst;
    _link_two_ports (br1, br1->ports + 1, 2,
                     br2, br2->ports + 0, 1);
  }

  return 0;
}

static CMD_DSCR_T lang[] = {
  THE_COMMAND("show", "get bridge[s] connuctivity")
  PARAM_STRING("bridge name", "all")
  THE_FUNC(show_bridge)

  THE_COMMAND("link", "link two bridges")
  PARAM_STRING("first bridge name", NULL)
  PARAM_NUMBER("port number on first bridge", 1, NUMBER_OF_PORTS, NULL)
  PARAM_STRING("second bridge name", NULL)
  PARAM_NUMBER("port number on second bridge", 1, NUMBER_OF_PORTS, NULL)
  THE_FUNC(link_bridges)

  THE_COMMAND("unlink", "unlink the port of the bridge")
  PARAM_STRING("bridge name", NULL)
  PARAM_NUMBER("port number on bridge", 1, NUMBER_OF_PORTS, NULL)
  THE_FUNC(unlink_port)

  THE_COMMAND("ring", "link all bridges into a ring")
  THE_FUNC(link_ring)

  END_OF_LANG
};

void mngr_start (void)
{
  if (0 != UiD_SocketInit (&main_sock, UID_REPL_PATH, UID_BIND_AS_SERVER)) {
    printf ("FATAL: can't init the connection\n");
    exit (-3);
  }

  cli_register_language (lang);
}

void mngr_shutdown (void)
{
  UID_MSG_T msg;
  BRIDGE_T* br;

  msg.header.sender_pid = getpid ();
  msg.header.cmd_type = UID_CNTRL;
  msg.body.cntrl.cmd = UID_BRIDGE_SHUTDOWN;

  for (br = br_lst; br; br = br->next) {
     UiD_SocketSendto (&br->sock, &msg, sizeof (UID_MSG_T));
  }
}

char *get_prompt (void)
{
  static char prompt[MAX_CLI_PROMT];
  snprintf (prompt, MAX_CLI_PROMT - 1, "%s Mngr > ", UT_sprint_time_stamp());
  return prompt;
}

int mngr_control (UID_MSG_T* msg, UID_SOCKET_T* sock_4_reply)
{
  switch (msg->body.cntrl.cmd) {
    default:
    case UID_PORT_CONNECT:
    case UID_PORT_DISCONNECT:
      printf ("Unexpected contol message '%d'\n", (int) msg->body.cntrl.cmd);
      break;
    case UID_BRIDGE_SHUTDOWN:
      printf ("Bridge B%ld shutdown :(\n", (long) msg->header.sender_pid);
      unregister_bridge (msg);
      break;
    case UID_BRIDGE_HANDSHAKE:
      printf ("Bridge B%ld hello :)\n", (long) msg->header.sender_pid);
      register_bridge (msg, sock_4_reply);
      break;
  }

  return 0;
}

int mngr_rx_bpdu (UID_MSG_T* msg, size_t msgsize)
{
  BRIDGE_T*     br;
  PORT_T*   port;
  
  for (br = br_lst; br; br = br->next) {
     if (br->pid == msg->header.sender_pid) {
       break;
     }
  }

  if (! br) {
    printf ("RX BPDU from unknown B%ld\n", msg->header.sender_pid);
    return 0;
  }

  port = br->ports + msg->header.source_port - 1;
  if (! port->bridge_partner) {
    printf ("RX BPDU from unlinked port p%02d of bridge B%ld ?\n",
            (int) msg->header.source_port,
            msg->header.sender_pid);
    return 0;
  }
 
  br = port->bridge_partner;
  msg->header.destination_port = port->port_partner;
  UiD_SocketSendto (&br->sock, msg, sizeof (UID_MSG_T));
  
  return 0;
}

char read_uid (void)
{
  UID_SOCKET_T  sock_4_reply;
  UID_MSG_T msg;
  size_t    msgsize;
  int       rc = 0;

  msgsize = UiD_SocketRecvfrom (&main_sock, &msg, MAX_UID_MSG_SIZE, &sock_4_reply);
  if (msgsize <= 0) {
    printf ("Something wrong in UIF ?\n");
    return 0;
  }

  switch (msg.header.cmd_type) {
    case UID_CNTRL:
      rc =  mngr_control (&msg, &sock_4_reply);
      break;
    case UID_BPDU:
      rc =  mngr_rx_bpdu (&msg, msgsize);
      break;
    default:
      printf ("Unknown message type %d\n", (int) msg.header.cmd_type);
      rc = 0;
  }

  return rc;
}

char shutdown_flag = 0;

int main_loop (void)
{
  fd_set                readfds;
  int                   rc, numfds, sock, kkk;

  //rl_callback_handler_install (get_prompt (), rl_read_cli);

  sock = GET_FILE_DESCRIPTOR(&main_sock);

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

    rc = select (numfds, &readfds, NULL, NULL, NULL);
    if (rc < 0) {             // Error
      if (EINTR == errno) continue; // don't break
      printf ("FATAL_MODE:select failed: %s\n", strerror(errno));
      return -2;
    }

    if (FD_ISSET(0, &readfds)) {
      rl_callback_read_char ();
    }

    if (FD_ISSET(sock, &readfds)) {
      shutdown_flag |= read_uid ();
    }

  } while(! shutdown_flag);
  return 0;
}

int main (int argc, char** argv)
{
  rl_init ();
  
  mngr_start ();

  main_loop ();

  mngr_shutdown ();

  rl_shutdown ();

  return 0;
}

