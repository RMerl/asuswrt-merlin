/*
 dhcp_release6 --iface <interface> --client-id <client-id> --server-id
 server-id --iaid <iaid>  --ip <IP>  [--dry-run] [--help]
 MUST be run as root - will fail otherwise
 */

/* Send a DHCPRELEASE message  to IPv6 multicast address  via the specified interface
 to tell the local DHCP server to delete a particular lease.
 
 The interface argument is the interface in which a DHCP
 request _would_ be received if it was coming from the client,
 rather than being faked up here.
 
 The client-id argument is colon-separated hex string and mandatory. Normally
 it can be found in leases file both on client and server

 The server-id argument is colon-separated hex string and mandatory. Normally
 it can be found in leases file both on client and server.
 
 The iaid argument is numeric string and mandatory. Normally
 it can be found in leases file both on client and server.
 
 IP is an IPv6 address to release
 
 If --dry-run is specified, dhcp_release6 just prints hexadecimal representation of
 packet to send to stdout and exits.
 
 If --help is specified, dhcp_release6 print usage information to stdout and exits
 
 
 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#define NOT_REPLY_CODE 115
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

enum DHCP6_TYPES{
    SOLICIT = 1,
    ADVERTISE = 2,
    REQUEST = 3,
    CONFIRM = 4,
    RENEW = 5,
    REBIND = 6,
    REPLY = 7,
    RELEASE = 8,
    DECLINE = 9,
    RECONFIGURE = 10,
    INFORMATION_REQUEST = 11,
    RELAY_FORW = 12,
    RELAY_REPL = 13
    
};
enum DHCP6_OPTIONS{
    CLIENTID = 1,
    SERVERID = 2,
    IA_NA = 3,
    IA_TA = 4,
    IAADDR = 5,
    ORO = 6,
    PREFERENCE = 7,
    ELAPSED_TIME = 8,
    RELAY_MSG = 9,
    AUTH = 11,
    UNICAST = 12,
    STATUS_CODE = 13,
    RAPID_COMMIT = 14,
    USER_CLASS = 15,
    VENDOR_CLASS = 16,
    VENDOR_OPTS = 17,
    INTERFACE_ID = 18,
    RECONF_MSG = 19,
    RECONF_ACCEPT = 20,
};

enum DHCP6_STATUSES{
    SUCCESS = 0,
    UNSPEC_FAIL = 1,
    NOADDR_AVAIL=2,
    NO_BINDING  = 3,
    NOT_ON_LINK = 4,
    USE_MULTICAST =5
};
static struct option longopts[] = {
    {"ip", required_argument, 0, 'a'},
    {"server-id", required_argument, 0, 's'},
    {"client-id", required_argument, 0, 'c'},
    {"iface", required_argument, 0, 'n'},
    {"iaid", required_argument, 0, 'i'},
    {"dry-run", no_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {0,     0,           0,   0}
};

const short DHCP6_CLIENT_PORT = 546;
const short DHCP6_SERVER_PORT = 547;

const char*  DHCP6_MULTICAST_ADDRESS = "ff02::1:2";

struct dhcp6_option{
    uint16_t type;
    uint16_t len;
    char  value[1024];
};

struct dhcp6_iaaddr_option{
    uint16_t type;
    uint16_t len;
    struct in6_addr ip;
    uint32_t preferred_lifetime;
    uint32_t valid_lifetime;
    
    
};

struct dhcp6_iana_option{
    uint16_t type;
    uint16_t len;
    uint32_t iaid;
    uint32_t t1;
    uint32_t t2;
    char options[1024];
};


struct dhcp6_packet{
    size_t len;
    char buf[2048];
    
} ;

size_t pack_duid(const char* str, char* dst){
    
    char* tmp = strdup(str);
    char* tmp_to_free = tmp;
    char *ptr;
    uint8_t write_pos = 0;
    while ((ptr = strtok (tmp, ":"))) {
        dst[write_pos] = (uint8_t) strtol(ptr, NULL, 16);
        write_pos += 1;
        tmp = NULL;
        
    }
    free(tmp_to_free);
    return write_pos;
}

struct dhcp6_option create_client_id_option(const char* duid){
    struct dhcp6_option option;
    option.type = htons(CLIENTID);
    bzero(option.value, sizeof(option.value));
    option.len  = htons(pack_duid(duid, option.value));
    return option;
}

struct dhcp6_option create_server_id_option(const char* duid){
    struct dhcp6_option   option;
    option.type = htons(SERVERID);
    bzero(option.value, sizeof(option.value));
    option.len  = htons(pack_duid(duid, option.value));
    return option;
}

struct dhcp6_iaaddr_option create_iaadr_option(const char* ip){
    struct dhcp6_iaaddr_option result;
    result.type =htons(IAADDR);
    /* no suboptions needed here, so length is 24  */
    result.len = htons(24);
    result.preferred_lifetime = 0;
    result.valid_lifetime = 0;
    int s = inet_pton(AF_INET6, ip, &(result.ip));
    if (s <= 0) {
        if (s == 0)
            fprintf(stderr, "Not in presentation format");
        else
            perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    return result;
}
struct dhcp6_iana_option  create_iana_option(const char * iaid, struct dhcp6_iaaddr_option  ia_addr){
    struct dhcp6_iana_option  result;
    result.type = htons(IA_NA);
    result.iaid = htonl(atoi(iaid));
    result.t1 = 0;
    result.t2 = 0;
    result.len = htons(12 + ntohs(ia_addr.len) + 2 * sizeof(uint16_t));
    memcpy(result.options, &ia_addr, ntohs(ia_addr.len) + 2 * sizeof(uint16_t));
    return result;
}

