/* This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

/* 
 * Simple program to demonstrate how OSPF API can be used. This
 * application retrieves the LSDB from the OSPF daemon and then
 * originates, updates and finally deletes an application-specific
 * opaque LSA. You can use this application as a template when writing
 * your own application.
 */

/* The following includes are needed in all OSPF API client
   applications. */

#include <zebra.h>
#include "prefix.h" /* needed by ospf_asbr.h */
#include "privs.h"
#include "log.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_opaque.h"
#include "ospfd/ospf_api.h"
#include "ospf_apiclient.h"

/* privileges struct. 
 * set cap_num_* and uid/gid to nothing to use NULL privs
 * as ospfapiclient links in libospf.a which uses privs.
 */
struct zebra_privs_t ospfd_privs =
{
  .user = NULL,
  .group = NULL,
  .cap_num_p = 0,
  .cap_num_i = 0
};

/* The following includes are specific to this application. For
   example it uses threads from libzebra, however your application is
   free to use any thread library (like pthreads). */

#include "ospfd/ospf_dump.h" /* for ospf_lsa_header_dump */
#include "thread.h"
#include "log.h"

/* Local portnumber for async channel. Note that OSPF API library will also
   allocate a sync channel at ASYNCPORT+1. */
#define ASYNCPORT 4000

/* Master thread */
struct thread_master *master;

/* Global variables */
struct ospf_apiclient *oclient;
char **args;

/* Our opaque LSAs have the following format. */
struct my_opaque_lsa
{
  struct lsa_header hdr; /* include common LSA header */
  u_char data[4]; /* our own data format then follows here */
};


/* ---------------------------------------------------------
 * Threads for asynchronous messages and LSA update/delete 
 * ---------------------------------------------------------
 */

static int
lsa_delete (struct thread *t)
{
  struct ospf_apiclient *oclient;
  struct in_addr area_id;
  int rc;

  oclient = THREAD_ARG (t);

  inet_aton (args[6], &area_id);

  printf ("Deleting LSA... ");
  rc = ospf_apiclient_lsa_delete (oclient, 
				  area_id, 
				  atoi (args[2]),       /* lsa type */
				  atoi (args[3]),	/* opaque type */
				  atoi (args[4]));	/* opaque ID */
  printf ("done, return code is = %d\n", rc);
  return rc;
}

static int
lsa_inject (struct thread *t)
{
  struct ospf_apiclient *cl;
  struct in_addr ifaddr;
  struct in_addr area_id;
  u_char lsa_type;
  u_char opaque_type;
  u_int32_t opaque_id;
  void *opaquedata;
  int opaquelen;

  static u_int32_t counter = 1;	/* Incremented each time invoked */
  int rc;

  cl = THREAD_ARG (t);

  inet_aton (args[5], &ifaddr);
  inet_aton (args[6], &area_id);
  lsa_type = atoi (args[2]);
  opaque_type = atoi (args[3]);
  opaque_id = atoi (args[4]);
  opaquedata = &counter;
  opaquelen = sizeof (u_int32_t);

  printf ("Originating/updating LSA with counter=%d... ", counter);
  rc = ospf_apiclient_lsa_originate(cl, ifaddr, area_id,
				    lsa_type,
				    opaque_type, opaque_id,
				    opaquedata, opaquelen);

  printf ("done, return code is %d\n", rc);

  counter++;

  return 0;
}


/* This thread handles asynchronous messages coming in from the OSPF
   API server */
static int
lsa_read (struct thread *thread)
{
  struct ospf_apiclient *oclient;
  int fd;
  int ret;

  printf ("lsa_read called\n");

  oclient = THREAD_ARG (thread);
  fd = THREAD_FD (thread);

  /* Handle asynchronous message */
  ret = ospf_apiclient_handle_async (oclient);
  if (ret < 0) {
    printf ("Connection closed, exiting...");
    exit(0);
  }

  /* Reschedule read thread */
  thread_add_read (master, lsa_read, oclient, fd);

  return 0;
}

/* ---------------------------------------------------------
 * Callback functions for asynchronous events 
 * ---------------------------------------------------------
 */

static void
lsa_update_callback (struct in_addr ifaddr, struct in_addr area_id,
		     u_char is_self_originated,
		     struct lsa_header *lsa)
{
  printf ("lsa_update_callback: ");
  printf ("ifaddr: %s ", inet_ntoa (ifaddr));
  printf ("area: %s\n", inet_ntoa (area_id));
  printf ("is_self_origin: %u\n", is_self_originated);

  /* It is important to note that lsa_header does indeed include the
     header and the LSA payload. To access the payload, first check
     the LSA type and then typecast lsa into the corresponding type,
     e.g.:
     
     if (lsa->type == OSPF_ROUTER_LSA) {
       struct router_lsa *rl = (struct router_lsa) lsa;
       ...
       u_int16_t links = rl->links;
       ...
    }
  */
       
  ospf_lsa_header_dump (lsa);
}

