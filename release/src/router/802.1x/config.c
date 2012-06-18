
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include <linux/if.h>			/* for IFNAMSIZ and co... */
#include <linux/wireless.h>

#include "rtdot1x.h"
#include "ieee802_1x.h"
#include "md5.h"

unsigned char BtoH(
    unsigned char ch)
{
    if (ch >= '0' && ch <= '9') return (ch - '0');        // Handle numerals
    if (ch >= 'A' && ch <= 'F') return (ch - 'A' + 0xA);  // Handle capitol hex digits
    if (ch >= 'a' && ch <= 'f') return (ch - 'a' + 0xA);  // Handle small hex digits
    return(255);
}

//
//  PURPOSE:  Converts ascii string to network order hex
//
//  PARAMETERS:
//    src    - pointer to input ascii string
//    dest   - pointer to output hex
//    destlen - size of dest
//
//  COMMENTS:
//
//    2 ascii bytes make a hex byte so must put 1st ascii byte of pair
//    into upper nibble and 2nd ascii byte of pair into lower nibble.
//
void AtoH(
    char            *src,
    unsigned char	*dest,
    int		        destlen)
{
    char *srcptr;
    unsigned char *destTemp;

    srcptr = src;   
    destTemp = (unsigned char *) dest; 

    while(destlen--)
    {
        *destTemp = BtoH(*srcptr++) << 4;    // Put 1st ascii byte in upper nibble.
        *destTemp += BtoH(*srcptr++);      // Add 2nd ascii byte to above.
        destTemp++;
    }
}

/**
 * rstrtok - Split a string into tokens
 * @s: The string to be searched
 * @ct: The characters to search for
 * * WARNING: strtok is deprecated, use strsep instead. However strsep is not compatible with old architecture.
 */
char * __rstrtok;
char * rstrtok(char * s,const char * ct)
{
	char *sbegin, *send;

	sbegin  = s ? s : __rstrtok;
	if (!sbegin)
	{
		return NULL;
	}

	sbegin += strspn(sbegin,ct);
	if (*sbegin == '\0')
	{
		__rstrtok = NULL;
		return( NULL );
	}

	send = strpbrk( sbegin, ct);
	if (send && *send != '\0')
		*send++ = '\0';

	__rstrtok = send;

	return (sbegin);
}


static int
Config_read_radius_addr(struct hostapd_radius_server **server,
                int *num_server, unsigned int addr, int def_port,
                struct hostapd_radius_server **curr_serv)
{
    struct hostapd_radius_server *nserv;
    int ret = 0;

	if (addr == 0)
		return -1;

    nserv = realloc(*server, (*num_server + 1) * sizeof(*nserv));
    if (nserv == NULL)
        return -1;

    *server = nserv;
    nserv = &nserv[*num_server];
    (*num_server)++;
    (*curr_serv) = nserv;

    memset(nserv, 0, sizeof(*nserv));
    nserv->port = def_port;
	
	//if (addr == 0)
	//	ret = -1;		
	//else		
    	nserv->addr.s_addr = addr;	

    return ret;
}

BOOLEAN Query_config_from_driver(int ioctl_sock, char *prefix_name, struct rtapd_config *conf, int *errors, int *flag)
{
	char 	*buf;	
	int 	len;	
    int		i, idx, m_num; 
	int		radius_count = 0, radius_port_count = 0, radius_key_count = 0;    
	int		num_eap_if = 0, num_preauth_if = 0; 
	PDOT1X_CMM_CONF pDot1xCmmConf;

	*flag = 0;
	*errors = 0;	

	len = sizeof(DOT1X_CMM_CONF);	
	buf = (char *) malloc(len + 1);
	if (buf == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR, "malloc() failed for Query_config_from_driver(len=%d)\n", len);
		return FALSE;
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, "alloc memory(%d) for Query_config_from_driver. \n", len);
		memset(buf, 0, len);
	}
				    
	if((RT_ioctl(ioctl_sock, RT_PRIV_IOCTL, buf, len, prefix_name, 0, OID_802_DOT1X_CONFIGURATION)) != 0)
	{
		DBGPRINT(RT_DEBUG_ERROR,"ioctl failed for Query_config_from_driver(len=%d, ifname=%s0)\n", len, prefix_name);
		free(buf);
		return FALSE;
	}
			
	pDot1xCmmConf = (PDOT1X_CMM_CONF)buf;

	// BssidNum
	conf->SsidNum = pDot1xCmmConf->mbss_num;
	if(conf->SsidNum > MAX_MBSSID_NUM)			
		conf->SsidNum = 1;			
	DBGPRINT(RT_DEBUG_TRACE, "MBSS number: %d\n", conf->SsidNum);

#if MULTIPLE_RADIUS
	m_num = conf->SsidNum;
#else
	m_num = 1;
