/** @file lldp_linux_framer.c
 * 
 * See LICENSE file for more info.
 *
 * Authors: Terry Simons (terry.simons@gmail.com)
 * 
 **/

#include "lldp_linux_framer.h"
#include "lldp_port.h"
#include "lldp_debug.h"
#include "platform/framehandler.h"

#include <netinet/in.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>


#define XENOSOCK 0

ssize_t lldp_write(struct lldp_port *lldp_port) {
  
  // Write the frame to the wire
  return sendto(lldp_port->socket, lldp_port->tx.frame, lldp_port->tx.sendsize, 0, NULL, 0);  
}

ssize_t lldp_read(struct lldp_port *lldp_port) {
    lldp_port->rx.recvsize = recvfrom(lldp_port->socket, lldp_port->rx.frame, lldp_port->mtu, 0, 0, 0);

    return(lldp_port->rx.recvsize);
}

void socketCleanupLLDP(struct lldp_port *lldp_port) {

    if(lldp_port != NULL) {
        if(lldp_port->rx.frame != NULL) {   
            debug_printf(DEBUG_INT, "Freeing %s rx framebuffer...\n", lldp_port->if_name);
            free(lldp_port->rx.frame);
            lldp_port->rx.frame = NULL;
        } else {
            debug_printf(DEBUG_NORMAL, "[ERROR] lldp_port->rx.frame was NULL!\n");
        }

        if(lldp_port->tx.frame != NULL) {
            debug_printf(DEBUG_INT, "Freeing %s tx framebuffer...\n", lldp_port->if_name);
            free(lldp_port->tx.frame);
            lldp_port->tx.frame = NULL;
        } else {
            debug_printf(DEBUG_NORMAL, "[ERROR] lldp_port->tx.frame was NULL!\n");
        }

        if(lldp_port->if_name != NULL) {
            debug_printf(DEBUG_INT, "Freeing %s...\n", lldp_port->if_name);
            free(lldp_port->if_name);
            lldp_port->if_name = NULL;
        } else {
            debug_printf(DEBUG_NORMAL, "[ERROR] lldp_port->if_name was NULL!\n");
        }

    } else {
        debug_printf(DEBUG_NORMAL, "[ERROR] lldp_port was NULL!\n");
    }
}

int socketInitializeLLDP(struct lldp_port *lldp_port)
{
  struct ifreq *ifr = calloc(1, sizeof(struct ifreq));
  struct sockaddr_ll *sll = calloc(1, sizeof(struct sockaddr_ll));
    int retval             = 0;

    if(lldp_port->if_name == NULL)
    {
        debug_printf(DEBUG_NORMAL, "Got NULL interface in %s():%d\n", __FUNCTION__, __LINE__);
        return XENOSOCK;
    }

    if(lldp_port->if_index <= 0)
    {
        debug_printf(DEBUG_NORMAL, "'%s' does not appear to be a valid interface name!\n", lldp_port->if_name);
        return XENOSOCK;
    }

    debug_printf(DEBUG_INT, "'%s' is index %d\n", lldp_port->if_name, lldp_port->if_index);

    // Set up the raw socket for the LLDP ethertype
    lldp_port->socket = socket(PF_PACKET, SOCK_RAW, htons(0x88CC));

    if(lldp_port->socket < 0)
    {
        debug_printf(DEBUG_NORMAL, "Couldn't initialize raw socket for interface %s!\n", lldp_port->if_name);
        return XENOSOCK;
    }

    // Set up the interface for binding
    sll->sll_family = PF_PACKET;
    sll->sll_ifindex = lldp_port->if_index;
    sll->sll_protocol = htons(0x88CC);

    // Bind the socket and the interface together to form a useable socket:
    retval = bind(lldp_port->socket, (struct sockaddr *)sll, sizeof(struct sockaddr_ll));

    if(retval < 0) {
        debug_printf(DEBUG_NORMAL, "Error binding raw socket to interface %s in %s():%d!\n", lldp_port->if_name, __FUNCTION__, __LINE__);

        return XENOSOCK;
    }

    ifr->ifr_ifindex = lldp_port->if_index;

    strncpy(ifr->ifr_name, &lldp_port->if_name[0], strlen(lldp_port->if_name));

    if(strlen(ifr->ifr_name) == 0) {
        debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n", __FUNCTION__, __LINE__);
        return XENOSOCK;
    }
    
    /*    retval = _getmac(lldp_port);

    if(retval < 0) {
        debug_printf(DEBUG_NORMAL, "Error getting hardware (MAC) address for interface '%s' in %s():%d - %d:%s!\n", lldp_port->if_name, __FUNCTION__, __LINE__, errno, strerror(errno));

        return retval;
    }

    retval = _getip(lldp_port);

    if (retval < 0) {
        debug_printf(DEBUG_NORMAL, "Error getting interface IP address for interface '%s' in %s():%d!\n", lldp_port->if_name, __FUNCTION__, __LINE__);   
	}*/

    refreshInterfaceData(lldp_port);

    retval = ioctl(lldp_port->socket, SIOCGIFFLAGS, ifr);

    if (retval == -1)
    {
        debug_printf(DEBUG_NORMAL, "Can't get flags for interface '%s' in %s():%d!\n", lldp_port->if_name, __FUNCTION__, __LINE__);
    }


    if ((ifr->ifr_flags & IFF_UP) == 0) {

        debug_printf(DEBUG_INT, "Interface '%s' is down. portEnabled = 0.\n", lldp_port->if_name);  

        lldp_port->portEnabled = 0;
    } 

    // set allmulti on interface
    // need to devise a graceful way to turn off allmulti otherwise it is left on for the interface when problem is terminated.
    retval = ioctl(lldp_port->socket, SIOCGIFFLAGS, ifr);

    if (retval == -1)
    {
        debug_printf(DEBUG_NORMAL, "Can't get flags for interface '%s' in %s():%d!\n", lldp_port->if_name, __FUNCTION__, __LINE__);
    }

    ifr->ifr_flags |=  IFF_ALLMULTI; // allmulti on (verified via ifconfig)
    //  ifr.ifr_flags &= ~IFF_ALLMULTI; // allmulti off (I think)

    retval = ioctl(lldp_port->socket, SIOCSIFFLAGS, ifr);

    if (retval == -1)
    {
        debug_printf(DEBUG_NORMAL, "Can't set flags for interface '%s' in %s():%d!\n", lldp_port->if_name, __FUNCTION__, __LINE__);
    }

    // Discover MTU of our interface.
    retval = ioctl(lldp_port->socket, SIOCGIFMTU, ifr);

    if(retval < 0) 
    {
        debug_printf(DEBUG_NORMAL, "Can't determine MTU for interface '%s' in %s():%d!\n", lldp_port->if_name, __FUNCTION__, __LINE__);

        return retval;
    }   

    lldp_port->mtu = ifr->ifr_ifru.ifru_mtu;

    debug_printf(DEBUG_INT, "[%s] MTU is %d\n", lldp_port->if_name, lldp_port->mtu);

    lldp_port->rx.frame = calloc(1, lldp_port->mtu - 4);
    lldp_port->tx.frame = calloc(1, (lldp_port->mtu - 2));

    if(!lldp_port->rx.frame) {
        debug_printf(DEBUG_NORMAL, "[ERROR] Unable to malloc buffer in %s() at line: %d!\n", __FUNCTION__, __LINE__);
    } else {
        debug_printf(DEBUG_INT, "Created framebuffer for %s at %x\n", lldp_port->if_name, &lldp_port->rx.frame);
    }

    if(!lldp_port->tx.frame) {
        debug_printf(DEBUG_NORMAL, "[ERROR] Unable to malloc buffer in %s() at line: %d!\n", __FUNCTION__, __LINE__);
    } else {
        debug_printf(DEBUG_INT, "Created framebuffer for %s at %x\n", lldp_port->if_name, &lldp_port->tx.frame);
    }

    debug_printf(DEBUG_INT, "Interface (%s) MTU is %d.\n", lldp_port->if_name, lldp_port->mtu);

    free(ifr);
    free(sll);

    return 0;
}