static void
lsa_delete_callback (struct in_addr ifaddr, struct in_addr area_id,
		     u_char is_self_originated,
		     struct lsa_header *lsa)
{
  printf ("lsa_delete_callback: ");
  printf ("ifaddr: %s ", inet_ntoa (ifaddr));
  printf ("area: %s\n", inet_ntoa (area_id));
  printf ("is_self_origin: %u\n", is_self_originated);

  ospf_lsa_header_dump (lsa);
}

static void
ready_callback (u_char lsa_type, u_char opaque_type, struct in_addr addr)
{
  printf ("ready_callback: lsa_type: %d opaque_type: %d addr=%s\n",
	  lsa_type, opaque_type, inet_ntoa (addr));

  /* Schedule opaque LSA originate in 5 secs */
  thread_add_timer (master, lsa_inject, oclient, 5);

  /* Schedule opaque LSA update with new value */
  thread_add_timer (master, lsa_inject, oclient, 10);

  /* Schedule delete */
  thread_add_timer (master, lsa_delete, oclient, 30);
}

static void
new_if_callback (struct in_addr ifaddr, struct in_addr area_id)
{
  printf ("new_if_callback: ifaddr: %s ", inet_ntoa (ifaddr));
  printf ("area_id: %s\n", inet_ntoa (area_id));
}

static void
del_if_callback (struct in_addr ifaddr)
{
  printf ("new_if_callback: ifaddr: %s\n ", inet_ntoa (ifaddr));
}

static void
ism_change_callback (struct in_addr ifaddr, struct in_addr area_id,
		     u_char state)
{
  printf ("ism_change: ifaddr: %s ", inet_ntoa (ifaddr));
  printf ("area_id: %s\n", inet_ntoa (area_id));
  printf ("state: %d [%s]\n", state, LOOKUP (ospf_ism_state_msg, state));
}

static void
nsm_change_callback (struct in_addr ifaddr, struct in_addr nbraddr,
		     struct in_addr router_id, u_char state)
{
  printf ("nsm_change: ifaddr: %s ", inet_ntoa (ifaddr));
  printf ("nbraddr: %s\n", inet_ntoa (nbraddr));
  printf ("router_id: %s\n", inet_ntoa (router_id));
  printf ("state: %d [%s]\n", state, LOOKUP (ospf_nsm_state_msg, state));
}


/* ---------------------------------------------------------
 * Main program 
 * ---------------------------------------------------------
 */

static int usage()
{
  printf("Usage: ospfclient <ospfd> <lsatype> <opaquetype> <opaqueid> <ifaddr> <areaid>\n");
  printf("where ospfd     : router where API-enabled OSPF daemon is running\n");
  printf("      lsatype   : either 9, 10, or 11 depending on flooding scope\n");
  printf("      opaquetype: 0-255 (e.g., experimental applications use > 128)\n");
  printf("      opaqueid  : arbitrary application instance (24 bits)\n");
  printf("      ifaddr    : interface IP address (for type 9) otherwise ignored\n");
  printf("      areaid    : area in IP address format (for type 10) otherwise ignored\n");
  
  exit(1);
}

int
main (int argc, char *argv[])
{
  struct thread thread;

  args = argv;

  /* ospfclient should be started with the following arguments:
   * 
   * (1) host (2) lsa_type (3) opaque_type (4) opaque_id (5) if_addr 
   * (6) area_id
   * 
   * host: name or IP of host where ospfd is running
   * lsa_type: 9, 10, or 11
   * opaque_type: 0-255 (e.g., experimental applications use > 128) 
   * opaque_id: arbitrary application instance (24 bits)
   * if_addr: interface IP address (for type 9) otherwise ignored
   * area_id: area in IP address format (for type 10) otherwise ignored
   */

  if (argc != 7)
    {
      usage();
    }

  /* Initialization */
  zprivs_init (&ospfd_privs);
  master = thread_master_create ();

  /* Open connection to OSPF daemon */
  oclient = ospf_apiclient_connect (args[1], ASYNCPORT);
  if (!oclient)
    {
      printf ("Connecting to OSPF daemon on %s failed!\n",
	      args[1]);
      exit (1);
    }

  /* Register callback functions. */
  ospf_apiclient_register_callback (oclient,
				    ready_callback,
				    new_if_callback,
				    del_if_callback,
				    ism_change_callback,
				    nsm_change_callback,
				    lsa_update_callback, 
				    lsa_delete_callback);

  /* Register LSA type and opaque type. */
  ospf_apiclient_register_opaque_type (oclient, atoi (args[2]),
				       atoi (args[3]));

  /* Synchronize database with OSPF daemon. */
  ospf_apiclient_sync_lsdb (oclient);

  /* Schedule thread that handles asynchronous messages */
  thread_add_read (master, lsa_read, oclient, oclient->fd_async);

  /* Now connection is established, run loop */
  while (1)
    {
      thread_fetch (master, &thread);
      thread_call (&thread);
    }

  /* Never reached */
  return 0;
}

