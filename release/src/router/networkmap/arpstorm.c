#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <stdio.h>
//#include <linux/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <bcmnvram.h>
#include <stdlib.h>
#include <asm/byteorder.h>
#include "networkmap.h"
#include <rtconfig.h>

unsigned char my_hwaddr[6];
unsigned char my_ipaddr[4];
unsigned char broadcast_hwaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
int scan_count=0;
/******** Build ARP Socket Function *********/
struct sockaddr_ll src_sockll, dst_sockll;

static int
iface_get_id(int fd, const char *device)
{
        struct ifreq    ifr;
        memset(&ifr, 0, sizeof(ifr));
        strlcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
        if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
                perror("iface_get_id ERR:\n");
                return -1;
        }

        return ifr.ifr_ifindex;
}
/*
 *  Bind the socket associated with FD to the given device.
 */
static int
iface_bind(int fd, int ifindex)
{
        int                     err;
        socklen_t               errlen = sizeof(err);
                                                                                                                                             
        memset(&src_sockll, 0, sizeof(src_sockll));
        src_sockll.sll_family          = AF_PACKET;
        src_sockll.sll_ifindex         = ifindex;
        src_sockll.sll_protocol        = htons(ETH_P_ARP);
                                                                                                                                             
        if (bind(fd, (struct sockaddr *) &src_sockll, sizeof(src_sockll)) == -1) {
                perror("bind device ERR:\n");
                return -1;
        }

        /* Any pending errors, e.g., network is down? */
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
                return -2;
        }
        if (err > 0) {
                return -2;
        }

        int alen = sizeof(src_sockll);
        if (getsockname(fd, (struct sockaddr*)&src_sockll, &alen) == -1) {
                perror("getsockname");
                exit(2);
        }
                                                                                                                                             
        if (src_sockll.sll_halen == 0) {
                printf("Interface is not ARPable (no ll address)\n");
                exit(2);
        }

	dst_sockll = src_sockll;

         return 0;
}

int create_socket(char *device)
{
        /* create socket */
        int sock_fd, device_id;
        sock_fd = socket(PF_PACKET, SOCK_DGRAM, 0); //2008.06.27 Yau change to UDP Socket
                                                                                                                                             
        if(sock_fd < 0)
                perror("create socket ERR:");
                                                                                                                                             
        device_id = iface_get_id(sock_fd, device);
                                                                                                                                             
        if (device_id == -1)
               printf("iface_get_id REEOR\n");
                                                                                                                                             
        if ( iface_bind(sock_fd, device_id) < 0)
                printf("iface_bind ERROR\n");
                                                                                                                                             
        return sock_fd;
}

int  sent_arppacket(int raw_sockfd, unsigned char * dst_ipaddr)
{
        ARP_HEADER * arp;

	char raw_buffer[46];

	memset(dst_sockll.sll_addr, -1, sizeof(dst_sockll.sll_addr));  // set dmac addr FF:FF:FF:FF:FF:FF                                                                                                                                              
        if (raw_buffer == NULL)
        {
                 perror("ARP: Oops, out of memory\r");
                return 1;
        }                                                                                                                          
	bzero(raw_buffer, 46);

        // Allow 14 bytes for the ethernet header
        arp = (ARP_HEADER *)(raw_buffer);// + 14);
        arp->hardware_type =htons(DIX_ETHERNET);
        arp->protocol_type = htons(IP_PACKET);
        arp->hwaddr_len = 6;
        arp->ipaddr_len = 4;
        arp->message_type = htons(ARP_REQUEST);
                                                                                                                                              
        // My hardware address and IP addresses
        memcpy(arp->source_hwaddr, my_hwaddr, 6);
        memcpy(arp->source_ipaddr, my_ipaddr, 4);
        // Destination hwaddr and dest IP addr
        memcpy(arp->dest_hwaddr, broadcast_hwaddr, 6);
        memcpy(arp->dest_ipaddr, dst_ipaddr, 4);

        if( (sendto(raw_sockfd, raw_buffer, 46, 0, (struct sockaddr *)&dst_sockll, sizeof(dst_sockll))) < 0 )
        {
                 perror("sendto");
                 return 1;
        }
	//NMP_DEBUG("Send ARP Request success to: .%d.%d\n", atoi(dst_ipaddr[2]), atoi(dst_ipaddr[3]));
	//int x = atoi(dst_ipaddr[3]);
	//NMP_DEBUG_M("Send ARP Request success to: .%02X\n", dst_ipaddr[3]);
        return 0;
}
/******* End of Build ARP Socket Function ********/

int main()
{
	int arp_sockfd;
	struct sockaddr_in router_addr;
	char router_ipaddr[17], router_mac[17];
	unsigned char scan_ipaddr[4]; // scan ip
        struct timeval arp_timeout;

	//Get Router's IP/Mac
	strcpy(router_ipaddr, nvram_safe_get("lan_ipaddr"));
	strcpy(router_mac, get_lan_hwaddr());
#ifdef RTCONFIG_GMAC3
        if(nvram_match("gmac3_enable", "1"))
                strcpy(router_mac, nvram_safe_get("et2macaddr"));
#endif
        inet_aton(router_ipaddr, &router_addr.sin_addr);
        memcpy(my_ipaddr,  &router_addr.sin_addr, 4);

	//Prepare scan 
        memset(scan_ipaddr, 0x00, 4);
        memcpy(scan_ipaddr, &router_addr.sin_addr, 3);

	if (strlen(router_mac)!=0) ether_atoe(router_mac, my_hwaddr);

        // create UDP socket and bind to "br0" to get ARP packet//
	arp_sockfd = create_socket(INTERFACE);

        if(arp_sockfd < 0) {
                perror("create socket ERR:");
		return -1;
	}else {
		setsockopt(arp_sockfd, SOL_SOCKET, SO_RCVTIMEO, &arp_timeout, sizeof(arp_timeout));//set receive timeout
		dst_sockll = src_sockll; //2008.06.27 Yau add copy sockaddr info to dst
		memset(dst_sockll.sll_addr, -1, sizeof(dst_sockll.sll_addr)); // set dmac addr FF:FF:FF:FF:FF:FF
	}

        while(1)
        {
		scan_count++;
		scan_ipaddr[3]++;
		if( scan_count<255 && memcmp(scan_ipaddr, my_ipaddr, 4) ) {
                        sent_arppacket(arp_sockfd, scan_ipaddr);
		}         
		else if(scan_count>255) { //Scan completed
			scan_count=0;
			scan_ipaddr[3]=0;
		}
		//sent_arppacket(arp_sockfd, scan_ipaddr);
	} //End of while
	close(arp_sockfd);
	return 0;
}
