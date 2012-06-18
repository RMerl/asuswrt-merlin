//
// add sync between adslate and auto_det
// terminate process may cause unexpected behavior
//


//
// programming note for router SOC
// DO NOT USE fscanf/fprintf
//
#define MAX_LEN 32
#define DETECT_RETRY	1

#include "../msg_define.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <features.h>
#include <bcmnvram.h>
//#include <nvram.h>
#include "dp.h"
//#include "nvram/typedefs.h"

static int m_msqid_to_d=0;
static int m_msqid_from_d=0;
static char time_diff[6];
static char Language[4];
char recv_msg[20];
int AdslAteQuit = 0;

FILE* m_FpDbgMsg = NULL;

void PutMsg(char* msg)
{
	if (m_FpDbgMsg != NULL)
	{
		fputs(msg, m_FpDbgMsg);		
	}
}

void delete_msg_q(void)
{
	// delete msg queue from kernel
	struct msqid_ds stMsgInfo;
	msgctl(m_msqid_from_d, IPC_RMID, &stMsgInfo);
}

void TestAdslAteAskQuit(void)
{
	// if adslate want to quit auto det
	char buf[32];
	strcpy(buf,nvram_safe_get("dsltmp_adslatequit"));
	if (!strcmp(buf,"1")) AdslAteQuit = 1;
}

int recv_ipc(void)
{
	msgbuf receive_buf;
	int infolen;
	int status = 0;

	while (1)
	{
		//
		// check adslate want auto_det closed
		//
		if((infolen=msgrcv(m_msqid_from_d,&receive_buf,MAX_IPC_MSG_BUF,0,0))<0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			else
			{
				printf("recv_ipc: msgrcv err %d\n",errno);
				break;
			}
		}
		else
		{
			//printf("%d",receive_buf.mtype);
			//printf(receive_buf.mtext);
			//printf("\n");
			memset(recv_msg, 0, 20);
			strcpy(recv_msg, receive_buf.mtext);
			status = 1;
			break;
		}
	}

	TestAdslAteAskQuit();
	return status;
}

