#include "pc_block.h"

// MAC address in list and not in time period -> redirect to blocking page
void config_blocking_redirect(FILE *fp){

	pc_s *pc_list = NULL, *enabled_list = NULL, *follow_pc;
	pc_event_s *follow_e;
	char *lan_if = nvram_safe_get("lan_ifname");
	char *lan_ip = nvram_safe_get("lan_ipaddr");
	char *lan_mask = nvram_safe_get("lan_netmask");
	char *datestr[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	char *fftype;
	int i;

	fftype = "PCREDIRECT";

	follow_pc = get_all_pc_list(&pc_list);
	if(follow_pc == NULL){
                _dprintf("Couldn't get the Parental-control rules correctly!\n");
                return;
        }

	follow_pc = match_enabled_pc_list(pc_list, &enabled_list, 1);
        free_pc_list(&pc_list);
        if(follow_pc == NULL){
                _dprintf("Couldn't get the enabled rules of Parental-control correctly!\n");
                return;
        }

	for(follow_pc = enabled_list; follow_pc != NULL; follow_pc = follow_pc->next){
		fprintf(fp, "-A PREROUTING -i %s -m mac --mac-source %s -j %s\n", lan_if, follow_pc->mac, fftype);	
                for(follow_e = follow_pc->events; follow_e != NULL; follow_e = follow_e->next){
                        if(follow_e->start_day == follow_e->end_day){
                                if(follow_e->start_hour == follow_e->end_hour){ // whole week.
					fprintf(fp, "-A %s -i %s -m mac --mac-source %s -j ACCEPT\n", fftype, lan_if, follow_pc->mac);
                                }
                                else{
					fprintf(fp, "-A %s -i %s -m time", fftype, lan_if);
                                        if(follow_e->start_hour > 0)
                                                fprintf(fp, " --timestart %d:0", follow_e->start_hour);
                                        if(follow_e->end_hour < 24)
                                                fprintf(fp, " --timestop %d:0", follow_e->end_hour);
					fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j ACCEPT\n", datestr[follow_e->start_day], follow_pc->mac);
                                }
                        }
                        else if(follow_e->start_day < follow_e->end_day
                                        || follow_e->end_day == 0
                                        ){ // start_day < end_day.
                                if(follow_e->end_day == 0)
                                        follow_e->end_day += 7;

				// first interval.
				fprintf(fp, "-A %s -i %s -m time", fftype, lan_if);
                                if(follow_e->start_hour > 0)
                                        fprintf(fp, " --timestart %d:0", follow_e->start_hour);
				fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j ACCEPT\n", datestr[follow_e->start_day], follow_pc->mac);

                                // middle interval.
                                if(follow_e->end_day-follow_e->start_day > 1){

					fprintf(fp, "-A %s -i %s -m time" DAYS_PARAM, fftype, lan_if);
                                        for(i = follow_e->start_day+1; i < follow_e->end_day; ++i)
                                                fprintf(fp, "%s%s", (i == follow_e->start_day+1)?"":",", datestr[i]);
					fprintf(fp, " -m mac --mac-source %s -j ACCEPT\n", follow_pc->mac);
                                }

                                // end interval.
                                if(follow_e->end_hour > 0){
                                        fprintf(fp, "-A %s -i %s -m time", fftype, lan_if);
					if(follow_e->end_hour < 24)
                                                fprintf(fp, " --timestop %d:0", follow_e->end_hour);
					fprintf(fp, DAYS_PARAM "%s -m mac --mac-source %s -j ACCEPT\n", datestr[follow_e->end_day], follow_pc->mac);
                                }
                        }
                        else
                                ; // Don't care "start_day > end_day".
                }

                // MAC address in list and not in time period -> Redirect to blocking page.
		fprintf(fp, "-A %s -i %s ! -d %s/%s -p tcp --dport 80 -m mac --mac-source %s -j DNAT --to-destination %s:%s\n", fftype, lan_if,lan_ip, lan_mask, follow_pc->mac, lan_ip, DFT_SERV_PORT);
        }

        free_pc_list(&enabled_list);
}

void pc_block_exit(int signo){
	
	csprintf("pc_block: safeexit");    
	signal(SIGTERM, SIG_IGN);

	FD_ZERO(&allset);
	close(serv_socket);                                       

	int i;
	for(i = 0; i < max_fd; ++i){
		close(i);
	}
	
	remove(PC_BLOCK_PID_FILE);
	exit(0);
}

static void close_socket(int sockfd){
	
	close(sockfd);
	FD_CLR(sockfd, &allset);
	client[fd_idx] = -1;
}

char *arp_mac(struct in_addr sin_addr){
	static char mac_address[30];
	unsigned char *ptr;
	int s_arp;
	struct arpreq areq;
	struct sockaddr_in *sin;
	
	memset((caddr_t)&areq, 0, sizeof(areq));
	sin = (struct sockaddr_in *)&areq.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr = sin_addr;

	if((s_arp = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("socket() failed");
		return NULL;
	}
	
	strcpy(areq.arp_dev, "br0");
	if(ioctl(s_arp, SIOCGARP, (caddr_t)&areq) < 0){
		perror("SIOCGARP");
		return NULL;
	}
	close(s_arp);

	if(areq.arp_flags & ATF_COM){
		ptr = (unsigned char *) areq.arp_ha.sa_data;
		sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
	}
	else{
		_dprintf("## pc_block: ARP failed ##\n");
		return NULL;
	}	

	return mac_address;
}

void handle_req(int sockfd, char *buf, char *mac){

	char page[2*MAX_LEN];
	char timebuf[100];
	char dut_addr[64];
	time_t now;

        if(!strncmp(buf, "GET /", 5)){
		memset(page, 0, sizeof(page));
		memset(dut_addr, 0, sizeof(dut_addr));
		now = time(NULL);
		strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
		sprintf(page, "%s%s%s%s%s%s", page, "HTTP/1.0 302 Moved Temporarily\r\n", "Server: pc_block\r\n", "Date: ", timebuf, "\r\n");
		strcpy(dut_addr, nvram_safe_get("lan_ipaddr"));

		sprintf(page, "%s%s%s%s%s%s%s", page, "Connection: close\r\n", "Location:http://", dut_addr, "/blocking.asp?mac=", mac,"\r\nContent-Type: text/plain\r\n\r\n<html></html>\r\n");
		write(sockfd, page, strlen(page));
                close_socket(sockfd);
        }
        else
                close_socket(sockfd);
}

void perform_http_serv(int sockfd, char *mac){

        ssize_t n;
        char buf[MAX_LEN];
        memset(buf, 0, sizeof(buf));
        if((n = read(sockfd, buf, MAX_LEN)) == 0){
                close_socket(sockfd);
                return;
        }
        else if(n < 0){
                perror("pc_block http server");
                return;
        }
        else
                handle_req(sockfd, buf, mac);

}

int pc_block_main(int argc, char *argv[]){

	char* serv_port;	
	int sock_opt;
	struct timeval tval;
	struct sockaddr_in sin;
	struct sockaddr_in client_addr;
	int client_len;
	int ready, max_idx, sockfd, nread;
_dprintf("## pc_block: start ##\n");	
	signal(SIGTERM, pc_block_exit);

	serv_port = DFT_SERV_PORT;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	if((sin.sin_port = htons((u_short)atoi(serv_port))) == 0){
		perror("fail to get service entry");
		exit(0);
	}
	
	if((serv_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		perror("fail to create socket");
		exit(0);
	}
	
	sock_opt = 1;
	if(setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&sock_opt, sizeof(sock_opt)) < 0){
		perror("fail to set socket option: SO_REUSEADDR");
		close(serv_socket);
		exit(0);
	}

	if(bind(serv_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		perror("fail to bind port");
		close(serv_socket);
		exit(0);
	}
	
	if(listen(serv_socket, 10) < 0){
		perror("fail to listen to port");
		close(serv_socket);
		exit(0);
	}
	
	FILE *fp = fopen(PC_BLOCK_PID_FILE, "w");
	if(fp != NULL){
                fprintf(fp, "%d", getpid());
                fclose(fp);
        }
	
	max_fd = serv_socket;
	max_idx = -1;
	client_len = sizeof(client_addr);
	FD_ZERO(&allset);
	FD_SET(serv_socket, &allset);

	for(fd_idx=0; fd_idx<MAX_CONN; ++fd_idx)
		client[fd_idx] = -1;

	for(;;){
		rdset = allset;

		tval.tv_sec = POLL_INTERVAL_SEC;
		tval.tv_usec = 0;

		if((ready = select(max_fd+1, &rdset, NULL, NULL, &tval)) <=0)
			continue;
		if(FD_ISSET(serv_socket, &rdset)){
			if((fd_cur = accept(serv_socket, (struct sockaddr *)&client_addr, (socklen_t *)&client_len)) <=0){
				perror("serv accept");
				continue;
			}
			
			for(fd_idx=0; fd_idx<MAX_CONN; ++fd_idx){
				if(client[fd_idx] < 0){
					client[fd_idx] = fd_cur;
					break;
				}
			}

			if(fd_idx == MAX_CONN){
				csprintf("## pc_block: servs full ##\n");
				close(fd_cur);
				continue;
			}

			FD_SET(fd_cur, &allset);
			if(fd_cur > max_fd)
				max_fd = fd_cur;
			if(fd_idx > max_idx)
				max_idx = fd_idx;
			if(--ready <=0)
				continue;
		}

		for(fd_idx=0; fd_idx<=max_idx; ++fd_idx){
			if((sockfd = client[fd_idx]) < 0)
				continue;
			if(FD_ISSET(sockfd, &rdset)){
				ioctl(sockfd, FIONREAD, &nread);
				if(nread == 0){
					close_socket(sockfd);
					continue;
				}

				if(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0)|O_NONBLOCK) < 0){
					_dprintf("## pc_block: set req[%d] nonblock fail ##\n", sockfd);
					continue;
				}
				fd_cur = sockfd;
				perform_http_serv(sockfd, arp_mac(client_addr.sin_addr));

				if(--ready <=0)
					break;
			}
		}

	}//end loop
}
