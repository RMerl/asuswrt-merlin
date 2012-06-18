/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <features.h>
#include "dp.h"
//#include <nvram/bcmnvram.h>

#define DHCP_DETECT
#define DHCP_SOCKET

#define eprintf(fmt, args...) do{\
	FILE *ffp = fopen("/tmp/detect_wrong.log", "a+");\
	if(ffp) {\
		fprintf(ffp, fmt, ## args);\
		fclose(ffp);\
	}\
}while(0)

struct ifreq ifr;
/***********************************************************************/
// ppp


char *
strDup(char const *str)
{
    char *copy = malloc(strlen(str)+1);
    if (!copy) {
        //rp_fatal("strdup failed");
        fprintf(stderr, "strdup failed\n");
        return (char*)0;
    }
    strcpy(copy, str);
    return copy;
}


int
openInterface(char const *ifname, UINT16_t type, unsigned char *hwaddr)
{
    int optval=1;
    int fd;
    //struct ifreq ifr;
    int domain, stype;
                                                                                                                                               
    struct sockaddr sa;
                                                                                                                                               
    memset(&sa, 0, sizeof(sa));
                                                                                                                                               
    domain = PF_INET;
    stype = SOCK_PACKET;
    if ((fd = socket(domain, stype, htons(type))) < 0) {
        /* Give a more helpful message for the common error case */
        if (errno == EPERM) {
            fprintf(stderr, "Cannot create raw socket -- pppoe must be run as root.\n");
                return -1;
        }
        perror("socket");
        return -1;
    }
	// test
	fprintf(stderr, "openInterface: socket [%d]\n", fd);

    if (type == Eth_PPPOE_Discovery)
    {
     if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
         perror("setsockopt");
         close(fd);
         return -1;
     }
    }
                                                                                                                                               
    /* Fill in hardware address */
    if (hwaddr) {
        memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
        if (NOT_UNICAST(hwaddr)) {
            char buffer[256];
            sprintf(buffer, "Interface %.16s has broadcast/multicast MAC address??", ifname);
            //rp_fatal(buffer);
            fprintf(stderr, buffer);
            return -1;
        }
    }
                                                                                                                                               
    /* Sanity check on MTU */
    strcpy(sa.sa_data, ifname);
                                                                                                                                               
    /* We're only interested in packets on specified interface */
    if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
	perror("bind ");
	close(fd);
        return -1;
    }
                                                                                                                                               
    return fd;
}

void
dumpHex(FILE *fp, unsigned char const *buf, int len)
{
    int i;
    int base;
                                                                                                                                               
    if (!fp) return;
                                                                                                                                               
    /* do NOT dump PAP packets */
    if (len >= 2 && buf[0] == 0xC0 && buf[1] == 0x23) {
        fprintf(fp, "(PAP Authentication Frame -- Contents not dumped)\n");
        return;
    }
                                                                                                                                               
    for (base=0; base<len; base += 16) {
        for (i=base; i<base+16; i++) {
            if (i < len) {
                fprintf(fp, "%02x ", (unsigned) buf[i]);
            } else {
                fprintf(fp, "   ");
            }
        }
        fprintf(fp, "  ");
        for (i=base; i<base+16; i++) {
            if (i < len) {
                if (isprint(buf[i])) {
                    fprintf(fp, "%c", buf[i]);
                } else {
                    fprintf(fp, ".");
                }
            } else {
                break;
            }
        }
        fprintf(fp, "\n");
    }
}
                                                                                                                                               
UINT16_t
etherType(PPPoEPacket *packet)
{
    UINT16_t type = (UINT16_t) ntohs(packet->ethHdr.h_proto);
    if (type != Eth_PPPOE_Discovery && type != Eth_PPPOE_Session) {
        //syslog(LOG_ERR, "Invalid ether type 0x%x", type);
        fprintf(stderr, "Invalid ether type 0x%x\n", type);
    }
    return type;
}

