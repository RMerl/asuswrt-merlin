/** @file lldp_main.c
 *
 * OpenLLDP Main
 *
 * See LICENSE file for more info.
 * 
 * 
 * Authors: Terry Simons (terry.simons@gmail.com)
 *          Jason Peterson (condurre@users.sourceforge.net)
 *
 *******************************************************************/

#ifdef WIN32
#include "stdintwin.h"
#define VERSION 0.4
#endif

#ifndef WIN32
#include <unistd.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <net/if.h>
#include <getopt.h>
#endif 

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#include "framehandlers/windows/lldp_windows_framer.h"
#endif

#ifdef __LINUX__
#include "framehandlers/linux/lldp_linux_framer.h"
#include <malloc.h>
#endif /* __LINUX__ */

#ifdef __FREEBSD__
#include <netinet/in.h>
#include <net/if_dl.h>
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/bpf.h>
#endif /* __FREEBSD __ */



#ifdef USE_CONFUSE
#include <confuse.h>		//for LLDP-MED Location config file
#endif // USE_CONFUSE

#include "lldp_port.h"
#include "lldp_debug.h"
#include "tlv/tlv.h"
#include "lldp_neighbor.h"
#include "rx_sm.h"
#include "tx_sm.h"
#include "tlv/tlv.h"
#include "tlv/tlv_common.h"

#include "platform/framehandler.h"

// This is set to argv[0] on startup.
char *program;

struct lci_s lci;
#ifdef USE_CONFUSE
static cfg_t *cfg = NULL;
#endif

void usage();

int initializeLLDP();
void cleanupLLDP();
void handle_segfault();

#define IF_NAMESIZE    16
char *iface_list[IF_NAMESIZE];
int  iface_filter = 0;   /* boolean */
int process_loopback = 0;

struct lldp_port *lldp_ports = NULL;

#ifdef BUILD_SERVICE
SERVICE_STATUS          ServiceStatus; 
SERVICE_STATUS_HANDLE   hStatus; 

void ControlHandler(DWORD request);
#endif


void walk_port_list() {
    struct lldp_port *lldp_port = lldp_ports;

    while(lldp_port != NULL) {
        debug_printf(DEBUG_INT, "Interface structure @ %X\n", lldp_port);
        debug_printf(DEBUG_INT, "\tName: %s\n", lldp_port->if_name);
        debug_printf(DEBUG_INT, "\tIndex: %d\n", lldp_port->if_index);
        debug_printf(DEBUG_INT, "\tMTU: %d\n", lldp_port->mtu);
        debug_printf(DEBUG_INT, "\tMAC: %X:%X:%X:%X:%X:%X\n", lldp_port->source_mac[0]
                                                            , lldp_port->source_mac[1]
                                                            , lldp_port->source_mac[2]
                                                            , lldp_port->source_mac[3]
                                                            , lldp_port->source_mac[4]
                                                            , lldp_port->source_mac[5]); 
        debug_printf(DEBUG_INT, "\tIP: %d.%d.%d.%d\n", lldp_port->source_ipaddr[0]
                                                     , lldp_port->source_ipaddr[1]
                                                     , lldp_port->source_ipaddr[2]
                                                     , lldp_port->source_ipaddr[3]);
        lldp_port = lldp_port->next;
    }
}

