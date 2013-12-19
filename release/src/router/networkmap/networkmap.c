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

unsigned char my_hwaddr[6];
unsigned char my_ipaddr[4];
unsigned char broadcast_hwaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned char refresh_ip_list[255][4];
int networkmap_fullscan;
int refresh_exist_table = 0, scan_count=0;

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
static void refresh_sig(int sig)
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

/******************************************/
int main()
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

	//Get Router's IP/Mac
	strcpy(router_ipaddr, nvram_safe_get("lan_ipaddr"));
	strcpy(router_mac, nvram_safe_get("et0macaddr"));
        inet_aton(router_ipaddr, &router_addr.sin_addr);
        memcpy(my_ipaddr,  &router_addr.sin_addr, 4);

	//Prepare scan 
        memset(scan_ipaddr, 0x00, 4);
        memcpy(scan_ipaddr, &router_addr.sin_addr, 3);
	networkmap_fullscan = 1;
	nvram_set("networkmap_fullscan", "1");

	if (strlen(router_mac)!=0) ether_atoe(router_mac, my_hwaddr);

	signal(SIGUSR1, refresh_sig); //catch UI refresh signal

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
		    else if(scan_count>=255) { //Scan completed
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
					FindAllApp(my_ipaddr, p_client_detail_info_tab);
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
                                p_client_detail_info_tab->ip_mac_num++;
				file_unlock(lock);

			    #ifdef DEBUG  //Write client info to file
                		fp_ip=fopen("/var/client_ip_mac.txt", "a");
                		if (fp_ip==NULL) {
                    		printf("File Open Error!\n");
                		}
                		else {
                        	printf("Fill: %d-> %d.%d", i,p_client_detail_info_tab->ip_addr[i][2],p_client_detail_info_tab->ip_addr[i][3]);

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
                        printf("File Open Error!\n");
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
		p_client_detail_info_tab->detail_info_num++;
	    }
	    if(p_client_detail_info_tab->detail_info_num == p_client_detail_info_tab->ip_mac_num)
		nvram_set("networkmap_status", "0");    // Done scanning and resolving
	} //End of main while loop
	close(arp_sockfd);
	return 0;
}
