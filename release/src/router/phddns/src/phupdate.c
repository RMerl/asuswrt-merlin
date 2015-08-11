#include <time.h>
#include "phupdate.h"
#include "log.h"
#include "blowfish.h"

#include "generate.h"
#include "lutil.h"
#include <stdlib.h>
#include <bcmnvram.h>

BOOL InitializeSockets(PHGlobal *phglobal)
{
	DestroySockets(phglobal);
	if (!phCreate(&(phglobal->m_tcpsocket),0,SOCK_STREAM,phglobal->szBindAddress))
	{
		return FALSE;
	}

	if (!phCreate(&(phglobal->m_udpsocket),0,SOCK_DGRAM,phglobal->szBindAddress))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL DestroySockets(PHGlobal *phglobal)
{
	phClose(&phglobal->m_tcpsocket);
	phClose(&phglobal->m_udpsocket);
	return TRUE;
}

BOOL BeginKeepAlive(PHGlobal *phglobal)
{
	if (!phConnect(phglobal->m_udpsocket, phglobal->szTcpConnectAddress,phglobal->nPort,&phglobal->nAddressIndex, NULL)) return FALSE;
	phglobal->nLastResponseID = time(0);
	return TRUE;
}

BOOL SendKeepAlive(PHGlobal *phglobal, int opCode)
{
	DATA_KEEPALIVE data;
	blf_ctx blf;
	char p1[KEEPALIVE_PACKET_LEN],p2[KEEPALIVE_PACKET_LEN];

	memset(&data,0,sizeof(data));
	data.lChatID = phglobal->nChatID;
	data.lID = phglobal->nStartID;
	data.lOpCode = opCode;
	data.lSum = 0 - (data.lID + data.lOpCode);
	data.lReserved = 0;

	if (!phglobal->bTcpUpdateSuccessed) return FALSE;

	LOG(1) ("SendKeepAlive() %d\n",opCode);


	InitBlowfish(&blf, (unsigned char*)phglobal->szChallenge,phglobal->nChallengeLen);
	memcpy(p1,&data,KEEPALIVE_PACKET_LEN);
	memcpy(p2,&data,KEEPALIVE_PACKET_LEN);
	Blowfish_EnCode(&blf, p1+4,p2+4,KEEPALIVE_PACKET_LEN-4);

	if (phglobal->bBigEndian)
    {
                reverse_byte_order((int*)p2, 5);
                //printf("debug-- reversed byte order\n");
     }

	phSend(phglobal->m_udpsocket, p2, KEEPALIVE_PACKET_LEN,0);
	//RecvKeepaliveResponse();
	return TRUE;
}

int RecvKeepaliveResponse(PHGlobal *phglobal)
{
	char temp[100];
	DATA_KEEPALIVE_EXT rdata;
	DATA_KEEPALIVE data;
	blf_ctx blf;
	char p1[KEEPALIVE_PACKET_LEN],p2[KEEPALIVE_PACKET_LEN];

	if (!phglobal->bTcpUpdateSuccessed) return errorOccupyReconnect;

	//prevent the thread to be suspended while waiting for data
	if (phDataReadable(phglobal->m_udpsocket, 0)<=0) 
	{
		return okNoData;
	}
	//DATA_KEEPALIVE data;
	//if (m_udpsocket.Receive(&data,sizeof(DATA_KEEPALIVE),0)<=0) return FALSE;
	if (phReceive(phglobal->m_udpsocket, temp,sizeof(temp),0)<=0) return okNoData;
	//printf("debug-- received some udp data\n");
	memcpy(&rdata, temp, sizeof(DATA_KEEPALIVE_EXT));
	//if (phglobal->bBigEndian) reverse_byte_order((int*)&rdata.ip, 1);

	data = rdata.keepalive;

	InitBlowfish(&blf, (unsigned char*)phglobal->szChallenge,phglobal->nChallengeLen);
	
	memcpy(p1,&data,KEEPALIVE_PACKET_LEN);
	if (phglobal->bBigEndian) reverse_byte_order((int*)p1, 5);
	memcpy(p2,&data,KEEPALIVE_PACKET_LEN);
	Blowfish_DeCode(&blf, p1+4,p2+4,KEEPALIVE_PACKET_LEN-4);
	memcpy(&data,p2,KEEPALIVE_PACKET_LEN);

	phglobal->nStartID = data.lID + 1;
	
	LOG(1) (("RecvKeepaliveResponse() Data comes, OPCODE:%d\n"),data.lOpCode);
	if (data.lID - phglobal->nLastResponseID > 3 && phglobal->nLastResponseID != -1)
	{
		return errorOccupyReconnect;
	}

	phglobal->nLastResponseID = data.lID;
	phglobal->tmLastResponse = time(0);

	phglobal->ip = rdata.ip;

	if (data.lOpCode == UDP_OPCODE_UPDATE_ERROR) return okServerER;
	//if (data.lOpCode == UDP_OPCODE_LOGOUT) return okNormal;
	
	return okKeepAliveRecved;
}

int ExecuteUpdate(PHGlobal *phglobal)
{
	char buffer[1024];
    
    char username[128] = "";
	char key[128] = "";
	char sendbuffer[256];
	
    char domains[255][255];
    char regicommand[255];
	int i,len, totaldomains;
	long challengetime = 0;

	char *chatid = NULL;
	char *startid = NULL;
	char *xmldata = NULL;
	int buflen = 0;

	LOG(1) ("ExecuteUpdate Connecting %s.\n",phglobal->szHost);
	
	if (!phConnect(phglobal->m_tcpsocket, phglobal->szHost,phglobal->nPort,&phglobal->nAddressIndex,phglobal->szTcpConnectAddress))
	{
		LOG(1) ("ExecuteUpdate errorConnectFailed.\n");
		phglobal->nAddressIndex++;
nvram_set("ddns_return_code", "connect_fail");
nvram_set("ddns_return_code_chk", "connect_fail");
		return errorConnectFailed;
	}
	//////////////////////////////////////////////////////////////////////////
	//Recv server hello string
	memset(buffer, 0, 128);
	len = phReadOneLine(phglobal->m_tcpsocket, buffer,sizeof(buffer));
	if (len <=0 )
	{
		LOG(1) ("ExecuteUpdate Recv server hello string failed.\n");
		phClose(&phglobal->m_tcpsocket);
		phglobal->nAddressIndex++;
nvram_set("ddns_return_code", "connect_fail");
nvram_set("ddns_return_code_chk", "connect_fail");
		return errorConnectFailed;
	}

	LOG(1) (("SEND AUTH REQUEST COMMAND..."));
	phSend(phglobal->m_tcpsocket, (char*)COMMAND_AUTH,sizeof(COMMAND_AUTH),0);
    LOG(1) (("OK.\n"));

	//////////////////////////////////////////////////////////////////////////
	//Recv server key string
	memset(buffer, 0, 128);
	len = phReadOneLine(phglobal->m_tcpsocket, buffer,sizeof(buffer));
	if (len <=0 )
	{
		LOG(1) (("ExecuteUpdate Recv server key string failed.\n"));
		phClose(&phglobal->m_tcpsocket);
nvram_set("ddns_return_code", "connect_fail");
nvram_set("ddns_return_code_chk", "connect_fail");
		return errorConnectFailed;
	}
    LOG(1) (("SERVER SIDE KEY \"%s\" RECEIVED.\n"),buffer);

	phglobal->nChallengeLen =  lutil_b64_pton(buffer+4, (unsigned char *)phglobal->szChallenge, 256);


	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//Generate encoded auth string
	len = GenerateCrypt(phglobal->szUserID, phglobal->szUserPWD, buffer+4, phglobal->clientinfo, phglobal->challengekey, sendbuffer);
	strcat(sendbuffer, "\r\n");
    //Generate ok.
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////
	//send auth data
	LOG(1) (("SEND AUTH DATA..."));
	phSend(phglobal->m_tcpsocket, sendbuffer,strlen(sendbuffer),0);
    LOG(1) (("OK\n"));

	memset(buffer, 0, 128);
	len = phReadOneLine(phglobal->m_tcpsocket, buffer,sizeof(buffer));
	buffer[3] = 0;

	if (len <=0 )
	{
		LOG(1) (("ExecuteUpdate Recv server auth response failed.\n"));
		phClose(&phglobal->m_tcpsocket);
		//modified skyvense 2005/10/08, for server db conn lost bug
		//return errorAuthFailed;
nvram_set("ddns_return_code", "connect_fail");
nvram_set("ddns_return_code_chk", "connect_fail");
		return errorConnectFailed;
	}
	if (strcmp(buffer,"250")!=0 && strcmp(buffer,"536")!=0)
	{
		LOG(1) ("CTcpThread::ExecuteUpdate auth failed.\n");
		phClose(&phglobal->m_tcpsocket);

nvram_set("ddns_return_code", "auth_fail");	
nvram_set("ddns_return_code_chk", "auth_fail");	
		if (strstr(buffer + 4, "Busy.") != NULL) return errorAuthBusy;
		return errorAuthFailed;
	}
	if (strcmp(buffer,"536") == 0) //find redirected server address, and let us connect again to new server
	{
		char *pos0 = strchr(buffer + 4, '<');
		if (pos0)
		{
			char *pos1 = strchr(pos0 + 1, '>');
			if (pos1)
			{
				*pos1 = '\0';
				strcpy(phglobal->szHost, pos0 + 1);
				
				phClose(&phglobal->m_tcpsocket);
				return okRedirecting;
			}
		}
nvram_set("ddns_return_code", "auth_fail");
nvram_set("ddns_return_code_chk", "auth_fail");
		return errorAuthFailed;
	}
	if (strcmp(buffer,"250") == 0) //get user type level, 0(free),1(pro),2(biz)
	{
		char *pos0 = strchr(buffer + 4, '<');
		if (pos0)
		{
			char *pos1 = strchr(pos0 + 1, '>');
			if (pos1)
			{
				*pos1 = '\0';
				phglobal->nUserType = atoi(pos0 + 1);
			}
		}
	}
	
	//////////////////////////////////////////////////////////////////////////
	//list domains
	for (i=0,totaldomains=0;i<255;i++)
    {
        memset(domains[i], 0, 255);
        phReadOneLine(phglobal->m_tcpsocket, domains[i],255);
        LOG(1) (("ExecuteUpdate domain \"%s\"\n"),domains[i]);
        totaldomains++;
		strcpy(phglobal->szActiveDomains[i],domains[i]);
        if (domains[i][0] == '.') break;
    }
	if (totaldomains<=0)
	{
		LOG(1) (("ExecuteUpdate Domain List Failed.\n"));
		phClose(&phglobal->m_tcpsocket);
		return errorDomainListFailed;
	}

	phglobal->cLastResult = okDomainListed;
	if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, phglobal->cLastResult, 0);
	//::SendMessage(theApp.m_hWndController,WM_DOMAIN_UPDATEMSG,okDomainListed,(long)domains);
	//////////////////////////////////////////////////////////////////////////
	//send domain regi commands list
	for (i=0;;i++)
    {
        if (domains[i][0] == '.') break;
		memset(regicommand, 0, 128);
        strcpy(regicommand, COMMAND_REGI);
        strcat(regicommand, " ");
        strcat(regicommand, domains[i]);
        strcat(regicommand, "\r\n");
        //printf("%s",regicommand);
        phSend(phglobal->m_tcpsocket,regicommand,strlen(regicommand),0);
    }

	//////////////////////////////////////////////////////////////////////////
	//send confirm
	LOG(1) (("SEND CNFM DATA..."));
    phSend(phglobal->m_tcpsocket,(char*)COMMAND_CNFM,strlen(COMMAND_CNFM),0);
    LOG(1) (("OK\n"));
	
	for (i=0;i<totaldomains-1;i++)
    {
		memset(buffer, 0, 128);
		len = phReadOneLine(phglobal->m_tcpsocket, buffer,sizeof(buffer));
		if (len <= 0)
		{
			LOG(1) (("ExecuteUpdate Recv server confirm response failed.\n"));
			phClose(&phglobal->m_tcpsocket);
			return errorDomainRegisterFailed;
		}
		LOG(1) (("ExecuteUpdate %s\n"),buffer);
		if (phglobal->cbOnDomainRegistered) phglobal->cbOnDomainRegistered(phglobal, domains[i]);
    }
	
	memset(buffer, 0, 128);
	len = phReadOneLine(phglobal->m_tcpsocket, buffer,sizeof(buffer));
	if (len <= 0)
	{
		LOG(1) (("ExecuteUpdate Recv server confirmed chatID response failed.\n"));
		phClose(&phglobal->m_tcpsocket);
		return errorDomainRegisterFailed;
	}
	LOG(1) (("%s\n"),buffer);

	//////////////////////////////////////////////////////////////////////////
	//find chatid & startid
	buffer[3] = 0;
	if (strcmp(buffer,"250")!=0)
	{
		phglobal->nChatID = phglobal->nStartID = 0;
		LOG(1) (("ExecuteUpdate register failed %s\n"),buffer);
	}
	else
	{
		chatid = buffer + 4;
		startid = NULL;

		for (i=0;i<strlen(chatid);i++)
		{
			if (buffer[i] == ' ')
			{
				buffer[i] = 0;
				startid = buffer + i + 1;
				break;
			}
		}
		phglobal->nChatID = atoi(chatid);
		if (startid) phglobal->nStartID = atoi(startid);
		LOG(1) (("ExecuteUpdate nChatID:%d, nStartID:%d\n"),phglobal->nChatID,phglobal->nStartID);
	}
	
	
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//after confirm domain register, we begin to get user information
	phSend(phglobal->m_tcpsocket,(void *)COMMAND_STAT_USER,sizeof(COMMAND_STAT_USER),0);
	memset(buffer, 0, 1024);
	len = phReadOneLine(phglobal->m_tcpsocket, buffer,sizeof(buffer));
	buffer[3] = 0;
	if (len <= 0 || strcmp(buffer,"250")!=0)
	{
		LOG(1) ("CTcpThread::ExecuteUpdate Recv server confirmed stat user response failed.\n");
		phClose(&phglobal->m_tcpsocket);
		return errorStatDetailInfoFailed;
	}
	
	buflen = MAX_PATH;
	xmldata = (char *)malloc(buflen);
	memset(xmldata, 0, buflen);
	
	for (;;)
	{
		memset(buffer, 0, 1024);
        len = phReadOneLine(phglobal->m_tcpsocket, buffer,1024);
        if (buffer[0] == '.' || len <= 0) break;
		if (buflen < strlen(xmldata) + len)
		{
			buflen += MAX_PATH;
			xmldata = realloc(xmldata, buflen);
			memset((xmldata + buflen) - MAX_PATH, 0, MAX_PATH);
		}
		strncat(xmldata, buffer, len);
	}
	LOG(1) ("userinfo: \r\n%s\r\n", xmldata);
	if (phglobal->cbOnUserInfo) phglobal->cbOnUserInfo(phglobal, xmldata, strlen(xmldata));
	free(xmldata);
	buflen = 0;
	

	phSend(phglobal->m_tcpsocket,(void *)COMMAND_STAT_DOM,sizeof(COMMAND_STAT_DOM),0);
	memset(buffer, 0, 1024);
	len = phReadOneLine(phglobal->m_tcpsocket, buffer,sizeof(buffer));
	buffer[3] = 0;
	if (len <= 0 || strcmp(buffer,"250")!=0)
	{
		LOG(1) ("CTcpThread::ExecuteUpdate Recv server confirmed stat user response failed.\n");
		phClose(&phglobal->m_tcpsocket);
		return errorStatDetailInfoFailed;
	}
	
	buflen = MAX_PATH;
	xmldata = (char *)malloc(buflen);
	memset(xmldata, 0, buflen);

	for (;;)
	{
		memset(buffer, 0, 1024);
        len = phReadOneLine(phglobal->m_tcpsocket, buffer,1024);
        if (buffer[0] == '.' || len <= 0) break;
		if (buflen < strlen(xmldata) + len)
		{
			buflen += MAX_PATH;
			xmldata = realloc(xmldata, buflen);
			memset((xmldata + buflen) - MAX_PATH, 0, MAX_PATH);
		}
		strncat(xmldata, buffer, len);
	}
	LOG(1) ("domaininfo: \r\n%s\r\n", xmldata);
	if (phglobal->cbOnAccountDomainInfo) phglobal->cbOnAccountDomainInfo(phglobal, xmldata, strlen(xmldata));
	free(xmldata);
	buflen = 0;

	//////////////////////////////////////////////////////////////////////////
	//good bye!
    	LOG(1) (("SEND QUIT COMMAND..."));
	phSend(phglobal->m_tcpsocket,(char*)COMMAND_QUIT,sizeof(COMMAND_QUIT),0);
    	LOG(1) (("OK.\n"));

	LOG(1) (("Update ddns_return_code!!!"));
	nvram_set("ddns_updated", "1");
	nvram_set("ddns_return_code", "200");
	nvram_set("ddns_return_code_chk", "200");
        for (i=0,totaldomains=0;i<255;i++)
    	{
                if(phglobal->szActiveDomains[i] && (phglobal->szActiveDomains[i][0] != '.') )
			nvram_set("ddns_hostname_x", phglobal->szActiveDomains[i]);
		LOG(1) (("Update Domain: %s\n", phglobal->szActiveDomains[i]));
		if(phglobal->szActiveDomains[i][0] == '.')
			break;
    	}

    	memset(buffer, 0, 128);
	len = phReadOneLine(phglobal->m_tcpsocket, buffer,sizeof(buffer));
	if (len <= 0)
	{
		LOG(1) (("ExecuteUpdate Recv server goodbye response failed.\n"));
		phClose(&phglobal->m_tcpsocket);
		return okDomainsRegistered;
	}
	LOG(1) (("%s\n"),buffer);                               
	phClose(&phglobal->m_tcpsocket);
	return okDomainsRegistered;
}

