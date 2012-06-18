/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Luke Kenneth Casson Leighton 1994-1998
   Copyright (C) Jeremy Allison 1994-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
   Revision History:

*/

#include "includes.h"
#include "smb.h"

extern int ClientNMB;
extern int ClientDGRAM;
extern int global_nmb_port;

extern int DEBUGLEVEL;

extern fstring myworkgroup;
extern char **my_netbios_names;
extern struct in_addr ipzero;

/* This is the broadcast subnets database. */
struct subnet_record *subnetlist = NULL;

/* Extra subnets - keep these separate so enumeration code doesn't
   run onto it by mistake. */

struct subnet_record *unicast_subnet = NULL;
struct subnet_record *remote_broadcast_subnet = NULL;
struct subnet_record *wins_server_subnet = NULL;

extern uint16 samba_nb_type; /* Samba's NetBIOS name type. */

/****************************************************************************
  Add a subnet into the list.
  **************************************************************************/

static void add_subnet(struct subnet_record *subrec)
{
	DLIST_ADD(subnetlist, subrec);
}

/* ************************************************************************** **
 * Comparison routine for ordering the splay-tree based namelists assoicated
 * with each subnet record.
 *
 *  Input:  Item  - Pointer to the comparison key.
 *          Node  - Pointer to a node the splay tree.
 *
 *  Output: The return value will be <0 , ==0, or >0 depending upon the
 *          ordinal relationship of the two keys.
 *
 * ************************************************************************** **
 */
static int namelist_entry_compare( ubi_trItemPtr Item, ubi_trNodePtr Node )
  {
  struct name_record *NR = (struct name_record *)Node;

  if( DEBUGLVL( 10 ) )
    {
    struct nmb_name *Iname = (struct nmb_name *)Item;

    Debug1( "nmbd_subnetdb:namelist_entry_compare()\n" );
    Debug1( "%d == memcmp( \"%s\", \"%s\", %d )\n",
            memcmp( Item, &(NR->name), sizeof(struct nmb_name) ),
            nmb_namestr(Iname), nmb_namestr(&NR->name), (int)sizeof(struct nmb_name) );
    }

  return( memcmp( Item, &(NR->name), sizeof(struct nmb_name) ) ); 
  } /* namelist_entry_compare */


/****************************************************************************
stop listening on a subnet
we don't free the record as we don't have proper reference counting for it
yet and it may be in use by a response record
  ****************************************************************************/
void close_subnet(struct subnet_record *subrec)
{
	DLIST_REMOVE(subnetlist, subrec);

	if (subrec->dgram_sock != -1) {
		close(subrec->dgram_sock);
		subrec->dgram_sock = -1;
	}
	if (subrec->nmb_sock != -1) {
		close(subrec->nmb_sock);
		subrec->nmb_sock = -1;
	}
}



/****************************************************************************
  Create a subnet entry.
  ****************************************************************************/

static struct subnet_record *make_subnet(char *name, enum subnet_type type,
					 struct in_addr myip, struct in_addr bcast_ip, 
					 struct in_addr mask_ip)
{
  struct subnet_record *subrec = NULL;
  int nmb_sock, dgram_sock;

  /* Check if we are creating a non broadcast subnet - if so don't create
     sockets.
   */

  if(type != NORMAL_SUBNET)
  {
    nmb_sock = -1;
    dgram_sock = -1;
  }
  else
  {
    /*
     * Attempt to open the sockets on port 137/138 for this interface
     * and bind them.
     * Fail the subnet creation if this fails.
     */

    if((nmb_sock = open_socket_in(SOCK_DGRAM, global_nmb_port,0, myip.s_addr,True)) == -1)
    {
      if( DEBUGLVL( 0 ) )
      {
        Debug1( "nmbd_subnetdb:make_subnet()\n" );
        Debug1( "  Failed to open nmb socket on interface %s ", inet_ntoa(myip) );
        Debug1( "for port %d.  ", global_nmb_port );
        Debug1( "Error was %s\n", strerror(errno) );
      }
      return NULL;
    }

    if((dgram_sock = open_socket_in(SOCK_DGRAM,DGRAM_PORT,3, myip.s_addr,True)) == -1)
    {
      if( DEBUGLVL( 0 ) )
      {
        Debug1( "nmbd_subnetdb:make_subnet()\n" );
        Debug1( "  Failed to open dgram socket on interface %s ", inet_ntoa(myip) );
        Debug1( "for port %d.  ", DGRAM_PORT );
        Debug1( "Error was %s\n", strerror(errno) );
      }
      return NULL;
    }

    /* Make sure we can broadcast from these sockets. */
    set_socket_options(nmb_sock,"SO_BROADCAST");
    set_socket_options(dgram_sock,"SO_BROADCAST");

  }

  subrec = (struct subnet_record *)malloc(sizeof(*subrec));
  
  if (!subrec) 
  {
    DEBUG(0,("make_subnet: malloc fail !\n"));
    close(nmb_sock);
    close(dgram_sock);
    return(NULL);
  }
  
  memset( (char *)subrec, '\0', sizeof(*subrec) );
  (void)ubi_trInitTree( subrec->namelist,
                        namelist_entry_compare,
                        ubi_trOVERWRITE );

  if((subrec->subnet_name = strdup(name)) == NULL)
  {
    DEBUG(0,("make_subnet: malloc fail for subnet name !\n"));
    close(nmb_sock);
    close(dgram_sock);
    ZERO_STRUCTP(subrec);
    free((char *)subrec);
    return(NULL);
  }