#endif

	// own_ip_addr
	conf->own_ip_addr.s_addr = pDot1xCmmConf->own_ip_addr;
	if (conf->own_ip_addr.s_addr != 0)
	{		
		(*flag) |= 0x01;
		DBGPRINT(RT_DEBUG_TRACE, "own ip address: '%s'(%x)\n", inet_ntoa(conf->own_ip_addr), conf->own_ip_addr.s_addr);					
	}
	else
	{
		(*errors)++;
		DBGPRINT(RT_DEBUG_ERROR, "Invalid own ip address \n");
	}

		
	for (i = 0; i < m_num; i++)
	{
		for (idx = 0; idx < pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_num; idx++)
		{			
#if MULTIPLE_RADIUS  	
			// RADIUS_Server ip address
			if (!Config_read_radius_addr(
        	    &conf->mbss_auth_servers[i],
	            &conf->mbss_num_auth_servers[i], 
	            pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_ip, 
	            1812,
            	&conf->mbss_auth_server[i]))
    		{        	
        	    radius_count++;
				DBGPRINT(RT_DEBUG_TRACE, "(no.%d) Radius ip address: '%s'(%x) for %s%d\n", conf->mbss_num_auth_servers[i],
										inet_ntoa(conf->mbss_auth_server[i]->addr), 
										conf->mbss_auth_server[i]->addr.s_addr, prefix_name, i);
   			}	

			// RADIUS_Port and RADIUS_Key      
			if (conf->mbss_auth_server[i] && conf->mbss_auth_server[i]->addr.s_addr != 0)
			{					
				if (pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_port > 0)
				{
					radius_port_count++;
					conf->mbss_auth_server[i]->port = pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_port;           					
					DBGPRINT(RT_DEBUG_TRACE, "(no.%d) Radius port: '%d' for %s%d\n", conf->mbss_num_auth_servers[i], conf->mbss_auth_server[i]->port, prefix_name, i);
				}
				else
					DBGPRINT(RT_DEBUG_ERROR, "(no.%d) Radius port is invalid for %s%d\n", conf->mbss_num_auth_servers[i], prefix_name, i);

				if (pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_key_len > 0)
				{
					radius_key_count++;
					conf->mbss_auth_server[i]->shared_secret = (u8 *)strdup((const char *)pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_key);            
	    	        conf->mbss_auth_server[i]->shared_secret_len = pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_key_len;
					DBGPRINT(RT_DEBUG_TRACE,"(no.%d) Radius key: '%s', key_len: %d for %s%d \n", 
						conf->mbss_num_auth_servers[i], conf->mbss_auth_server[i]->shared_secret, conf->mbss_auth_server[i]->shared_secret_len, prefix_name, i);	
				}
				else
					DBGPRINT(RT_DEBUG_ERROR, "(no.%d) Radius key is invalid for %s%d\n", conf->mbss_num_auth_servers[i], prefix_name, i);
			
			}
#else
			// RADIUS_Server ip address
			if (!Config_read_radius_addr(
	            &conf->auth_servers,
	            &conf->num_auth_servers, 
	            pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_ip, 
	            1812,
	            &conf->auth_server))
		    {        	
		            radius_count++;
			}	
		    DBGPRINT(RT_DEBUG_TRACE, "(no.%d) Radius ip address: '%s'(%x)\n", 
												conf->num_auth_servers,
												inet_ntoa(conf->auth_server->addr), 
												conf->auth_server->addr.s_addr);

			// RADIUS_Port and RADIUS_Key  
			if (conf->auth_server && conf->auth_server->addr.s_addr != 0)
			{
				if (pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_port > 0)
				{
					radius_port_count++;
		    		conf->auth_server->port = pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_port;
					DBGPRINT(RT_DEBUG_TRACE,"(no.%d) Radius port: '%d'\n", conf->num_auth_servers, conf->auth_server->port);
				}
				else
					DBGPRINT(RT_DEBUG_ERROR, "(no.%d) Radius port is invalid\n", conf->num_auth_servers);

				if (pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_key_len > 0)
				{
					radius_key_count++;
					conf->auth_server->shared_secret = (u8 *)strdup((const char *)pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_key);            
		        	conf->auth_server->shared_secret_len = pDot1xCmmConf->Dot1xBssInfo[i].radius_srv_info[idx].radius_key_len;
					DBGPRINT(RT_DEBUG_TRACE,"(no.%d) Radius key: '%s', key_len: %d \n", conf->num_auth_servers, 
					conf->auth_server->shared_secret, conf->auth_server->shared_secret_len);	
				} 
				else
					DBGPRINT(RT_DEBUG_ERROR, "(no.%d) Radius key is invalid\n", conf->num_auth_servers);
		
			}       
#endif			
		}			
	}				
			
	// Sanity check for radius ip address
	if (radius_count != 0)
		(*flag) |= 0x02;
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, "No any valid radius ip address \n");
		(*errors)++;
	}
	// Sanity check for radius port number
    if (radius_count == radius_port_count)
		(*flag) |= 0x04;
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, "No enough radius port \n");
		(*errors)++;           
	}         

	// Sanity check for radius key 
	if (radius_count == radius_key_count)
		(*flag) |= 0x08;
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, "No enough radius key \n");
		(*errors)++;   
	}   

   	// radius_retry_primary_interval
   	conf->radius_retry_primary_interval = pDot1xCmmConf->retry_interval;
	if (conf->radius_retry_primary_interval > 0)
		DBGPRINT(RT_DEBUG_TRACE,"Radius retry primary interval %d seconds. \n", conf->radius_retry_primary_interval);

	// session_timeout_interval
	conf->session_timeout_interval = pDot1xCmmConf->session_timeout_interval;                   
    if (conf->session_timeout_interval == 0)
    {
        conf->session_timeout_set= 0;		
    }	
    else
    {
        conf->session_timeout_set= 1;

		if (conf->session_timeout_interval < 60)
			conf->session_timeout_interval = REAUTH_TIMER_DEFAULT_reAuthPeriod;
				    	
    	DBGPRINT(RT_DEBUG_TRACE,"Radius session timeout interval %d seconds. \n", conf->session_timeout_interval);
    }
    DBGPRINT(RT_DEBUG_TRACE,"session_timeout policy is %s \n", conf->session_timeout_set ? "enabled" : "disabled");

	/* QuietPeriod */
	conf->quiet_interval = pDot1xCmmConf->quiet_interval;
	if (conf->quiet_interval < AUTH_PAE_DEFAULT_quietPeriod)
		conf->quiet_interval = AUTH_PAE_DEFAULT_quietPeriod;
	DBGPRINT(RT_DEBUG_TRACE, "Quiet period %d seconds \n", conf->quiet_interval);
	

	for (i = 0; i < m_num; i++)
	{
		int	g_key_len = 0;

		// DefaultKeyID	
		if (pDot1xCmmConf->Dot1xBssInfo[i].ieee8021xWEP)
		{
			// set group key index
			conf->DefaultKeyID[i] = pDot1xCmmConf->Dot1xBssInfo[i].key_index;	

			// set unicast key index
			if (conf->DefaultKeyID[i] == 3)
				conf->individual_wep_key_idx[i] = 0;	
			else
				conf->individual_wep_key_idx[i] = 3;	
					
			DBGPRINT(RT_DEBUG_TRACE,"IEEE8021X WEP: group key index(%d) and unicast key index(%d) for %s%d\n", 
																	conf->DefaultKeyID[i], conf->individual_wep_key_idx[i], prefix_name, i);

			g_key_len = pDot1xCmmConf->Dot1xBssInfo[i].key_length;
			if (g_key_len == 5 || g_key_len == 13)
			{
				conf->individual_wep_key_len[i] = g_key_len;
				memset(conf->IEEE8021X_ikey[i], 0, WEP8021X_KEY_LEN);
	            memcpy(conf->IEEE8021X_ikey[i], pDot1xCmmConf->Dot1xBssInfo[i].key_material, g_key_len);

				DBGPRINT(RT_DEBUG_TRACE,"IEEE8021X WEP: use Key%dStr as shared Key and its key_len is %d for %s%d\n",
											conf->DefaultKeyID[i]+1, g_key_len, prefix_name, i);			
			}
		}

		/* NAS Identifier */
		if (pDot1xCmmConf->Dot1xBssInfo[i].nasId_len > 0)
		{
			memcpy(conf->nasId[i], pDot1xCmmConf->Dot1xBssInfo[i].nasId, pDot1xCmmConf->Dot1xBssInfo[i].nasId_len);
			conf->nasId_len[i] = pDot1xCmmConf->Dot1xBssInfo[i].nasId_len;		
		}
		DBGPRINT(RT_DEBUG_TRACE, "NAS-Identifier: %s and len=%d for %s%d \n", 
									conf->nasId[i], conf->nasId_len[i], prefix_name, i);
		

		// EAPifname
		if (pDot1xCmmConf->EAPifname_len[i] > 0)
		{
			memset(conf->eap_if_name[num_eap_if], 0, IFNAMSIZ);	
			memcpy(conf->eap_if_name[num_eap_if], pDot1xCmmConf->EAPifname[i], pDot1xCmmConf->EAPifname_len[i]);	 
			
			DBGPRINT(RT_DEBUG_TRACE, "(no.%d) EAPifname: %s \n", num_eap_if + 1, conf->eap_if_name[num_eap_if]);
			num_eap_if ++;
		}

		// PreAuthifname
		if (pDot1xCmmConf->PreAuthifname_len[i] > 0)
		{
			memset(conf->preauth_if_name[num_preauth_if], 0, IFNAMSIZ);	
			memcpy(conf->preauth_if_name[num_preauth_if], pDot1xCmmConf->PreAuthifname[i], pDot1xCmmConf->PreAuthifname_len[i]);	 

			DBGPRINT(RT_DEBUG_TRACE,"(no.%d) PreAuthifname: %s \n", num_preauth_if + 1, conf->preauth_if_name[num_preauth_if]);
			num_preauth_if ++;
		}

	}

	if (num_eap_if > 0)
		conf->num_eap_if = num_eap_if;
	if (num_preauth_if > 0)
		conf->num_preauth_if = num_preauth_if;

	free(buf);

	return TRUE;
				
}