void refreshInterfaceData(struct lldp_port *lldp_port) {
  _getmac(lldp_port);
  
  debug_printf(DEBUG_INT, "%s MAC: ", lldp_port->if_name);
  debug_hex_printf(DEBUG_INT, lldp_port->source_mac, 6);

  _getip(lldp_port);

  debug_printf(DEBUG_INT, "%s IP: %d.%d.%d.%d\n",
	       lldp_port->if_name,
	       lldp_port->source_ipaddr[0],
	       lldp_port->source_ipaddr[1],
	       lldp_port->source_ipaddr[2],
	       lldp_port->source_ipaddr[3]);

}

/***********************************************
 * Get the MAC address of an interface
 ***********************************************/
static int _getmac(struct lldp_port *lldp_port) {
  struct ifreq *ifr = calloc(1, sizeof(struct ifreq));
  int retval = 0;

  ifr->ifr_ifindex = lldp_port->if_index;

  strncpy(ifr->ifr_name, &lldp_port->if_name[0], strlen(lldp_port->if_name));

  retval = ioctl(lldp_port->socket, SIOCGIFHWADDR, ifr);

  if(retval < 0)
    {
      free(ifr);
      return retval;
    }

  memcpy(&lldp_port->source_mac[0], &ifr->ifr_hwaddr.sa_data[0], 6);

  free(ifr);
  return 0;
}

/***********************************************
 * Get the IP address of an interface
 ***********************************************/
static int _getip(struct lldp_port *lldp_port) {
  struct ifreq *ifr = calloc(1, sizeof(struct ifreq));
  int retval = 0;
  
  strncpy(ifr->ifr_name, &lldp_port->if_name[0], strlen(lldp_port->if_name));

  // get interface ip address
  retval = ioctl(lldp_port->socket, SIOCGIFADDR, ifr);
  
  if (retval < 0) {
    
    debug_printf(DEBUG_NORMAL, "SIOCGIFADDR returned an error: %d\n", retval);
    
    // set source ip address to 0.0.0.0 on error. 
    memset(&lldp_port->source_ipaddr[0], 0x00, 4);
    
    // we should consider reverting to the MAC address on error instead.
    
    free(ifr);
    return retval;
  } else {
    memcpy(&lldp_port->source_ipaddr[0], &ifr->ifr_addr.sa_data[2], 4);
  }
  
  free(ifr);
  
  return retval;
}
