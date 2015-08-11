#include "phglobal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>      /* Needed only for _O_RDWR definition */
#include <stdlib.h>
#include <stdio.h>

#ifndef WIN32
#include <termios.h>
#endif

#include <time.h>

const char *convert_status_code(int nCode)
{
	const char* p = "";
	switch (nCode)
	{
	case okConnecting:
		p = "okConnecting";
		break;
	case okConnected:
		p = "okConnected";
		break;
	case okAuthpassed:
		p = "okAuthpassed";
		break;
	case okDomainListed:
		p = "okDomainListed";
		break;
	case okDomainsRegistered:
		p = "okDomainsRegistered";
		break;
	case okKeepAliveRecved:
		p = "okKeepAliveRecved";
		break;
	case okRetrievingMisc:
		p = "okRetrievingMisc";
		break;
	case errorConnectFailed:
		p = "errorConnectFailed";
		break;
	case errorSocketInitialFailed:
		p = "errorSocketInitialFailed";
		break;
	case errorAuthFailed:
		p = "errorAuthFailed";
		break;
	case errorDomainListFailed:
		p = "errorDomainListFailed";
		break;
	case errorDomainRegisterFailed:
		p = "errorDomainRegisterFailed";
		break;
	case errorUpdateTimeout:
		p = "errorUpdateTimeout";
		break;
	case errorKeepAliveError:
		p = "errorKeepAliveError";
		break;
	case errorRetrying:
		p = "errorRetrying";
		break;
	case okNormal:
		p = "okNormal";
		break;
	case okNoData:
		p = "okNoData";
		break;
	case okServerER:
		p = "okServerER";
		break;
	case errorOccupyReconnect:
		p = "errorOccupyReconnect";
		break;
	case okRedirecting:
		p = "okRedirecting";
		break;
	case errorAuthBusy:
		p = "errorAuthBusy";
		break;
	case errorStatDetailInfoFailed:
		p = "errorAuthBusy";
		break;
	}

	return p;
}


const char *my_inet_ntoa(int ip)
{
	struct in_addr addr;
	addr.s_addr = ip;
	return inet_ntoa(addr);
}

void reverse_byte_order(int *in_array,int arraysize)
{

	unsigned int   i,k;
	signed char *p_data;   /*data pointer*/
	signed char *p_temp;   /*temporaty data pointer */
	int temp;

	/*printf("before %d %d\n",input_data[0],input_data[1]);*/
	p_data = (signed char *) in_array - 1;
	for ( k = 0 ; k < arraysize ; k++ )
	{
		temp = *( in_array + k );
		p_temp = ( signed char * ) ( &temp ) + 4;

		for  ( i = 0 ; i < 4 ; i++ )
		{
			*(++p_data) = *(--p_temp);
		}
	}
	/*printf("after %d %d\n",input_data[0],input_data[1]);*/

	/*free(start_ptr);*/
}

void reverse_byte_order_short(short *in_array,int arraysize)
{

	unsigned int   i,k;
	signed char *p_data;   /*data pointer*/
	signed char *p_temp;   /*temporaty data pointer */
	short temp;

	/*printf("before %d %d\n",input_data[0],input_data[1]);*/
	p_data = (signed char *) in_array - 1;
	for ( k = 0 ; k < arraysize ; k++ )
	{
		temp = *( in_array + k );
		p_temp = ( signed char * ) ( &temp ) + 2;

		for  ( i = 0 ; i < 2 ; i++ )
		{
			*(++p_data) = *(--p_temp);
		}
	}
	/*printf("after %d %d\n",input_data[0],input_data[1]);*/

	/*free(start_ptr);*/
}


static void defOnStatusChanged(PHGlobal* global, int status, int data)
{
	printf("defOnStatusChanged %s", convert_status_code(status));
	if (status == okKeepAliveRecved)
	{
		printf(", IP: %d", data);
	}
	if (status == okDomainsRegistered)
	{
		printf(", UserType: %d", data);
	}
	printf("\n");
}

static void defOnDomainRegistered(PHGlobal* global, char *domain)
{
	printf("defOnDomainRegistered %s\n", domain);
}

static void defOnUserInfo(PHGlobal* global, char *userInfo, int len)
{
	printf("defOnUserInfo %s\n", userInfo);
}

static void defOnAccountDomainInfo(PHGlobal* global, char *domainInfo, int len)
{
	printf("defOnAccountDomainInfo %s\n", domainInfo);
}

void init_global(PHGlobal *global)
{
	int x = 1;
	char *p = (char *)&x;
	if (*p) global->bBigEndian = FALSE;
	else global->bBigEndian = TRUE;

	strcpy(global->szHost,"phddns60.oray.net");
	strcpy(global->szUserID,"");
	strcpy(global->szUserPWD,"");
	strcpy(global->szBindAddress,"");
	global->nUserType = 0;
	global->nPort = 6060;

	global->bTcpUpdateSuccessed = FALSE;
	strcpy(global->szChallenge,"");
	global->nChallengeLen = 0;
	global->nChatID = global->nStartID = global->nLastResponseID = global->nAddressIndex = 0;
	global->tmLastResponse = -1;
	global->ip = 0;
	strcpy(global->szTcpConnectAddress,"");

	global->cLastResult = -1;

	global->uptime = time(0);
	global->lasttcptime = 0;

	strcpy(global->szActiveDomains[0],".");

	global->bNeed_connect = TRUE;
	global->tmLastSend = 0;

	global->m_tcpsocket = global->m_udpsocket = INVALID_SOCKET;
	global->user_data = NULL;
	global->cbOnStatusChanged = NULL;
	global->cbOnDomainRegistered = NULL;
	global->cbOnUserInfo = NULL;
	global->cbOnAccountDomainInfo = NULL;
}

void set_default_callback(PHGlobal *global)
{
	global->cbOnStatusChanged = defOnStatusChanged;
	global->cbOnDomainRegistered = defOnDomainRegistered;
	global->cbOnUserInfo = defOnUserInfo;
	global->cbOnAccountDomainInfo = defOnAccountDomainInfo;
}

