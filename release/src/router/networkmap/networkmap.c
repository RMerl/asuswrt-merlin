#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include "../shared/shutils.h"    // for eval()
#include "../shared/rtstate.h"
#include <bcmnvram.h>
#include <stdlib.h>
#include <asm/byteorder.h>
#include "networkmap.h"
//#include "endianness.h"
//2011.02 Yau add shard memory
#include <sys/ipc.h>
#include <sys/shm.h>
#include <rtconfig.h>
//#include "asusdiscovery.h"
#ifdef RTCONFIG_BWDPI
#include <bwdpi.h>
#endif
#ifdef RTCONFIG_NOTIFICATION_CENTER
#include <libnt.h>
int TRIGGER_FLAG;
#endif


#define vstrsep(buf, sep, args...) _vstrsep(buf, sep, args, NULL)

unsigned char my_hwaddr[6];
unsigned char my_ipaddr[4];
unsigned char broadcast_hwaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned char refresh_ip_list[255][4];
int networkmap_fullscan, lock, mdns_lock, nvram_lock;
int refresh_exist_table = 0, scan_count=0;
//Rawny: save client_list in memory
FILE *fp_ncl; //nmp_client_list FILE
#ifdef RTCONFIG_BWDPI
SINGLE_CLIENT_SIZE = 432;
#else
SINGLE_CLIENT_SIZE = 70;
#endif

#ifdef NMP_DB
char *nmp_client_list;
#endif
int delete_sig;
int show_info;

FILE *fp_upnp, *fp_smb;

typedef struct mDNSClientList_struct mDNSClientList;
struct mDNSClientList_struct
{
        unsigned char IPaddr[255][4];
        char Name[255][32];
        char Model[255][16];
};
mDNSClientList *shmClientList;

struct apple_model_handler {
	char *phototype;
	char *model;
	char *type;
};

struct apple_name_handler {
        char *name;
        char *type;
};

struct upnp_type_handler {
	char *server;
	char *type;
};

struct apple_model_handler apple_model_handlers[] = {
	{ "k68ap", 	"iPhone",	"10" },
        { "n82ap", 	"iPhone 3G",	"10" },
        { "n88ap", 	"iPhone 3GS",	"10" },
        { "n90ap", 	"iPhone 4",	"10" },
        { "n90bap", 	"iPhone 4",	"10" },
        { "n92ap", 	"iPhone 4",	"10" },
        { "n41ap", 	"iPhone 5",	"10" },
        { "n42ap",      "iPhone 5",	"10" },
        { "n48ap",      "iPhone 5c",	"10" },
        { "n49ap",      "iPhone 5c",	"10" },
        { "n51ap",      "iPhone 5s",	"10" },
        { "n53ap",      "iPhone 5s",	"10" },
	{ "n61ap",      "iPhone 6",     "10" },
	{ "n56ap",      "iPhone 6 Plus","10" },
	{ "n71ap",      "iPhone 6s",    "10" },
	{ "n71map",     "iPhone 6s",    "10" },
	{ "n66ap",      "iPhone 6s Plus","10" },
	{ "n66map",     "iPhone 6s Plus","10" },
	{ "n69ap",      "iPhone SE",    "10" },
        { "n45ap",      "iPod touch",	"21" },
        { "n72ap",      "iPod touch 2G","21" },
        { "n18ap",      "iPod touch 3G","21" },
        { "n81ap",      "iPod touch 4G","21" },
        { "n78ap",      "iPod touch 5G","21" },
        { "n78aap",     "iPod touch 5G","21" },
	{ "n102ap",     "iPod touch 6G","21" },
        { "K48ap",      "iPad",		"21" },
        { "K93ap",      "iPad 2",	"21" },
        { "k94ap",      "iPad 2",	"21" },
        { "k95ap",      "iPad 2",	"21" },
        { "k93aap",     "iPad 2",	"21" },
        { "j1ap",       "iPad 3",	"21" },
        { "j2ap",       "iPad 3",	"21" },
        { "j2aap",      "iPad 3",	"21" },
        { "p101ap",     "iPad 4",	"21" },
        { "p102ap",     "iPad 4",	"21" },
        { "p103ap",     "iPad 4",	"21" },
        { "j71ap",      "iPad Air",	"21" },
        { "j72ap",      "iPad Air",	"21" },
        { "j73ap",      "iPad Air",	"21" },
	{ "j81ap",      "iPad Air 2",   "21" },
	{ "j82ap",      "iPad Air 2",   "21" },
	{ "j98ap",      "iPad Pro",     "21" },
	{ "j99ap",      "iPad Pro",     "21" },
	{ "j127ap",     "iPad Pro",     "21" },
	{ "j128ap",	"iPad Pro",	"21" },
        { "p105ap",     "iPad mini 1G",	"21" },
        { "p106ap",     "iPad mini 1G",	"21" },
        { "p107ap",     "iPad mini 1G",	"21" },
        { "j85ap",      "iPad mini 2",	"21" },
        { "j86ap",      "iPad mini 2",	"21" },
        { "j87ap",      "iPad mini 2",	"21" },
	{ "j85map",     "iPad mini 3",  "21" },
	{ "j86map",     "iPad mini 3",  "21" },
	{ "j87map",     "iPad mini 3",  "21" },
	{ "j96ap",      "iPad mini 4",  "21" },
	{ "j97ap",      "iPad mini 4",  "21" },
        { "k66ap", 	"Apple TV 2G",	"11" },
        { "j33ap",      "Apple TV 3G",	"11" },
        { "j33iap",     "Apple TV 3G",	"11" },
	{ "j42dap",     "Apple TV 4G",  "11" },
	{ "rt288x",	"AiCam",	"5"  },
	{ NULL,		NULL,		NULL }
};

struct apple_name_handler apple_name_handlers[] = {
	{ "iPhone",	"10" },
        { "MacBook",    "6" },
	{ "MacMini",   	"14" },
	{ "NAS-M25",	"4"  },
	{ "QNAP-TS210",	"4"  },
        { NULL,         NULL }
};

struct upnp_type_handler upnp_type_handlers[] = {
	{"Windows", 	"1" },
	{NULL, 		NULL}
};

extern toLowerCase(char *str);

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
        if (getsockname(fd, (struct sockaddr*)&src_sockll, (socklen_t *)&alen) == -1) {
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
        /* create UDP socket */
        int sock_fd, device_id;
        sock_fd = socket(PF_PACKET, SOCK_DGRAM, 0);

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
	//NMP_DEBUG_M("Send ARP Request success to: .%d.%d\n", (int *)dst_ipaddr[2],(int *)dst_ipaddr[3]);
        return 0;
}
/******* End of Build ARP Socket Function ********/

/*********** Signal function **************/
static int refresh_sig(void)
{
        NMP_DEBUG("Refresh network map!\n");
        networkmap_fullscan = 1;
        refresh_exist_table = 0;
	scan_count = 0;	
	nvram_set("networkmap_status", "1");
	nvram_set("networkmap_fullscan", "1");

#if 0
	//reset exixt ip table
        memset(&client_detail_info_tab, 0x00, sizeof(client_detail_info_tabLE));
        p_client_detail_info_tab->num = 0;
	//remove file;
	ret = eval("rm", "/var/networkmap.dat");
#endif
	return 0;
}

static int safe_leave(int signo){
//	fclose(fp_upnp);
//	fclose(fp_smb);
	file_unlock(lock);
	file_unlock(mdns_lock);
	file_unlock(nvram_lock);
	#ifdef NMP_DB
	free(nmp_client_list);
	#endif
	printf("Leave......\n");
	return 0;
}

