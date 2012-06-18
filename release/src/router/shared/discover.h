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
//#include <net/ethernet.h>
#include <features.h>

#include <sys/uio.h>
#include <fcntl.h>
#include <sys/cdefs.h>
#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#include <linux/if_ether.h>
//#include <netinet/if_ether.h>
#include <ctype.h>

#define eprintf(fmt, args...) do{\
	FILE *ffp = fopen("/tmp/detect_wrong.log", "a+");\
	if(ffp) {\
		fprintf(ffp, fmt, ## args);\
		fclose(ffp);\
	}\
}while(0)

/************************************/
/* Defaults _you_ may want to tweak */
/************************************/
                                                                                                                                               
/* the period of time the client is allowed to use that address */
#define LEASE_TIME              (60*60*24*10) /* 10 days of seconds */
                                                                                                                                               
/* where to find the DHCP server configuration file */
#define DHCPD_CONF_FILE         "/etc/udhcpd.conf"
                                                                                                                                               
/*****************************************************************/
/* Do not modify below here unless you know what you are doing!! */
/*****************************************************************/
                                                                                                                                               
/* DHCP protocol -- see RFC 2131 */
#define SERVER_PORT             67
#define CLIENT_PORT             68
                                                                                                                                               
#define DHCP_MAGIC              0x63825363
                                                                                                                                               
/* DHCP option codes (partial list) */
#define DHCP_PADDING            0x00
#define DHCP_SUBNET             0x01
#define DHCP_TIME_OFFSET        0x02
#define DHCP_ROUTER             0x03
#define DHCP_TIME_SERVER        0x04
#define DHCP_NAME_SERVER        0x05
#define DHCP_DNS_SERVER         0x06
#define DHCP_LOG_SERVER         0x07
#define DHCP_COOKIE_SERVER      0x08
#define DHCP_LPR_SERVER         0x09
#define DHCP_HOST_NAME          0x0c
#define DHCP_BOOT_SIZE          0x0d
#define DHCP_DOMAIN_NAME        0x0f
#define DHCP_SWAP_SERVER        0x10
#define DHCP_ROOT_PATH          0x11
#define DHCP_IP_TTL             0x17
#define DHCP_MTU                0x1a
#define DHCP_BROADCAST          0x1c
#define DHCP_NTP_SERVER         0x2a
#define DHCP_WINS_SERVER        0x2c
#define DHCP_REQUESTED_IP       0x32
#define DHCP_LEASE_TIME         0x33
#define DHCP_OPTION_OVER        0x34
#define DHCP_MESSAGE_TYPE       0x35
#define DHCP_SERVER_ID          0x36
#define DHCP_PARAM_REQ          0x37
#define DHCP_MESSAGE            0x38
#define DHCP_MAX_SIZE           0x39
#define DHCP_T1                 0x3a
#define DHCP_T2                 0x3b
#define DHCP_VENDOR             0x3c
#define DHCP_CLIENT_ID          0x3d
                                                                                                                                               
#define DHCP_END                0xFF
                                                                                                                                               
                                                                                                                                               
#define BOOTREQUEST             1
#define BOOTREPLY               2
                                                                                                                                               
#define ETH_10MB                1
#define ETH_10MB_LEN            6
#define DHCPDISCOVER            1
#define DHCPOFFER               2
#define DHCPREQUEST             3
#define DHCPDECLINE             4
#define DHCPACK                 5
#define DHCPNAK                 6
#define DHCPRELEASE             7
#define DHCPINFORM              8
                                                                                                                                               
#define BROADCAST_FLAG          0x8000
                                                                                                                                               
#define OPTION_FIELD            0
#define FILE_FIELD              1
#define SNAME_FIELD             2
                                                                                                                                               
/* miscellaneous defines */
#define MAC_BCAST_ADDR          (unsigned char *) "\xff\xff\xff\xff\xff\xff"
#define OPT_CODE 0
#define OPT_LEN 1
#define OPT_DATA 2

#define INIT_SELECTING  0
#define REQUESTING      1
#define BOUND           2
#define RENEWING        3
#define REBINDING       4
#define INIT_REBOOT     5
#define RENEW_REQUESTED 6
#define RELEASED        7
#define DEFAULT_SCRIPT  "/usr/share/udhcpc/default.script"
#define VERSION "0.9.8-pre"

struct client_config_t {
        char foreground;                /* Do not fork */
        char quit_after_lease;          /* Quit after obtaining lease */
        char abort_if_no_lease;         /* Abort if no lease */
        char background_if_no_lease;    /* Fork to background if no lease */
        char *interface;                /* The name of the interface to use */
        char *pidfile;                  /* Optionally store the process ID */
        char *script;                   /* User script to run at dhcp events */
        unsigned char *clientid;        /* Optional client id to use */
        unsigned char *hostname;        /* Optional hostname to use */
        int ifindex;                    /* Index number of the interface to use */
        unsigned char arp[6];           /* Our arp address */
};

