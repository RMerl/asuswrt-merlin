

#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <linux/if.h>			/* for IFNAMSIZ and co... */
#include <linux/wireless.h>

#include "rtdot1x.h"
#include "eloop.h"
#include "ieee802_1x.h"
#include "eapol_sm.h"
#include "ap.h"
#include "sta_info.h"
#include "radius_client.h"
#include "config.h"

//#define RT2860AP_SYSTEM_PATH   "/etc/Wireless/RT2860AP/RT2860AP.dat"


struct hapd_interfaces {
	int count;
	rtapd **rtapd;
};

u32    RTDebugLevel = RT_DEBUG_ERROR;

/*
	========================================================================
	
	Routine Description:
		Compare two memory block

	Arguments:
		Adapter						Pointer to our adapter

	Return Value:
		0:			memory is equal
		1:			pSrc1 memory is larger
		2:			pSrc2 memory is larger

	Note:
		
	========================================================================
*/
u16	RTMPCompareMemory(void *pSrc1,void *pSrc2, u16 Length)
{
	char *pMem1;
	char *pMem2;
	u16	Index = 0;

	pMem1 = (char*) pSrc1;
	pMem2 = (char*) pSrc2;

	for (Index = 0; Index < Length; Index++)
	{
		if (pMem1[Index] > pMem2[Index])
			return (1);
		else if (pMem1[Index] < pMem2[Index])
			return (2);
	}

	// Equal
	return (0);
}

int RT_ioctl(
		int 			sid, 
		int 			param, 
		char  			*data, 
		int 			data_len, 
		char 			*prefix_name, 
		unsigned char 	apidx, 
		int 			flags)
{
    //char			name[12];
    int				ret = 1;
    struct iwreq	wrq;

    //sprintf(name, "ra%d", apidx);
    //name[3] = '\0';

    //strcpy(wrq.ifr_name, name);
	sprintf(wrq.ifr_name, "%s%d", prefix_name, apidx);

    wrq.u.data.flags = flags;
	wrq.u.data.length = data_len;
    wrq.u.data.pointer = (caddr_t) data;

    ret = ioctl(sid, param, &wrq);	
    
    return ret;
}

void dot1x_set_IdleTimeoutAction(
		rtapd *rtapd,
		struct sta_info *sta,
		u32		idle_timeout)
{
	DOT1X_IDLE_TIMEOUT dot1x_idle_time;

	memset(&dot1x_idle_time, 0, sizeof(DOT1X_IDLE_TIMEOUT));

	memcpy(dot1x_idle_time.StaAddr, sta->addr, MAC_ADDR_LEN);
	
	dot1x_idle_time.idle_timeout = 
		((idle_timeout < DEFAULT_IDLE_INTERVAL) ? DEFAULT_IDLE_INTERVAL : idle_timeout);

	if (RT_ioctl(rtapd->ioctl_sock, 
				 RT_PRIV_IOCTL, 
				 (char *)&dot1x_idle_time, 
				 sizeof(DOT1X_IDLE_TIMEOUT), 
				 rtapd->prefix_wlan_name, sta->ApIdx, 
				 RT_OID_802_DOT1X_IDLE_TIMEOUT))
	{				   
    	DBGPRINT(RT_DEBUG_ERROR,"Failed to RT_OID_802_DOT1X_IDLE_TIMEOUT\n");
    	return;
	}   

}

static void Handle_reload_config(
	rtapd 	*rtapd)
{
	struct rtapd_config *newconf;
#if MULTIPLE_RADIUS
	int i;
#endif

	DBGPRINT(RT_DEBUG_TRACE, "Reloading configuration\n");

	/* create new config */					
	newconf = Config_read(rtapd->ioctl_sock, rtapd->prefix_wlan_name);
	if (newconf == NULL)
    	{
		DBGPRINT(RT_DEBUG_ERROR, "Failed to read new configuration file - continuing with old.\n");
		return;
	}

	/* TODO: update dynamic data based on changed configuration
	 * items (e.g., open/close sockets, remove stations added to
	 * deny list, etc.) */
	Radius_client_flush(rtapd);
	Config_free(rtapd->conf);
	rtapd->conf = newconf;
    	Apd_free_stas(rtapd);

	/* when reStartAP, no need to reallocate sock
    for (i = 0; i < rtapd->conf->SsidNum; i++)
    {
        if (rtapd->sock[i] >= 0)
            close(rtapd->sock[i]);
            
	    rtapd->sock[i] = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	    if (rtapd->sock[i] < 0)
        {
		    perror("socket[PF_PACKET,SOCK_RAW]");
		    return;
	    }
    }*/

#if MULTIPLE_RADIUS
	for (i = 0; i < MAX_MBSSID_NUM; i++)
		rtapd->radius->mbss_auth_serv_sock[i] = -1;
#else
	rtapd->radius->auth_serv_sock = -1;
#endif

    if (Radius_client_init(rtapd))
    {
	    DBGPRINT(RT_DEBUG_ERROR,"RADIUS client initialization failed.\n");
	    return;
    }
#if MULTIPLE_RADIUS
	for (i = 0; i < rtapd->conf->SsidNum; i++)
		DBGPRINT(RT_DEBUG_TRACE, "auth_serv_sock[%d] = %d\n", i, rtapd->radius->mbss_auth_serv_sock[i]);
#else
    DBGPRINT(RT_DEBUG_TRACE,"rtapd->radius->auth_serv_sock = %d\n",rtapd->radius->auth_serv_sock);
#endif
	
}