struct dhcp6_packet create_release_packet(const char* iaid, const char* ip, const char* client_id, const char* server_id){
    struct dhcp6_packet result;
    bzero(result.buf, sizeof(result.buf));
    /* message_type */
    result.buf[0] = RELEASE;
    /* tx_id */
    bzero(result.buf+1, 3);
    
    struct dhcp6_option client_option = create_client_id_option(client_id);
    struct dhcp6_option server_option = create_server_id_option(server_id);
    struct dhcp6_iaaddr_option iaaddr_option = create_iaadr_option(ip);
    struct dhcp6_iana_option iana_option = create_iana_option(iaid, iaaddr_option);
    int offset = 4;
    memcpy(result.buf + offset, &client_option, ntohs(client_option.len) + 2*sizeof(uint16_t));
    offset += (ntohs(client_option.len)+ 2 *sizeof(uint16_t) );
    memcpy(result.buf + offset, &server_option, ntohs(server_option.len) + 2*sizeof(uint16_t) );
    offset += (ntohs(server_option.len)+ 2* sizeof(uint16_t));
    memcpy(result.buf + offset, &iana_option, ntohs(iana_option.len) + 2*sizeof(uint16_t) );
    offset += (ntohs(iana_option.len)+ 2* sizeof(uint16_t));
    result.len = offset;
    return result;
}

uint16_t parse_iana_suboption(char* buf, size_t len){
    size_t current_pos = 0;
    char option_value[1024];
    while (current_pos < len) {
        uint16_t option_type, option_len;
        memcpy(&option_type,buf + current_pos, sizeof(uint16_t));
        memcpy(&option_len,buf + current_pos + sizeof(uint16_t), sizeof(uint16_t));
        option_type = ntohs(option_type);
        option_len = ntohs(option_len);
        current_pos += 2 * sizeof(uint16_t);
        if (option_type == STATUS_CODE){
            uint16_t status;
            memcpy(&status, buf + current_pos, sizeof(uint16_t));
            status = ntohs(status);
            if (status != SUCCESS){
                memcpy(option_value, buf + current_pos + sizeof(uint16_t) , option_len - sizeof(uint16_t));
                option_value[option_len-sizeof(uint16_t)] ='\0';
                fprintf(stderr, "Error: %s\n", option_value);
            }
            return status;
        }
    }
    return -2;
}

int16_t parse_packet(char* buf, size_t len){
    uint8_t type  = buf[0];
    /*skipping tx id. you need it, uncomment following line
        uint16_t tx_id = ntohs((buf[1] <<16) + (buf[2] <<8) + buf[3]);
     */
    size_t current_pos = 4;
    if (type != REPLY ){
        return NOT_REPLY_CODE;
    }
    char option_value[1024];
    while (current_pos < len) {
        uint16_t option_type, option_len;
        memcpy(&option_type,buf + current_pos, sizeof(uint16_t));
        memcpy(&option_len,buf + current_pos + sizeof(uint16_t), sizeof(uint16_t));
        option_type = ntohs(option_type);
        option_len = ntohs(option_len);
        current_pos += 2 * sizeof(uint16_t);
        if (option_type == STATUS_CODE){
            uint16_t status;
            memcpy(&status, buf + current_pos, sizeof(uint16_t));
            status = ntohs(status);
            if (status != SUCCESS){
                memcpy(option_value, buf + current_pos +sizeof(uint16_t) , option_len -sizeof(uint16_t));
                fprintf(stderr, "Error: %d %s\n", status, option_value);
                return status;
            }
            
        }
        if (option_type == IA_NA ){
            uint16_t result = parse_iana_suboption(buf + current_pos +24, option_len -24);
            if (result){
                return result;
            }
        }
        current_pos += option_len;

    }
    return -1;
}