int
parsePacket(PPPoEPacket *packet, ParseFunc *func, void *extra)
{
    UINT16_t len = ntohs(packet->length);
    unsigned char *curTag;
    UINT16_t tagType, tagLen;
                                                                                                                                               
        fprintf(stderr, "parse packet\n");
    if (packet->ver != 1) {
        //syslog(LOG_ERR, "Invalid PPPoE version (%d)", (int) packet->ver);
        return -1;
    }
    if (packet->type != 1) {
        //syslog(LOG_ERR, "Invalid PPPoE type (%d)", (int) packet->type);
        return -1;
    }
                                                                                                                                               
    /* Do some sanity checks on packet */
    if (len > ETH_DATA_LEN - 6) { /* 6-byte overhead for PPPoE header */
        //syslog(LOG_ERR, "Invalid PPPoE packet length (%u)", len);
        return -1;
    }
                                                                                                                                               
    /* Step through the tags */
    curTag = packet->payload;
    while(curTag - packet->payload < len) {
        /* Alignment is not guaranteed, so do this by hand... */
        tagType = (((UINT16_t) curTag[0]) << 8) +
            (UINT16_t) curTag[1];
        tagLen = (((UINT16_t) curTag[2]) << 8) +
            (UINT16_t) curTag[3];
        if (tagType == TAG_END_OF_LIST) {
            return 0;
        }
        if ((curTag - packet->payload) + tagLen + TAG_HDR_SIZE > len) {
            //syslog(LOG_ERR, "Invalid PPPoE tag length (%u)", tagLen);
            return -1;
        }
        func(tagType, tagLen, curTag+TAG_HDR_SIZE, extra);
        //curTag = curTag + TAG_HDR_SIZE + tagLen;
    }
    return 0;
}
                                                                                                                                               
void
parseForHostUniq(UINT16_t type, UINT16_t len, unsigned char *data,
                 void *extra)
{
    int *val = (int *) extra;
    if (type == TAG_HOST_UNIQ && len == sizeof(pid_t)) {
        pid_t tmp;
        memcpy(&tmp, data, len);
        if (tmp == getpid()) {
            *val = 1;
        }
    }
}
                                                                                                                                               
int
packetIsForMe(PPPoEConnection *conn, PPPoEPacket *packet)
{
    int forMe = 0;
                                                                                                                                               
    /* If packet is not directed to our MAC address, forget it */
    if (memcmp(packet->ethHdr.h_dest, conn->myEth, ETH_ALEN)) return 0;
                                                                                                                                               
                                                                                                                                               
    /* If we're not using the Host-Unique tag, then accept the packet */
    if (!conn->useHostUniq) return 1;
                                                                                                                                               
    parsePacket(packet, parseForHostUniq, &forMe);
    return forMe;
}

int
sendPacket(PPPoEConnection *conn, int sock, PPPoEPacket *pkt, int size)
{
    struct sockaddr sa;
                                                                                                                                               
    if (!conn) {
        fprintf(stderr, "relay and server not supported on Linux 2.0 kernels\n");
        return -1;
    }
    strcpy(sa.sa_data, conn->ifName);
    if (sendto(sock, pkt, size, 0, &sa, sizeof(sa)) < 0) {
      return -1;
    }
    return 0;
}
                                                                                                                                               
int
receivePacket(int sock, PPPoEPacket *pkt, int *size)
{
    if ((*size = recv(sock, pkt, sizeof(PPPoEPacket), 0)) < 0) {
        return -1;
    }
    return 0;
}
                                                                                                                                               
