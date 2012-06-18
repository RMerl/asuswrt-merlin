/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   NBT client - used to lookup netbios names
   Copyright (C) Andrew Tridgell 1994-1998
   
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
   
*/

#define NO_SYSLOG

#include "includes.h"

extern int DEBUGLEVEL;

extern struct in_addr ipzero;

static BOOL use_bcast = True;
static BOOL got_bcast = False;
static struct in_addr bcast_addr;
static BOOL recursion_desired = False;
static BOOL translate_addresses = False;
static int ServerFD= -1;
static int RootPort = 0;
static BOOL find_status=False;

/****************************************************************************
  open the socket communication
  **************************************************************************/
static BOOL open_sockets(void)
{
  ServerFD = open_socket_in( SOCK_DGRAM,
                             (RootPort ? 137 :0),
                             3,
                             interpret_addr(lp_socket_address()), True );

  if (ServerFD == -1)
    return(False);

  set_socket_options(ServerFD,"SO_BROADCAST");

  DEBUG(3, ("Socket opened.\n"));
  return True;
}


/****************************************************************************
usage on the program
****************************************************************************/
static void usage(void)
{
  printf("Usage: nmblookup [-M] [-B bcast address] [-d debuglevel] name\n");
  printf("Version %s\n",VERSION);
  printf("\t-d debuglevel         set the debuglevel\n");
  printf("\t-B broadcast address  the address to use for broadcasts\n");
  printf("\t-U unicast   address  the address to use for unicast\n");
  printf("\t-M                    searches for a master browser\n");
  printf("\t-R                    set recursion desired in packet\n");
  printf("\t-S                    lookup node status as well\n");
  printf("\t-T                    translate IP addresses into names\n");
  printf("\t-r                    Use root port 137 (Win95 only replies to this)\n");
  printf("\t-A                    Do a node status on <name> as an IP Address\n");
  printf("\t-i NetBIOS scope      Use the given NetBIOS scope for name queries\n");
  printf("\t-s smb.conf file      Use the given path to the smb.conf file\n");
  printf("\t-h                    Print this help message.\n");
  printf("\n  If you specify -M and name is \"-\", nmblookup looks up __MSBROWSE__<01>\n");
  printf("\n");
}


/****************************************************************************
send out one query
****************************************************************************/
static BOOL query_one(char *lookup, unsigned int lookup_type)
{
	int j, count;
	struct in_addr *ip_list=NULL;

	if (got_bcast) {
		printf("querying %s on %s\n", lookup, inet_ntoa(bcast_addr));
		ip_list = name_query(ServerFD,lookup,lookup_type,use_bcast,
				     use_bcast?True:recursion_desired,
				     bcast_addr,&count,NULL);
	} else {
		struct in_addr *bcast;
		for (j=iface_count() - 1;
		     !ip_list && j >= 0;
		     j--) {
			bcast = iface_n_bcast(j);
			printf("querying %s on %s\n", 
			       lookup, inet_ntoa(*bcast));
			ip_list = name_query(ServerFD,lookup,lookup_type,
					     use_bcast,
					     use_bcast?True:recursion_desired,
					     *bcast,&count,NULL);
		}
	}

	if (!ip_list) return False;

	for (j=0;j<count;j++) {
		if (translate_addresses) {
			struct hostent *host = gethostbyaddr((char *)&ip_list[j], sizeof(ip_list[j]), AF_INET);
			if (host) {
				printf("%s, ", host -> h_name);
			}
		}
		printf("%s %s<%02x>\n",inet_ntoa(ip_list[j]),lookup, lookup_type);
	}

	/* We can only do find_status if the ip address returned
	   was valid - ie. name_query returned true.
	*/
	if (find_status) {
		printf("Looking up status of %s\n",inet_ntoa(ip_list[0]));
		name_status(ServerFD,lookup,lookup_type,True,ip_list[0],NULL,NULL,NULL);
		printf("\n");
	}

	if (ip_list) free(ip_list);

	return (ip_list != NULL);
}


/****************************************************************************
  main program
****************************************************************************/
int main(int argc,char *argv[])
{
  int opt;
  unsigned int lookup_type = 0x0;
  pstring lookup;
  extern int optind;
  extern char *optarg;
  BOOL find_master=False;
  int i;
  static pstring servicesf = CONFIGFILE;
  BOOL lookup_by_ip = False;
  int commandline_debuglevel = -2;

  DEBUGLEVEL = 1;
  *lookup = 0;

  TimeInit();

  setup_logging(argv[0],True);

  charset_initialise();

  while ((opt = getopt(argc, argv, "d:B:U:i:s:SMrhART")) != EOF)
    switch (opt)
      {
      case 'B':
	bcast_addr = *interpret_addr2(optarg);
	got_bcast = True;
	use_bcast = True;
	break;
      case 'U':
	bcast_addr = *interpret_addr2(optarg);
	got_bcast = True;
	use_bcast = False;
	break;
      case 'T':
        translate_addresses = !translate_addresses;
	break;
      case 'i':
		{
			extern pstring global_scope;
			pstrcpy(global_scope,optarg);
			strupper(global_scope);
		}
	break;
      case 'M':
	find_master = True;
	break;
      case 'S':
	find_status = True;
	break;
      case 'R':
	recursion_desired = True;
	break;
      case 'd':
	commandline_debuglevel = DEBUGLEVEL = atoi(optarg);
	break;
      case 's':
	pstrcpy(servicesf, optarg);
	break;
      case 'r':
        RootPort = -1;
        break;
      case 'h':
	usage();
	exit(0);
	break;
      case 'A':
        lookup_by_ip = True;
        break;
      default:
	usage();
	exit(1);
      }

  if (argc < 2) {
    usage();
    exit(1);
  }

  if (!lp_load(servicesf,True,False,False)) {
    fprintf(stderr, "Can't load %s - run testparm to debug it\n", servicesf);
  }

  /*
   * Ensure we reset DEBUGLEVEL if someone specified it
   * on the command line.
   */

  if(commandline_debuglevel != -2)
    DEBUGLEVEL = commandline_debuglevel;

  load_interfaces();
  if (!open_sockets()) return(1);

  for (i=optind;i<argc;i++)
  {
      char *p;
      struct in_addr ip;

      fstrcpy(lookup,argv[i]);

      if(lookup_by_ip)
      {
        fstrcpy(lookup,"*");
        ip = *interpret_addr2(argv[i]);
        printf("Looking up status of %s\n",inet_ntoa(ip));
        name_status(ServerFD,lookup,lookup_type,True,ip,NULL,NULL,NULL);
        printf("\n");
        continue;
      }

      if (find_master) {
	if (*lookup == '-') {
	  fstrcpy(lookup,"\01\02__MSBROWSE__\02");
	  lookup_type = 1;
	} else {
	  lookup_type = 0x1d;
	}
      }

      p = strchr(lookup,'#');

      if (p) {
        *p = 0;
        sscanf(p+1,"%x",&lookup_type);
      }

      if (!query_one(lookup, lookup_type)) {
	      printf("name_query failed to find name %s\n", lookup);
      }
  }
  
  return(0);
}
