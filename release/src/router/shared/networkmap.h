/*
#include <sys/socket.h>
#include <stdio.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
*/
#include <syslog.h>
#include "../shared/version.h"

#define FALSE   0
#define TRUE    1
#define INTERFACE 	"br0"
#define MODEL_NAME 	RT_BUILD_NAME
#define ARP_BUFFER_SIZE	512

// Hardware type field in ARP message
#define DIX_ETHERNET            1
// Type number field in Ethernet frame
#define IP_PACKET               0x0800
#define ARP_PACKET              0x0806
#define RARP_PACKET             0x8035
// Message type field in ARP messages
#define ARP_REQUEST             1
#define ARP_RESPONSE            2
#define RARP_REQUEST            3
#define RARP_RESPONSE           4
#define RCV_TIMEOUT             2 //sec
#define MAXDATASIZE             512
#define LPR                     0x02
#define LPR_RESPONSE            0x00
                                                                                                                                             
//Service Port
#define HTTP_PORT               80
#define NBNS_PORT               137
#define NBSS_PORT 		139
#define LPD_PORT                515
#define MDNS_PORT               5353
#define RAW_PORT                9100

//for itune
#define uint16 unsigned short
#define PROTOCOL_NAME "_daap._tcp.local"
#define SET_UINT16( num, buff) num = htons(*(uint16*)buff); buff += 2

//for UPNP
#define LINE_SIZE               200
#define SERVICE_NUM             10
#define UPNP_BUFSIZE            1500
#define MIN_SEARCH_TIME         3
#define MAX_SEARCH_TIME         180
#define SSDP_IP                 "239.255.255.250"
#define SSDP_PORT               1900
#define MATCH_PREFIX(a, b)  	(strncmp((a),(b),sizeof(b)-1)==0)
#define IMATCH_PREFIX(a, b)  	(strncasecmp((a),(b),sizeof(b)-1)==0)

//for smb
#define TIME_OUT_TIME 		5
#define NBSS_REQ                1
#define NBSS_RSP                2
#define SMB_NEGOTIATE_REQ       3
#define SMB_NEGOTIATE_RSP       4
#define SMB_SESSON_ANDX_REQ     5
#define SMB_SESSON_ANDX_RSP     6

#ifdef DEBUG
	#define NMP_DEBUG(fmt, args...) printf(fmt, ## args)
	//#define NMP_DEBUG(fmt, args...) syslog(LOG_NOTICE, fmt, ## args)
#else
	#define NMP_DEBUG(fmt, args...)
#endif

#ifdef DEBUG_MORE
        #define NMP_DEBUG_M(fmt, args...) printf(fmt, ## args)
	//#define NMP_DEBUG_M(fmt, args...) syslog(LOG_NOTICE, fmt, ## args)
#else
        #define NMP_DEBUG_M(fmt, args...)
#endif

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;

//Device service info data structure
typedef struct {
        unsigned char   ip_addr[255][4];
        unsigned char   mac_addr[255][6];
	unsigned char   user_define[255][16];
	unsigned char   device_name[255][32];
	unsigned char	apl_dev[255][16];
        int             type[255];
        int             http[255];
        int             printer[255];
        int             itune[255];
	int		exist[255];
        int             ip_mac_num;
	int 		detail_info_num;
} CLIENT_DETAIL_INFO_TABLE, *P_CLIENT_DETAIL_INFO_TABLE;

typedef struct Raw_socket {
        int raw_sockfd;
        int raw_buflen;
        char device;
        unsigned char *raw_buffer;
}r_socket;

// walf test
typedef struct
{
	unsigned short  hardware_type; 
   	unsigned short  protocol_type;           
   	unsigned char hwaddr_len;
   	unsigned char ipaddr_len;               
   	unsigned short  message_type;
   	unsigned char source_hwaddr[6];              
   	unsigned char source_ipaddr[4];
   	unsigned char dest_hwaddr[6];    
   	unsigned char dest_ipaddr[4];
} ARP_HEADER;

typedef struct
{
  	unsigned char dest_hwaddr[6];
  	unsigned char source_hwaddr[6];
  	unsigned short  frame_type;
} ETH_HEADER;

/* NetBIOS Datagram header: 14 Bytes */
typedef struct
{
  unsigned char message_type;
  unsigned char frag_node;
  unsigned short datagram_id;
  unsigned char source_ipaddr[4];
  unsigned short src_port;
  unsigned short datagram_len;
  unsigned short packet_off;
} NETBIOS_D_HEADER;
                                                                                                                                              
/* NetBIOS Datagram data section */
typedef struct
{
        unsigned char src_name[34]; // 255 bytes maximum
        unsigned char dst_name[34]; // 255 bytes maximum
        unsigned char usr_data[512]; // maximum of 512 bytes
}NETBIOS_D_SECTION;

