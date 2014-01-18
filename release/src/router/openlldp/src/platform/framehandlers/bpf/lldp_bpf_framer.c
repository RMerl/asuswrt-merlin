/** @file lldp_bpf_framer.c
 * 
 * See LICENSE file for more info.
 *
 * Authors: Terry Simons (terry.simons@gmail.com)
 * 
 **/

#include <sys/types.h>
#include "lldp_bpf_framer.h"
#include "lldp_port.h"
#include "lldp_debug.h"
#include "platform/framehandler.h"

#include "bpflib.h"
#include <fcntl.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdint.h>
#include <stdbool.h>

// BPF includes:
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <sys/socket.h>
#include <net/if.h>
#ifndef NETBSD
#include <net/ethernet.h>
#endif // !NETBSD
// End BPF includes

#include <netinet/in.h>
#include <net/if_dl.h>

#define XENOSOCK -1;

#define TRUE 1
#define FALSE 0

ssize_t lldp_write(struct lldp_port *lldp_port) {

  // Write the frame to the wire.
  return write(lldp_port->socket, lldp_port->tx.frame, lldp_port->tx.sendsize);                                                                              
}

ssize_t lldp_read(struct lldp_port *lldp_port) {
  // allocate the bpf_buf to recieve as many packets as will fit in lldp_port->mtu
  // which is the bpf internal buffer size.
  struct bpf_hdr *bpf_buf = malloc(lldp_port->mtu);

  lldp_port->rx.recvsize = read(lldp_port->socket, bpf_buf, lldp_port->mtu);
  
  // Allocate the buffer to be the length of the captured packet
  uint8_t *frame_buffer = malloc(bpf_buf->bh_caplen);

  //XXX:  BUG HERE - We could actually have more than one packet in bpf_buf
  //      we should process bpf_buf in a loop until we have processed all
  //      of the packets in the buffer.  This would mean changing lldp_port->rx
  //      so that there was a linked list of packets in frame so that the next
  //      code section could process all the packets in the queue.  
  //
  //      However the chance of more than one packet being in the buffer is low
  //      and we can safely drop any other frames as well. 
  if(frame_buffer) {
    debug_printf(DEBUG_INT, "(%s) Raw BPF Frame with BPF header: \n", lldp_port->if_name);
    debug_printf(DEBUG_INT, "BPF Header Length: %d\n", bpf_buf->bh_hdrlen);
    debug_hex_dump(DEBUG_INT, (uint8_t *)bpf_buf, lldp_port->rx.recvsize);
  
    // Copy the captured data to the buffer, NOTE this may not be the whole packet!!!
    memcpy(frame_buffer, ((char*) bpf_buf + bpf_buf->bh_hdrlen), bpf_buf->bh_caplen);
    
    debug_printf(DEBUG_INT, "(%s) Raw BPF Frame without BPF header: \n", lldp_port->if_name);
    debug_hex_dump(DEBUG_INT, (uint8_t *)frame_buffer, bpf_buf->bh_caplen);
    // Correct the rx.recvsize to reflect the lenght of the packet without the bpf_hdr
    lldp_port->rx.recvsize = bpf_buf->bh_caplen;
    
    // Free the tmp buffer 
    free(bpf_buf);

    // Now free the old buffer
    free(lldp_port->rx.frame);
    
    // Now assign the new buffer
    lldp_port->rx.frame = frame_buffer;
  } else {
    debug_printf(DEBUG_NORMAL, "Couldn't malloc!  Skipping frame to prevent leak...\n");
  }

  return(lldp_port->rx.recvsize);
}