void
sendPADI(PPPoEConnection *conn)
{
    PPPoEPacket packet;
    unsigned char *cursor = packet.payload;
    PPPoETag *svc = (PPPoETag *) (&packet.payload);
    UINT16_t namelen = 0;
    UINT16_t plen;
    if (conn->serviceName) {
        namelen = (UINT16_t) strlen(conn->serviceName);
    }
    plen = TAG_HDR_SIZE + namelen;
    CHECK_ROOM(cursor, packet.payload, plen);
                                                                                                                                               
    /* Set destination to Ethernet broadcast address */
    memset(packet.ethHdr.h_dest, 0xFF, ETH_ALEN);
    memcpy(packet.ethHdr.h_source, conn->myEth, ETH_ALEN);
                                                                                                                                               
    packet.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    packet.ver = 1;
    packet.type = 1;
    packet.code = CODE_PADI;
    packet.session = 0;
                                                                                                                                               
    svc->type = TAG_SERVICE_NAME;
    svc->length = htons(namelen);
    CHECK_ROOM(cursor, packet.payload, namelen+TAG_HDR_SIZE);
                                                                                                                                               
    if (conn->serviceName) {
        memcpy(svc->payload, conn->serviceName, strlen(conn->serviceName));
    }
    cursor += namelen + TAG_HDR_SIZE;
                                                                                                                                               
    /* If we're using Host-Uniq, copy it over */
    if (conn->useHostUniq) {
        PPPoETag hostUniq;
        pid_t pid = getpid();
        hostUniq.type = htons(TAG_HOST_UNIQ);
        hostUniq.length = htons(sizeof(pid));
        memcpy(hostUniq.payload, &pid, sizeof(pid));
        CHECK_ROOM(cursor, packet.payload, sizeof(pid) + TAG_HDR_SIZE);
        memcpy(cursor, &hostUniq, sizeof(pid) + TAG_HDR_SIZE);
        cursor += sizeof(pid) + TAG_HDR_SIZE;
        plen += sizeof(pid) + TAG_HDR_SIZE;
    }
                                                                                                                                               
    packet.length = htons(plen);
    sendPacket(conn, conn->discoverySocket, &packet, (int) (plen + HDR_SIZE));
printf("Send PADI to fd= %d\n",  conn->discoverySocket);

}


#define PPP_PAYLOAD_PREFIX "\xc0\x21\x01\x00\x00\x11\x01\x04\x05\xc4\x05\x06"
#define PPP_PAYLOAD_TAIL "\x0d\x03\x06"



void
sendPADI_pppoa(PPPoEConnection *conn)
{
    PPPoAoverPPPoEPacket packet;
    UINT16_t plen = 0;
    UINT16_t plen_payload = 0;
    int TryCnt;
    char tc_mac[32];
    unsigned int mac;

	// mac address is adsl mac
	for (TryCnt = 0; TryCnt < 10; TryCnt++)
	{
		FILE* fpLog;
		fpLog = fopen("/tmp/adsl/tc_mac.txt","rb");
		if (fpLog != NULL)
		{
			fgets(tc_mac,sizeof(tc_mac),fpLog);
			fclose(fpLog);
			break;
		}
		usleep(1000*1000);
	}		
	
//	printf(tc_mac);
//	printf("\n");

	tc_mac[2] = 0;
	tc_mac[5] = 0;
	tc_mac[8] = 0;
	tc_mac[11] = 0;
	tc_mac[14] = 0;
	tc_mac[17] = 0;

	sscanf(&tc_mac[0],"%x", &mac);
	packet.ethHdr.h_dest[0] = mac;
        sscanf(&tc_mac[3],"%x", &mac);
	packet.ethHdr.h_dest[1] = mac;
        sscanf(&tc_mac[6],"%x", &mac);
	packet.ethHdr.h_dest[2] = mac;
        sscanf(&tc_mac[9],"%x", &mac);
	packet.ethHdr.h_dest[3] = mac;
        sscanf(&tc_mac[12],"%x", &mac);
	packet.ethHdr.h_dest[4] = mac;
        sscanf(&tc_mac[15],"%x", &mac);
	packet.ethHdr.h_dest[5] = mac;
	
//	printf("mac ok\n");

	memcpy(packet.ethHdr.h_source, conn->myEth, ETH_ALEN);	

	plen += 12;
                                                                                                                                               
    packet.ethHdr.h_proto = htons(Eth_PPPOE_Session);
    packet.ver = 1;
    packet.type = 1;
    packet.code = CODE_SESS;
    packet.session = htons(0x009a);
    packet.length = htons(0x0013);

	plen += 8;    

    // ppp
    memcpy(&packet.payload[plen_payload], PPP_PAYLOAD_PREFIX, sizeof(PPP_PAYLOAD_PREFIX)-1);
    plen_payload += (sizeof(PPP_PAYLOAD_PREFIX)-1);
        

	// magic number
    packet.payload[plen_payload++] = 0; // magic number
    packet.payload[plen_payload++] = 1; // magic number
    packet.payload[plen_payload++] = 2; // magic number
    packet.payload[plen_payload++] = 3; // magic number

    // ppp
    memcpy(&packet.payload[plen_payload], PPP_PAYLOAD_TAIL, sizeof(PPP_PAYLOAD_TAIL)-1);
    plen_payload += (sizeof(PPP_PAYLOAD_TAIL)-1);
    
    plen += plen_payload;

    sendPacket(conn, conn->discoverySocket, &packet, (int) (plen));
printf("Send PADI_pppoa to fd= %d\n",  conn->discoverySocket);

}