typedef struct
{
  unsigned short transaction_id;
  unsigned char flags[2];
  unsigned short questions;
  unsigned short answer_rrs;
  unsigned short authority_rrs;
  unsigned short additional_rrs;
  unsigned char name[34];
  unsigned short type;
  unsigned short name_class;
  unsigned char ttl[4];
  unsigned short data_len;
  unsigned char number_of_names;
  unsigned char device_name1[16];
  unsigned char name_flags1[2];
  unsigned char device_name2[16];
  unsigned char name_flags2[2];
  unsigned char device_name3[16];
  unsigned char name_flags3[2];
  unsigned char device_name4[16];
  unsigned char name_flags4[2];
  unsigned char device_name5[16];
  unsigned char name_flags5[2];
  unsigned char device_name6[16];
  unsigned char name_flags6[2];
  unsigned char mac_addr[6];
  unsigned char name_info[58];
} NBNS_RESPONSE;

struct LPDProtocol
{
    unsigned char cmd_code;  	/* command code */
    unsigned char options[32];  /* Queue name */
    unsigned char lf;
};

//for itune
typedef struct dns_header_s{
  uint16 id;
  uint16 flags;
  uint16 qdcount;
  uint16 ancount;
  uint16 nscount;
  uint16 arcount;
} dns_header_s;

//for UPNP
struct service
{
        char name[LINE_SIZE];
        char url[LINE_SIZE];
};
                                                                                                                                             
struct device_info
{
        // name:                <friendlyName>
        char friendlyname[LINE_SIZE];
        // Manufacturer:        <manufacturer>
        char manufacturer[LINE_SIZE];
        // Description:         <modelDescription>
        char description[LINE_SIZE];
        // Model Name:          <modelName>
        char modelname[LINE_SIZE];
        // Model Number:        <modelNumber>
        char modelnumber[LINE_SIZE];
        // Device Address:      <presentationURL>
        char presentation[LINE_SIZE];
        // the service information
        struct service service[SERVICE_NUM];
        int service_num;
};

//for smb
typedef struct {
       unsigned char msg_type;
       unsigned char flags;
       unsigned short length;
} NBSS_HEADER;
                                                                                                                                             
union {
        struct {
            UCHAR  ErrorClass;        // Error class
            UCHAR  Reserved;          // Reserved for future use
            USHORT Error;             // Error code
        } DosError;
        ULONG Status;                 // 32-bit error code
} Status;
                                                                                                                                             
union {
        USHORT Pad[6];                // Ensure section is 12 bytes long
        struct {
            USHORT PidHigh;           // High part of PID
            ULONG  Unused;            // Not used
            ULONG  Unused2;
    	} Extra;
} Pad;

typedef struct
{
    UCHAR Protocol[4];                // Contains 0xFF,'SMB'
    UCHAR Command;                    // Command code
    UCHAR Status[4];
    UCHAR  Flags;                     // Flags
    USHORT Flags2;                    // More flags
    USHORT Pad[6];
    USHORT Tid;                       // Tree identifier
    USHORT Pid;                       // Caller's process id
    USHORT Uid;                       // Unauthenticated user id
    USHORT Mid;                       // multiplex id
} SMB_HEADER;

typedef struct
{
    UCHAR  WordCount;                     // Count of parameter words = 13
    UCHAR  AndXCommand;                   // Secondary (X) command;  0xFF = none
    UCHAR  AndXReserved;                  // Reserved (must be 0)
    USHORT AndXOffset;                    // Offset to next command WordCount
    USHORT MaxBufferSize;                 // Client's maximum buffer size
    USHORT MaxMpxCount;                   // Actual maximum multiplexed pending requests
    USHORT VcNumber;                      // 0=first (only),nonzero=additional VC number
    ULONG  SessionKey;                    // Session key (valid iff VcNumber != 0)
    USHORT CaseInsensitivePasswordLength; // Account password size, ANSI
    USHORT CaseSensitivePasswordLength;   // Account password size, Unicode
    USHORT SecurityBlobLen;
    ULONG  Reserved;                      // must be 0
    ULONG  Capabilities;                  // Client capabilities
} SMB_SESSION_SETUPX_REQ;
                                                                                                                                             
typedef struct
{
    USHORT ByteCount;                     //Count of data bytes;    min = 0
    UCHAR  CaseInsensitivePassword[32];     //Account Password, ANSI
    UCHAR  CaseSensitivePassword[32];       //Account Password, Unicode
    UCHAR AccountName[32];                 //Account Name, Unicode
    UCHAR PrimaryDomain[32];               //Client's primary domain, Unicode
    UCHAR NativeOS[128];                    //Client's native operating system, Unicode
    UCHAR NativeLanMan[128];                //Client's native LAN Manager type, Unicode
} SMB_CLIENT_INFO;

typedef struct
{
        UCHAR *des_hostname;
        UCHAR *my_hostname;
        UCHAR *account;
        UCHAR *primarydomain;
        UCHAR *nativeOS;
        UCHAR *nativeLanMan;
        USHORT des_hostname_len;
        USHORT my_hostname_len;
        USHORT account_len;
        USHORT primarydomain_len;
        USHORT nativeOS_len;
        USHORT nativeLanMan_len;
} MY_DEVICE_INFO;

int FindAllApp( unsigned char *src_ip, P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab);
