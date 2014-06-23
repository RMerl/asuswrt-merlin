#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include "../shared/shutils.h"    // for eval()
#include <bcmnvram.h>
#include "networkmap.h"
#include "endianness.h"
//2011.02 Yau add shard memory
#include <sys/ipc.h>
#include <sys/shm.h>
#include <rtconfig.h>

unsigned char my_hwaddr[6];
unsigned char my_ipaddr[4];
unsigned char broadcast_hwaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned char refresh_ip_list[255][4];
int networkmap_fullscan;
int refresh_exist_table = 0, scan_count=0;
char *nmp_client_list;

/******** Build ARP Socket Function *********/
struct sockaddr_ll src_sockll, dst_sockll;

static int
iface_get_id(int fd, const char *device)
{
        struct ifreq    ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
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
	NMP_DEBUG_M("Send ARP Request success to: .%d.%d\n", (int *)dst_ipaddr[2],(int *)dst_ipaddr[3]);
        return 0;
}
/******* End of Build ARP Socket Function ********/

/*********** Signal function **************/
static void refresh_sig(void)
{
        NMP_DEBUG("Refresh network map!\n");
        networkmap_fullscan = 1;
        refresh_exist_table = 0;
	scan_count = 0;
	nvram_set("networkmap_status", "1");
	eval("rm", "/var/client*");
#if 0
	//reset exixt ip table
        memset(&client_detail_info_tab, 0x00, sizeof(client_detail_info_tabLE));
        p_client_detail_info_tab->num = 0;
	//remove file;
	ret = eval("rm", "/var/networkmap.dat");
#endif
}


#ifdef NMP_DB
int commit_no = 0;
int client_updated = 0;

void
convert_mac_to_string(unsigned char *mac, char *mac_str)
{
	sprintf(mac_str, "%02x%02x%02x%02x%02x%02x",
		*mac,*(mac+1),*(mac+2),*(mac+3),*(mac+4),*(mac+5));
}

void
check_nmp_db(CLIENT_DETAIL_INFO_TABLE *p_client_tab, int client_no)
{
        char new_mac[13];
        char *search_list, *nv, *nvp, *b;
	char *db_mac, *db_user_def, *db_device_name, *db_type, *db_http, *db_printer, *db_itune;

NMP_DEBUG("check_nmp_db:\n");
	search_list = strdup(nmp_client_list);
        convert_mac_to_string(p_client_tab->mac_addr[client_no], new_mac);

NMP_DEBUG("search_list= %s\n", search_list);
        if(strstr(search_list, new_mac)==NULL)
		return;

        nvp = nv = search_list;

	while (nv && (b = strsep(&nvp, "<")) != NULL) {
                if (vstrsep(b, ">", &db_mac, &db_user_def, &db_device_name, &db_type, &db_http, &db_printer, &db_itune) != 7)
                                continue;

		NMP_DEBUG("DB: %s,%s,%s,%s,%s,%s,%s\n", db_mac, db_user_def, db_device_name, db_type, db_http, db_printer, db_itune);
        	if (!strcmp(db_mac, new_mac)) {
			NMP_DEBUG("*** %s at DB!!! Update to memory\n",new_mac);
			strcpy(p_client_tab->user_define[client_no], db_user_def);
		        strcpy(p_client_tab->device_name[client_no], db_device_name);
		        p_client_tab->type[client_no] = atoi(db_type);
			p_client_tab->http[client_no] = atoi(db_http);
			p_client_tab->printer[client_no] = atoi(db_printer);
			p_client_tab->itune[client_no] = atoi(db_itune);
			break;
		}
	}

	free(search_list);
}