/***********************************************************************/

u_int16_t checksum(void *addr, int count)
{
        /* Compute Internet Checksum for "count" bytes
         *         beginning at location "addr".
         */
        register int32_t sum = 0;
        u_int16_t *source = (u_int16_t *) addr;
                                                                                                                                               
        while (count > 1)  {
                /*  This is the inner loop */
                sum += *source++;
                count -= 2;
        }
                                                                                                                                               
        /*  Add left-over byte, if any */
        if (count > 0) {
                /* Make sure that the left-over byte is added correctly both
                 * with little and big endian hosts */
                u_int16_t tmp = 0;
                *(unsigned char *) (&tmp) = * (unsigned char *) source;
                sum += tmp;
        }
        /*  Fold 32-bit sum to 16 bits */
        while (sum >> 16)
                sum = (sum & 0xffff) + (sum >> 16);
                                                                                                                                               
        return ~sum;
}

int listen_socket(unsigned int ip, int port, char *inf)
{
        struct ifreq interface;
        int fd;
        struct sockaddr_in addr;
	//struct sockaddr sa; //Yau add
        int n = 1;
                                                                                                                  
        //DEBUG(LOG_INFO, "Opening listen socket on 0x%08x:%d %s\n", ip, port, inf);
        if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                //DEBUG(LOG_ERR, "socket call failed: %s", strerror(errno));
                return -1;
        }
                                                                                                                                               
        memset(&addr, 0, sizeof(addr));
	//memset(&sa, 0, sizeof(sa)); //Yau add
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = ip;
                                                                                                                                               
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof(n)) == -1) {
                close(fd);
                return -1;
        }
        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char *) &n, sizeof(n)) == -1) {
                close(fd);
                return -1;
        }
                                                                                                                                               
        strncpy(interface.ifr_ifrn.ifrn_name, inf, IFNAMSIZ);
        if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,(char *)&interface, sizeof(interface)) < 0) {
                close(fd);
                return -1;
        }

printf("bind to inf: %s\n", inf);
	//strcpy(sa.sa_data, inf);//Yau add
	//if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) { //Yau add
        if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1) {
		printf("bind error!\n");
                close(fd);
                return -1;
        }
        return fd;
}