static struct client_config_t client_config;

struct dhcpMessage {
        u_int8_t op;
        u_int8_t htype;
        u_int8_t hlen;
        u_int8_t hops;
        u_int32_t xid;
        u_int16_t secs;
        u_int16_t flags;
        u_int32_t ciaddr;
        u_int32_t yiaddr;
        u_int32_t siaddr;
        u_int32_t giaddr;
        u_int8_t chaddr[16];
        u_int8_t sname[64];
        u_int8_t file[128];
        u_int32_t cookie;
        u_int8_t options[308]; /* 312 - cookie */
};
                                                                                                                                               
struct udp_dhcp_packet {
        struct iphdr ip;
        struct udphdr udp;
        struct dhcpMessage data;
};

#define TYPE_MASK       0x0F
                                                                                                                                               
enum {
        OPTION_IP=1,
        OPTION_IP_PAIR,
        OPTION_STRING,
        OPTION_BOOLEAN,
        OPTION_U8,
        OPTION_U16,
        OPTION_S16,
        OPTION_U32,
        OPTION_S32
};
                                                                                                                                               
#define OPTION_REQ      0x10 /* have the client request this option */
#define OPTION_LIST     0x20 /* There can be a list of 1 or more of these */
                                                                                                                                               
struct dhcp_option {
        char name[10];
        char flags;
        unsigned char code;
};
                                                                                                                                               
static int state;
//static unsigned long requested_ip; /* = 0 */
//static unsigned long server_addr;
//static unsigned long timeout;
//static int packet_num; /* = 0 */
static int cfd;
//static int signal_pipe[2];
                                                                                                                                               
#define LISTEN_NONE 0
#define LISTEN_KERNEL 1
#define LISTEN_RAW 2
static int listen_mode;

/* supported options are easily added here */
static struct dhcp_option options[] = {
        /* name[10]     flags                                   code */
        {"subnet",      OPTION_IP | OPTION_REQ,                 0x01},
        {"timezone",    OPTION_S32,                             0x02},
        {"router",      OPTION_IP | OPTION_LIST | OPTION_REQ,   0x03},
        {"timesvr",     OPTION_IP | OPTION_LIST,                0x04},
        {"namesvr",     OPTION_IP | OPTION_LIST,                0x05},
        {"dns",         OPTION_IP | OPTION_LIST | OPTION_REQ,   0x06},
        {"logsvr",      OPTION_IP | OPTION_LIST,                0x07},
        {"cookiesvr",   OPTION_IP | OPTION_LIST,                0x08},
        {"lprsvr",      OPTION_IP | OPTION_LIST,                0x09},
        {"hostname",    OPTION_STRING | OPTION_REQ,             0x0c},
        {"bootsize",    OPTION_U16,                             0x0d},
        {"domain",      OPTION_STRING | OPTION_REQ,             0x0f},
        {"swapsvr",     OPTION_IP,                              0x10},
        {"rootpath",    OPTION_STRING,                          0x11},
        {"ipttl",       OPTION_U8,                              0x17},
        {"mtu",         OPTION_U16,                             0x1a},
        {"broadcast",   OPTION_IP | OPTION_REQ,                 0x1c},
        {"ntpsrv",      OPTION_IP | OPTION_LIST,                0x2a},
        {"wins",        OPTION_IP | OPTION_LIST | OPTION_REQ,   0x2c},
        {"requestip",   OPTION_IP,                              0x32},
        {"lease",       OPTION_U32,                             0x33},
        {"dhcptype",    OPTION_U8,                              0x35},
        {"serverid",    OPTION_IP,                              0x36},
        {"message",     OPTION_STRING,                          0x38},
        {"tftp",        OPTION_STRING,                          0x42},
        {"bootfile",    OPTION_STRING,                          0x43},
        {"",            0x00,                                   0x00}
};
                                                                                                                                          
/* Lengths of the different option types */
static int option_lengths[] = {
        [OPTION_IP] =           4,
        [OPTION_IP_PAIR] =      8,
        [OPTION_BOOLEAN] =      1,
        [OPTION_STRING] =       1,
        [OPTION_U8] =           1,
        [OPTION_U16] =          2,
        [OPTION_S16] =          2,
        [OPTION_U32] =          4,
        [OPTION_S32] =          4
};

u_int16_t checksum(void *addr, int count);

int listen_socket(unsigned int ip, int port, char *inf);

int raw_socket(int ifindex);
int raw_packet(struct dhcpMessage *payload, u_int32_t source_ip, int source_port,
                   u_int32_t dest_ip, int dest_port, unsigned char *dest_arp, int ifindex);