static void Handle_read(int sock, void *eloop_ctx, void *sock_ctx)
{                              
	rtapd *rtapd = eloop_ctx;
	int len;
	unsigned char buf[3000];
	u8 *sa, *da, *pos, *pos_vlan, apidx=0, isVlanTag=0;
	u16 ethertype,i;
    priv_rec *rec;
    size_t left;
	u8 	RalinkIe[9] = {221, 7, 0x00, 0x0c, 0x43, 0x00, 0x00, 0x00, 0x00}; 

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0)
    {
		perror("recv");
        Handle_term(15,eloop_ctx,sock_ctx);
        return;
	}

	rec = (priv_rec*)buf;
    left = len -sizeof(*rec)+1;
	if (left <= 0)
	{
		DBGPRINT(RT_DEBUG_ERROR," too short recv\n");
		return;
	}
						
    sa = rec->saddr;
	da = rec->daddr;
	ethertype = rec->ethtype[0] << 8;
	ethertype |= rec->ethtype[1];
			
#ifdef ETH_P_VLAN
	if(ethertype == ETH_P_VLAN)
    {
    	pos_vlan = rec->xframe;

        if(left >= 4)
        {
			ethertype = *(pos_vlan+2) << 8;
			ethertype |= *(pos_vlan+3);
		}
			
		if((ethertype == ETH_P_PRE_AUTH) || (ethertype == ETH_P_PAE))
		{
			isVlanTag = 1;
			DBGPRINT(RT_DEBUG_TRACE,"Recv vlan tag for 802.1x. (%02x %02x)\n", *(pos_vlan), *(pos_vlan+1));		
		}
    }
#endif
	
	if ((ethertype == ETH_P_PRE_AUTH) || (ethertype == ETH_P_PAE))	
    {
        // search this packet is coming from which interface
		for (i = 0; i < rtapd->conf->SsidNum; i++)
		{		    
			if (memcmp(da, rtapd->own_addr[i], 6) == 0)
		    {
		        apidx = i;		        
		        break;
		    }
		}
		
		if(i >= rtapd->conf->SsidNum)
		{
	        DBGPRINT(RT_DEBUG_WARN, "Receive unexpected DA (%02x:%02x:%02x:%02x:%02x:%02x)\n",
										MAC2STR(da));
		    return;
		}

		if (ethertype == ETH_P_PRE_AUTH)
		{
			DBGPRINT(RT_DEBUG_TRACE, "Receive WPA2 pre-auth packet for %s%d\n", rtapd->prefix_wlan_name, apidx);
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, "Receive EAP packet for %s%d\n", rtapd->prefix_wlan_name, apidx);
		}
    }
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, "Receive unexpected ethertype 0x%04X!!!\n", ethertype);
		return;
	}

    pos = rec->xframe;
    
    //strip 4 bytes for valn tag
    if(isVlanTag)
    {
    	pos += 4;
    	left -= 4;
	}
    
	/* Check if this is a internal command or not */
	if (left == sizeof(RalinkIe) && 
		RTMPCompareMemory(pos, RalinkIe, 5) == 0)
	{
		u8	icmd = *(pos + 5);
	
		switch(icmd)
		{
			case DOT1X_DISCONNECT_ENTRY:
			{
				struct sta_info *s;

				s = rtapd->sta_hash[STA_HASH(sa)];
				while (s != NULL && memcmp(s->addr, sa, 6) != 0)
				s = s->hnext;

				DBGPRINT(RT_DEBUG_TRACE, "Receive discard-notification form wireless driver.\n");
				if (s)
				{
					DBGPRINT(RT_DEBUG_TRACE,"This station(%02x:%02x:%02x:%02x:%02x:%02x) is removed.\n", MAC2STR(sa));
					Ap_free_sta(rtapd, s);						
				}
				else
				{
					DBGPRINT(RT_DEBUG_INFO, "This station(%02x:%02x:%02x:%02x:%02x:%02x) doesn't exist.\n", MAC2STR(sa));
				}
			}
			break;

			case DOT1X_RELOAD_CONFIG:
				Handle_reload_config(rtapd);
			break;

			default:
				DBGPRINT(RT_DEBUG_ERROR, "Unknown internal command(%d)!!!\n", icmd);
			break;
		}
	}
	else
	{
		/* Process the general EAP packet */
    	ieee802_1x_receive(rtapd, sa, &apidx, pos, left, ethertype, sock);
	}
}