#if defined(RTCONFIG_QCA) && defined(RTCONFIG_WIRELESSREPEATER)
char *getStaMAC()
{
	char buf[512];
	FILE *fp;
	int len,unit;
	char *pt1,*pt2;
	unit=nvram_get_int("wlc_band");

	sprintf(buf, "ifconfig sta%d", unit);

	fp = popen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			pt1 = strstr(buf, "HWaddr ");
			if (pt1)
			{
				pt2 = pt1 + strlen("HWaddr ");
				chomp(pt2);
				return pt2;
			}
		}
	}
	return NULL;
}
#endif

#ifdef NMP_DB
int commit_no = 0;
int client_updated = 0;

void
convert_mac_to_string(unsigned char *mac, char *mac_str)
{
	sprintf(mac_str, "%02x%02x%02x%02x%02x%02x",
		*mac,*(mac+1),*(mac+2),*(mac+3),*(mac+4),*(mac+5));
}

int
check_nmp_db(CLIENT_DETAIL_INFO_TABLE *p_client_tab, int client_no)
{
        char new_mac[13];
        char *search_list, *nv, *nvp, *b;
	char *db_mac, *db_user_def, *db_device_name, *db_type, *db_http, *db_printer, *db_itune, *db_apple_model;
#ifdef RTCONFIG_BWDPI
	char *db_bwdpi_host, *db_bwdpi_vendor, *db_bwdpi_type, *db_bwdpi_device;
#endif
	int ret = 0;

NMP_DEBUG("check_nmp_db:\n");
	search_list = strdup(nmp_client_list);
        convert_mac_to_string(p_client_tab->mac_addr[client_no], new_mac);

//NMP_DEBUG("search_list= %s\n", search_list);
        if(strstr(search_list, new_mac)==NULL)
		return ret;

        nvp = nv = search_list;

	while (nv && (b = strsep(&nvp, "<")) != NULL) {
#ifdef RTCONFIG_BWDPI
                if (vstrsep(b, ">", &db_mac, &db_user_def, &db_device_name, &db_apple_model, &db_type, &db_http, &db_printer, &db_itune, &db_bwdpi_host, &db_bwdpi_vendor, &db_bwdpi_type, &db_bwdpi_device) != 12) continue;
#else
                if (vstrsep(b, ">", &db_mac, &db_user_def, &db_device_name, &db_apple_model, &db_type, &db_http, &db_printer, &db_itune) != 8) continue; 
#endif 

		//NMP_DEBUG_M("DB:-%s,%s,%s,%s,%s,%s,%s,%s-\n", db_mac, db_user_def, db_device_name, db_apple_model, db_type, db_http, db_printer, db_itune);
#ifdef RTCONFIG_BWDPI
		//NMP_DEBUG_M("BWDPI:-%s,%s-\n", db_bwdpi_hostname, db_bwdpi_devicetype);
#endif

        	if (!strcmp(db_mac, new_mac)) {
			NMP_DEBUG("*** %s at DB!!! Update to memory\n",new_mac);
			strlcpy((char *)p_client_tab->user_define[client_no], db_user_def, sizeof (p_client_tab->user_define[client_no]));
		        strlcpy((char *)p_client_tab->device_name[client_no], db_device_name, sizeof (p_client_tab->device_name[client_no]));
			strlcpy((char *)p_client_tab->apple_model[client_no], db_apple_model, sizeof (p_client_tab->apple_model[client_no]));
		        p_client_tab->type[client_no] = atoi(db_type);
			p_client_tab->http[client_no] = atoi(db_http);
			p_client_tab->printer[client_no] = atoi(db_printer);
			p_client_tab->itune[client_no] = atoi(db_itune);
#ifdef RTCONFIG_BWDPI
                  	strlcpy(p_client_tab->bwdpi_host[client_no], db_bwdpi_host, sizeof (p_client_tab->bwdpi_host[client_no]));
			strlcpy(p_client_tab->bwdpi_vendor[client_no], db_bwdpi_vendor, sizeof (p_client_tab->bwdpi_vendor[client_no]));
			strlcpy(p_client_tab->bwdpi_type[client_no], db_bwdpi_type, sizeof (p_client_tab->bwdpi_type[client_no]));
                        strlcpy(p_client_tab->bwdpi_device[client_no], db_bwdpi_device, sizeof (p_client_tab->bwdpi_device[client_no]));
#endif
			ret = 1;
			break;
		}
	}

	free(search_list);

	return ret;
}