void usage(const char* arg, FILE* stream){
    const char* usage_string ="--ip IPv6 --iface IFACE --server-id SERVER_ID --client-id CLIENT_ID --iaid IAID [--dry-run] | --help";
    fprintf (stream, "Usage: %s %s\n", arg, usage_string);
    
}

int send_release_packet(const char* iface, struct dhcp6_packet* packet){
    
    struct sockaddr_in6 server_addr, client_addr;
    char response[1400];
    int sock = socket(PF_INET6, SOCK_DGRAM, 0);
    int i = 0;
    if (sock < 0) {
        perror("creating socket");
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, 25, iface, strlen(iface)) == -1)  {
        perror("SO_BINDTODEVICE");
        close(sock);
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    client_addr.sin6_family = AF_INET6;
    client_addr.sin6_port = htons(DHCP6_CLIENT_PORT);
    client_addr.sin6_flowinfo = 0;
    client_addr.sin6_scope_id =0;
    inet_pton(AF_INET6, "::", &client_addr.sin6_addr);
    bind(sock, (struct sockaddr*)&client_addr, sizeof(struct sockaddr_in6));
    inet_pton(AF_INET6, DHCP6_MULTICAST_ADDRESS, &server_addr.sin6_addr);
    server_addr.sin6_port = htons(DHCP6_SERVER_PORT);
    int16_t recv_size = 0;
    for (i = 0; i < 5; i++) {
        if (sendto(sock, packet->buf, packet->len, 0,
               (struct sockaddr *)&server_addr,
               sizeof(server_addr)) < 0) {
            perror("sendto failed");
            exit(4);
        }
        recv_size = recvfrom(sock, response, sizeof(response), MSG_DONTWAIT, NULL, 0);
        if (recv_size == -1){
            if (errno == EAGAIN){
                sleep(1);
                continue;
            }else {
                perror("recvfrom");
            }
        }
        int16_t result = parse_packet(response, recv_size);
        if (result == NOT_REPLY_CODE){
            sleep(1);
            continue;
        }
        return result;
    }
    fprintf(stderr, "Response timed out\n");
    return -1;
    
}


int main(int argc, char *  const argv[]) {
    const char* UNINITIALIZED = "";
    const char* iface = UNINITIALIZED;
    const char* ip = UNINITIALIZED;
    const char* client_id = UNINITIALIZED;
    const char* server_id = UNINITIALIZED;
    const char* iaid = UNINITIALIZED;
    int dry_run = 0;
    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "a:s:c:n:i:hd", longopts, &option_index);
        if (c == -1){
            break;
        }
        switch(c){
            case 0:
                if (longopts[option_index].flag !=0){
                    break;
                }
                printf ("option %s", longopts[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf ("\n");
                break;
            case 'i':
                iaid = optarg;
                break;
            case 'n':
                iface = optarg;
                break;
            case 'a':
                ip = optarg;
                break;
            case 'c':
                client_id = optarg;
                break;
            case 'd':
                dry_run = 1;
                break;
            case 's':
                server_id = optarg;
                break;
            case 'h':
                usage(argv[0], stdout);
                return 0;
            case '?':
                usage(argv[0], stderr);
                return -1;
             default:
                abort();
                
        }
        
    }
    if (iaid == UNINITIALIZED){
        fprintf(stderr, "Missing required iaid parameter\n");
        usage(argv[0], stderr);
        return -1;
    }
    if (server_id == UNINITIALIZED){
        fprintf(stderr, "Missing required server-id parameter\n");
        usage(argv[0], stderr);
        return -1;
    }
    if (client_id == UNINITIALIZED){
        fprintf(stderr, "Missing required client-id parameter\n");
        usage(argv[0], stderr);
        return -1;
    }
    if (ip == UNINITIALIZED){
        fprintf(stderr, "Missing required ip parameter\n");
        usage(argv[0], stderr);
        return -1;
    }
    if (iface == UNINITIALIZED){
        fprintf(stderr, "Missing required iface parameter\n");
        usage(argv[0], stderr);
        return -1;
    }



    struct dhcp6_packet packet = create_release_packet(iaid, ip, client_id, server_id);
    if (dry_run){
        uint16_t i;
        for(i=0;i<packet.len;i++){
            printf("%hhx", packet.buf[i]);
        }
        printf("\n");
        return 0;
    }
    return send_release_packet(iface, &packet);
    
}