int init_multi(const char *ifname, int add)
{
  struct ifreq ifr;
  int s,n;
  const char *addr = "01.80.C2.00.00.0E";
  unsigned int e1,e2,e3,e4,e5,e6;
  struct sockaddr_dl *dlp = NULL;
  unsigned char *bp       = NULL;

  s = socket(PF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    perror("socket");
    return -1;
  }

  if( (n = sscanf( addr, "%x.%x.%x.%x.%x.%x",
		   &e1, &e2, &e3, &e4, &e5, &e6 )) != 6 )
    {
      printf( "bad args\n" );
      return -1;
    }

  dlp = (struct sockaddr_dl *)&ifr.ifr_addr;
  dlp->sdl_len = sizeof(struct sockaddr_dl);
  dlp->sdl_family = AF_LINK;
  dlp->sdl_index = 0;
  dlp->sdl_nlen = 0;
  dlp->sdl_alen = 6;
  dlp->sdl_slen = 0;

  // NB: This is to silence compiler warnints, but what's goin on here?
  bp = (unsigned char *)LLADDR(dlp);

  bp[0] = e1;
  bp[1] = e2;
  bp[2] = e3;
  bp[3] = e4;
  bp[4] = e5;
  bp[5] = e6;
  
  if (ioctl(s, add ? SIOCADDMULTI : SIOCDELMULTI, (caddr_t) &ifr) < 0) {
    perror("ioctl[SIOC{ADD/DEL}MULTI]");
      close(s);
      return -1;
  }
  close(s);

  return 0;
}

int socketInitializeLLDP(struct lldp_port *lldp_port) {
    if(lldp_port->if_name == NULL) {
        debug_printf(DEBUG_NORMAL, "Got NULL interface in %s():%d\n", __FUNCTION__, __LINE__);
        exit(-1);
    }

    if(!lldp_port->if_index > 0) {
        debug_printf(DEBUG_NORMAL, "'%s' does not appear to be a valid interface name!\n", lldp_port->if_name);
        return XENOSOCK;
    }

    debug_printf(DEBUG_INT, "'%s' is index %d\n", lldp_port->if_name, lldp_port->if_index);

    lldp_port->socket = bpf_new();

    if(lldp_port->socket < 0) {
        debug_printf(DEBUG_NORMAL, "[Error] (%d) : %s (%s:%d)\n", errno, strerror(errno), __FUNCTION__, __LINE__);
        return XENOSOCK;
    }


    // This is necessary for Mac OS X at least... 
    if(bpf_set_immediate(lldp_port->socket, 1) > 0) {
        debug_printf(DEBUG_NORMAL, "[Error] (%d) : %s (%s:%d)\n", errno, strerror(errno), __FUNCTION__, __LINE__);
        bpf_dispose(lldp_port->socket);
        return XENOSOCK;
    }

    struct bpf_insn progcodes[] = {
        BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12), // inspect ethernet_frame_type
        BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x88CC, 0, 1), // if EAPOL frame, continue with next instruction, else jump
        BPF_STMT(BPF_RET+BPF_K, (u_int)-1),
        BPF_STMT(BPF_RET+BPF_K, 0)
    };

    struct bpf_program prog = {
        4,
        (struct bpf_insn*) &progcodes
    };

    if(ioctl(lldp_port->socket, BIOCSETF, &prog) < 0) {
        debug_printf(DEBUG_NORMAL, "[Error] (%d) : %s (%s:%d)\n", errno, strerror(errno), __FUNCTION__, __LINE__);
        bpf_dispose(lldp_port->socket);
        return XENOSOCK;
    }

    if(bpf_setif(lldp_port->socket, lldp_port->if_name) < 0) {
        debug_printf(DEBUG_NORMAL, "[Error] (%d) : %s (%s:%d)\n", errno, strerror(errno), __FUNCTION__, __LINE__);
        bpf_dispose(lldp_port->socket);
        return XENOSOCK;
    }

    // Enable multicast on the interface
    if(init_multi(lldp_port->if_name, 1) < 0) {
      // Setting multicast failed, so let's try falling back to promiscuous mode
      // as a last resort...
      debug_printf(DEBUG_NORMAL, "Unable to set multicast mode, trying promiscuous... ");
      if(bpf_set_promiscuous(lldp_port->socket) == 0) {
	debug_printf(DEBUG_NORMAL, "Success!\n");
      } else {
	debug_printf(DEBUG_NORMAL, "Failure!\n");
      }
    }

    // Set the socket to be non-blocking
    if (fcntl(lldp_port->socket, F_SETFL, O_NONBLOCK) > 0)
    {
        debug_printf(DEBUG_NORMAL, "[Error] (%d) : %s (%s:%d)\n", errno, strerror(errno), __FUNCTION__, __LINE__);
        bpf_dispose(lldp_port->socket);
        return XENOSOCK;      
    }

    // Get the size of the BPF buffer
    if (bpf_get_blen(lldp_port->socket, &lldp_port->mtu) < 0) {
        debug_printf(DEBUG_NORMAL, "[Error] (%d) : %s (%s:%d)\n", errno, strerror(errno), __FUNCTION__, __LINE__);
        bpf_dispose(lldp_port->socket);
        return XENOSOCK;
    }

    // Set the following to 1 to enable lldpd to see sent frames.
    bpf_see_sent(lldp_port->socket, 0);

    _getmac(lldp_port->source_mac, lldp_port->if_name);

    _getip(lldp_port->source_ipaddr, lldp_port->if_name);

    debug_printf(DEBUG_NORMAL, "%s MTU: %d\n", lldp_port->if_name, lldp_port->mtu);

    if(lldp_port->mtu > 0) {
        lldp_port->rx.frame = malloc(lldp_port->mtu);
        lldp_port->tx.frame = malloc(lldp_port->mtu);
    } else {
        debug_printf(DEBUG_NORMAL, "Frame buffer MTU is 0, ditching interface.\n");
        bpf_dispose(lldp_port->socket);
        return XENOSOCK;
    }

    return 0;
}