int raw_socket(int ifindex)
{
        int fd;
        struct sockaddr_ll sock;
                                                                                                                                               
        if ((fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
                return -1;
        }
                                                                                                                                               
        sock.sll_family = AF_PACKET;
        sock.sll_protocol = htons(ETH_P_IP);
        sock.sll_ifindex = ifindex;
        if (bind(fd, (struct sockaddr *) &sock, sizeof(sock)) < 0) {
                close(fd);
                return -1;
        }
                                                                                                                                               
        return fd;
}
                                                                                                                                               
/* Constuct a ip/udp header for a packet, and specify the source and dest hardware address */
int raw_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
                   u_int32_t dest_ip, int dest_port, unsigned char *dest_arp, int ifindex)
{
        int fd;
        int result;
        struct sockaddr_ll dest;
        struct udp_dhcp_packet packet;

        if ((fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
                //DEBUG(LOG_ERR, "socket call failed: %s", strerror(errno));
                fprintf(stderr, "socket call failed: %s\n", strerror(errno));
                return -1;
        }
                                                                                                                                               
        memset(&dest, 0, sizeof(dest));
        memset(&packet, 0, sizeof(packet));
                                                                                                                                               
        dest.sll_family = AF_PACKET;
        dest.sll_protocol = htons(ETH_P_IP);
        dest.sll_ifindex = ifindex;
        dest.sll_halen = 6;
        memcpy(dest.sll_addr, dest_arp, 6);
        if (bind(fd, (struct sockaddr *)&dest, sizeof(struct sockaddr_ll)) < 0) {
                //DEBUG(LOG_ERR, "bind call failed: %s", strerror(errno));
                fprintf(stderr, "bind call failed: %s\n", strerror(errno));
                close(fd);
                return -1;
        }
                                                                                                                                               
        packet.ip.protocol = IPPROTO_UDP;
        packet.ip.saddr = source_ip;
        packet.ip.daddr = dest_ip;
        packet.udp.source = htons(source_port);
        packet.udp.dest = htons(dest_port);
        packet.udp.len = htons(sizeof(packet.udp) + sizeof(struct dhcpMessage)); /* cheat on the psuedo-header */
        packet.ip.tot_len = packet.udp.len;
        memcpy(&(packet.data), payload, sizeof(struct dhcpMessage));
        packet.udp.check = checksum(&packet, sizeof(struct udp_dhcp_packet));
                                                                                                                                               
        packet.ip.tot_len = htons(sizeof(struct udp_dhcp_packet));
        packet.ip.ihl = sizeof(packet.ip) >> 2;
        packet.ip.version = IPVERSION;
        packet.ip.ttl = IPDEFTTL;
        packet.ip.check = checksum(&(packet.ip), sizeof(packet.ip));
                                                                                                                                               
printf("sendto...\n");
        result = sendto(fd, &packet, sizeof(struct udp_dhcp_packet), 0, (struct sockaddr *) &dest, sizeof(dest));
        if (result <= 0) {
                //DEBUG(LOG_ERR, "write on socket failed: %s", strerror(errno));
                fprintf(stderr, "write on socket failed: %s", strerror(errno));
        }
        close(fd);
        return result;
}

extern void PutMsg(char* msg);


int get_packet(struct dhcpMessage *packet, int fd)
{
        int bytes;
        int i;
        const char broken_vendors[][8] = {
                "MSFT 98",
                ""
        };
        char unsigned *vendor;

	struct RecvdhcpMessage Recvpacket;

        memset(&Recvpacket, 0, sizeof(struct RecvdhcpMessage));

        bytes = read(fd, &Recvpacket, sizeof(struct RecvdhcpMessage));
        if (bytes < 0) {
                //DEBUG(LOG_INFO, "couldn't read on listening socket, ignoring");
                return -1;
        }

						{
							char DumpMsgBuf[100];
							char HexBuf[16];
							unsigned char* pByte;
							int i;
							sprintf(DumpMsgBuf,"dhcp_len:%d\n",bytes);							
							PutMsg(DumpMsgBuf);							
							pByte = (unsigned char*)&Recvpacket;
							for (i=0; i<bytes; i++)
							{
								if ((i % 16) == 0)
								{
									if (i != 0) PutMsg(DumpMsgBuf);								
									strcpy(DumpMsgBuf, "");
								}								
								sprintf(HexBuf, "%02x",*pByte++);
								strcat(DumpMsgBuf, HexBuf);
							}
							if (strlen(DumpMsgBuf) > 0) PutMsg(DumpMsgBuf);
						}        

	char *ppp = & Recvpacket;
	memcpy(packet, ppp+42, sizeof(struct dhcpMessage));
/*
unsigned char *ptr = packet;
int x;
for(x =0; x<278 ; x++) {
	printf("%02x ", *ptr);
	if(x%16==15)
		printf("\n");
	ptr++;
}
printf("\n");

ptr = &packet->cookie;
for(x=0; x<16; x++){
	printf("%02x ", *ptr); 
	ptr++;
}
printf("\n");
*/

// FIX
// may wrong justify
// review code
/*
        if (ntohl(packet->cookie) != DHCP_MAGIC) {
                //LOG(LOG_ERR, "received bogus message, ignoring");
                return -2;
        }
*/

// ununsed code        
        //DEBUG(LOG_INFO, "Received a packet");
        /*
        if (packet->op == BOOTREQUEST && (vendor = get_option(packet, DHCP_VENDOR))) {
                for (i = 0; broken_vendors[i][0]; i++) {
                        if (vendor[OPT_LEN - 2] == (unsigned char) strlen(broken_vendors[i]) &&
                            !strncmp(vendor, broken_vendors[i], vendor[OPT_LEN - 2])) {
                                //DEBUG(LOG_INFO, "broken client (%s), forcing broadcast",
                                //        broken_vendors[i]);
                                packet->flags |= htons(BROADCAST_FLAG);
                        }
                }
        }
        */
        return bytes;
}

/* return -1 on errors that are fatal for the socket, -2 for those that aren't */
int get_raw_packet(struct dhcpMessage *payload, int fd)
{
        int bytes;
        struct udp_dhcp_packet packet;
        u_int32_t source, dest;
        u_int16_t check;
                                                                                                                                               
        memset(&packet, 0, sizeof(struct udp_dhcp_packet));
        bytes = read(fd, &packet, sizeof(struct udp_dhcp_packet));
        if (bytes < 0) {
                //DEBUG(LOG_INFO, "couldn't read on raw listening socket -- ignoring");
                fprintf(stderr, "couldn't read on raw listening socket -- ignoring");
                usleep(500000); /* possible down interface, looping condition */
eprintf("--- get_raw_packet: couldn't read on raw listening socket! ---\n");
                return -1;
        }
                                                                                                                                               
        if (bytes < (int) (sizeof(struct iphdr) + sizeof(struct udphdr))) {
                //DEBUG(LOG_INFO, "message too short, ignoring");
eprintf("--- get_raw_packet: message too short! ---\n");
                return -2;
        }
                                                                                                                                               
        if (bytes < ntohs(packet.ip.tot_len)) {
                //DEBUG(LOG_INFO, "Truncated packet");
eprintf("--- get_raw_packet: Truncated packet! ---\n");
                //return -2;
                return -100;
        }
                                                                                                                                               
        /* ignore any extra garbage bytes */
        bytes = ntohs(packet.ip.tot_len);
                                                                                                                                               
        /* Make sure its the right packet for us, and that it passes sanity checks */
        if (packet.ip.protocol != IPPROTO_UDP || packet.ip.version != IPVERSION ||
            packet.ip.ihl != sizeof(packet.ip) >> 2 || packet.udp.dest != htons(CLIENT_PORT) ||
            bytes > (int) sizeof(struct udp_dhcp_packet) ||
            ntohs(packet.udp.len) != (short) (bytes - sizeof(packet.ip))) {
                //DEBUG(LOG_INFO, "unrelated/bogus packet");
eprintf("--- get_raw_packet: unrelated/bogus packet! ---\n");
                return -2;
        }
                                                                                                                                               
        /* check IP checksum */
        check = packet.ip.check;
        packet.ip.check = 0;
        if (check != checksum(&(packet.ip), sizeof(packet.ip))) {
                //DEBUG(LOG_INFO, "bad IP header checksum, ignoring");
eprintf("--- get_raw_packet: bad IP header checksum! ---\n");
                return -1;
        }
                                                                                                                                               
        /* verify the UDP checksum by replacing the header with a psuedo header */
        source = packet.ip.saddr;
        dest = packet.ip.daddr;
        check = packet.udp.check;
        packet.udp.check = 0;
        memset(&packet.ip, 0, sizeof(packet.ip));
                                                                                                                                               
        packet.ip.protocol = IPPROTO_UDP;
        packet.ip.saddr = source;
        packet.ip.daddr = dest;
        packet.ip.tot_len = packet.udp.len; /* cheat on the psuedo-header */
        if (check && check != checksum(&packet, bytes)) {
                //DEBUG(LOG_ERR, "packet with bad UDP checksum received, ignoring");
eprintf("--- get_raw_packet: packet with bad UDP checksum received! ---\n");
                return -2;
        }
                                                                                                                                               
        memcpy(payload, &(packet.data), bytes - (sizeof(packet.ip) + sizeof(packet.udp)));
                                                                                                                                               
        if (ntohl(payload->cookie) != DHCP_MAGIC) {
                //LOG(LOG_ERR, "received bogus message (bad magic) -- ignoring");
eprintf("--- get_raw_packet: received bogus message! ---\n");
                return -2;
        }
        //DEBUG(LOG_INFO, "oooooh!!! got some!");
eprintf("--- get_raw_packet: Got some correct message! ---\n");
        return bytes - (sizeof(packet.ip) + sizeof(packet.udp));
                                                                                                                                               
}

/* get an option with bounds checking (warning, not aligned). */
unsigned char *get_option(struct dhcpMessage *packet, int code)
{
        int i, length;
        unsigned char *optionptr;
        int over = 0, done = 0, curr = OPTION_FIELD;
                                                                                                                                               
        optionptr = packet->options;
        i = 0;
        length = 308;
        while (!done) {
                if (i >= length) {
                        //LOG(LOG_WARNING, "bogus packet, option fields too long.");
                        return NULL;
                }
                if (optionptr[i + OPT_CODE] == code) {
                        if (i + 1 + optionptr[i + OPT_LEN] >= length) {
                                //LOG(LOG_WARNING, "bogus packet, option fields too long.");
                                return NULL;
                        }
                        return optionptr + i + 2;
                }
                switch (optionptr[i + OPT_CODE]) {
                case DHCP_PADDING:
                        i++;
                        break;
                case DHCP_OPTION_OVER:
                        if (i + 1 + optionptr[i + OPT_LEN] >= length) {
                                //LOG(LOG_WARNING, "bogus packet, option fields too long.");
                                return NULL;
                        }
                        over = optionptr[i + 3];
                        i += optionptr[OPT_LEN] + 2;
                        break;
                case DHCP_END:
                        if (curr == OPTION_FIELD && over & FILE_FIELD) {
                                optionptr = packet->file;
                                i = 0;
                                length = 128;
                                curr = FILE_FIELD;
                        } else if (curr == FILE_FIELD && over & SNAME_FIELD) {
                                optionptr = packet->sname;
                                i = 0;
                                length = 64;
                                curr = SNAME_FIELD;
                        } else done = 1;
                        break;
                default:
                        i += optionptr[OPT_LEN + i] + 2;
                }
        }
        return NULL;
}

/* return the position of the 'end' option (no bounds checking) */
int end_option(unsigned char *optionptr)
{
        int i = 0;
                                                                                                                                               
        while (optionptr[i] != DHCP_END) {
                if (optionptr[i] == DHCP_PADDING) i++;
                else i += optionptr[i + OPT_LEN] + 2;
        }
        return i;
}
                                                                                                                                               
                                                                                                                                               
/* add an option string to the options (an option string contains an option code,
 * length, then data) */
int add_option_string(unsigned char *optionptr, unsigned char *string)
{
        int end = end_option(optionptr);
                                                                                                                                               
        /* end position + string length + option code/length + end option */
        if (end + string[OPT_LEN] + 2 + 1 >= 308) {
                fprintf(stderr, "Option 0x%02x did not fit into the packet!\n", string[OPT_CODE]);
                return 0;
        }
        //fprintf(stderr, "adding option 0x%02x\n", string[OPT_CODE]);
        memcpy(optionptr + end, string, string[OPT_LEN] + 2);
        optionptr[end + string[OPT_LEN] + 2] = DHCP_END;
        return string[OPT_LEN] + 2;
}

/* add a one to four byte option to a packet */
int add_simple_option(unsigned char *optionptr, unsigned char code, u_int32_t data)
{
        char length = 0;
        int i;
        unsigned char option[2 + 4];
        unsigned char *u8;
        u_int16_t *u16;
        u_int32_t *u32;
        u_int32_t aligned;
        u8 = (unsigned char *) &aligned;
        u16 = (u_int16_t *) &aligned;
        u32 = &aligned;
                                                                                                                                               
        for (i = 0; options[i].code; i++)
                if (options[i].code == code) {
                        length = option_lengths[options[i].flags & TYPE_MASK];
                }
                                                                                                                                               
        if (!length) {
                //DEBUG(LOG_ERR, "Could not add option 0x%02x", code);
                return 0;
        }
                                                                                                                                               
        option[OPT_CODE] = code;
        option[OPT_LEN] = length;
                                                                                                                                               
        switch (length) {
                case 1: *u8 =  data; break;
                case 2: *u16 = data; break;
                case 4: *u32 = data; break;
        }
        memcpy(option + 2, &aligned, length);
        return add_option_string(optionptr, option);
}

void init_header(struct dhcpMessage *packet, char type)
{
        memset(packet, 0, sizeof(struct dhcpMessage));
        switch (type) {
        case DHCPDISCOVER:
        case DHCPREQUEST:
        case DHCPRELEASE:
        case DHCPINFORM:
                packet->op = BOOTREQUEST;
                break;
        case DHCPOFFER:
        case DHCPACK:
        case DHCPNAK:
                packet->op = BOOTREPLY;
        }
        packet->htype = ETH_10MB;
        packet->hlen = ETH_10MB_LEN;
        packet->cookie = htonl(DHCP_MAGIC);
        packet->options[0] = DHCP_END;
        add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);
}