void
write_to_nvram(CLIENT_DETAIL_INFO_TABLE *p_client_tab)
{
	char new_mac[13], *dst_list;
	char *nv, *nvp, *b, *search_list;
	char *db_mac, *db_user_def, *db_device_name, *db_type, *db_http, *db_printer, *db_itune;

	convert_mac_to_string(p_client_tab->mac_addr[p_client_tab->detail_info_num], new_mac);

NMP_DEBUG("*** write_to_nvram: %s ***\n",new_mac);
	search_list = strdup(nmp_client_list);

	b = strstr(search_list, new_mac);
	if(b!=NULL) { //find the client in the DB
		dst_list = malloc(sizeof(char)*10000);
NMP_DEBUG_M("client data in DB: %s\n", new_mac);

	        nvp = nv = b;
		*(b-1) = '\0';
		strcpy(dst_list, search_list);
NMP_DEBUG_M("dst_list= %s\n", dst_list);
		//b++;
		while (nv && (b = strsep(&nvp, "<")) != NULL) {
			if (b == NULL) continue;
			if (vstrsep(b, ">", &db_mac, &db_user_def, &db_device_name, &db_type, &db_http, &db_printer, &db_itune) != 7) continue;
NMP_DEBUG_M("%s,%s,%d,%d,%d,%d\n", db_mac, db_user_def, db_device_name, atoi(db_type), atoi(db_http), atoi(db_printer), atoi(db_itune));

			if (!strcmp(p_client_tab->device_name[p_client_tab->detail_info_num], db_device_name) &&
			    p_client_tab->type[p_client_tab->detail_info_num] == atoi(db_type) &&
			    p_client_tab->http[p_client_tab->detail_info_num] == atoi(db_http) &&
			    p_client_tab->printer[p_client_tab->detail_info_num] == atoi(db_printer) &&
			    p_client_tab->itune[p_client_tab->detail_info_num] == atoi(db_itune) )
			{
NMP_DEBUG("DATA the same!\n");
				return;
			}
			sprintf(dst_list, "%s<%s<%s", dst_list, db_mac, db_user_def);

		        if (strcmp(p_client_tab->device_name[p_client_tab->detail_info_num], "")) {
      	        	        NMP_DEBUG("Update device name: %s.\n", p_client_tab->device_name[p_client_tab->detail_info_num]);
				sprintf(dst_list, "%s>%s", dst_list, p_client_tab->device_name[p_client_tab->detail_info_num]);
		        }
			else
				sprintf(dst_list, "%s>%s", dst_list, db_device_name);
                        if (p_client_tab->type[p_client_tab->detail_info_num] != 6) {
				client_updated = 1;
                                NMP_DEBUG("Update type: %d\n", p_client_tab->type[p_client_tab->detail_info_num]);
                                sprintf(dst_list, "%s>%d", dst_list, p_client_tab->type[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list, "%s>%s", dst_list, db_type);
                        if (!strcmp(db_http, "0") ) {
                                client_updated = 1;
                                NMP_DEBUG("Update http: %d\n", p_client_tab->http[p_client_tab->detail_info_num]);
                                sprintf(dst_list, "%s>%d", dst_list, p_client_tab->http[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list, "%s>%s", dst_list, db_http);
                        if (!strcmp(db_printer, "0") ) {
                                client_updated = 1;
                                NMP_DEBUG("Update type: %d\n", p_client_tab->printer[p_client_tab->detail_info_num]);
                                sprintf(dst_list, "%s>%d", dst_list, p_client_tab->printer[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list, "%s>%s", dst_list, db_printer);
                        if (!strcmp(db_itune, "0")) {
                                client_updated = 1;
                                NMP_DEBUG("Update type: %d\n", p_client_tab->itune[p_client_tab->detail_info_num]);
                                sprintf(dst_list, "%s>%d", dst_list, p_client_tab->itune[p_client_tab->detail_info_num]);
                        }
                        else
                                sprintf(dst_list, "%s>%s", dst_list, db_itune);

			if(nvp != NULL) {
				strcat(dst_list, "<");
				strcat(dst_list, nvp);
			}
			nmp_client_list = strdup(dst_list);
			free(dst_list);

NMP_DEBUG_M("*** Update nmp_client_list:\n%s\n", nmp_client_list);
		        nvram_set("nmp_client_list", nmp_client_list);
			break;
		}
	}
	else { //new client
NMP_DEBUG_M("new client: %d-%s,%s,%d\n",p_client_tab->detail_info_num,
	        new_mac,
        	p_client_tab->device_name[p_client_tab->detail_info_num],
	        p_client_tab->type[p_client_tab->detail_info_num]);

		sprintf(nmp_client_list,"%s<%s>>%s>%d>%d>%d>%d", nmp_client_list, 
		new_mac,
	        p_client_tab->device_name[p_client_tab->detail_info_num],
       	 	p_client_tab->type[p_client_tab->detail_info_num],
		p_client_tab->http[p_client_tab->detail_info_num],
		p_client_tab->printer[p_client_tab->detail_info_num],
		p_client_tab->itune[p_client_tab->detail_info_num]
		);
		nvram_set("nmp_client_list", nmp_client_list);	
	}

}

void
reset_db() {
NMP_DEBUG("RESET DB!!!\n");
	commit_no = 0;
	nvram_set("nmp_client_list", "");
	nvram_commit();
	*nmp_client_list = NULL;
	nmp_client_list = strdup(nvram_get("nmp_client_list"));
	refresh_sig();
}
#endif

#if 0 //Bonjour
#define TypeBufferSize 80
static int num_printed;
static void DNSSD_API browse_reply(DNSServiceRef sdref, const DNSServiceFlags flags, uint32_t ifIndex, DNSServiceErrorType errorCode,
        const char *replyName, const char *replyType, const char *replyDomain, void *context)
        {
        char *op = (flags & kDNSServiceFlagsAdd) ? "Add" : "Rmv";
        (void)sdref;        // Unused
        (void)context;      // Unused
NMP_DEBUG("Browse Reply: %d\n", num_printed);
printf("Browse Reply: %d\n", num_printed);
        if (num_printed++ == 0) NMP_DEBUG("Timestamp     A/R Flags if %-25s %-25s %s\n", "Domain", "Service Type", "Instance Name");
        //printtimestamp();
        if (errorCode) NMP_DEBUG("Error code %d\n", errorCode);
        else NMP_DEBUG("%s%6X%3d %-25s %-25s %s\n", op, flags, ifIndex, replyDomain, replyType, replyName);
        if (!(flags & kDNSServiceFlagsMoreComing)) fflush(stdout);

        // To test selective cancellation of operations of shared sockets,
        // cancel the current operation when we've got a multiple of five results
        //if (operation == 'S' && num_printed % 5 == 0) DNSServiceRefDeallocate(sdref);
        }
#endif

/******************************************/
int main(int argc, char *argv[])
{
	int arp_sockfd, arp_getlen, i;
	int send_count=0, file_num=0;
	struct sockaddr_in router_addr, device_addr;
	char router_ipaddr[17], router_mac[17], buffer[ARP_BUFFER_SIZE];
	unsigned char scan_ipaddr[4]; // scan ip
	FILE *fp_ip;
	fd_set rfds;
        ARP_HEADER * arp_ptr;
        struct timeval tv1, tv2, arp_timeout;
	int shm_client_detail_info_id;
	int ip_dup, mac_dup, real_num;
	int lock;

        FILE *fp = fopen("/var/run/networkmap.pid", "w");
        if(fp != NULL){
                fprintf(fp, "%d", getpid());
                fclose(fp);
        }
	#ifdef DEBUG
		eval("rm", "/var/client*");
	#endif

	//Initial client tables
	lock = file_lock("networkmap");
	shm_client_detail_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
        if (shm_client_detail_info_id == -1){
    	    fprintf(stderr,"shmget failed\n");
	    file_unlock(lock);
            exit(1);
    	}

	CLIENT_DETAIL_INFO_TABLE *p_client_detail_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shmat(shm_client_detail_info_id,(void *) 0,0);
	//Reset shared memory
	memset(p_client_detail_info_tab, 0x00, sizeof(CLIENT_DETAIL_INFO_TABLE));
	p_client_detail_info_tab->ip_mac_num = 0;
	p_client_detail_info_tab->detail_info_num = 0;
	file_unlock(lock);

	#ifdef NMP_DB
		nmp_client_list = strdup(nvram_safe_get("nmp_client_list"));
		NMP_DEBUG_M("NMP Client:\n%s\n", nmp_client_list);
		signal(SIGUSR2, reset_db);
	#endif

	//Get Router's IP/Mac
	strcpy(router_ipaddr, nvram_safe_get("lan_ipaddr"));
#ifdef RTCONFIG_RGMII_BRCM5301X
	strcpy(router_mac, nvram_safe_get("et1macaddr"));
#else
	strcpy(router_mac, nvram_safe_get("et0macaddr"));
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

#if 0//Call Bonjour
static uint32_t opinterface = kDNSServiceInterfaceIndexAny;
static DNSServiceRef client    = NULL;
DNSServiceErrorType err;
char mdns_buf[TypeBufferSize], *typ, *dom;

dom = "";
typ = "_services._dns-sd._udp";
NMP_DEBUG("opinterface=%d,typ=%s,dom=%s,\n",opinterface, typ, dom);
err = DNSServiceBrowse(&client, 0, opinterface, typ, dom, browse_reply, NULL);
NMP_DEBUG("err= %ld\n", (long int)err);
if (!client || err != kDNSServiceErr_NoError) { NMP_DEBUG("DNSService call failed %ld\n", (long int)err); return (-1); }
return 0;
#endif

        // create UDP socket and bind to "br0" to get ARP packet//
	arp_sockfd = create_socket(INTERFACE);

        if(arp_sockfd < 0)
                perror("create socket ERR:");
	else {
	        arp_timeout.tv_sec = 0;
        	arp_timeout.tv_usec = 50000;
		setsockopt(arp_sockfd, SOL_SOCKET, SO_RCVTIMEO, &arp_timeout, sizeof(arp_timeout));//set receive timeout
		dst_sockll = src_sockll; //Copy sockaddr info to dst
		memset(dst_sockll.sll_addr, -1, sizeof(dst_sockll.sll_addr)); // set dmac= FF:FF:FF:FF:FF:FF
	}

        while(1)//main while loop
        {
	    while(1) { //full scan and reflush recv buffer
		fullscan:
                if(networkmap_fullscan == 1) { //Scan all IP address in the subnetwork
		    if(scan_count == 0) { 
			asusdiscovery();	//find asus device
			// (re)-start from the begining
			memset(scan_ipaddr, 0x00, 4);
			memcpy(scan_ipaddr, &router_addr.sin_addr, 3);
	                arp_timeout.tv_sec = 0;
        	        arp_timeout.tv_usec = 50000;
                	setsockopt(arp_sockfd, SOL_SOCKET, SO_RCVTIMEO, &arp_timeout, sizeof(arp_timeout));//set receive timeout
			NMP_DEBUG("Starting full scan!\n");

                        //reset client tables
			lock = file_lock("networkmap");
        		memset(p_client_detail_info_tab, 0x00, sizeof(CLIENT_DETAIL_INFO_TABLE));
        		p_client_detail_info_tab->detail_info_num = 0;
			file_unlock(lock);
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

		memset(buffer, 0, ARP_BUFFER_SIZE);
		arp_getlen=recvfrom(arp_sockfd, buffer, ARP_BUFFER_SIZE, 0, NULL, NULL);

	   	if(arp_getlen == -1) {
			if( scan_count<255)
				goto fullscan;
			else
				break;
		}
		else {
		    arp_ptr = (ARP_HEADER*)(buffer);
                    NMP_DEBUG("*Receive an ARP Packet from: %d.%d.%d.%d, len:%d\n",
				(int *)arp_ptr->source_ipaddr[0],(int *)arp_ptr->source_ipaddr[1],
				(int *)arp_ptr->source_ipaddr[2],(int *)arp_ptr->source_ipaddr[3],
				arp_getlen);

		    //Check ARP packet if source ip and router ip at the same network
                    if( !memcmp(my_ipaddr, arp_ptr->source_ipaddr, 3) ) {

			swapbytes16(arp_ptr->message_type);

			//ARP Response packet to router
			if( arp_ptr->message_type == 0x02 &&   		       	// ARP response
                       	    memcmp(arp_ptr->dest_ipaddr, my_ipaddr, 4) == 0 && 	// dest IP
                       	    memcmp(arp_ptr->dest_hwaddr, my_hwaddr, 6) == 0) 	// dest MAC
			{
			    //NMP_DEBUG("   It's an ARP Response to Router!\n");
                            NMP_DEBUG("*RCV %d.%d.%d.%d-%02X:%02X:%02X:%02X:%02X:%02X\n",
                            (int *)arp_ptr->source_ipaddr[0],(int *)arp_ptr->source_ipaddr[1],
                            (int *)arp_ptr->source_ipaddr[2],(int *)arp_ptr->source_ipaddr[3],
                            arp_ptr->source_hwaddr[0],arp_ptr->source_hwaddr[1],
                            arp_ptr->source_hwaddr[2],arp_ptr->source_hwaddr[3],
                            arp_ptr->source_hwaddr[4],arp_ptr->source_hwaddr[5]);

                            for(i=0; i<p_client_detail_info_tab->ip_mac_num; i++) {
				ip_dup = memcmp(p_client_detail_info_tab->ip_addr[i], arp_ptr->source_ipaddr, 4);
                                mac_dup = memcmp(p_client_detail_info_tab->mac_addr[i], arp_ptr->source_hwaddr, 6);

				if((ip_dup == 0) && (mac_dup == 0))
					break;
				else if((ip_dup != 0) && (mac_dup != 0))
					continue;

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
					file_unlock(lock);
					real_num = p_client_detail_info_tab->detail_info_num;
					p_client_detail_info_tab->detail_info_num = i;
                                        #ifdef NMP_DB
                                                check_nmp_db(p_client_detail_info_tab, i);
                                        #endif
					FindAllApp(my_ipaddr, p_client_detail_info_tab);
					FindHostname(p_client_detail_info_tab);
					p_client_detail_info_tab->detail_info_num = real_num;
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
                                #ifdef NMP_DB
                                        check_nmp_db(p_client_detail_info_tab, i);
                                #endif
                                p_client_detail_info_tab->ip_mac_num++;
				file_unlock(lock);

			    #ifdef DEBUG  //Write client info to file
                		fp_ip=fopen("/var/client_ip_mac.txt", "a");
                		if (fp_ip==NULL) {
                    		NMP_DEBUG("File Open Error!\n");
                		}
                		else {
                        	NMP_DEBUG_M("Fill: %d-> %d.%d\n", i,p_client_detail_info_tab->ip_addr[i][2],p_client_detail_info_tab->ip_addr[i][3]);

                        	fprintf(fp_ip, "%d.%d.%d.%d,%02X:%02X:%02X:%02X:%02X:%02X\n",
                      	 	    p_client_detail_info_tab->ip_addr[i][0],p_client_detail_info_tab->ip_addr[i][1],
                       	 	    p_client_detail_info_tab->ip_addr[i][2],p_client_detail_info_tab->ip_addr[i][3],
                        	    p_client_detail_info_tab->mac_addr[i][0],p_client_detail_info_tab->mac_addr[i][1],
                            	    p_client_detail_info_tab->mac_addr[i][2],p_client_detail_info_tab->mac_addr[i][3],
                       		    p_client_detail_info_tab->mac_addr[i][4],p_client_detail_info_tab->mac_addr[i][5]);
                    		}
                    		fclose(fp_ip);
			    #endif
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

	    //Find All Application of clients
	    //NMP_DEBUG("\ndetail ? ip : %d ? %d\n\n", p_client_detail_info_tab->detail_info_num, p_client_detail_info_tab->ip_mac_num);
	    if(p_client_detail_info_tab->detail_info_num < p_client_detail_info_tab->ip_mac_num) {
		nvram_set("networkmap_status", "1");
		FindAllApp(my_ipaddr, p_client_detail_info_tab);
		FindHostname(p_client_detail_info_tab);

		#ifdef DEBUG //Fill client detail info table
                fp_ip=fopen("/var/client_detail_info.txt", "a");
                if (fp_ip==NULL) {
                        NMP_DEBUG("File Open Error!\n");
                }
                else {
                        fprintf(fp_ip, "%s,%d,%d,%d,%d\n",
                                p_client_detail_info_tab->device_name[p_client_detail_info_tab->detail_info_num], 
				p_client_detail_info_tab->type[p_client_detail_info_tab->detail_info_num], 
				p_client_detail_info_tab->http[p_client_detail_info_tab->detail_info_num],
                                p_client_detail_info_tab->printer[p_client_detail_info_tab->detail_info_num], 
				p_client_detail_info_tab->itune[p_client_detail_info_tab->detail_info_num]);
                        fclose(fp_ip);
                }
		#endif
		#ifdef NMP_DB
			write_to_nvram(p_client_detail_info_tab);
		#endif
		p_client_detail_info_tab->detail_info_num++;
	    }
	    #ifdef NMP_DB
	    else {
		NMP_DEBUG_M("commit_no, cli_no, updated: %d, %d, %d\n", 
		commit_no, p_client_detail_info_tab->detail_info_num, client_updated);
		if( (commit_no != p_client_detail_info_tab->detail_info_num) || client_updated ) {
			NMP_DEBUG("Commit nmp client list\n");
			nvram_commit();
		    	commit_no = p_client_detail_info_tab->detail_info_num;
			client_updated = 0;
		}
	    }
	    #endif

	    if(p_client_detail_info_tab->detail_info_num == p_client_detail_info_tab->ip_mac_num)
		nvram_set("networkmap_status", "0");    // Done scanning and resolving
	} //End of main while loop
	close(arp_sockfd);
	return 0;
}