/***********************************************
 * Get the MAC address of an interface
 ***********************************************/
int _getmac(uint8_t *dest, char *ifname) {

    struct ifaddrs *ifap;

    memset(dest, 0x0, 6);

    if (getifaddrs(&ifap) == 0) {
        struct ifaddrs *p;
        for (p = ifap; p; p = p->ifa_next) {
            if (p->ifa_addr->sa_family == AF_LINK && strcmp(p->ifa_name, ifname) == 0) {
                struct sockaddr_dl* sdp = (struct sockaddr_dl*) p->ifa_addr;		
                memcpy(dest, sdp->sdl_data + sdp->sdl_nlen, 6);
                printf("%s MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                        p->ifa_name,
                        dest[0],
                        dest[1],
                        dest[2],
                        dest[3],
                        dest[4],
                        dest[5]);
                freeifaddrs(ifap);
                return TRUE;
            }
        }
        freeifaddrs(ifap);
    }

    return FALSE;
}

/***********************************************
 * Get the IP address of an interface
 ***********************************************/
int _getip(uint8_t *dest, char *ifname) {

    struct ifaddrs *ifap;

    if (getifaddrs(&ifap) == 0) {
        struct ifaddrs *p;
        for (p = ifap; p; p = p->ifa_next) {
            if (p->ifa_addr->sa_family == AF_INET && strcmp(p->ifa_name, ifname) == 0) {
                memcpy(dest, &p->ifa_addr->sa_data[2], 4);
                printf("%s IP: %d.%d.%d.%d\n",
                        p->ifa_name,
		        dest[0],
                        dest[1],
                        dest[2],
                        dest[3]);
                freeifaddrs(ifap);
                return TRUE;
            }
        }
        freeifaddrs(ifap);
    }

    return FALSE;
}

// Dunno if there's a better way to handle this... maybe an OS specific event?
void refreshInterfaceData(struct lldp_port *lldp_port) {
  _getmac(lldp_port->source_mac, lldp_port->if_name);
  
  _getip(lldp_port->source_ipaddr, lldp_port->if_name);  
}

void socketCleanupLLDP(struct lldp_port *lldp_port)
{
  //init_multi(lldp_port->if_name, 0);
  bpf_dispose(lldp_port->socket);
}