/* initialize a packet with the proper defaults */
static void init_packet(struct dhcpMessage *packet, char type, struct client_config_t *client_config)
{
        struct vendor  {
                char vendor, length;
                char str[sizeof("udhcp "VERSION)];
        } vendor_id = { DHCP_VENDOR,  sizeof("udhcp "VERSION) - 1, "udhcp "VERSION};
                                                                                                                                               
        init_header(packet, type);
        memcpy(packet->chaddr, client_config->arp, 6);
        add_option_string(packet->options, client_config->clientid);
        add_option_string(packet->options, (unsigned char *) &vendor_id);
}

/* Add a paramater request list for stubborn DHCP servers. Pull the data
 * from the struct in options.c. Don't do bounds checking here because it
 * goes towards the head of the packet. */
static void add_requests(struct dhcpMessage *packet)
{
        int end = end_option(packet->options);
        int i, len = 0;
                                                                                                                                               
        packet->options[end + OPT_CODE] = DHCP_PARAM_REQ;
        for (i = 0; options[i].code; i++)
                if (options[i].flags & OPTION_REQ)
                        packet->options[end + OPT_DATA + len++] = options[i].code;
        packet->options[end + OPT_LEN] = len;
        packet->options[end + OPT_DATA + len] = DHCP_END;
                                                                                                                                               
}