struct rtapd_config * Config_read(int ioctl_sock, char *prefix_name)
{
    struct rtapd_config *conf;        
    int errors = 0, i = 0;
    int flag = 0;
                 
    conf = malloc(sizeof(*conf));
    if (conf == NULL)
    {
        DBGPRINT(RT_DEBUG_TRACE, "Failed to allocate memory for configuration data.\n");        
        return NULL;
    }
    memset(conf, 0, sizeof(*conf));

    conf->SsidNum = 1;
    conf->session_timeout_set = 0xffff;
    
    /* Some related variable per BSS set to default */
    for (i = 0; i < MAX_MBSSID_NUM; i++)
    {
		/* initial default shared-key material and index */
		conf->DefaultKeyID[i] = 0;										// broadcast key index
		conf->individual_wep_key_idx[i] = 3;							// unicast key index 
    	conf->individual_wep_key_len[i] = WEP8021X_KEY_LEN;				// key length
    	hostapd_get_rand(conf->IEEE8021X_ikey[i], WEP8021X_KEY_LEN);    // generate shared key randomly

		/* Initial NAS-ID */		
		memcpy(conf->nasId[i], "RalinkAP", 8);
		conf->nasId[i][8] = '0' + i;
		conf->nasId_len[i] = 9;
  	}
  