unsigned char *get_option(struct dhcpMessage *packet, int code);
int get_packet(struct dhcpMessage *packet, int fd);
int get_raw_packet(struct dhcpMessage *payload, int fd);
int end_option(unsigned char *optionptr);
                                                                                                                                               
int add_option_string(unsigned char *optionptr, unsigned char *string);

int add_simple_option(unsigned char *optionptr, unsigned char code, u_int32_t data);

void init_header(struct dhcpMessage *packet, char type);

//static void init_packet(struct dhcpMessage *packet, char type);

//static void add_requests(struct dhcpMessage *packet);

int send_dhcp_discover(unsigned long xid);

int read_interface(char *interface, int *ifindex, u_int32_t *addr, unsigned char *arp);

unsigned long random_xid(void);

//static void change_mode(int new_mode);


/**********************************   ppp   *************************************************/

char DEFAULT_IF[16];
#define BPF_BUFFER_IS_EMPTY 1
#define BPF_BUFFER_HAS_DATA 0
                                                                                                                                               
typedef unsigned short UINT16_t;
                                                                                                                                               
typedef unsigned int UINT32_t;
                                                                                                                                               
/* Ethernet frame types according to RFC 2516 */
#define ETH_PPPOE_DISCOVERY 0x8863
#define ETH_PPPOE_SESSION   0x8864
                                                                                                                                               
/* But some brain-dead peers disobey the RFC, so frame types are variables */
                                                                                                                                               
static UINT16_t Eth_PPPOE_Discovery = ETH_PPPOE_DISCOVERY;
static UINT16_t Eth_PPPOE_Session   = ETH_PPPOE_SESSION;
                                                                                                                                               
/* PPPoE codes */
#define CODE_PADI           0x09
#define CODE_PADO           0x07
#define CODE_PADR           0x19
#define CODE_PADS           0x65
#define CODE_PADT           0xA7
                                                                                                                                               
/* Extensions from draft-carrel-info-pppoe-ext-00 */
/* I do NOT like PADM or PADN, but they are here for completeness */
#define CODE_PADM           0xD3
#define CODE_PADN           0xD4
                                                                                                                                               
#define CODE_SESS           0x00
                                                                                                                                               
/* PPPoE Tags */
#define TAG_END_OF_LIST        0x0000
#define TAG_SERVICE_NAME       0x0101
#define TAG_AC_NAME            0x0102
#define TAG_HOST_UNIQ          0x0103
#define TAG_AC_COOKIE          0x0104
#define TAG_VENDOR_SPECIFIC    0x0105
#define TAG_RELAY_SESSION_ID   0x0110
#define TAG_SERVICE_NAME_ERROR 0x0201
#define TAG_AC_SYSTEM_ERROR    0x0202
#define TAG_GENERIC_ERROR      0x0203
                                                                                                                                               
/* Extensions from draft-carrel-info-pppoe-ext-00 */
/* I do NOT like these tags one little bit */
#define TAG_HURL               0x111
#define TAG_MOTM               0x112
#define TAG_IP_ROUTE_ADD       0x121
/* Discovery phase states */
#define STATE_SENT_PADI     0
#define STATE_RECEIVED_PADO 1
#define STATE_SENT_PADR     2
#define STATE_SESSION       3
#define STATE_TERMINATED    4
                                                                                                                                               
/* How many PADI/PADS attempts? */
#define MAX_PADI_ATTEMPTS 3
                                                                                                                                               
/* Initial timeout for PADO/PADS */
#define PADI_TIMEOUT 5
/* States for scanning PPP frames */
#define STATE_WAITFOR_FRAME_ADDR 0
#define STATE_DROP_PROTO         1
#define STATE_BUILDING_PACKET    2
                                                                                                                                               
/* Special PPP frame characters */
#define FRAME_ESC    0x7D
#define FRAME_FLAG   0x7E
#define FRAME_ADDR   0xFF
#define FRAME_CTRL   0x03
#define FRAME_ENC    0x20
                                                                                                                                               
#define IPV4ALEN     4
#define SMALLBUF   256
                                                                                                                                               
/* A PPPoE Packet, including Ethernet headers */
typedef struct PPPoEPacketStruct {
    struct ethhdr ethHdr;       /* Ethernet header */
//#ifdef PACK_BITFIELDS_REVERSED
//    unsigned int type:4;        /* PPPoE Type (must be 1) */
//    unsigned int ver:4;         /* PPPoE Version (must be 1) */
//#else
    unsigned int ver:4;         /* PPPoE Version (must be 1) */
    unsigned int type:4;        /* PPPoE Type (must be 1) */
//#endif
    unsigned int code:8;        /* PPPoE code */
    unsigned int session:16;    /* PPPoE session */
    unsigned int length:16;     /* Payload length */
    unsigned char payload[ETH_DATA_LEN]; /* A bit of room to spare */
} PPPoEPacket;
                                                                                                                                               