/* Broadcast a DHCP discover packet to the network, with an optionally requested IP */
int send_dhcp_discover(struct client_config_t *client_config)
{
printf("send_dhcp_discover\n");
        struct dhcpMessage packet;
        init_packet(&packet, DHCPDISCOVER, client_config);
        packet.xid = client_config->xid;
        add_requests(&packet);
        return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
                                SERVER_PORT, MAC_BCAST_ADDR, client_config->ifindex);
}
/*
int send_dhcp_discover(unsigned long xid)
{
        struct dhcpMessage packet;
        init_packet(&packet, DHCPDISCOVER, client_config);
        packet.xid = xid;
        add_requests(&packet);
        return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
                                SERVER_PORT, MAC_BCAST_ADDR, ifindex);
}
*/
int read_interface(char *interface, int *ifindex, u_int32_t *addr, unsigned char *arp)
{
        int fd;
        //struct ifreq ifr;
        struct sockaddr_in *our_ip;
                                                                                                                                               
        memset(&ifr, 0, sizeof(struct ifreq));
        if((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
                ifr.ifr_addr.sa_family = AF_INET;
                strcpy(ifr.ifr_name, interface);

		// test
		//fprintf(stderr, "read interface, socket is %d\n", fd);

                if (addr) {
                        if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
                                our_ip = (struct sockaddr_in *) &ifr.ifr_addr;
                                *addr = our_ip->sin_addr.s_addr;
                        } else {
				close(fd);
                                return -1;
                        }
                }
                                                                                                                                               
                if (ioctl(fd, SIOCGIFINDEX, &ifr) == 0) {
                        *ifindex = ifr.ifr_ifindex;
                } else {
                        return -1;
                }
                if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
                        memcpy(arp, ifr.ifr_hwaddr.sa_data, 6);
                } else {
                        fprintf(stderr, "get hardware address failed!: %s", strerror(errno));
			close(fd);
                        return -1;
                }
        } else {
                return -1;
        }
	// test
	//fprintf(stderr, "end read interface [%d]\n", fd);
        close(fd);
        return 0;
}

/* Create a random xid */
unsigned long random_xid(void)
{
	// test
	//fprintf(stderr, "xid\n");

        static int initialized;
        if (!initialized) {
                int fd;
                unsigned long seed;
                                                                                                                                               
                //fd = open("/dev/urandom", 0);
                //if (fd < 0 || read(fd, &seed, sizeof(seed)) < 0) {
                        //LOG(LOG_WARNING, "Could not load seed from /dev/urandom: %s",
                        //        strerror(errno));
                seed = time(0);
                //}
                //if (fd >= 0){
		//	 close(fd);
		//}
                srand(seed);
                initialized++;
        }
        return rand();
}