int Apd_init_sockets(rtapd *rtapd)
{
    struct ifreq ifr;
	struct sockaddr_ll addr;
    int i;

	// 1. init ethernet interface socket for pre-auth
	for (i = 0; i < rtapd->conf->num_preauth_if; i++)
	{
		rtapd->eth_sock[i] = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_PRE_AUTH));
	    if (rtapd->eth_sock[i] < 0)
    	{
    	    perror("socket[PF_PACKET,SOCK_RAW](eth_sock)");
			return -1;
    	}

	    if (eloop_register_read_sock(rtapd->eth_sock[i], Handle_read, rtapd, NULL))
    	{
    	    DBGPRINT(RT_DEBUG_ERROR,"Could not register read socket(eth_sock)\n");
			return -1;
    	}
	
    	memset(&ifr, 0, sizeof(ifr));
	    strcpy(ifr.ifr_name, rtapd->conf->preauth_if_name[i]);
    	DBGPRINT(RT_DEBUG_TRACE,"Register pre-auth interface as (%s)\n", ifr.ifr_name);

	    if (ioctl(rtapd->eth_sock[i], SIOCGIFINDEX, &ifr) != 0)
    	{
    	    perror("ioctl(SIOCGIFHWADDR)(eth_sock)");
   	     	return -1;
    	}

    	memset(&addr, 0, sizeof(addr));
		addr.sll_family = AF_PACKET;
		addr.sll_ifindex = ifr.ifr_ifindex;
		if (bind(rtapd->eth_sock[i], (struct sockaddr *) &addr, sizeof(addr)) < 0)
    	{
			perror("bind");
			return -1;
		}
	    DBGPRINT(RT_DEBUG_TRACE,"Pre-auth raw packet socket binding on %s(socknum=%d,ifindex=%d)\n", 
									ifr.ifr_name, rtapd->eth_sock[i], addr.sll_ifindex);
	}

	// 2. init wireless interface socket for EAP negotiation      		
	for (i = 0; i < rtapd->conf->num_eap_if; i++)
	{
		rtapd->wlan_sock[i] = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_PAE));
        
	    if (rtapd->wlan_sock[i] < 0)
        {
            perror("socket[PF_PACKET,SOCK_RAW]");
    		return -1;
        }

	    if (eloop_register_read_sock(rtapd->wlan_sock[i], Handle_read, rtapd, NULL))
        {
            DBGPRINT(RT_DEBUG_ERROR,"Could not register read socket\n");
    		return -1;
        }
	
        memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, rtapd->conf->eap_if_name[i]);
		DBGPRINT(RT_DEBUG_TRACE,"Register EAP interface as (%s)\n", ifr.ifr_name);

	    if (ioctl(rtapd->wlan_sock[i], SIOCGIFINDEX, &ifr) != 0)
        {
            perror("ioctl(SIOCGIFHWADDR)");
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
    	addr.sll_family = AF_PACKET;
    	addr.sll_ifindex = ifr.ifr_ifindex;
	    if (bind(rtapd->wlan_sock[i], (struct sockaddr *) &addr, sizeof(addr)) < 0)
        {
    		perror("bind");
    		return -1;
    	}
	    DBGPRINT(RT_DEBUG_TRACE, "EAP raw packet socket binding on %s (socknum=%d,ifindex=%d)\n", 
									ifr.ifr_name, rtapd->wlan_sock[i], addr.sll_ifindex);
	}

    
	// 3. Get wireless interface MAC address
    for(i = 0; i < rtapd->conf->SsidNum; i++)
    {
		int s = -1;
	
		s = socket(AF_INET, SOCK_DGRAM, 0); 

		if (s < 0)
        {
            perror("socket[AF_INET,SOCK_DGRAM]");
    		return -1;
        }
    
    	memset(&ifr, 0, sizeof(ifr));
		sprintf(ifr.ifr_name, "%s%d",rtapd->prefix_wlan_name, i);
    	//sprintf(ifr.ifr_name, "ra%d", i);
    
		// Get MAC address
    	if (ioctl(s, SIOCGIFHWADDR, &ifr) != 0)
    	{
        	perror("ioctl(SIOCGIFHWADDR)");
			close(s);
        	return -1;
    	}

    	DBGPRINT(RT_DEBUG_INFO," Device %s has ifr.ifr_hwaddr.sa_family %d\n",ifr.ifr_name, ifr.ifr_hwaddr.sa_family);
		if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
    	{
			DBGPRINT(RT_DEBUG_ERROR,"IF-%s : Invalid HW-addr family 0x%04x\n", ifr.ifr_name, ifr.ifr_hwaddr.sa_family);
			close(s);
			return -1;
		}

		memcpy(rtapd->own_addr[i], ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    	DBGPRINT(RT_DEBUG_TRACE, "IF-%s MAC Address = " MACSTR "\n", ifr.ifr_name, MAC2STR(rtapd->own_addr[i]));

		close(s);
	}	


	return 0;
}