int phddns_getkeepalive(int user_type) {
  return (user_type >= 3 ) ? 15 : ((user_type >= 1) ? 30 : 60);
}

int get_retry_seconds(int user_type) {
	int nMaxRetrySeconds = 320; //5 * 60 + 20
	switch (user_type)
	{
	case 1://pro
	case 2://biz
		nMaxRetrySeconds = 160; //5 * 30 + 10
		break;
	case 3://premium
		nMaxRetrySeconds = 80; //5 * 15 + 5
		break;
	case 0://free
	default:
		nMaxRetrySeconds = 320; //5 * 60 + 20
		break;
	}
	return nMaxRetrySeconds;
}

int get_keepalive_warn_seconds(int user_type) {
	int nMaxKeepaliveWarnSeconds = 130; // 2 * 60 + 10
	switch (user_type)
	{
	case 1://pro
	case 2://biz
		nMaxKeepaliveWarnSeconds = 65; // 2 * 30 + 5
		break;
	case 3://premium
		nMaxKeepaliveWarnSeconds = 35; // 2 * 15 + 5
		break;
	case 0://free
	default:
		nMaxKeepaliveWarnSeconds = 130; // 2 * 60 + 10
		break;
	}
	return nMaxKeepaliveWarnSeconds;
}

int phddns_step(PHGlobal *phglobal)
{
	int ret = 0;
	if (phglobal->bNeed_connect)
	{
		strcpy(phglobal->szActiveDomains[0],".");
		
		phglobal->cLastResult = okConnecting;
		
		if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, phglobal->cLastResult, 0);
		
		if (!InitializeSockets(phglobal))
		{
			LOG(1) ("InitializeSockets failed, waiting for 5 seconds to retry...\n");
			phglobal->cLastResult = errorConnectFailed;
			if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, phglobal->cLastResult, 0);
			return 5;
		}
		
		ret = ExecuteUpdate(phglobal);