#ifdef BUILD_SERVICE
// We are building as a service, so this should be our ServiceMain()
int ServiceMain(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif // BUILD_SERVICE
{
#ifndef WIN32
    uid_t uid;
    struct timeval timeout;
    struct timeval un_timeout;
    int fork = 1;
#endif // WIN32
    int op = 0;
    char *theOpts = "i:d:fshl:o";
    int socket_width = 0;
    time_t current_time = 0;
    time_t last_check = 0;
    int result = 0;
    struct lldp_port *lldp_port = NULL;

#ifdef BUILD_SERVICE
   ServiceStatus.dwServiceType = 
      SERVICE_WIN32; 
   ServiceStatus.dwCurrentState = 
      SERVICE_START_PENDING; 
   ServiceStatus.dwControlsAccepted   =  
      SERVICE_ACCEPT_STOP | 
      SERVICE_ACCEPT_SHUTDOWN;
   ServiceStatus.dwWin32ExitCode = 0; 
   ServiceStatus.dwServiceSpecificExitCode = 0; 
   ServiceStatus.dwCheckPoint = 0; 
   ServiceStatus.dwWaitHint = 0; 
 
   hStatus = RegisterServiceCtrlHandler(
      "OpenLLDP", 
      (LPHANDLER_FUNCTION)ControlHandler); 
   if (hStatus == (SERVICE_STATUS_HANDLE)0) 
   { 
      // Registering Control Handler failed
      return -1; 
   }  
/*	ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
	ServiceStatus.dwWin32ExitCode = 0xfe; 
	SetServiceStatus(hStatus, &ServiceStatus);*/
#endif  // BUILD_SERVICE

    program = argv[0];

#ifndef WIN32
    // Process any arguments we were passed in.
    while ((op = getopt(argc, argv, theOpts)) != EOF) {
      switch (op) {
      case 'd':
	// Set the debug level.
	if ((atoi(optarg) == 0) && (optarg[0] != '0')) {
	  debug_alpha_set_flags(optarg);
	} else {
	  debug_set_flags(atoi(optarg));
	}
	break;
      case 'i':
	iface_filter = 1;
	memcpy(iface_list, optarg, strlen(optarg));
	iface_list[IF_NAMESIZE - 1] = '\0';
	debug_printf(DEBUG_NORMAL, "Using interface %s\n", iface_list);
	break;
      case 'l':
#ifdef USE_CONFUSE
	lci.config_file = optarg;
#else
	debug_printf(DEBUG_NORMAL, "OpenLLDP wasn't compiled with libconfuse support.\n");
	exit(1);
#endif // USE_CONFUSE
	break;
      case 'o':
	// Turn on the looback interface. :)
	process_loopback = 1;
	break;
      case 'f':
	fork = 0;
	break;
      case 's':
	unlink(local.sun_path);
	break;
      case 'h':
      default:
	usage();
	exit(0);
	break;
      };
    }

    neighbor_local_sd = socket(AF_UNIX, SOCK_STREAM, 0);

    if(neighbor_local_sd < 0)
      {
	debug_printf(DEBUG_NORMAL, "[Error] Unable to open unix domain socket for client registration!\n");
      }

    local.sun_family = AF_UNIX;

    strcpy(local.sun_path, "/var/run/lldpd.sock");

    debug_printf(DEBUG_NORMAL, "%s:%d\n", local.sun_path, strlen(local.sun_path));
 
    // Bind to the neighbor list socket.
    result = bind(neighbor_local_sd, (struct sockaddr *)&local, sizeof(local));

    if(result != 0) {
      debug_printf(DEBUG_NORMAL, "[Error] Unable to bind to the unix domain socket for client registration!\n");
    }

    result = listen(neighbor_local_sd, 5);

    if(result != 0) {
      debug_printf(DEBUG_NORMAL, "[Error] Unable to listen to the unix domain socket for client registration!\n");
    }

    // Set the socket permissions
    if(chmod("/var/run/lldpd.sock", S_IWOTH) != 0) {
      debug_printf(DEBUG_NORMAL, "[Error] Unable to set permissions for domain socket!\n");
    }

    /* Needed for select() */
    fd_set readfds;

    fd_set unixfds;

    // get uid of user executing program. 
    uid = getuid();

    if (uid != 0) {
        debug_printf(DEBUG_NORMAL, "You must be running as root to run %s!\n", program);
        exit(0);
    }

#endif // WIN32

	lci.config_file = NULL;

    /* Initialize2 the LLDP subsystem */
    /* This should happen on a per-interface basis */
    if(initializeLLDP() == 0) {
        debug_printf(DEBUG_NORMAL, "No interface found to listen on\n");
    }

    get_sys_desc();
    get_sys_fqdn();

    #ifdef USE_CONFUSE
    //read the location config file for the first time!
    lci_config ();
    #endif // USE_CONFUSE

#ifdef BUILD_SERVICE
     // We report the running status to SCM. 
   ServiceStatus.dwCurrentState = 
      SERVICE_RUNNING; 
   SetServiceStatus (hStatus, &ServiceStatus);

   while (ServiceStatus.dwCurrentState == 
          SERVICE_RUNNING)
   {
	   // Sleep for 1 seconds.
	   Sleep(1000);
   }
#endif // BUILD_SERVICE

#ifndef WIN32

   if (fork) {
     if (daemon(0,0) != 0)
       debug_printf(DEBUG_NORMAL, "Unable to daemonize (%m) at: %s():%d\n",
		    __FUNCTION__, __LINE__);
   }
#endif // WIN32

    while(1) {



#ifndef WIN32
      /* Set up the neighbor client domain socket */
      if(neighbor_local_sd > 0) {
	FD_ZERO(&unixfds);	
	FD_SET(neighbor_local_sd, &unixfds);
      } else {
	debug_printf(DEBUG_NORMAL, "Couldn't initialize the socket (%d)\n", neighbor_local_sd);
      }

        /* Set up select() */
        FD_ZERO(&readfds);
#endif // WIN32

        lldp_port = lldp_ports;

        while(lldp_port != NULL) {
            // This is not the interface you are looking for...
            if(lldp_port->if_name == NULL)
            {
                debug_printf(DEBUG_NORMAL, "[ERROR] Interface index %d with name is NULL at: %s():%d\n", lldp_port->if_index, __FUNCTION__, __LINE__);
                continue;
            }

#ifndef WIN32
            FD_SET(lldp_port->socket, &readfds);

            if(lldp_port->socket > socket_width)
            {
                socket_width = lldp_port->socket;
            }
#endif

            lldp_port = lldp_port->next;

        }

        time(&current_time);

#ifndef WIN32
        // Will be used to tell select how long to wait for...
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Timeout after 1 second if nothing is ready
        result = select(socket_width+1, &readfds, NULL, NULL, &timeout);
#endif // WIN32

        // Everything is cool... process the sockets
        lldp_port = lldp_ports;

        while(lldp_port != NULL) {
            // This is not the interface you are looking for...
            if(lldp_port->if_name == NULL) {
                debug_printf(DEBUG_NORMAL, "[ERROR] Interface index %d with name is NULL at: %s():%d\n", lldp_port->if_index, __FUNCTION__, __LINE__);
                continue;
            }

#ifndef WIN32
            if(result > 0) {
                if(FD_ISSET(lldp_port->socket, &readfds)) {
                    debug_printf(DEBUG_INT, "%s is readable!\n", lldp_port->if_name);

                    // Get the frame back from the OS-specific frame handler.
                    lldp_read(lldp_port);

                    if(lldp_port->rx.recvsize <= 0) {
                        if(errno != EAGAIN && errno != ENETDOWN) {
                            printf("Error: (%d) : %s (%s:%d)\n", errno, strerror(errno), __FUNCTION__, __LINE__);
                        }
                    } else {
                        debug_printf(DEBUG_INT, "Got an LLDP frame %d bytes long on %s!\n", lldp_port->rx.recvsize, lldp_port->if_name);

                        //      debug_hex_dump(DEBUG_INT, lldp_port->rx.frame, lldp_port->rx.recvsize);

                        // Mark that we received a frame so the state machine can process it.
                        lldp_port->rx.rcvFrame = 1;

                        rxStatemachineRun(lldp_port);
                    }
                }

            }
#endif // WIN32
            if((result == 0) || (current_time > last_check)) {
                lldp_port->tick = 1;

                txStatemachineRun(lldp_port); 
                rxStatemachineRun(lldp_port);

#ifndef WIN32
		// Will be used to tell select how long to wait for...
		un_timeout.tv_sec  = 0;
		un_timeout.tv_usec = 2;

		result = select(neighbor_local_sd+1, &unixfds, NULL, NULL, &un_timeout);

		if(result > 0) {
		  if(FD_ISSET(neighbor_local_sd, &unixfds)) {
		    debug_printf(DEBUG_NORMAL, "Got a request on the unix domain socket!\n");

		    socklen_t addrlen = sizeof(remote);

		    neighbor_remote_sd = accept(neighbor_local_sd, (struct sockaddr *)&remote, &addrlen);
		    
		    if(neighbor_remote_sd < 0) {
		      debug_printf(DEBUG_NORMAL, "Couldn't accept remote client socket!\n");
		    } else {		    
		      char *client_msg = lldp_neighbor_information(lldp_ports);
		      int bytes_tx = strlen(client_msg);
		      int bytes_sent = 0;

		      while(bytes_tx > 0) {
			
			debug_printf(DEBUG_NORMAL, "Transmitting %d bytes to client...\n", bytes_tx);
			
			bytes_sent = send(neighbor_remote_sd, client_msg, strlen(client_msg), 0);

			debug_printf(DEBUG_NORMAL, "%d bytes left to send.  Bytes already sent: %d\n\n", bytes_tx, bytes_sent);
		       
			bytes_tx -= bytes_sent;

		      }		    

		      free(client_msg);
		      close(neighbor_remote_sd);		      
		    }
		  }

		}
#endif // WIN32

                lldp_port->tick = 0;
            }

            if(result < 0) {
                if(errno != EINTR) {
                    debug_printf(DEBUG_NORMAL, "[ERROR] %s\n", strerror(errno));
                }
            }

            lldp_port = lldp_port->next;

        }

        time(&last_check);     
    }

    return 0;
}


/***************************************
 *
 * Trap a segfault, and exit cleanly.
 *
 ***************************************/
void handle_segfault()
{
    fprintf(stderr, "[FATAL] SIGSEGV  (Segmentation Fault)!\n");

    fflush(stderr); fflush(stdout);
    exit(-1);
}

/***************************************
 *
 * Trap a HUP, and read location config file new.
 *
 ***************************************/
void
handle_hup()
{
#ifdef USE_CONFUSE
  debug_printf(DEBUG_NORMAL, "[INFO] SIGHUP-> read config file again!\n");
  lci_config();
#endif // USE_CONFUSE
}

int initializeLLDP()
{
    int if_index = 0;
    char if_name[IF_NAMESIZE];
    struct lldp_port *lldp_port = NULL;
    int nb_ifaces = 0;

    /* We need to initialize an LLDP port-per interface */
    /* "lldp_port" will be changed to point at the interface currently being serviced */
    for (if_index = MIN_INTERFACES; if_index < MAX_INTERFACES; if_index++)
    {
#ifndef WIN32
        if(if_indextoname(if_index, if_name) == NULL)
            continue;

        /* keep only the iface specified by -i */
        if (iface_filter) {
            if(strncmp(if_name, (const char*)iface_list, IF_NAMESIZE) != 0) {
                debug_printf(DEBUG_NORMAL, "Skipping interface %s (not %s)\n", if_name, iface_list);
                continue;
            }
        }
#endif // WIN32

	// NB: Probably need to handle OS-specific instances such as "lo"?
	if((strstr(if_name, "lo") != NULL)) 
	  {
	    // Don't process the loopback interface unless we were told to
	    if(process_loopback == 0)
	      {
		continue;
	      }
	  }

        if(strncmp(if_name, "wlt", 3) == 0) {
            debug_printf(DEBUG_NORMAL, "Skipping WLT interface because it's voodoo\n");
            continue;
        }

        if(strncmp(if_name, "sit", 3) == 0) {
            continue;
        }

        if(strncmp(if_name, "git", 3) == 0) {
            continue;
        }

        if(strncmp(if_name, "vmnet", 6) == 0) {
            continue;
        }

        /* Create our new interface struct */
        lldp_port = malloc(sizeof(struct lldp_port));
	memset(lldp_port, 0x0, sizeof(struct lldp_port));

        /* Add it to the global list */
        lldp_port->next = lldp_ports;

        lldp_port->if_index = if_index;
        lldp_port->if_name = malloc(IF_NAMESIZE);
        if(lldp_port->if_name == NULL) {
            free(lldp_port);
            lldp_port = NULL;
            continue;
        }
        
        nb_ifaces++;
        memcpy(lldp_port->if_name, if_name, IF_NAMESIZE);

        debug_printf(DEBUG_INT, "%s (index %d) found. Initializing...\n", lldp_port->if_name, lldp_port->if_index);

        // We want the first state to be LLDP_WAIT_PORT_OPERATIONAL, so we'll blank out everything here.
        lldp_port->portEnabled = 1;

        /* Initialize the socket for this interface */
        if(socketInitializeLLDP(lldp_port) != 0) {
            debug_printf(DEBUG_NORMAL, "[ERROR] Problem initializing socket for %s\n", lldp_port->if_name);
            free(lldp_port->if_name);
            lldp_port->if_name = NULL;
            free(lldp_port);
            lldp_port = NULL;
            continue;
        } else {
            debug_printf(DEBUG_EXCESSIVE, "Finished initializing socket for index %d with name %s\n", lldp_port->if_index, lldp_port->if_name);
        }

        debug_printf(DEBUG_EXCESSIVE, "Initializing TX SM for index %d with name %s\n", lldp_port->if_index, lldp_port->if_name);
        lldp_port->tx.state = TX_LLDP_INITIALIZE;
        txInitializeLLDP(lldp_port);
        debug_printf(DEBUG_EXCESSIVE, "Initializing RX SM for index %d with name %s\n", lldp_port->if_index, lldp_port->if_name);
        lldp_port->rx.state = LLDP_WAIT_PORT_OPERATIONAL;
        rxInitializeLLDP(lldp_port);
        lldp_port->portEnabled  = 0;
        lldp_port->adminStatus  = enabledRxTx;

        debug_printf(DEBUG_EXCESSIVE, "Initializing TLV subsystem for index %d with name %s\n", lldp_port->if_index, lldp_port->if_name);
        /* Initialize the TLV subsystem for this interface */
        tlvInitializeLLDP(lldp_port);
	       

	// Send out the first LLDP frame
	// This allows other devices to see us right when we come up, rather than 
	// having to wait for a full timer cycle
	txChangeToState(lldp_port, TX_IDLE);
	mibConstrInfoLLDPDU(lldp_port);
	txFrame(lldp_port);

        debug_printf(DEBUG_EXCESSIVE, "Adding index %d with name %s to global port list\n", lldp_port->if_index, lldp_port->if_name);
        /* Reset the global list to point at the top of the list */
        /* We only want to get here if initialization succeeded  */
        lldp_ports = lldp_port;
    }

    /* Don't forget to initialize the TLV validators... */
    initializeTLVFunctionValidators();

#ifndef WIN32
    // When we quit, cleanup.
    signal(SIGTERM, cleanupLLDP);
    signal(SIGINT, cleanupLLDP);
    signal(SIGQUIT, cleanupLLDP);
    signal(SIGSEGV, handle_segfault);
    signal(SIGHUP, handle_hup);
#endif // WIN32

    return nb_ifaces;
}

void cleanupLLDP(struct lldp_port *lldp_port) {
  struct lldp_msap *msap_cache = NULL;

  debug_printf(DEBUG_NORMAL, "[AYBABTU] We Get Signal!\n");

  lldp_port = lldp_ports;

  while(lldp_port != NULL) {
    
    msap_cache = lldp_port->msap_cache;
    
    while(msap_cache != NULL) {
      if(msap_cache->tlv_list != NULL) {
	destroy_tlv_list(&msap_cache->tlv_list);
      }
      
      if(msap_cache->id != NULL) 
	free(msap_cache->id);
      
      msap_cache = msap_cache->next;
    }

    free(lldp_port->msap_cache);
    lldp_port->msap_cache = NULL;

    if(lldp_port->if_name != NULL) {
      debug_printf(DEBUG_NORMAL, "[CLEAN] %s (%d)\n", lldp_port->if_name, lldp_port->if_index);
      
      tlvCleanupLLDP(lldp_port);
      
      socketCleanupLLDP(lldp_port);     
    } else {
      debug_printf(DEBUG_NORMAL, "[ERROR] Interface index %d with name is NULL at: %s():%d\n", lldp_port->if_index, __FUNCTION__, __LINE__);
    }
    
    lldp_port = lldp_port->next;
    
    // Clean the previous node and move up.
    free(lldp_ports);
    lldp_ports = lldp_port;
  }
  
#ifndef WIN32
    if(neighbor_local_sd > 0)
      close(neighbor_local_sd);
    unlink(local.sun_path);
#endif

    #ifdef USE_CONFUSE
    if(cfg != NULL)
      cfg_free(cfg);
    #endif

#ifndef BUILD_SERVICE
    exit(0);
#else
      ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
      ServiceStatus.dwWin32ExitCode = 0xff; 
      SetServiceStatus(hStatus, &ServiceStatus);
#endif
}

/****************************************
 *
 * Display our usage information.
 *
 ****************************************/
void usage()
{
    debug_printf(DEBUG_EXCESSIVE, "Entering %s():%d\n", __FUNCTION__, __LINE__);

    debug_printf(DEBUG_NORMAL, "\nlldpd %s\n", VERSION);
    debug_printf(DEBUG_NORMAL, "(c) Copyright 2002 - 2007 The OpenLLDP Group\n");
    debug_printf(DEBUG_NORMAL, "Licensed under the BSD license."
            "\n\n");
    debug_printf(DEBUG_NORMAL, "This product borrows some code from the Open1X project"
            ". (http://www.open1x.org)\n\n");
    debug_printf(DEBUG_NORMAL, "Usage: %s "
            "[-i <device>] "
            "[-d <debug_level>] "
	    "[-l <med location file>]"
            "[-f] "
            "[-s] "
	    "[-o] "
            "\n", program);

    debug_printf(DEBUG_NORMAL, "\n\n");
    debug_printf(DEBUG_NORMAL, "-i <interface> : Use <interface> for LLDP transactions"
            "\n");
    debug_printf(DEBUG_NORMAL, "-d <debug_level/flags> : Set debug verbosity."
            "\n");
    debug_printf(DEBUG_NORMAL, "-l <file> : LLDP-MED Location Identification Configuration File.\n");
    debug_printf(DEBUG_NORMAL, "-f : Run in forground mode.\n");
    debug_printf(DEBUG_NORMAL, "-s : Remove the existing control socket if found.  (Should only be used in system init scripts!)\n");
    debug_printf(DEBUG_NORMAL, "-o : Process LLDP frames on the loopback interface (mainly for testing)\n");
    debug_printf(DEBUG_NORMAL, "\n\n");

    debug_printf(DEBUG_NORMAL, " <debug_level> can be any of : \n");
    debug_printf(DEBUG_NORMAL, "\tA : Enable ALL debug flags.\n");
    debug_printf(DEBUG_NORMAL, "\tc : Enable CONFIG debug flag.\n");
    debug_printf(DEBUG_NORMAL, "\ts : Enable STATE debug flag.\n");
    debug_printf(DEBUG_NORMAL, "\tt : Enable TLV debug flag.\n");
    debug_printf(DEBUG_NORMAL, "\tm : Enable MSAP debug flag.\n");
    debug_printf(DEBUG_NORMAL, "\ti : Enable INT debug flag.\n");
    debug_printf(DEBUG_NORMAL, "\tn : Enable SNMP debug flag.\n");
    debug_printf(DEBUG_NORMAL, "\tx : Enable EXCESSIVE debug flag.\n");
}


#ifdef USE_CONFUSE
/****************************************
 *
 * LCI Location Configuration Information - read from config file
 *
 ****************************************/
void
lci_config()
{
  static int first = 1;
  int j;

  if (first)
    {
      first = 0;
      if (lci.config_file == NULL)
	lci.config_file = "lldp.conf";
    }
  else
    cfg_free(cfg);//free cfg before reading new configuration!
  
  debug_printf (DEBUG_NORMAL, "Using config file %s\n", lci.config_file);
  
  lci.coordinate_based_lci = NULL;
  lci.location_data_format = -1;  //location identification TLV is not created when location_data_format is set to -1. If location_data_format is not found in the location config or no location config file exists, no location identification TLV will be created!
  lci.civic_what = 1;
  lci.civic_countrycode = NULL;
  lci.elin = NULL;

  cfg_opt_t opts[] = {
    CFG_SIMPLE_INT ("location_data_format", &lci.location_data_format),
    CFG_SIMPLE_STR ("coordinate_based_lci", &lci.coordinate_based_lci),
    CFG_SIMPLE_INT ("civic_what", &lci.civic_what),
    CFG_SIMPLE_STR ("civic_countrycode", &lci.civic_countrycode),
    CFG_SIMPLE_STR ("elin", &lci.elin),
    CFG_SIMPLE_STR ("civic_ca0", &lci.civic_ca[0]),//CA Type 0 is language
    CFG_SIMPLE_STR ("civic_ca1", &lci.civic_ca[1]),
    CFG_SIMPLE_STR ("civic_ca2", &lci.civic_ca[2]),
    CFG_SIMPLE_STR ("civic_ca3", &lci.civic_ca[3]),
    CFG_SIMPLE_STR ("civic_ca4", &lci.civic_ca[4]),
    CFG_SIMPLE_STR ("civic_ca5", &lci.civic_ca[5]),
    CFG_SIMPLE_STR ("civic_ca6", &lci.civic_ca[6]),
    CFG_SIMPLE_STR ("civic_ca7", &lci.civic_ca[7]),
    CFG_SIMPLE_STR ("civic_ca8", &lci.civic_ca[8]),
    CFG_SIMPLE_STR ("civic_ca9", &lci.civic_ca[9]),
    CFG_SIMPLE_STR ("civic_ca10", &lci.civic_ca[10]),
    CFG_SIMPLE_STR ("civic_ca11", &lci.civic_ca[11]),
    CFG_SIMPLE_STR ("civic_ca12", &lci.civic_ca[12]),
    CFG_SIMPLE_STR ("civic_ca13", &lci.civic_ca[13]),
    CFG_SIMPLE_STR ("civic_ca14", &lci.civic_ca[14]),
    CFG_SIMPLE_STR ("civic_ca15", &lci.civic_ca[15]),
    CFG_SIMPLE_STR ("civic_ca16", &lci.civic_ca[16]),
    CFG_SIMPLE_STR ("civic_ca17", &lci.civic_ca[17]),
    CFG_SIMPLE_STR ("civic_ca18", &lci.civic_ca[18]),
    CFG_SIMPLE_STR ("civic_ca19", &lci.civic_ca[19]),
    CFG_SIMPLE_STR ("civic_ca20", &lci.civic_ca[20]),
    CFG_SIMPLE_STR ("civic_ca21", &lci.civic_ca[21]),
    CFG_SIMPLE_STR ("civic_ca22", &lci.civic_ca[22]),
    CFG_SIMPLE_STR ("civic_ca23", &lci.civic_ca[23]),
    CFG_SIMPLE_STR ("civic_ca24", &lci.civic_ca[24]),
    CFG_SIMPLE_STR ("civic_ca25", &lci.civic_ca[25]),
    CFG_SIMPLE_STR ("civic_ca26", &lci.civic_ca[26]),
    CFG_SIMPLE_STR ("civic_ca27", &lci.civic_ca[27]),
    CFG_SIMPLE_STR ("civic_ca28", &lci.civic_ca[28]),
    CFG_SIMPLE_STR ("civic_ca29", &lci.civic_ca[29]),
    CFG_SIMPLE_STR ("civic_ca30", &lci.civic_ca[30]),
    CFG_SIMPLE_STR ("civic_ca31", &lci.civic_ca[31]),
    CFG_SIMPLE_STR ("civic_ca32", &lci.civic_ca[32]),
    
    CFG_END ()
  };
  
  //set all civic location elements to NULL in case not defined in config
  for (j = 0; j < 33; j++)
    {
      lci.civic_ca[j] = NULL;
    }
  
  //read config file for location information configuration
  cfg = cfg_init(opts, 0);
  cfg_parse(cfg, lci.config_file);
  
  /*
    if (lci.location_data_format = 2)
    for (j = 0; j < 33; j++)
    {
    if(strlen (lci.civic_ca[j]) > 255 )
    {
    debug_printf(DEBUG_NORMAL, "[ERROR] invalid civic location: CAType %i is too long!\n", j);
    lci.location_data_format = -1;
    }
    }
*/
}
#endif // USE_CONFUSE


#ifdef BUILD_SERVICE
/**
 * Extra functions that are needed to build as a Windows service.
 **/

void ControlHandler(DWORD request) 
{ 
   switch(request) 
   { 
      case SERVICE_CONTROL_STOP: 
         ServiceStatus.dwWin32ExitCode = 0; 
         ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
         SetServiceStatus (hStatus, &ServiceStatus);
         return; 
 
      case SERVICE_CONTROL_SHUTDOWN: 
         ServiceStatus.dwWin32ExitCode = 0; 
         ServiceStatus.dwCurrentState = SERVICE_STOPPED; 
         SetServiceStatus (hStatus, &ServiceStatus);
         return; 
        
      default:
         break;
    } 
 
    // Report current status
    SetServiceStatus (hStatus, &ServiceStatus);
 
    return; 
}

void main() 
{ 
   SERVICE_TABLE_ENTRY ServiceTable[2];
   ServiceTable[0].lpServiceName = "OpenLLDP";
   ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

   ServiceTable[1].lpServiceName = NULL;
   ServiceTable[1].lpServiceProc = NULL;

   // Start the control dispatcher thread for our service
   StartServiceCtrlDispatcher(ServiceTable);  
}

#endif // BUILD_SERVICE