static void Apd_cleanup(rtapd *rtapd)
{
	int i;

	for (i = 0; i < MAX_MBSSID_NUM; i++)
	{
		if (rtapd->wlan_sock[i] >= 0)
			close(rtapd->wlan_sock[i]);
		if (rtapd->eth_sock[i] >= 0)
			close(rtapd->eth_sock[i]);	
	}	
	if (rtapd->ioctl_sock >= 0)
		close(rtapd->ioctl_sock);
    
	Radius_client_deinit(rtapd);

	Config_free(rtapd->conf);
	rtapd->conf = NULL;

	free(rtapd->prefix_wlan_name);
}

static int Apd_setup_interface(rtapd *rtapd)
{   
#if MULTIPLE_RADIUS
	int		i;
#endif

	if (Apd_init_sockets(rtapd))
		return -1;    
    
	if (Radius_client_init(rtapd))
    {
		DBGPRINT(RT_DEBUG_ERROR,"RADIUS client initialization failed.\n");
		return -1;
	}

	if (ieee802_1x_init(rtapd))
    {
		DBGPRINT(RT_DEBUG_ERROR,"IEEE 802.1X initialization failed.\n");
		return -1;
	}
#if MULTIPLE_RADIUS
	for (i = 0; i < rtapd->conf->SsidNum; i++)
		DBGPRINT(RT_DEBUG_TRACE,"auth_serv_sock[%d] = %d\n", i, rtapd->radius->mbss_auth_serv_sock[i]);
#else	
    DBGPRINT(RT_DEBUG_TRACE,"rtapd->radius->auth_serv_sock = %d\n",rtapd->radius->auth_serv_sock);
#endif

	return 0;
}

static void usage(void)
{
	DBGPRINT(RT_DEBUG_OFF, "USAGE :  	rtdot1xd [optional command]\n");
	DBGPRINT(RT_DEBUG_OFF, "[optional command] : \n");
	DBGPRINT(RT_DEBUG_OFF, "-i <card_number> : indicate which card is used\n");
	DBGPRINT(RT_DEBUG_OFF, "-d <debug_level> : set debug level\n");
	
	exit(1);
}

static rtapd * Apd_init(const char *prefix_name)
{
	rtapd *rtapd;
	int		i;

	rtapd = malloc(sizeof(*rtapd));
	if (rtapd == NULL)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Could not allocate memory for rtapd data\n");
		goto fail;
	}
	memset(rtapd, 0, sizeof(*rtapd));

	rtapd->prefix_wlan_name = strdup(prefix_name);
	if (rtapd->prefix_wlan_name == NULL)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Could not allocate memory for prefix_wlan_name\n");
		goto fail;
	}

    // init ioctl socket
    rtapd->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (rtapd->ioctl_sock < 0)
    {
	    DBGPRINT(RT_DEBUG_ERROR,"Could not init ioctl socket \n");	
	    goto fail;
    }
   

	rtapd->conf = Config_read(rtapd->ioctl_sock, rtapd->prefix_wlan_name);
	if (rtapd->conf == NULL)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Could not allocate memory for rtapd->conf \n");
		goto fail;
	}

	for (i = 0; i < MAX_MBSSID_NUM; i++)
	{
		rtapd->wlan_sock[i] = -1;
		rtapd->eth_sock[i] = -1;
	}

	return rtapd;