//Yau
LOG(1) ("  ===> ExecuteUpdate ret = %d\n\n", ret);
		phglobal->cLastResult = ret;
		if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, phglobal->cLastResult, ret == okDomainsRegistered ? phglobal->nUserType : 0);
		if (ret == okDomainsRegistered) 
		{
			//OnUserInfo(phglobal->szUserInfo);
			//OnAccountDomainInfo(phglobal->szDomainInfo);
			if (phglobal->nChatID == 0) //user has no domains, no need to begin keepalive
			{
				LOG(1) ("ExecuteUpdate OK, nChatID 0\n");
			}
			else
			{
				LOG(1) ("ExecuteUpdate OK, BeginKeepAlive!\n");
				
				BeginKeepAlive(phglobal);
				
			}
			phglobal->bTcpUpdateSuccessed = TRUE;
			phglobal->tmLastResponse = time(0);
			phglobal->bNeed_connect = FALSE;
			phglobal->lasttcptime = phglobal->tmLastSend = time(0);
		}
		else 
		{
			if (ret == okRedirecting)
			{
				phglobal->bTcpUpdateSuccessed = FALSE;
				phglobal->bNeed_connect = TRUE;
				LOG(1) ("Need redirect, waiting for 5 seconds...\n");
				return 5;
			}
			if (ret == errorAuthFailed)
			{
				LOG(1) ("ExecuteUpdate AuthFailed, waiting for 600 seconds to retry...\n");
				return 600;
			}
			
			LOG(1) ("ExecuteUpdate failed, waiting for 30 seconds to retry...\n");
			return 30;
		}
		phglobal->nLastResponseID = -1;
	}
	else
	{
		if (phglobal->nChatID == 0)//user has no domains, no need to begin keepalive
		{
			return 300;
		}

		if (time(0) - phglobal->tmLastSend > phddns_getkeepalive(phglobal->nUserType) )  // old code (phglobal->nUserType >= 1 ? 30 : 60)
		{
			SendKeepAlive(phglobal, UDP_OPCODE_UPDATE_VER2);
			phglobal->tmLastSend = time(0);
		}
		ret = RecvKeepaliveResponse(phglobal);
		if (ret != okNormal && ret != okNoData) phglobal->cLastResult = ret;
		if (ret == errorOccupyReconnect)
		{
			LOG(1) ("RecvKeepaliveResponse failed, waiting for 30 seconds to reconnect...\n");
			phglobal->bNeed_connect = TRUE;
			phglobal->bTcpUpdateSuccessed = FALSE;

			if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, phglobal->cLastResult, 0);
			return 30;
		}
		else
		{
			if (ret == okKeepAliveRecved)
			{
				struct in_addr t;
				t.s_addr = phglobal->ip;
				LOG(1) ("Keepalive response received, client ip: %s\n",inet_ntoa(t));
				if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, phglobal->cLastResult, phglobal->ip);
			}
		}

		if (time(0) - phglobal->tmLastResponse > get_retry_seconds(phglobal->nUserType) && phglobal->tmLastResponse != -1) 
		{
			LOG(1) ("No response from server for %d seconds, reconnect immediately...\n", get_retry_seconds(phglobal->nUserType));
			phglobal->bTcpUpdateSuccessed = FALSE;
			phglobal->bNeed_connect = TRUE;
			if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, errorRetrying, 0);
			
			return 1;
		}
		else if (((time(0) - phglobal->tmLastResponse) > get_keepalive_warn_seconds(phglobal->nUserType)) && phglobal->tmLastResponse != -1)
		{
			//LOG(1) ("No response from server for %d seconds, keepAlive error...\n", ret);
			if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, errorKeepAliveError, 0);

			return 1;
		}

		//if (time(0) - phglobal->tmLastResponse > (phglobal->nUserType >= 1 ? 130 : 320) && phglobal->tmLastResponse != -1)
		//{
		//	LOG(1) ("No response from server for %d seconds, reconnect immediately...\n", (phglobal->nUserType == 1 ? 160 : 320));
		//	phglobal->bTcpUpdateSuccessed = FALSE;
		//	phglobal->bNeed_connect = TRUE;

		//	if (phglobal->cbOnStatusChanged) phglobal->cbOnStatusChanged(phglobal, errorOccupyReconnect, 0);
		//	return 1;
		//}
	}
	return 1;
}

void phddns_stop(PHGlobal *phglobal)
{
	SendKeepAlive(phglobal, UDP_OPCODE_LOGOUT);
	phglobal->tmLastSend = time(0);
	sleep(1); //ensure data sent
	DestroySockets(phglobal);
}