	// initial default EAP IF name and Pre-Auth IF name	as "br0"
	conf->num_eap_if = 1;	
	conf->num_preauth_if = 1;	
	strcpy(conf->eap_if_name[0], "br0");	
	strcpy(conf->preauth_if_name[0], "br0");

	// Get parameters from deiver through IOCTL cmd
	if(!Query_config_from_driver(ioctl_sock, prefix_name, conf, &errors, &flag))
	{
		Config_free(conf);
    	return NULL;
	}
       
#if MULTIPLE_RADIUS
	for (i = 0; i < MAX_MBSSID_NUM; i++)
	{
		struct hostapd_radius_server *servs, *cserv, *nserv;
		int c;

		conf->mbss_auth_server[i] = conf->mbss_auth_servers[i];

		if (!conf->mbss_auth_server[i])
			continue;
						
		cserv	= conf->mbss_auth_server[i];
		servs 	= conf->mbss_auth_servers[i];								
			
		DBGPRINT(RT_DEBUG_TRACE, "%s%d, Current IP: %s \n", prefix_name, i, inet_ntoa(cserv->addr));			
		for (c = 0; c < conf->mbss_num_auth_servers[i]; c++)
		{				
			nserv = &servs[c];             
			DBGPRINT(RT_DEBUG_TRACE, "	   Server IP List: %s \n", inet_ntoa(nserv->addr));
		}				
	}
#else
    conf->auth_server = conf->auth_servers;
#endif
	
    if (errors)
    {
        DBGPRINT(RT_DEBUG_ERROR,"%d errors for radius setting\n", errors);
        Config_free(conf);
        conf = NULL;
    }
    if ((flag&0x0f)!=0x0f)
    {
        DBGPRINT(RT_DEBUG_ERROR,"Not enough necessary parameters are found, flag = %x\n", flag);
        Config_free(conf);
        conf = NULL;
    }
		
    return conf;
}


static void Config_free_radius(struct hostapd_radius_server *servers, int num_servers)
{
    int i;

    for (i = 0; i < num_servers; i++)
    {
        free(servers[i].shared_secret);
    }
    free(servers);
}

void Config_free(struct rtapd_config *conf)
{
#if MULTIPLE_RADIUS
	int	i;
#endif
	
    if (conf == NULL)
        return;

#if MULTIPLE_RADIUS
	for (i = 0; i < MAX_MBSSID_NUM; i++)
	{
		if (conf->mbss_auth_servers[i])
			Config_free_radius(conf->mbss_auth_servers[i], conf->mbss_num_auth_servers[i]);
	}
#else
    Config_free_radius(conf->auth_servers, conf->num_auth_servers);
#endif
    free(conf);
}