  DEBUG(2, ("making subnet name:%s ", name ));
  DEBUG(2, ("Broadcast address:%s ", inet_ntoa(bcast_ip)));
  DEBUG(2, ("Subnet mask:%s\n", inet_ntoa(mask_ip)));
 
  subrec->namelist_changed = False;
  subrec->work_changed = False;
 
  subrec->bcast_ip = bcast_ip;
  subrec->mask_ip  = mask_ip;
  subrec->myip = myip;
  subrec->type = type;
  subrec->nmb_sock = nmb_sock;
  subrec->dgram_sock = dgram_sock;
  
  return subrec;
}


/****************************************************************************
  Create a normal subnet
**************************************************************************/
struct subnet_record *make_normal_subnet(struct interface *iface)
{
	struct subnet_record *subrec;

	subrec = make_subnet(inet_ntoa(iface->ip), NORMAL_SUBNET,
			     iface->ip, iface->bcast, iface->nmask);
	if (subrec) {
		add_subnet(subrec);
	}
	return subrec;
}


/****************************************************************************
  Create subnet entries.
**************************************************************************/

BOOL create_subnets(void)
{    
  int num_interfaces = iface_count();
  int i;
  struct in_addr unicast_ip;
  extern struct in_addr loopback_ip;

  if(num_interfaces == 0)
  {
    DEBUG(0,("create_subnets: No local interfaces !\n"));
    return False;
  }

  /* 
   * Create subnets from all the local interfaces and thread them onto
   * the linked list. 
   */

  for (i = 0 ; i < num_interfaces; i++)
  {
    struct interface *iface = get_interface(i);

    /*
     * We don't want to add a loopback interface, in case
     * someone has added 127.0.0.1 for smbd, nmbd needs to
     * ignore it here. JRA.
     */

    if (ip_equal(iface->ip, loopback_ip)) {
      DEBUG(2,("create_subnets: Ignoring loopback interface.\n" ));
      continue;
    }

    if (!make_normal_subnet(iface)) return False;
  }

  /* 
   * If we have been configured to use a WINS server, then try and
   * get the ip address of it here. If we are the WINS server then
   * set the unicast subnet address to be the first of our own real
   * addresses.
   */

  if(*lp_wins_server())
  {
    struct in_addr real_wins_ip;
    real_wins_ip = *interpret_addr2(lp_wins_server());

    if (!zero_ip(real_wins_ip))
    {
      unicast_ip = real_wins_ip;
    }
    else
    {
      /* The smb.conf's wins server parameter MUST be a host_name
         or an ip_address. */
      DEBUG(0,("invalid smb.conf parameter 'wins server'\n"));
      return False;
    }
  } 
  else if(lp_we_are_a_wins_server())
  {
    /* Pick the first interface ip address as the WINS server ip. */
    unicast_ip = *iface_n_ip(0);
  }
  else
  {
    /* We should not be using a WINS server at all. Set the
      ip address of the subnet to be zero. */
    unicast_ip = ipzero;
  }

  /*
   * Create the unicast and remote broadcast subnets.
   * Don't put these onto the linked list.
   * The ip address of the unicast subnet is set to be
   * the WINS server address, if it exists, or ipzero if not.
   */

  unicast_subnet = make_subnet( "UNICAST_SUBNET", UNICAST_SUBNET, 
                                 unicast_ip, unicast_ip, unicast_ip);

  remote_broadcast_subnet = make_subnet( "REMOTE_BROADCAST_SUBNET",
                                         REMOTE_BROADCAST_SUBNET,
                                         ipzero, ipzero, ipzero);

  if((unicast_subnet == NULL) || (remote_broadcast_subnet == NULL))
    return False;

  /* 
   * If we are WINS server, create the WINS_SERVER_SUBNET - don't put on
   * the linked list.
   */

  if (lp_we_are_a_wins_server())
  {
    if((wins_server_subnet = make_subnet("WINS_SERVER_SUBNET",
                                       WINS_SERVER_SUBNET, 
                                       ipzero, ipzero, ipzero)) == NULL)
      return False;
  }

  return True;
}

/*******************************************************************
Function to tell us if we can use the unicast subnet.
******************************************************************/

BOOL we_are_a_wins_client(void)
{
  static int cache_we_are_a_wins_client = -1;

  if(cache_we_are_a_wins_client == -1)
    cache_we_are_a_wins_client = (ip_equal(ipzero, unicast_subnet->myip) ? 
                                  False : True);

  return cache_we_are_a_wins_client;
}

/*******************************************************************
Access function used by NEXT_SUBNET_INCLUDING_UNICAST
******************************************************************/

struct subnet_record *get_next_subnet_maybe_unicast(struct subnet_record *subrec)
{
  if(subrec == unicast_subnet)
    return NULL;
  else if((subrec->next == NULL) && we_are_a_wins_client())
    return unicast_subnet;
  else
    return subrec->next;
}

/*******************************************************************
 Access function used by retransmit_or_expire_response_records() in
 nmbd_packets.c. Patch from Andrey Alekseyev <fetch@muffin.arcadia.spb.ru>
 Needed when we need to enumerate all the broadcast, unicast and
 WINS subnets.
******************************************************************/

struct subnet_record *get_next_subnet_maybe_unicast_or_wins_server(struct subnet_record *subrec)
{
  if(subrec == unicast_subnet)
  {
    if(wins_server_subnet)
      return wins_server_subnet;
    else
      return NULL;
  }

  if(wins_server_subnet && subrec == wins_server_subnet)
    return NULL;

  if((subrec->next == NULL) && we_are_a_wins_client())
    return unicast_subnet;
  else
    return subrec->next;
}