fail:
	if (rtapd) {		
		if (rtapd->conf)
			Config_free(rtapd->conf);

		if (rtapd->prefix_wlan_name)
			free(rtapd->prefix_wlan_name);

		free(rtapd);
	}
	return NULL;
	
}

static void Handle_usr1(int sig, void *eloop_ctx, void *signal_ctx)
{
	struct hapd_interfaces *rtapds = (struct hapd_interfaces *) eloop_ctx;
	struct rtapd_config *newconf;
	int i;

	DBGPRINT(RT_DEBUG_TRACE,"Reloading configuration\n");
	for (i = 0; i < rtapds->count; i++)
    {
		rtapd *rtapd = rtapds->rtapd[i];
		newconf = Config_read(rtapd->ioctl_sock, rtapd->prefix_wlan_name);
		if (newconf == NULL)
        {
			DBGPRINT(RT_DEBUG_ERROR,"Failed to read new configuration file - continuing with old.\n");
			continue;
		}

		/* TODO: update dynamic data based on changed configuration
		 * items (e.g., open/close sockets, remove stations added to
		 * deny list, etc.) */
		Radius_client_flush(rtapd);
		Config_free(rtapd->conf);
		rtapd->conf = newconf;
        Apd_free_stas(rtapd);

/* when reStartAP, no need to reallocate sock
        for (i = 0; i < rtapd->conf->SsidNum; i++)
        {
            if (rtapd->sock[i] >= 0)
                close(rtapd->sock[i]);
                
    	    rtapd->sock[i] = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    	    if (rtapd->sock[i] < 0)
            {
    		    perror("socket[PF_PACKET,SOCK_RAW]");
    		    return;
    	    }
        }*/

#if MULTIPLE_RADIUS
		for (i = 0; i < MAX_MBSSID_NUM; i++)
			rtapd->radius->mbss_auth_serv_sock[i] = -1;
#else
		rtapd->radius->auth_serv_sock = -1;
#endif

	    if (Radius_client_init(rtapd))
        {
		    DBGPRINT(RT_DEBUG_ERROR,"RADIUS client initialization failed.\n");
		    return;
	    }
#if MULTIPLE_RADIUS
		for (i = 0; i < rtapd->conf->SsidNum; i++)
			DBGPRINT(RT_DEBUG_TRACE, "auth_serv_sock[%d] = %d\n", i, rtapd->radius->mbss_auth_serv_sock[i]);
#else
        DBGPRINT(RT_DEBUG_TRACE,"rtapd->radius->auth_serv_sock = %d\n",rtapd->radius->auth_serv_sock);
#endif
	}
}