/* Header size of a PPPoE packet */
#define PPPOE_OVERHEAD 6  /* type, code, session, length */
#define HDR_SIZE (sizeof(struct ethhdr) + PPPOE_OVERHEAD)
#define MAX_PPPOE_PAYLOAD (ETH_DATA_LEN - PPPOE_OVERHEAD)
#define MAX_PPPOE_MTU (MAX_PPPOE_PAYLOAD - 2)
                                                                                                                                               
/* PPPoE Tag */
                                                                                                                                               
typedef struct PPPoETagStruct {
    unsigned int type:16;       /* tag type */
    unsigned int length:16;     /* Length of payload */
    unsigned char payload[ETH_DATA_LEN]; /* A LOT of room to spare */
} PPPoETag;
                                                                                                                                               
/* Header size of a PPPoE tag */
#define TAG_HDR_SIZE 4
                                                                                                                                               
/* Chunk to read from stdin */
#define READ_CHUNK 4096
                                                                                                                                               
/* Function passed to parsePacket */
typedef void ParseFunc(UINT16_t type,
                       UINT16_t len,
                       unsigned char *data,
                       void *extra);
                                                                                                                                               
#define PPPINITFCS16    0xffff  /* Initial FCS value */
                                                                                                                                               
/* Keep track of the state of a connection -- collect everything in
   one spot */
                                                                                                                                               
typedef struct PPPoEConnectionStruct {
    int discoveryState;         /* Where we are in discovery */
    int discoverySocket;        /* Raw socket for discovery frames */
    int sessionSocket;          /* Raw socket for session frames */
    unsigned char myEth[ETH_ALEN]; /* My MAC address */
    unsigned char peerEth[ETH_ALEN]; /* Peer's MAC address */
    UINT16_t session;           /* Session ID */
    char *ifName;               /* Interface name */
    char *serviceName;          /* Desired service name, if any */
    char *acName;               /* Desired AC name, if any */
    int synchronous;            /* Use synchronous PPP */
    int useHostUniq;            /* Use Host-Uniq tag */
    int printACNames;           /* Just print AC names */
    int skipDiscovery;          /* Skip discovery */
    int noDiscoverySocket;      /* Don't even open discovery socket */
    int killSession;            /* Kill session and exit */
    FILE *debugFile;            /* Debug file for dumping packets */
    int numPADOs;               /* Number of PADO packets received */
    PPPoETag cookie;            /* We have to send this if we get it */
    PPPoETag relayId;           /* Ditto */
} PPPoEConnection;
/* Structure used to determine acceptable PADO or PADS packet */
struct PacketCriteria {
    PPPoEConnection *conn;
    int acNameOK;
    int serviceNameOK;
    int seenACName;
    int seenServiceName;
};
                                                                                                                                               
#define CHECK_ROOM(cursor, start, len) \
do {\
    if (((cursor)-(start))+(len) > MAX_PPPOE_PAYLOAD) { \
        fprintf(stderr, "Would create too-long packet\n"); \
        return; \
    } \
} while(0)
                                                                                                                                               
/* True if Ethernet address is broadcast or multicast */
#define NOT_UNICAST(e) ((e[0] & 0x01) != 0)
#define BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) == 0xFF)
#define NOT_BROADCAST(e) ((e[0] & e[1] & e[2] & e[3] & e[4] & e[5]) != 0xFF)
                                                                                                                                               
static char const RCSID[] =
"$Id: dp.h,v 1.1.1.1 2008/07/21 09:20:37 james26_jang Exp $";
                                                                                                                                               
                                                                                                                                               
char *strDup(char const *str);

#define SET_STRING(var, val) do { if (var) free(var); var = strDup(val); } while(0);
                                                                                                                                               
int
openInterface(char const *ifname, UINT16_t type, unsigned char *hwaddr);

void
dumpHex(FILE *fp, unsigned char const *buf, int len);

UINT16_t
etherType(PPPoEPacket *packet);

//void
//dumpPacket(FILE *fp, PPPoEPacket *packet, char const *dir);

int
parsePacket(PPPoEPacket *packet, ParseFunc *func, void *extra);

void
parseForHostUniq(UINT16_t type, UINT16_t len, unsigned char *data,
                 void *extra);

int
packetIsForMe(PPPoEConnection *conn, PPPoEPacket *packet);

void
parsePADOTags(UINT16_t type, UINT16_t len, unsigned char *data,
              void *extra);

int
sendPacket(PPPoEConnection *conn, int sock, PPPoEPacket *pkt, int size);

int
receivePacket(int sock, PPPoEPacket *pkt, int *size);

void
sendPADI(PPPoEConnection *conn);

void
waitForPADO(PPPoEConnection *conn, int timeout);

void
discovery(PPPoEConnection *conn);

int discover_all();