int stop_auto_det(void)
{
	msgbuf send_buf;
	int status = 0;

	printf("IPC_STOP_AUTO_DET\n");
	send_buf.mtype=IPC_STOP_AUTO_DET;
	strcpy(send_buf.mtext,"stopdet");
	//printf("msg :%s\n",send_buf.mtext);

	if(msgsnd(m_msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
	{
		printf("stop_auto_det: msgsnd fail\n");
	}
	else
	{
		//printf("msgsnd ok\n");
		recv_ipc();
		return 1;
	}

	return status;
}

int link_state(void)
{
	msgbuf send_buf;
	int status = 0;

// for debug
	if(nvram_match("dslx_debug", "1"))
	{
		return 1;
	}
	
	//FILE* fp;
	//fp = fopen("/tmp/adsl_link_state.txt", "w");
	//printf("IPC_LINK_STATE\n");
	send_buf.mtype=IPC_LINK_STATE;
	strcpy(send_buf.mtext,"linksts");
	//printf("msg :%s\n",send_buf.mtext);


	if(msgsnd(m_msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
	{
		printf("link_state: msgsnd fail\n");
	}
	else
	{
		//printf("msgsnd ok\n");
		recv_ipc();
		//if(fp)
		//{
			//fputs(recv_msg, fp);
			//close(fp);
		//}
		//printf("Link: %s\n", recv_msg);
		nvram_set("dsltmp_autodet_state", recv_msg);
		if(!strcmp(recv_msg, "up"))
			return 1;
		else
			return 0;
	}

	return status;
}

int start_auto_det(void)
{
	msgbuf send_buf;
	int status = 0;

	printf("IPC_START_AUTO_DET\n");
	send_buf.mtype=IPC_START_AUTO_DET;
	strcpy(send_buf.mtext,"startdet");
	//printf("msg :%s\n",send_buf.mtext);

	if(msgsnd(m_msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
	{
		printf("start_auto_det: msgsnd fail\n");
	}
	else
	{
		//printf("msgsnd ok\n");
		recv_ipc();
		return status;
	}

	return status;
}

int add_pvc(msgbuf *send_buf)
{
	int status = 0;

	if(msgsnd(m_msqid_to_d,send_buf,MAX_IPC_MSG_BUF,0)<0)
	{
		printf("add_pvc: msgsnd fail\n");
	}
	else
	{
		//printf("msgsnd ok\n");
		recv_ipc();
		return status;
	}

	return status;
}

int del_all_pvc(void)
{
	msgbuf send_buf;
	int status = 0;

	printf("IPC_DEL_ALL_PVC\n");
	send_buf.mtype=IPC_DEL_ALL_PVC;
	strcpy(send_buf.mtext,"delall");
	//printf("msg :%s\n",send_buf.mtext);

	if(msgsnd(m_msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
	{
		printf("del_all_pvc: msgsnd fail\n");
	}
	else
	{
		printf("msgsnd ok\n");
		recv_ipc();
	}

	return status;
}

void detect_ret_to_nvram(DetectPVC *detect_pvc)
{
	char detect_ret[8];
	memset(detect_ret, 0, 8);
	sprintf(detect_ret, "%d", detect_pvc->vpi);
	nvram_set("dsltmp_autodet_vpi", detect_ret);
	sprintf(detect_ret, "%d", detect_pvc->vci);
	nvram_set("dsltmp_autodet_vci", detect_ret);
	sprintf(detect_ret, "%d", detect_pvc->encap);
	nvram_set("dsltmp_autodet_encap", detect_ret);
}

void write_out_auto_det_time(int value)
{
    FILE* fpBoot;
    char tmp2[80];    
    fpBoot = fopen("/tmp/adsl/auto_det_time.txt","wb");
    if (fpBoot != NULL)
    {
    	sprintf(tmp2,"AutoDet:%d\n",value);
        fputs(tmp2,fpBoot);
        fclose(fpBoot);
    }
}

void write_out_det_num(int value0, int value1)
{
    FILE* fpBoot;
    char tmp2[80];    
    fpBoot = fopen("/tmp/adsl/det_num_total.txt","wb");
    if (fpBoot != NULL)
    {
    	sprintf(tmp2,"%d",value0);
        fputs(tmp2,fpBoot);
        fclose(fpBoot);
    }
    fpBoot = fopen("/tmp/adsl/det_num_curr.txt","wb");
    if (fpBoot != NULL)
    {
    	sprintf(tmp2,"%d",value1);
        fputs(tmp2,fpBoot);
        fclose(fpBoot);
    }    
}



void auto_det_main(char* auto_det_list)
{
	char buf[32];
	char full_auto_lst_fn[64];
	msgbuf send_buf;
	time_t start_time,end_time;	
	int dif;
	//msgbuf receive_buf;
	//int infolen;
	FILE* fp;
	unsigned short* pWord;
	int* pInt;
	DetectPVC detect_pvc[8];
	char detect_ret[8];

	int i, ppp_ifindex, count;
	int comma_count;
	char wan_if[9];
	unsigned char *message;
	unsigned long xid = 0;
	fd_set rfds;
	int retval;
	struct timeval tv;
	int len;
	int max_fd;
	PPPoEPacket ppp_packet;
	struct dhcpMessage packet;
	//struct RecvdhcpMessage Recvpacket;
	int ppp_len;
	DetectInfo detect_info[8];
	int total_det = 0;
	int curr_det = 0;	

	FILE* fp2;
	char time_diff[6];
	char Language[4];
	char detect_buf[80], output_buf[80], *head, *tail, field[6];
	int det_index, mode=0, detect_done=0, get_prtcl=0;

	printf("Auto_detect init...\n");
	PutMsg("Auto_detect start...");	

	strcpy(buf,nvram_safe_get("dsltmp_tc_resp_to_d"));
	m_msqid_to_d=atoi(buf);

	if ((m_msqid_from_d=msgget(IPC_PRIVATE,0700))<0)
	{
		printf("msgget err\n");
		return;
	}
	else
	{
		printf("msgget ok\n");
	}

	printf("MSQ_to_daemon : %d\n",m_msqid_to_d);
	printf("MSQ_from_daemon : %d\n",m_msqid_from_d);

	send_buf.mtype=IPC_CLIENT_MSG_Q_ID;
	pInt = (int*)send_buf.mtext;
	*pInt = m_msqid_from_d;
	if(msgsnd(m_msqid_to_d,&send_buf,MAX_IPC_MSG_BUF,0)<0)
	{
		printf("msgsnd fail (IPC_CLIENT_MSG_Q_ID)\n");
		goto delete_msgq_and_quit;
	}
	else
	{
		printf("msgsnd ok (IPC_CLIENT_MSG_Q_ID)\n");
	}

	PutMsg("start check link state");	


	while(1)
	{
		if (AdslAteQuit) break;	
		if(link_state())
			break;
	}

	if (AdslAteQuit) goto delete_msgq_and_quit;		

	PutMsg("link state ok");		

	start_time = time (NULL);

	strcpy(full_auto_lst_fn,"/www/");
	strcat(full_auto_lst_fn,auto_det_list);
	fp2 = fopen(full_auto_lst_fn, "r");
	if(fp2 != NULL)
	{
		while(1)
		{
			char* ret;
			ret = fgets(detect_buf, MAX_LEN, fp2);
			if (ret == NULL) break;
			total_det++;
		}
		fclose(fp2);
	}
	//fp2 = fopen("/www/Detect_list.txt", "r");
        strcpy(full_auto_lst_fn,"/www/");
	strcat(full_auto_lst_fn,auto_det_list);
	fp2 = fopen(full_auto_lst_fn, "r");
                        	

	if(fp2 == NULL)
	{
		printf("Don't detect any client!\n");
	}
	else
	{
		memset(output_buf, 0, MAX_LEN);
		memset(detect_buf, 0, MAX_LEN);
		PutMsg("det_status=detecting");						
		nvram_set("dsltmp_autodet_state", "Detecting");
	Get_Detect_PVC:
		if (AdslAteQuit) goto delete_msgq_and_quit; 	
		printf("Get Detect PVC...\n");
		PutMsg("del all pvc");	
		del_all_pvc();
		det_index = 0;
		count = 0;
		memset(detect_pvc, 0, sizeof(DetectPVC)*8);
		write_out_det_num(total_det, curr_det);		
		while(1)
		{
			if(fgets(detect_buf, MAX_LEN, fp2))
			{
				len = strlen(detect_buf);
//				printf("%d\n",len);
				comma_count = 0;
				for (i=0; i<len; i++)
				{
					if (detect_buf[i] == ',') comma_count++;
				}
				if (comma_count != 3) continue;

				detect_buf[len-1] = ',';
				head = detect_buf;
				for(i = 0; i < 4; ++i)
				{
					tail = strchr(head, ',');
					if(tail != NULL)
					{
						memset(field, 0, 6);
						strncpy(field, head, (tail-head));
					}

					if(i==0)
					{
						detect_pvc[det_index].vpi = atoi(field);
					}
					else if(i==1)
						detect_pvc[det_index].vci = atoi(field);
					else if(i==2)
						detect_pvc[det_index].prtcl = atoi(field);
					else if(i==3)
						detect_pvc[det_index].encap = atoi(field);

					head = tail+1;
				}
				printf("\n%d: %d, %d, %d\n", det_index, detect_pvc[det_index].vpi,
				detect_pvc[det_index].vci, detect_pvc[det_index].prtcl);

				if(det_index==7)
					break;
				det_index++;
			}
			else
			{
				detect_done = 1;
				break;
			}
		}//end of while



#if 1		
		for(i=0; i<8; i++)
		{
			if(detect_pvc[i].prtcl!=0)
			{
				char MsgBuf[80];
				printf("%d: IPC_ADD_PVC %d/%d, %d\n", i, detect_pvc[i].vpi, detect_pvc[i].vci, detect_pvc[i].prtcl);
				sprintf(MsgBuf, "%d: IPC_ADD_PVC %d/%d, %d\n", i, detect_pvc[i].vpi, detect_pvc[i].vci, detect_pvc[i].prtcl);
				PutMsg(MsgBuf);						   
				send_buf.mtype=IPC_ADD_PVC;
				memset(&send_buf.mtext, 0, 20);
				pWord = (unsigned short*)send_buf.mtext;
				*pWord++ = i; // idx (0-7)
				*pWord++ = i+1; // always equal to idx_1
				*pWord++ = detect_pvc[i].vpi;
				*pWord++ = detect_pvc[i].vci;
				*pWord++ = detect_pvc[i].encap;
				if (detect_pvc[i].prtcl == 2) *pWord++ = 1;
				else *pWord++=0;
				//*pWord++ = detect_pvc[i].mode;
				*pWord++ = 0;
				add_pvc(&send_buf);
				//sleep(1);
			}
		}
#endif

		/* Initialize connection info */
		memset(&detect_info, 0, sizeof(DetectInfo)*8);
		for(i=0; i<8; i++)
		{
			detect_info[i].detect_prtcl = detect_pvc[i].prtcl;
			sprintf(wan_if,"eth2.1.%d", i+1);
			if(detect_pvc[i].prtcl==1)
			{
				detect_info[i].conn.discoverySocket = -1;
				detect_info[i].conn.sessionSocket = -1;
				detect_info[i].conn.useHostUniq = 1;
				strcpy(detect_info[i].conn.ifName, wan_if);
				detect_info[i].sock_fd = -1;
			}
			else if(detect_pvc[i].prtcl==2)
			{
				detect_info[i].conn.discoverySocket = -1;
				detect_info[i].conn.sessionSocket = -1;
				detect_info[i].conn.useHostUniq = 1;
				strcpy(detect_info[i].conn.ifName, wan_if);
				detect_info[i].sock_fd = -1;
			}			
			else if(detect_pvc[i].prtcl==3)
			{
				detect_info[i].client_config.foreground= 0;
				detect_info[i].client_config.quit_after_lease= 0;
				detect_info[i].client_config.abort_if_no_lease= 0;
				detect_info[i].client_config.background_if_no_lease= 0;
				SET_STRING(detect_info[i].client_config.interface, wan_if);
				detect_info[i].client_config.pidfile= NULL;
				detect_info[i].client_config.script= DEFAULT_SCRIPT;
				detect_info[i].client_config.clientid= NULL;
				detect_info[i].client_config.hostname= NULL;
				detect_info[i].client_config.ifindex= 0;
				memset(detect_info[i].client_config.arp, '\0', 6);
				detect_info[i].client_config.clientid = malloc(6 + 3);
				detect_info[i].client_config.clientid[OPT_CODE] = DHCP_CLIENT_ID;
				detect_info[i].client_config.clientid[OPT_LEN] = 7;
				detect_info[i].client_config.clientid[OPT_DATA] = 1;
				memcpy(detect_info[i].client_config.clientid + 3, detect_info[i].client_config.arp, 6);
			}

		}
		//state = INIT_SELECTING;
		//change_mode(LISTEN_RAW);
		for(i=0; i<8; i++)	 /*init socket */
		{
			printf("PVC %d init socket... %d\n", i, detect_info[i].detect_prtcl);
			if(detect_info[i].detect_prtcl==0)//None
				break;
			else if(detect_info[i].detect_prtcl==1||detect_info[i].detect_prtcl==2)  //PPPoE,PPPoA
			{
//printf("PPPOE, IF: %s\n", detect_info[i].conn.ifName);
				if(read_interface(detect_info[i].conn.ifName, &ppp_ifindex, NULL, detect_info[i].conn.myEth) < 0)
					printf( "read interface error!\n");

				if (detect_info[i].detect_prtcl==1)
				{
					if((detect_info[i].conn.discoverySocket = openInterface(detect_info[i].conn.ifName, Eth_PPPOE_Discovery, detect_info[i].conn.myEth)) < 0)
					{
						printf("open interface fail [%d]\n", detect_info[i].conn.discoverySocket);
						detect_info[i].detect_prtcl=0;
					}
				}
				else
				{
					// pppoa
					if((detect_info[i].conn.discoverySocket = openInterface(detect_info[i].conn.ifName, Eth_PPPOE_Session, detect_info[i].conn.myEth)) < 0)
					{
						printf("open interface fail [%d]\n", detect_info[i].conn.discoverySocket);
						detect_info[i].detect_prtcl=0;
					}				
				}
				detect_info[i].sock_fd = detect_info[i].conn.discoverySocket;
			}
			else if(detect_info[i].detect_prtcl==3)   //Dynamic IP
			{
//printf("DHCP, IF: %s\n", detect_info[i].client_config.interface);
				if(read_interface(detect_info[i].client_config.interface, &detect_info[i].client_config.ifindex, NULL, detect_info[i].client_config.arp) < 0)
					printf( "read interface error!\n");

				detect_info[i].sock_fd = openInterface(detect_info[i].client_config.interface, 0x0800, detect_info[i].client_config.arp);
			}
			max_fd = detect_info[i].sock_fd;
		}


		while( count < DETECT_RETRY )
		{
			FD_ZERO(&rfds);

			for(i=0; i<8; i++)
			{
				if(detect_info[i].detect_prtcl==1)
				{
					FD_SET(detect_info[i].sock_fd, &rfds);
					sendPADI(&detect_info[i].conn);
				}
				else if(detect_info[i].detect_prtcl==2)
				{
//					printf("pppoa\n");
					FD_SET(detect_info[i].sock_fd, &rfds);
					sendPADI_pppoa(&detect_info[i].conn);
				}				
				else if(detect_info[i].detect_prtcl==3)
				{
					FD_SET(detect_info[i].sock_fd, &rfds);
					detect_info[i].client_config.xid = random_xid();
					send_dhcp_discover(&detect_info[i].client_config);
				}
			}
			tv.tv_sec = 2;
			tv.tv_usec = 0;
			retval = select(max_fd + 1, &rfds, NULL, NULL, &tv);

			for(i=0; i<8; i++)
			{
				if(FD_ISSET(detect_info[i].sock_fd, &rfds))
				{
					if(detect_info[i].detect_prtcl == 1)
					{
						receivePacket(detect_info[i].conn.discoverySocket, &ppp_packet, &ppp_len);
						{
							char DumpMsgBuf[100];
							char HexBuf[16];
							unsigned char* pByte;
							int i;
							sprintf(DumpMsgBuf,"ppp_len:%d\n",ppp_len);							
							PutMsg(DumpMsgBuf);							
							pByte = (unsigned char*)&ppp_packet;
							for (i=0; i<ppp_len; i++)
							{
								if ((i % 16) == 0)
								{
									if (i != 0) PutMsg(DumpMsgBuf);								
									strcpy(DumpMsgBuf, "");
								}
								sprintf(HexBuf, "%02x ",*pByte++);
								strcat(DumpMsgBuf, HexBuf);
							}
							if (strlen(DumpMsgBuf) > 0) PutMsg(DumpMsgBuf);
							sprintf(DumpMsgBuf,"ppp_packet_code:%d",ppp_packet.code);							
							PutMsg(DumpMsgBuf);
						}						
						if (ppp_packet.code == CODE_PADO)
						{
							char MsgBuf[80];
							sprintf(MsgBuf, "--- PVC %d: Got the PPPoE! ---\n", i);							
							PutMsg(MsgBuf);
							printf(MsgBuf);
							nvram_set("dsltmp_autodet_state", "pppoe");
							detect_ret_to_nvram(&detect_pvc[i]);
							get_prtcl = 1;
							count = DETECT_RETRY;
							break;
						}
					}
					else if(detect_info[i].detect_prtcl == 2)
					{
						receivePacket(detect_info[i].conn.discoverySocket, &ppp_packet, &ppp_len);
						{
							char DumpMsgBuf[100];
							char HexBuf[16];
							unsigned char* pByte;
							int i;
							sprintf(DumpMsgBuf,"ppp_len:%d\n",ppp_len);							
							PutMsg(DumpMsgBuf);							
							pByte = (unsigned char*)&ppp_packet;
							for (i=0; i<ppp_len; i++)
							{
								if ((i % 16) == 0)
								{
									if (i != 0) PutMsg(DumpMsgBuf);								
									strcpy(DumpMsgBuf, "");
								}
								sprintf(HexBuf, "%02x ",*pByte++);
								strcat(DumpMsgBuf, HexBuf);
							}
							if (strlen(DumpMsgBuf) > 0) PutMsg(DumpMsgBuf);
							sprintf(DumpMsgBuf,"ppp_packet_code:%d",ppp_packet.code);							
							PutMsg(DumpMsgBuf);
						}
						if (ppp_packet.code == CODE_SESS)
						{
							char MsgBuf[80];
							sprintf(MsgBuf, "--- PVC %d: Got the PPPoA! ---\n", i);							
							PutMsg(MsgBuf);
							printf(MsgBuf);							
							nvram_set("dsltmp_autodet_state", "pppoa");
							detect_ret_to_nvram(&detect_pvc[i]);
							get_prtcl = 1;
							count = DETECT_RETRY;
							break;
						}
					}					
					else if(detect_info[i].detect_prtcl == 3)
					{
						len = get_packet(&packet, detect_info[i].sock_fd);
//printf("Get DHCP response len= %d\n",len);
						if (len == -1 && errno != EINTR)
						{
							printf("--- dhcp EINTR ---\n");
							PutMsg("dhcp EINTR");							
						}
						else if (len == -2)
						{
							printf("dhcp wrong - magic\n");						
							PutMsg("dhcp magic");							
						}
						else if (packet.xid != detect_info[i].client_config.xid)
						{
							printf("dhcp wrong - xid\n");	
							PutMsg("dhcp xid");							
						}
						else
						{
							message = get_option(&packet, DHCP_MESSAGE_TYPE);
							if (message == NULL)
							{
								printf("dhcp wrong - option\n");
								PutMsg("dhcp option"); 						
							}
							if (*message == DHCPOFFER)
							{
								char MsgBuf[80];
								sprintf(MsgBuf, "--- PVC %d: Got the DHCP! ---\n", i);							
								PutMsg(MsgBuf);
								nvram_set("dsltmp_autodet_state", "dhcp");
								detect_ret_to_nvram(&detect_pvc[i]);
								get_prtcl = 1;
								count = DETECT_RETRY;
								break;
							}
						}

					}
				}
			}

			count++;
		}

		FD_ZERO(&rfds);
		for(i=0; i<8; i++)
		{
			if(detect_info[i].sock_fd>0)
			{
				close(detect_info[i].sock_fd);
			}
		}

		if(!detect_done&&!get_prtcl)
		{
			curr_det+=8;
			if (curr_det>total_det) curr_det = total_det;
			write_out_det_num(total_det, curr_det);	
			goto Get_Detect_PVC;
		}

		if(!get_prtcl)
		{
			PutMsg("det_status=fail");						
			nvram_set("dsltmp_autodet_state", "Fail");
		}

		close(fp2);
	}//endof open file if

delete_msgq_and_quit:
	stop_auto_det();
	delete_msg_q();

	end_time = time (NULL);
	dif = end_time-start_time;
	write_out_auto_det_time(dif);	

	PutMsg("exit auto_det");						

	printf("EXIT auto_det\n");
}


int main(int argc, char* argv[])
{
	// ate want auto detection stops?
	int AdslReady = 0;
	int WaitAdslCnt;
	char AutoDetListFn[64];
	char buf[32];

	TestAdslAteAskQuit();
	if (AdslAteQuit) {
		printf("ADSLATE WANT AUTO_DET QUIT\n");
		return; 
	}

	// wait tp_init
	for (WaitAdslCnt = 0; WaitAdslCnt<120; WaitAdslCnt++)
	{
		strcpy(buf,nvram_safe_get("dsltmp_tcbootup"));
		
		if (!strcmp(buf,"1"))
		{
			AdslReady = 1;
			break;
		}
//		printf("%d\n",WaitAdslCnt);
		usleep(1000*1000*1);
		TestAdslAteAskQuit();
		if (AdslAteQuit) return;			
	}
	if (AdslReady) 
	{
//		printf("ADSL OK\n");
	}
	else
	{
		printf("!!! ADSL FAIL !!!\n");
		return;
	}

	
	if (argc == 2 && !strcmp(argv[1],"BR"))
	{
		strcpy(AutoDetListFn, "Detect_list_BR.txt");
	}
	else
	{
		strcpy(AutoDetListFn, "Detect_list.txt");
	}
	printf(argv[1]);
	printf(",");
	printf(AutoDetListFn);
	printf("\n");
	
	m_FpDbgMsg = fopen("/tmp/adsl/auto_det.txt","w");
	auto_det_main(AutoDetListFn);
	if (m_FpDbgMsg != NULL) fclose(m_FpDbgMsg);		
	return 0;
}