void
write_to_DB(CLIENT_DETAIL_INFO_TABLE *p_client_tab)
{
	char new_mac[13], *dst_list, *dst_list_tmp;
	char *nv, *nvp, *b, *search_list;
	char *db_mac, *db_user_def, *db_device_name, *db_type, *db_http, *db_printer, *db_itune, *db_apple_model;
#ifdef RTCONFIG_BWDPI
        char *db_bwdpi_host, *db_bwdpi_vendor, *db_bwdpi_type, *db_bwdpi_device;
#endif
/*
int i = p_client_tab->detail_info_num;
NMP_DEBUG("* %d- %02X:%02X:%02X:%02X:%02X:%02X\n", i,
          p_client_tab->mac_addr[i][0],p_client_tab->mac_addr[i][1],
          p_client_tab->mac_addr[i][2],p_client_tab->mac_addr[i][3],
          p_client_tab->mac_addr[i][4],p_client_tab->mac_addr[i][5]);
*/


	convert_mac_to_string(p_client_tab->mac_addr[p_client_tab->detail_info_num], new_mac);

NMP_DEBUG("*** write_to_memory: %s ***\n",new_mac);
	search_list = strdup(nmp_client_list);

	b = strstr(search_list, new_mac);
	if(b!=NULL) { //find the client in the DB
		dst_list = malloc(sizeof(char)*(strlen(nmp_client_list)+SINGLE_CLIENT_SIZE));
		dst_list_tmp = malloc(sizeof(char)*(strlen(nmp_client_list)+SINGLE_CLIENT_SIZE));

NMP_DEBUG_M("client data in DB: %s\n", new_mac);

	        nvp = nv = b;
		*(b-1) = '\0';
		strcpy(dst_list, search_list);
		//b++;
		while (nv && (b = strsep(&nvp, "<")) != NULL) {
			if (b == NULL) continue;
#ifdef RTCONFIG_BWDPI
                	if (vstrsep(b, ">", &db_mac, &db_user_def, &db_device_name, &db_apple_model, &db_type, &db_http, &db_printer, &db_itune, &db_bwdpi_host, &db_bwdpi_vendor, &db_bwdpi_type, &db_bwdpi_device) != 12) continue;
#else
			if (vstrsep(b, ">", &db_mac, &db_user_def, &db_device_name, &db_apple_model, &db_type, &db_http, &db_printer, &db_itune) != 8) continue;
#endif

NMP_DEBUG_M("-%s,%s,%s,%s,%d,%d,%d,%d-\n", db_mac, db_user_def, db_device_name, db_apple_model, atoi(db_type), atoi(db_http), atoi(db_printer), atoi(db_itune));
#ifdef RTCONFIG_BWDPI
                NMP_DEBUG_M("BWDPI: %s,%s,%s,%s\n", db_bwdpi_host, db_bwdpi_vendor, db_bwdpi_type, db_bwdpi_device);
#endif
			if (!strcmp((char *)p_client_tab->device_name[p_client_tab->detail_info_num], db_device_name) &&
			    !strcmp((char *)p_client_tab->apple_model[p_client_tab->detail_info_num], db_apple_model) &&
			    p_client_tab->type[p_client_tab->detail_info_num] == atoi(db_type) &&
			    p_client_tab->http[p_client_tab->detail_info_num] == atoi(db_http) &&
			    p_client_tab->printer[p_client_tab->detail_info_num] == atoi(db_printer) &&
			    p_client_tab->itune[p_client_tab->detail_info_num] == atoi(db_itune) 
#ifdef RTCONFIG_BWDPI
                            && !strcmp(p_client_tab->bwdpi_host[p_client_tab->detail_info_num], db_bwdpi_host)
			    && !strcmp(p_client_tab->bwdpi_vendor[p_client_tab->detail_info_num], db_bwdpi_vendor)
			    && !strcmp(p_client_tab->bwdpi_type[p_client_tab->detail_info_num], db_bwdpi_type)
                            && !strcmp(p_client_tab->bwdpi_device[p_client_tab->detail_info_num], db_bwdpi_device)
#endif
			)
			{
NMP_DEBUG("DATA the same!\n");
				free(dst_list);
				free(dst_list_tmp);
				return;
			}
			sprintf(dst_list_tmp, "%s<%s>%s", dst_list, db_mac, db_user_def);
			strcpy(dst_list, dst_list_tmp);

		        if (strcmp((char *)p_client_tab->device_name[p_client_tab->detail_info_num], "")) {
				client_updated = 1;
      	        	        NMP_DEBUG("Update device name: %s.\n", p_client_tab->device_name[p_client_tab->detail_info_num]);
				sprintf(dst_list_tmp, "%s>%s", dst_list, p_client_tab->device_name[p_client_tab->detail_info_num]);
		        }
			else
				sprintf(dst_list_tmp, "%s>%s", dst_list, db_device_name);
			strcpy(dst_list, dst_list_tmp);

                        if (strcmp((char *)p_client_tab->apple_model[p_client_tab->detail_info_num], "")) {
				client_updated = 1;
                                NMP_DEBUG("Update Apple device: %s.\n", p_client_tab->apple_model[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%s", dst_list, p_client_tab->apple_model[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_apple_model);
			strcpy(dst_list, dst_list_tmp);

                        if (p_client_tab->type[p_client_tab->detail_info_num] != 0) {
				client_updated = 1;
                                NMP_DEBUG("Update type: %d\n", p_client_tab->type[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%d", dst_list, p_client_tab->type[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_type);
			strcpy(dst_list, dst_list_tmp);

                        if (!strcmp(db_http, "0") ) {
                                client_updated = 1;
                                NMP_DEBUG("Update http: %d\n", p_client_tab->http[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%d", dst_list, p_client_tab->http[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_http);
			strcpy(dst_list, dst_list_tmp);

                        if (!strcmp(db_printer, "0") ) {
                                client_updated = 1;
                                NMP_DEBUG("Update printer: %d\n", p_client_tab->printer[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%d", dst_list, p_client_tab->printer[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_printer);
			strcpy(dst_list, dst_list_tmp);

                        if (!strcmp(db_itune, "0")) {
                                client_updated = 1;
                                NMP_DEBUG("Update iTune: %d\n", p_client_tab->itune[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%d", dst_list, p_client_tab->itune[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_itune);
			strcpy(dst_list, dst_list_tmp);
#ifdef RTCONFIG_BWDPI
                        if (strcmp(p_client_tab->bwdpi_host[p_client_tab->detail_info_num], "")) {
                                client_updated = 1;
                                NMP_DEBUG("Update bwdpi_host: %s.\n", p_client_tab->bwdpi_host[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%s", dst_list, p_client_tab->bwdpi_host[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_bwdpi_host);
			strcpy(dst_list, dst_list_tmp);

                        if (strcmp(p_client_tab->bwdpi_vendor[p_client_tab->detail_info_num], "")) {
                                client_updated = 1;
                                NMP_DEBUG("Update bwdpi_vendor: %s.\n", p_client_tab->bwdpi_vendor[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%s", dst_list, p_client_tab->bwdpi_vendor[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_bwdpi_vendor);
			strcpy(dst_list, dst_list_tmp);

                        if (strcmp(p_client_tab->bwdpi_type[p_client_tab->detail_info_num], "")) {
                                client_updated = 1;
                                NMP_DEBUG("Update bwdpi_type: %s.\n", p_client_tab->bwdpi_type[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%s", dst_list, p_client_tab->bwdpi_type[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_bwdpi_type);
			strcpy(dst_list, dst_list_tmp);

                        if (strcmp(p_client_tab->bwdpi_device[p_client_tab->detail_info_num], "")) {
                                client_updated = 1;
                                NMP_DEBUG("Update bwdpi_device: %s.\n", p_client_tab->bwdpi_device[p_client_tab->detail_info_num]);
                                sprintf(dst_list_tmp, "%s>%s", dst_list, p_client_tab->bwdpi_device[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list_tmp, "%s>%s", dst_list, db_bwdpi_device);
			strcpy(dst_list, dst_list_tmp);

#endif
			NMP_DEBUG_M("nv %s\n nvp:%s\n b:%s\n dist_list:%s\n", nv, nvp, b, dst_list);
			if(nvp != NULL) {
				strcat(dst_list, "<");
				strcat(dst_list, nvp);
			}
			nmp_client_list = realloc(nmp_client_list, sizeof(char)*(strlen(dst_list)+1));
			strcpy(nmp_client_list, dst_list);

NMP_DEBUG_M("*** Update nmp_client_list:\n%s\n", nmp_client_list);
			break;
		}
		free(dst_list);
		free(dst_list_tmp);
	}
	else { //new client
		nmp_client_list = realloc(nmp_client_list, sizeof(char)*(strlen(search_list)+SINGLE_CLIENT_SIZE));
		dst_list_tmp = malloc(sizeof(char)*(strlen(search_list)+SINGLE_CLIENT_SIZE));
// NOTE: Shouldn't that be buffer +1, for null terminator in case of maximum length?

		if (strlen(search_list))
			strcpy(nmp_client_list, search_list);
NMP_DEBUG_M("new client: %d-%s,%s,%d\n",p_client_tab->detail_info_num,
	        new_mac,
        	p_client_tab->device_name[p_client_tab->detail_info_num],
	        p_client_tab->type[p_client_tab->detail_info_num]);

		sprintf(dst_list_tmp,"%s<%s>>%s>%s>%d>%d>%d>%d", nmp_client_list, 
		new_mac,
	        p_client_tab->device_name[p_client_tab->detail_info_num],
		p_client_tab->apple_model[p_client_tab->detail_info_num],
       	 	p_client_tab->type[p_client_tab->detail_info_num],
		p_client_tab->http[p_client_tab->detail_info_num],
		p_client_tab->printer[p_client_tab->detail_info_num],
		p_client_tab->itune[p_client_tab->detail_info_num]
		);
		strcpy(nmp_client_list, dst_list_tmp);

#ifdef RTCONFIG_BWDPI
		sprintf(dst_list_tmp,"%s>%s>%s>%s>%s", nmp_client_list,
                p_client_tab->bwdpi_host[p_client_tab->detail_info_num],
                p_client_tab->bwdpi_vendor[p_client_tab->detail_info_num],
                p_client_tab->bwdpi_type[p_client_tab->detail_info_num],
                p_client_tab->bwdpi_device[p_client_tab->detail_info_num]);
		strcpy(nmp_client_list, dst_list_tmp);
#endif

		free(dst_list_tmp);
	}


	free(search_list);
}

int
DeletefromDB(CLIENT_DETAIL_INFO_TABLE *p_client_tab) {
	char *mac_str;
	char *dst_list;
	char *nv, *nvp, *b, *search_list;
	char *db_mac, *db_user_def, *db_device_name, *db_type, *db_http, *db_printer, *db_itune, *db_apple_model;
#ifdef RTCONFIG_BWDPI
	char *db_bwdpi_host, *db_bwdpi_vendor, *db_bwdpi_type, *db_bwdpi_device;
#endif
	int del_ret = 0;


	mac_str = p_client_tab->delete_mac;
NMP_DEBUG("*** delete_from_memory: %s ***\n%s\n", mac_str, nmp_client_list);
	search_list = strdup(nmp_client_list);

	b = strstr(search_list, mac_str);
	if(b!=NULL) { //find the client in the DB
		dst_list = malloc(sizeof(char)*(strlen(nmp_client_list)+1));
NMP_DEBUG_M("client data in DB: %s\n", mac_str);

		nvp = nv = b;
		*(b-1) = '\0';
		strcpy(dst_list, search_list);
		//b++;
		while (nv && (b = strsep(&nvp, "<")) != NULL) {
			if (b == NULL) continue;
#ifdef RTCONFIG_BWDPI
			if (vstrsep(b, ">", &db_mac, &db_user_def, &db_device_name, &db_apple_model, &db_type, &db_http, &db_printer, &db_itune, &db_bwdpi_host, &db_bwdpi_vendor, &db_bwdpi_type, &db_bwdpi_device) != 12) continue;
#else
			if (vstrsep(b, ">", &db_mac, &db_user_def, &db_device_name, &db_apple_model, &db_type, &db_http, &db_printer, &db_itune) != 8) continue;
#endif

NMP_DEBUG_M("-%s,%s,%s,%s,%d,%d,%d,%d-\n", db_mac, db_user_def, db_device_name, db_apple_model, atoi(db_type), atoi(db_http), atoi(db_printer), atoi(db_itune));
#ifdef RTCONFIG_BWDPI
			NMP_DEBUG_M("BWDPI: %s,%s,%s,%s\n", db_bwdpi_host, db_bwdpi_vendor, db_bwdpi_type, db_bwdpi_device);
#endif
			NMP_DEBUG("nv %s\n nvp:%s\n b:%s\n dist_list:%s\n", nv, nvp, b, dst_list);
			if(nvp != NULL) {
				strcat(dst_list, "<");
				strcat(dst_list, nvp);
			}
			nmp_client_list = realloc(nmp_client_list, sizeof(char)*(strlen(dst_list)+1));
			strlcpy(nmp_client_list, dst_list, strlen(dst_list)+1);
			free(dst_list);
NMP_DEBUG_M("*** Update nmp_client_list:\n%s\n", nmp_client_list);
			del_ret = 1; 
			break;
		}
	}

	free(search_list);
	memset(p_client_tab->delete_mac, 0, sizeof(p_client_tab->delete_mac));	
	return del_ret;
}

void
reset_db() {
NMP_DEBUG("RESET DB!!!\n");
	commit_no = 0;
	if ((fp_ncl=fopen(NMP_CLIENT_LIST_FILENAME, "w"))) {
		fclose(fp_ncl);
	}
	memset(nmp_client_list, 0, strlen(nmp_client_list)+1);	
	refresh_sig();
}
void
delete_sig_on() {
NMP_DEBUG("DELETE OFFLINE CLIENT FROM DB!!!\n");
	delete_sig = 1;
}
#endif

void
show_client_info() {
	show_info = 1;
}

static void AppleModelCheck(char *model, char *name, int *type, char *shm_model)
{
	struct apple_model_handler *model_handler;
	struct apple_name_handler *name_handler;

        for (model_handler = &apple_model_handlers[0]; model_handler->phototype; model_handler++) {
                if((shm_model != NULL) && strstr(shm_model, model_handler->phototype))
                {
                        strcpy(model, model_handler->model);
			*type =  atoi(model_handler->type);
			NMP_DEBUG_M("1. Apple Check get model=%s, type=%d\n", model, *type);
                        return;
                }
	}
        for (name_handler = &apple_name_handlers[0]; name_handler->name; name_handler++) {
                if((name != NULL) && strstr(name, name_handler->name))
                {
                        *type =  atoi(name_handler->type);
			NMP_DEBUG_M("2. Apple Check name=%s, find type=%d\n", name, *type);
                        break;
                }
        }

	return;
}

#ifdef RTCONFIG_UPNPC
static int QuerymUPnPCInfo(P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab, int x)
{
        unsigned char *a;
        int i;
        char search_list[128], client_ip[16];
	char *nv, *nvp, *b;
        char *upnpc_ip, *upnpc_type, *upnpc_friendlyname;
	struct upnp_type_handler *upnp_handler;
	FILE *fp;

	sprintf(client_ip, "%d.%d.%d.%d",
	p_client_detail_info_tab->ip_addr[x][0],
	p_client_detail_info_tab->ip_addr[x][1],
	p_client_detail_info_tab->ip_addr[x][2],
	p_client_detail_info_tab->ip_addr[x][3]);

	if( (fp = fopen("/tmp/miniupnpc.log", "r")) != NULL )
	{
		while( fgets(search_list, sizeof(search_list), fp) )
		{
        		if( strstr(search_list, client_ip) )
			{
        			nvp = nv = search_list;

			        while (nv && (b = strsep(&nvp, "<")) != NULL) {
        	        		if (vstrsep(b, ">", &upnpc_ip, &upnpc_type, &upnpc_friendlyname) != 3) 
						continue;
				}

				if(p_client_detail_info_tab->type[x] == 0) {	
			            for (upnp_handler = &upnp_type_handlers[0]; upnp_handler->server; upnp_handler++) {
			                if(!strcmp(upnpc_type, upnp_handler->server))
			                {
			                        NMP_DEBUG("MiniUPnP get type!!! %s = %s\n", upnpc_type, upnp_handler->type);
						p_client_detail_info_tab->type[x] = atoi(upnp_handler->type);
                        			break;
			                }
				    }
			        }
				
				if(p_client_detail_info_tab->device_name[x] == NULL) {
				    if((strcmp(upnpc_friendlyname, "") && !strstr(upnpc_friendlyname, "UPnP Access Point"))) {
					NMP_DEBUG("MiniUPnP get name: %s\n", upnpc_friendlyname);
					strlcpy(p_client_detail_info_tab->device_name[x], upnpc_friendlyname, sizeof(p_client_detail_info_tab->device_name[x]));
				    }
				}
			}
		}
		fclose(fp);
	}
	return 0;
}
#endif

#ifdef RTCONFIG_BONJOUR
static int QuerymDNSInfo(P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab, int x)
{
	unsigned char *a;
	int i;

/*
printf("==== mDNS Query: %d?%d: %d.%d.%d.%d ====\n", x,
p_client_detail_info_tab->ip_mac_num,
p_client_detail_info_tab->ip_addr[x][0],
p_client_detail_info_tab->ip_addr[x][1],
p_client_detail_info_tab->ip_addr[x][2],
p_client_detail_info_tab->ip_addr[x][3]
);
*/
	mdns_lock = file_lock("mDNSNetMonitor");

	i = 0;
	while(shmClientList->IPaddr[i][0]!=NULL && i<255 ) {
		a = shmClientList->IPaddr[i];
		if(!memcmp(a,p_client_detail_info_tab->ip_addr[p_client_detail_info_tab->ip_mac_num],4)) {
			NMP_DEBUG_M("Query mDNS get: %d, %d.%d.%d.%d/%s/%s_\n", i,
                        a[0],a[1],a[2],a[3], shmClientList->Name[i], shmClientList->Model[i]);
			if(shmClientList->Name[i]!=NULL && strcmp(shmClientList->Name[i],p_client_detail_info_tab->device_name[x]))
				strlcpy(p_client_detail_info_tab->device_name[x], shmClientList->Name[i], sizeof(p_client_detail_info_tab->device_name[x]));
                        if(shmClientList->Model[i]!=NULL && strcmp(shmClientList->Name[i],p_client_detail_info_tab->apple_model[x]))
				toLowerCase(shmClientList->Model[i]);
                                AppleModelCheck(p_client_detail_info_tab->apple_model[x],
						p_client_detail_info_tab->device_name[x],
						&p_client_detail_info_tab->type[x],
						shmClientList->Model[i]);
			break;
		}
		i++;
	}

	file_unlock(mdns_lock);

	return 0;
}
#endif

void StringChk(char *chk_string)
{
        char *ptr = chk_string;
        while(*ptr!=NULL) {
                if(*ptr<0x20 || *ptr>0x7E)
                        *ptr = ' ';
                ptr++;
        }
}

#ifdef RTCONFIG_BWDPI
static int QueryBwdpiInfo(P_CLIENT_DETAIL_INFO_TABLE p_client_detail_info_tab, int x)
{
	bwdpi_device bwdpi_dev_info;
	char mac[18];

	sprintf(mac,"%02X:%02X:%02X:%02X:%02X:%02X",
		p_client_detail_info_tab->mac_addr[x][0],
		p_client_detail_info_tab->mac_addr[x][1],
		p_client_detail_info_tab->mac_addr[x][2],
		p_client_detail_info_tab->mac_addr[x][3],
                p_client_detail_info_tab->mac_addr[x][4],
                p_client_detail_info_tab->mac_addr[x][5]
	);
NMP_DEBUG("==== Bwdpi Query: %s\n", mac);

	if(bwdpi_client_info(&mac, &bwdpi_dev_info)) {
NMP_DEBUG("  Get: %s/%s/%s/%s\n", bwdpi_dev_info.hostname,bwdpi_dev_info.vendor_name,
				  bwdpi_dev_info.type_name,bwdpi_dev_info.device_name);
		strlcpy(p_client_detail_info_tab->bwdpi_host[x], bwdpi_dev_info.hostname, sizeof(p_client_detail_info_tab->bwdpi_host[x]));
		strlcpy(p_client_detail_info_tab->bwdpi_vendor[x], bwdpi_dev_info.vendor_name, sizeof(p_client_detail_info_tab->bwdpi_vendor[x]));
		strlcpy(p_client_detail_info_tab->bwdpi_type[x], bwdpi_dev_info.type_name, sizeof(p_client_detail_info_tab->bwdpi_type[x]));
		strlcpy(p_client_detail_info_tab->bwdpi_device[x], bwdpi_dev_info.device_name, sizeof(p_client_detail_info_tab->bwdpi_device[x]));
		StringChk(p_client_detail_info_tab->bwdpi_host[x]);
		StringChk(p_client_detail_info_tab->bwdpi_vendor[x]);
		StringChk(p_client_detail_info_tab->bwdpi_type[x]);
		StringChk(p_client_detail_info_tab->bwdpi_device[x]);
	}
	
	#ifdef RTCONFIG_NOTIFICATION_CENTER
	if(bwdpi_dev_info.device_name){
		extern int TRIGGER_FLAG;
		NOTIFY_EVENT_T *event_t = initial_nt_event();
		if(!(TRIGGER_FLAG>>FLAG_XBOX_PS & 1) && (strstr(bwdpi_dev_info.device_name, "Xbox") || strstr(bwdpi_dev_info.device_name, "PlayStation"))){
			event_t->event = HINT_XBOX_PS_EVENT;
			snprintf(event_t->msg, sizeof(event_t->msg), "%s", "HINT_XBOX_PS_EVENT");
			NMP_DEBUG("NT_CENTER: Send event lan with Xbox PlayStation ID:%08x msg:%s!\n", event_t->event, event_t->msg);
			send_trigger_event(event_t);
			TRIGGER_FLAG |= (1<<FLAG_XBOX_PS);
			NMP_DEBUG("========check TRIGGER_FLAG %d\n", TRIGGER_FLAG);
		}
		else if(!(TRIGGER_FLAG>>FLAG_UPNP_RENDERER & 1) && (strstr(bwdpi_dev_info.device_name, "Chromecast"))){
			event_t->event = HINT_UPNP_RENDERER_EVENT;
			snprintf(event_t->msg, sizeof(event_t->msg), "%s", "HINT_UPNP_RENDERER_EVENT");
			NMP_DEBUG("NT_CENTER: Send event UPnP renderer ID:%08x msg:%s!\n", event_t->event, event_t->msg);
			send_trigger_event(event_t);
			TRIGGER_FLAG |= (1<<FLAG_UPNP_RENDERER);
			NMP_DEBUG("========check TRIGGER_FLAG %d\n", TRIGGER_FLAG);
		}	
		else if(!(TRIGGER_FLAG>>FLAG_OSX_INLAN & 1) && (strstr(bwdpi_dev_info.device_name, "Mac"))){
			event_t->event = HINT_OSX_INLAN_EVENT;
			snprintf(event_t->msg, sizeof(event_t->msg), "%s", "HINT_OSX_INLAN_EVENT");
			NMP_DEBUG("NT_CENTER: Send event LAN with OSX device ID:%08x msg:%s!\n", event_t->event, event_t->msg);
			send_trigger_event(event_t);
			TRIGGER_FLAG |= (1<<FLAG_OSX_INLAN);
			NMP_DEBUG("========check TRIGGER_FLAG %d\n", TRIGGER_FLAG);
		}
		nt_event_free(event_t);
	}
	#endif
		
//NMP_DEBUG("Result: %s, %s\n\n", p_client_detail_info_tab->bwdpi_hostname[x],
//p_client_detail_info_tab->bwdpi_devicetype[x]);

	return 0;
}
#endif


/******************************************/
int main(int argc, char *argv[])
{
	int arp_sockfd, arp_getlen, i;
	struct sockaddr_in router_addr;
	char router_ipaddr[17], router_mac[17], buffer[ARP_BUFFER_SIZE];
	unsigned char scan_ipaddr[4]; // scan ip
	FILE *fp_ip;
	fd_set rfds;
        ARP_HEADER * arp_ptr;
        struct timeval tv1, tv2, arp_timeout;
	int shm_client_detail_info_id , shm_mdns_id;
	int ip_dup, mac_dup, real_num;
	int lock;
	int query_ret, chk_DB_ret;
	unsigned short msg_type;
#if defined(RTCONFIG_QCA) && defined(RTCONFIG_WIRELESSREPEATER)	
	char *mac;
#endif
	//Rawny: save client_list in memory 
	unsigned int size_ncl; //size of nmp_client_list
	int SINGLE_CLIENT_SIZE;
	
        FILE *fp = fopen("/var/run/networkmap.pid", "w");
        if(fp != NULL){
                fprintf(fp, "%d", getpid());
                fclose(fp);
        }

	//fp_upnp = fopen("/tmp/upnp.log", "w");
//	fp_smb = fopen("tmp/smb.log", "w");

	//Initial Shared Memory
	//client tables
	lock = file_lock("networkmap");
	shm_client_detail_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
        if (shm_client_detail_info_id == -1){
    	    fprintf(stderr,"client info shmget failed\n");
	    file_unlock(lock);
            exit(1);
    	}

	CLIENT_DETAIL_INFO_TABLE *p_client_detail_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shmat(shm_client_detail_info_id,(void *) 0,0);
	memset(p_client_detail_info_tab, 0x00, sizeof(CLIENT_DETAIL_INFO_TABLE));
	p_client_detail_info_tab->ip_mac_num = 0;
	p_client_detail_info_tab->detail_info_num = 0;
	file_unlock(lock);

	//mDNSNetMonitor
	mdns_lock = file_lock("mDNSNetMonitor");
        shm_mdns_id = shmget((key_t)1002, sizeof(mDNSClientList), 0666|IPC_CREAT);

        if (shm_mdns_id == -1){
                fprintf(stderr,"mDNS shmget failed\n");
                file_unlock(mdns_lock);
                return 0;
        }

        shmClientList = (mDNSClientList *)shmat(shm_mdns_id,(void *) 0,0);
        if (shmClientList == (void *)-1){
                fprintf(stderr,"shmat failed\n");
                file_unlock(mdns_lock);
                return 0;
        }
	file_unlock(mdns_lock);
	//////

	#ifdef NMP_DB
		if ((fp_ncl=fopen(NMP_CLIENT_LIST_FILENAME, "r"))) {
			fseek(fp_ncl, 0L, SEEK_END);
			size_ncl = ftell(fp_ncl);
			NMP_DEBUG("nmp_client_list FILE size %d\n", size_ncl);
			fseek(fp_ncl, 0L, SEEK_SET);
			if (size_ncl && size_ncl < NCL_LIMIT) {
				nmp_client_list = malloc(sizeof(char)*size_ncl+1);
				if (fread(nmp_client_list, 1, size_ncl, fp_ncl) != size_ncl) {
					NMP_DEBUG("Read Client list DB ERR....Reset DB!\n");
					memset(nmp_client_list, 0, NCL_LIMIT);
				} 
				nmp_client_list[size_ncl] = '\0';
				NMP_DEBUG("Read Client list DB: %s from %s\n", nmp_client_list, NMP_CLIENT_LIST_FILENAME);
			}
			else {
				nmp_client_list = malloc(sizeof(char)*SINGLE_CLIENT_SIZE);
				NMP_DEBUG("Read Client list DB fail!\nSize is %d...remove oversize file.\n", size_ncl);
				eval("rm", NMP_CLIENT_LIST_FILENAME);				
			}
			fclose(fp_ncl);
		}
		else
			nmp_client_list = malloc(sizeof(char)*SINGLE_CLIENT_SIZE);

		//signal(SIGUSR2, reset_db);
	#endif

	//Get Router's IP/Mac
	strcpy(router_ipaddr, nvram_safe_get("lan_ipaddr"));
#if defined(RTCONFIG_RGMII_BRCM5301X)
	strcpy(router_mac, nvram_safe_get("et1macaddr"));
#else
#if defined(RTCONFIG_QCA)
#ifdef RTCONFIG_WIRELESSREPEATER	
	if(nvram_get_int("sw_mode")==SW_MODE_REPEATER  && (mac=getStaMAC())!=NULL)
		strlcpy(router_mac, mac, sizeof(router_mac));	
	else   
#endif	   
		strcpy(router_mac, nvram_safe_get("et1macaddr"));
#else	
	strcpy(router_mac, nvram_safe_get("et0macaddr"));
#endif
#endif	
#ifdef RTCONFIG_GMAC3
        if(nvram_match("gmac3_enable", "1"))
		strcpy(router_mac, nvram_safe_get("et2macaddr"));
#endif
        inet_aton(router_ipaddr, &router_addr.sin_addr);
        memcpy(my_ipaddr,  &router_addr.sin_addr, 4);

	//Prepare scan 
	networkmap_fullscan = 1;
	nvram_set("networkmap_fullscan", "1");

	if (argc > 1) {
		if (strcmp(argv[1], "--bootwait") == 0) {
			sleep(30);
		}
	}
	if (strlen(router_mac)!=0) ether_atoe(router_mac, my_hwaddr);

	signal(SIGUSR1, refresh_sig); //catch UI refresh signal
	signal(SIGTERM, safe_leave);
	delete_sig = 0;
	signal(SIGUSR2, delete_sig_on);
	show_info = 0;
	//signal(SIGUSR2, show_client_info);
	
        // create UDP socket and bind to "br0" to get ARP packet//
	arp_sockfd = create_socket(INTERFACE);

        if(arp_sockfd < 0)
                perror("create socket ERR:");
	else {
	        arp_timeout.tv_sec = 0;
        	arp_timeout.tv_usec = 5000;
		setsockopt(arp_sockfd, SOL_SOCKET, SO_RCVTIMEO, &arp_timeout, sizeof(arp_timeout));//set receive timeout
		dst_sockll = src_sockll; //Copy sockaddr info to dst
		memset(dst_sockll.sll_addr, -1, sizeof(dst_sockll.sll_addr)); // set dmac= FF:FF:FF:FF:FF:FF
	}

	//initial trigger flag
	#ifdef RTCONFIG_NOTIFICATION_CENTER
	TRIGGER_FLAG = atoi(nvram_safe_get("networkmap_trigger_flag"));
	if(TRIGGER_FLAG < 0 || TRIGGER_FLAG > 15) TRIGGER_FLAG = 0;
	NMP_DEBUG(" Test networkmap trigger flag >>> %d!\n", TRIGGER_FLAG);	
	#endif

        while(1)//main while loop
        {
	    while(1) { //full scan and reflush recv buffer
		fullscan:
                if(networkmap_fullscan == 1) { //Scan all IP address in the subnetwork
		    if(scan_count == 0) { 
#ifdef RTCONFIG_BONJOUR
			eval("mDNSQuery");	//send mDNS service dicsovery
#endif
			eval("asusdiscovery");	//find asus device
			// (re)-start from the begining
			memset(scan_ipaddr, 0x00, 4);
			memcpy(scan_ipaddr, &router_addr.sin_addr, 3);
	                arp_timeout.tv_sec = 0;
        	        arp_timeout.tv_usec = 5000;
                	setsockopt(arp_sockfd, SOL_SOCKET, SO_RCVTIMEO, &arp_timeout, sizeof(arp_timeout));//set receive timeout
			NMP_DEBUG("Starting full scan!\n");
			
                        if(nvram_match("refresh_networkmap", "1")) {//reset client tables
				lock = file_lock("networkmap");
        			memset(p_client_detail_info_tab, 0x00, sizeof(CLIENT_DETAIL_INFO_TABLE));
        			//p_client_detail_info_tab->detail_info_num = 0;
				//p_client_detail_info_tab->ip_mac_num = 0;
				file_unlock(lock);
				nvram_unset("refresh_networkmap");
			}
			else {
				int x = 0;
				for(; x<255; x++)
					p_client_detail_info_tab->exist[x]=0;
			}
		    }
		    scan_count++;
		    scan_ipaddr[3]++;

		    if( scan_count<255 && memcmp(scan_ipaddr, my_ipaddr, 4) ) {
                        sent_arppacket(arp_sockfd, scan_ipaddr);
		    }         
		    else if(scan_count==255) { //Scan completed
                	arp_timeout.tv_sec = 2;
                	arp_timeout.tv_usec = 0; //Reset timeout at monitor state for decase cpu loading
                	setsockopt(arp_sockfd, SOL_SOCKET, SO_RCVTIMEO, &arp_timeout, sizeof(arp_timeout));//set receive timeout
			networkmap_fullscan = 0;
			//scan_count = 0;
			nvram_set("networkmap_fullscan", "0");
			NMP_DEBUG("Finish full scan!\n");
		    }
                }// End of full scan

		if( show_info )
		{ 	
			int ii = 0;
			printf("\nIP / MAC / DevName / Apple Dev / Type / HTTP / iTune / Printer\n");
			while( ii < p_client_detail_info_tab->ip_mac_num)
			{
#ifdef RTCONFIG_BWDPI
				printf(" %d.%d.%d.%d /%02X:%02X:%02X:%02X:%02X:%02X/%s/%s/%d/%d/%d/%d/%s/%s/%s/%s\n",
                                p_client_detail_info_tab->ip_addr[ii][0],p_client_detail_info_tab->ip_addr[ii][1],
                                p_client_detail_info_tab->ip_addr[ii][2],p_client_detail_info_tab->ip_addr[ii][3],
                                p_client_detail_info_tab->mac_addr[ii][0],p_client_detail_info_tab->mac_addr[ii][1],
                                p_client_detail_info_tab->mac_addr[ii][2],p_client_detail_info_tab->mac_addr[ii][3],
                                p_client_detail_info_tab->mac_addr[ii][4],p_client_detail_info_tab->mac_addr[ii][5],
				p_client_detail_info_tab->device_name[ii],p_client_detail_info_tab->apple_model[ii],
				p_client_detail_info_tab->type[ii],p_client_detail_info_tab->http[ii],
				p_client_detail_info_tab->itune[ii], p_client_detail_info_tab->printer[ii],
                		p_client_detail_info_tab->bwdpi_host[ii],
                		p_client_detail_info_tab->bwdpi_vendor[ii],
                		p_client_detail_info_tab->bwdpi_type[ii],
                		p_client_detail_info_tab->bwdpi_device[ii]
				);
#else
				printf(" %d.%d.%d.%d /%02X:%02X:%02X:%02X:%02X:%02X/%s/%s/%d/%d/%d\n",
                                p_client_detail_info_tab->ip_addr[ii][0],p_client_detail_info_tab->ip_addr[ii][1],
                                p_client_detail_info_tab->ip_addr[ii][2],p_client_detail_info_tab->ip_addr[ii][3],
                                p_client_detail_info_tab->mac_addr[ii][0],p_client_detail_info_tab->mac_addr[ii][1],
                                p_client_detail_info_tab->mac_addr[ii][2],p_client_detail_info_tab->mac_addr[ii][3],
                                p_client_detail_info_tab->mac_addr[ii][4],p_client_detail_info_tab->mac_addr[ii][5],
				p_client_detail_info_tab->device_name[ii],p_client_detail_info_tab->apple_model[ii],
				p_client_detail_info_tab->type[ii],p_client_detail_info_tab->http[ii],
				p_client_detail_info_tab->itune[ii], p_client_detail_info_tab->printer[ii]
				);
#endif
				ii++;
			}
			show_info = 0;
		}

		memset(buffer, 0, ARP_BUFFER_SIZE);
		arp_getlen=recvfrom(arp_sockfd, buffer, ARP_BUFFER_SIZE, 0, NULL, NULL);
	   	if(arp_getlen == -1) {
			if( scan_count<255) {
				goto fullscan;
			}
			else
				break;
		}
		else {
		    arp_ptr = (ARP_HEADER*)(buffer);
/*
                    NMP_DEBUG_M("*Receive an ARP Packet from: %d.%d.%d.%d to %d.%d.%d.%d:%02X:%02X:%02X:%02X - len:%d\n",
				(int)arp_ptr->source_ipaddr[0],(int)arp_ptr->source_ipaddr[1],
				(int)arp_ptr->source_ipaddr[2],(int)arp_ptr->source_ipaddr[3],
				(int)arp_ptr->dest_ipaddr[0],(int)arp_ptr->dest_ipaddr[1],
				(int)arp_ptr->dest_ipaddr[2],(int)arp_ptr->dest_ipaddr[3],
                                arp_ptr->dest_hwaddr[0],arp_ptr->dest_hwaddr[1],
                                arp_ptr->dest_hwaddr[2],arp_ptr->dest_hwaddr[3],
				arp_getlen);
*/
		    //Check ARP packet if source ip and router ip at the same network
                    if( !memcmp(my_ipaddr, arp_ptr->source_ipaddr, 3) ) {
			msg_type = ntohs(arp_ptr->message_type);

			if( //ARP packet to router
			   ( msg_type == 0x02 &&   		       	// ARP response
                       	    memcmp(arp_ptr->dest_ipaddr, my_ipaddr, 4) == 0 && 	// dest IP
                       	    memcmp(arp_ptr->dest_hwaddr, my_hwaddr, 6) == 0) 	// dest MAC
			    ||
			   (msg_type == 0x01 &&                    // ARP request
                            memcmp(arp_ptr->dest_ipaddr, my_ipaddr, 4) == 0)    // dest IP
			){
			    //NMP_DEBUG("   It's an ARP Response to Router!\n");
                            NMP_DEBUG("*RCV %d.%d.%d.%d-%02X:%02X:%02X:%02X:%02X:%02X,%d,%02x\n",
                            arp_ptr->source_ipaddr[0],arp_ptr->source_ipaddr[1],
                            arp_ptr->source_ipaddr[2],arp_ptr->source_ipaddr[3],
                            arp_ptr->source_hwaddr[0],arp_ptr->source_hwaddr[1],
                            arp_ptr->source_hwaddr[2],arp_ptr->source_hwaddr[3],
                            arp_ptr->source_hwaddr[4],arp_ptr->source_hwaddr[5], scan_count, msg_type);

                            for(i=0; i<p_client_detail_info_tab->ip_mac_num; i++) {
				ip_dup = memcmp(p_client_detail_info_tab->ip_addr[i], arp_ptr->source_ipaddr, 4);
                                mac_dup = memcmp(p_client_detail_info_tab->mac_addr[i], arp_ptr->source_hwaddr, 6);
				if((ip_dup == 0) && (mac_dup == 0)) {
					lock = file_lock("networkmap");
					p_client_detail_info_tab->exist[i] = 1;
					file_unlock(lock);
					break;
				}
				else if((ip_dup != 0) && (mac_dup != 0)) {
					continue;
				}

				else if( (scan_count>=255) && ((ip_dup != 0) && (mac_dup == 0)) ) { 
					NMP_DEBUG("IP changed, update immediately\n");
					NMP_DEBUG("*CMP %d.%d.%d.%d-%02X:%02X:%02X:%02X:%02X:%02X\n",
                                    	p_client_detail_info_tab->ip_addr[i][0],p_client_detail_info_tab->ip_addr[i][1],
                                    	p_client_detail_info_tab->ip_addr[i][2],p_client_detail_info_tab->ip_addr[i][3],
                                    	p_client_detail_info_tab->mac_addr[i][0],p_client_detail_info_tab->mac_addr[i][1],
                                    	p_client_detail_info_tab->mac_addr[i][2],p_client_detail_info_tab->mac_addr[i][3],
                                    	p_client_detail_info_tab->mac_addr[i][4],p_client_detail_info_tab->mac_addr[i][5]);
					
					lock = file_lock("networkmap");
	                                memcpy(p_client_detail_info_tab->ip_addr[i],
        	                                arp_ptr->source_ipaddr, 4);
                	                memcpy(p_client_detail_info_tab->mac_addr[i],
                        	                arp_ptr->source_hwaddr, 6);
					p_client_detail_info_tab->exist[i] = 1;
					file_unlock(lock);
					/*
					real_num = p_client_detail_info_tab->detail_info_num;
					p_client_detail_info_tab->detail_info_num = i;
                                        #ifdef NMP_DB
                                                check_nmp_db(p_client_detail_info_tab, i);
                                        #endif
					FindAllApp(my_ipaddr, p_client_detail_info_tab);
					FindHostname(p_client_detail_info_tab);
					p_client_detail_info_tab->detail_info_num = real_num;
					*/
					break;
				}

                            }
			    //NMP_DEBUG("Out check!\n");
			    //i=0, table is empty.
			    //i=num, no the same ip at table.
			    if(i==p_client_detail_info_tab->ip_mac_num){
				lock = file_lock("networkmap");
				memcpy(p_client_detail_info_tab->ip_addr[p_client_detail_info_tab->ip_mac_num], 
					arp_ptr->source_ipaddr, 4);
                                memcpy(p_client_detail_info_tab->mac_addr[p_client_detail_info_tab->ip_mac_num], 
					arp_ptr->source_hwaddr, 6);
				p_client_detail_info_tab->exist[p_client_detail_info_tab->ip_mac_num] = 1;
				chk_DB_ret = 0;
				#ifdef NMP_DB
					chk_DB_ret = check_nmp_db(p_client_detail_info_tab, i);
				#endif
				if (!chk_DB_ret) {
					#ifdef RTCONFIG_BONJOUR
						query_ret = QuerymDNSInfo(p_client_detail_info_tab, i);
					#endif
					#ifdef RTCONFIG_UPNPC
						query_ret = QuerymUPnPCInfo(p_client_detail_info_tab, i);
					#endif
				}

				NMP_DEBUG("Fill: %d-> %d.%d.%d.%d\n", i,
				p_client_detail_info_tab->ip_addr[i][0],
                                p_client_detail_info_tab->ip_addr[i][1],
				p_client_detail_info_tab->ip_addr[i][2],
				p_client_detail_info_tab->ip_addr[i][3]);

                                p_client_detail_info_tab->ip_mac_num++;
				file_unlock(lock);
                	    }
			}
			else { //Nomo ARP Packet or ARP response to other IP
        	                //Compare IP and IP buffer if not exist
                        	for(i=0; i<p_client_detail_info_tab->ip_mac_num; i++) {
                                        if( !memcmp(p_client_detail_info_tab->ip_addr[i], arp_ptr->source_ipaddr, 4) &&
					    !memcmp(p_client_detail_info_tab->mac_addr[i], arp_ptr->source_hwaddr, 6)) {
                                              	NMP_DEBUG_M("Find the same IP/MAC at the table!\n");
                	                        break;
                        	        }
                        	}
                        	if( i==p_client_detail_info_tab->ip_mac_num ) //Find a new IP or table is empty! Send an ARP request.
				{
					NMP_DEBUG("New device or IP/MAC changed!!\n");
					if(memcmp(my_ipaddr, arp_ptr->source_ipaddr, 4))
                                		sent_arppacket(arp_sockfd, arp_ptr->source_ipaddr);
					else
						NMP_DEBUG("New IP is the same as Router IP! Ignore it!\n");
				}
			}//End of Nomo ARP Packet
		    }//Source IP in the same subnetwork
		}//End of arp_getlen != -1
	    } // End of while for flush buffer
	/*
	int y = 0;
	while(p_client_detail_info_tab->type[y]!=0){
            NMP_DEBUG("%d: %d.%d.%d.%d,%02X:%02X:%02X:%02X:%02X:%02X,%s,%d,%d,%d,%d,%d\n", y ,
                                p_client_detail_info_tab->ip_addr[y][0],p_client_detail_info_tab->ip_addr[y][1],
                                p_client_detail_info_tab->ip_addr[y][2],p_client_detail_info_tab->ip_addr[y][3],
                                p_client_detail_info_tab->mac_addr[y][0],p_client_detail_info_tab->mac_addr[y][1],
                                p_client_detail_info_tab->mac_addr[y][2],p_client_detail_info_tab->mac_addr[y][3],
                                p_client_detail_info_tab->mac_addr[y][4],p_client_detail_info_tab->mac_addr[y][5],
                                p_client_detail_info_tab->device_name[y],
                                p_client_detail_info_tab->type[y],
                                p_client_detail_info_tab->http[y],
                                p_client_detail_info_tab->printer[y],
                                p_client_detail_info_tab->itune[y],
				p_client_detail_info_tab->exist[y]);
		y++;
        }
	*/
		#ifdef NMP_DB
		//RAwny: check delete signal
		if(delete_sig) {
			client_updated = DeletefromDB(p_client_detail_info_tab);
			delete_sig = 0;
		}
		#endif
			
	    //Find All Application of clients
	    //NMP_DEBUG("\ndetail?ip : %d?%d\n\n", p_client_detail_info_tab->detail_info_num, p_client_detail_info_tab->ip_mac_num);
	    if(p_client_detail_info_tab->detail_info_num < p_client_detail_info_tab->ip_mac_num) {
		NMP_DEBUG_M("Deep Scan and write to DB!\n");
		nvram_set("networkmap_status", "1");
		FindAllApp(my_ipaddr, p_client_detail_info_tab, p_client_detail_info_tab->detail_info_num);
		lock = file_lock("networkmap");
		FindHostname(p_client_detail_info_tab);
		StringChk(p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num]);
		#ifdef RTCONFIG_BWDPI
		query_ret = QueryBwdpiInfo(p_client_detail_info_tab, p_client_detail_info_tab->detail_info_num);
		#endif
		file_unlock(lock);
		#ifdef NMP_DB
			//Rawny: Check if DB memory size will over limit(512KB)
			if ((NCL_LIMIT - strlen(nmp_client_list)) > SINGLE_CLIENT_SIZE) 
				write_to_DB(p_client_detail_info_tab);
		#endif
		p_client_detail_info_tab->detail_info_num++;
		NMP_DEBUG_M("Finish Deep Scan no.%d!\n", p_client_detail_info_tab->detail_info_num);
	    }
	    #ifdef NMP_DB
	    else {
		NMP_DEBUG_M("commit_no, cli_no, updated: %d, %d, %d\n", 
		commit_no, p_client_detail_info_tab->detail_info_num, client_updated);
		if( (commit_no != p_client_detail_info_tab->detail_info_num) || client_updated ) {
			NMP_DEBUG_M("Write to DB file!\n");
			NMP_DEBUG("Commit nmp client list\n");
			#ifdef RTCONFIG_NOTIFICATION_CENTER
			nvram_set_int("networkmap_trigger_flag", TRIGGER_FLAG);
			NMP_DEBUG("========check TRIGGER_FLAG %d\n", TRIGGER_FLAG);
			#endif
			nvram_lock = file_lock("usenvram");
			nvram_commit();
			file_unlock(nvram_lock);
			if ((fp_ncl=fopen(NMP_CLIENT_LIST_FILENAME, "w"))) {
				fprintf(fp_ncl, "%s", nmp_client_list);
				fclose(fp_ncl);
			}

		    	commit_no = p_client_detail_info_tab->detail_info_num;
			client_updated = 0;
			NMP_DEBUG_M("Finish Write to DB file!\n");
		}
	    }
	    #endif
	    if(p_client_detail_info_tab->detail_info_num == p_client_detail_info_tab->ip_mac_num) 
		nvram_set("networkmap_status", "0");    // Done scanning and resolving
	} //End of main while loop
	shmdt(p_client_detail_info_tab);
	close(arp_sockfd);
	return 0;
}