void Handle_term(int sig, void *eloop_ctx, void *signal_ctx)
{
	//FILE    *f;
	//char    buf[256], *pos;
	//int     line = 0, i;
    //int     filesize,cur = 0;
    //char    *ini_buffer;             /* storage area for .INI file */

	DBGPRINT(RT_DEBUG_ERROR,"Signal %d received - terminating\n", sig);

#if 0
	f = fopen(RT2860AP_SYSTEM_PATH, "r");
	if (f == NULL)
    {
		DBGPRINT(RT_DEBUG_ERROR,"Could not open configuration file '%s' for reading.\n", RT2860AP_SYSTEM_PATH);
		return;
	}

    if ((fseek(f, 0, SEEK_END))!=0)
        return;
    filesize=ftell(f);
	DBGPRINT(RT_DEBUG_ERROR,"filesize %d   - terminating\n", filesize);

    if ((ini_buffer=(char *)malloc(filesize + 1 ))==NULL)
        return;   //out of memory
    fseek(f,0,SEEK_SET);
    fread(ini_buffer, filesize, 1, f);
    fseek(f,0,SEEK_SET);
    ini_buffer[filesize]='\0';

	while ((fgets(buf, sizeof(buf), f)))
    {
		line++;
		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0')
        {
			if (*pos == '\n')
            {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		pos = strchr(buf, '=');
		if (pos == NULL)
        {
		    pos = strchr(buf, '[');                
			continue;
		}
		*pos = '\0';
		pos++;

        if ((strcmp(buf, "pid") == 0) )
        {
            cur = 0;
            while(cur < (int)filesize)
            {  
                if ((ini_buffer[cur]=='p') && (ini_buffer[cur+1]=='i') && (ini_buffer[cur+2]=='d'))
                {
                    cur += 4;
                    for( i=4; i>=0; i--)
                    {
                        if (ini_buffer[cur] !='\n' )
                        {
                            ini_buffer[cur] =0x30;
                        }
                        else
                        {
                            break;
                        }
                        cur++;
                    }   
                    break;
                }
                cur++;
            }
		} 
    }
    fseek(f,0,SEEK_SET);
    fprintf(f, "%s", ini_buffer);    
    fclose(f);
#endif

	eloop_terminate();
}


int main(int argc, char *argv[])
{
	struct hapd_interfaces interfaces;
       pid_t child_pid;
	int ret = 1, i;
	int c;
       pid_t auth_pid;
    char prefix_name[IFNAMSIZ+1] = "";
    
	//strcpy(prefix_name, "ra");	// remove by chhung
	
	for (;;)
    {
		c = getopt(argc, argv, "d:i:h");
		if (c < 0)
			break;

		switch (c)
        {
			case 'd': 
				/* 	set Debug level -
						RT_DEBUG_OFF		0
						RT_DEBUG_ERROR		1
						RT_DEBUG_WARN		2
						RT_DEBUG_TRACE		3
						RT_DEBUG_INFO		4 
				*/
				printf("Set debug level as %s\n", optarg);
				RTDebugLevel = (int)strtol(optarg, 0, 10);
				break;

			case 'i': 
				// Assign the wireless interface when support multiple cards
				sprintf(prefix_name, "%s%02d_", prefix_name, ((int)strtol(optarg, 0, 10) - 1));
			    break;

			case 'h':	
		    default:
				usage();
			    break;
		}
	} 
	// +++ add by chhung, for concurrent AP  
	if (strcmp(prefix_name, "") == 0) {
		if (strstr(argv[0], "rtinicapd") != NULL)
			strcpy(prefix_name, "rai");
		else
			strcpy(prefix_name, "ra");
	}
	// --- add by chhung, for concurrent AP

	printf("Ralink DOT1X daemon, version = '%s' \n", dot1x_version);
//	DBGPRINT(RT_DEBUG_TRACE, "prefix_name = '%s'\n", prefix_name);
	printf("prefix_name = '%s'\n", prefix_name);


    child_pid = fork();
    if (child_pid == 0)
    {           
		auth_pid = getpid();
		DBGPRINT(RT_DEBUG_TRACE, "Porcess ID = %d\n",auth_pid);
        
        openlog("rtdot1xd",0,LOG_DAEMON);
        // set number of configuration file 1
        interfaces.count = 1;
        interfaces.rtapd = malloc(sizeof(rtapd *));
        if (interfaces.rtapd == NULL)
        {
            DBGPRINT(RT_DEBUG_ERROR,"malloc failed\n");
            exit(1);    
        }

        eloop_init(&interfaces);
        eloop_register_signal(SIGINT, Handle_term, NULL);
        eloop_register_signal(SIGTERM, Handle_term, NULL);
        eloop_register_signal(SIGUSR1, Handle_usr1, NULL);
        eloop_register_signal(SIGHUP, Handle_usr1, NULL);

        interfaces.rtapd[0] = Apd_init(prefix_name);
        if (!interfaces.rtapd[0])
            goto out;
        if (Apd_setup_interface(interfaces.rtapd[0]))
            goto out;
        
		// Notify driver about PID
        RT_ioctl(interfaces.rtapd[0]->ioctl_sock, RT_PRIV_IOCTL, (char *)&auth_pid, sizeof(int), prefix_name, 0, RT_SET_APD_PID | OID_GET_SET_TOGGLE);
        
        eloop_run();

        Apd_free_stas(interfaces.rtapd[0]);
	    ret = 0;

out:
	    for (i = 0; i < interfaces.count; i++)
        {
		    if (!interfaces.rtapd[i])
			    continue;

		    Apd_cleanup(interfaces.rtapd[i]);
		    free(interfaces.rtapd[i]);
	    }

	    free(interfaces.rtapd);
	    eloop_destroy();
        closelog();
	    return ret;
    }
    else        
        return 0;
}

