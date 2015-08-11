
#include "phsocket.h"
#include <time.h>

#ifdef _WIN32
#define socklen_t int
#pragma comment(lib, "Ws2_32.lib")
#else
#define INVALID_SOCKET	-1
#define closesocket close
#define ioctlsocket ioctl
inline int getSocketError()
{
	return errno;
}

#endif


BOOL phAccept(int m_hSocket, int *rSocket, struct sockaddr_in *lpsockaddr_in, int* lpsockaddr_inLen)
{
	int hTemp = accept(m_hSocket, (struct sockaddr *)lpsockaddr_in, (socklen_t *)lpsockaddr_inLen);
	if(hTemp == INVALID_SOCKET)
	{
		return FALSE;
	}
	else
	{
		*rSocket = hTemp;
	}
	return TRUE;
}


BOOL phBind(int m_hSocket, unsigned short nSocketPort, char * lpszSocketAddress)
{
	struct sockaddr_in mysockaddr_in;
	memset(&mysockaddr_in,0,sizeof(struct sockaddr_in));

	mysockaddr_in.sin_family = AF_INET;

	if(lpszSocketAddress == NULL)
		mysockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	else
	{
		long lResult = inet_addr(lpszSocketAddress);
		if(lResult == INADDR_NONE)
		{
			//m_LastError = -1;
			//return FALSE;
			lResult = INADDR_ANY;
		}
		mysockaddr_in.sin_addr.s_addr = lResult;
	}

	mysockaddr_in.sin_port = htons((u_short)nSocketPort);

	if(bind(m_hSocket, (const struct sockaddr*)&mysockaddr_in, sizeof(struct sockaddr_in)) < 0)
	{
		return FALSE;
	}
	return TRUE;
}

void phClose(int *m_hSocket)
{
	if(*m_hSocket != INVALID_SOCKET)
	{
		closesocket(*m_hSocket);
		*m_hSocket = INVALID_SOCKET;
	}
}

BOOL phConnect(int m_hSocket, char * lpszHostAddress, unsigned short nHostPort, int *nAddressIndex, char *szSelectedAddress)
{
	struct sockaddr_in mysockaddr_in;
	struct hostent * lphost = NULL;
	int index = 0;
	int total = 0;
	int i=0;
	int ret = -1;

	if (lpszHostAddress == NULL) return FALSE;
	memset(&mysockaddr_in,0,sizeof(struct sockaddr_in));

	mysockaddr_in.sin_family = AF_INET;
	mysockaddr_in.sin_addr.s_addr = inet_addr(lpszHostAddress);

	if (mysockaddr_in.sin_addr.s_addr == INADDR_NONE)
	{
		
		lphost = gethostbyname(lpszHostAddress);
		if (lphost != NULL)
		{
			while (lphost->h_addr_list[i]) {total++; i++;}

			if (nAddressIndex)
			{
				if (*nAddressIndex==-1)
				{
					if (total>1)
					{
						index = time(0)%total;
						*nAddressIndex = index;
					}
					else index = 0;
				}
				else
				{
					if (*nAddressIndex >= total) *nAddressIndex = 0;
					index = *nAddressIndex;
				}
			}
			
			mysockaddr_in.sin_addr.s_addr = ((struct in_addr *)lphost->h_addr_list[index])->s_addr;
			if (szSelectedAddress)
			{
				strcpy(szSelectedAddress,inet_ntoa(mysockaddr_in.sin_addr));
				//AfxMessageBox(szSelectedAddress);
			}
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		if (szSelectedAddress) strcpy(szSelectedAddress,inet_ntoa(mysockaddr_in.sin_addr));
	}

	mysockaddr_in.sin_port = htons((u_short)nHostPort);

	//////////////////////////////////////////////////////////////////////////

	ret = connect(m_hSocket, (const struct sockaddr*)&mysockaddr_in, sizeof(struct sockaddr_in));
	if(ret < 0)
	{
		return FALSE;
	}
	//////////////////////////////////////////////////////////////////////////
	return TRUE;
}

BOOL phrawConnect(int m_hSocket, struct sockaddr_in* psockaddr_in, int nsockaddr_inLen)
{
	int nConResult;
	if (psockaddr_in == NULL) return FALSE;

	nConResult = connect(m_hSocket, (const struct sockaddr*)psockaddr_in, nsockaddr_inLen);
	if(nConResult < 0)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL phCreate(int *m_hSocket, unsigned short nSocketPort, int nSocketType, char * lpszSocketAddress)
{
	if(*m_hSocket != INVALID_SOCKET)
	{
		return FALSE;
	}

	*m_hSocket = socket(PF_INET, nSocketType, 0);
	if(*m_hSocket == INVALID_SOCKET)
	{
		return FALSE;
	}
	if(phBind(*m_hSocket, nSocketPort, lpszSocketAddress))
		return TRUE;	//Normal exit
	phClose(m_hSocket);
	return FALSE;
}

BOOL Listen(int m_hSocket, int nConnectionBacklog/*=5*/)
{
	if(listen(m_hSocket, nConnectionBacklog) != 0)	//error occured
	{
		return FALSE;
	}
	return TRUE;
}

int phReceive(int m_hSocket, void* lpBuf, int nBufLen, int nFlag)
{
	int nResult;
	nResult = recv(m_hSocket, (char*)lpBuf, nBufLen, nFlag);
	return nResult;
}

int phReceiveFrom(int m_hSocket, void* lpBuf, int nBufLen, char *rSocketAddress, unsigned short *rSocketPort, int nFlags)
{
	struct sockaddr_in mysockaddr_in;
	int nsockaddr_inLen;
	int nResult;

	memset(&mysockaddr_in, 0, sizeof(struct sockaddr_in));

	nsockaddr_inLen = sizeof(struct sockaddr_in);

	nResult = recvfrom(m_hSocket, (char*)lpBuf, nBufLen, nFlags, (struct sockaddr*)&mysockaddr_in, (socklen_t *)&nsockaddr_inLen);
	if(nResult < 0)
	{
		return nResult;
	}
	else
	{
		*rSocketPort = ntohs(mysockaddr_in.sin_port);
		strcpy(rSocketAddress,inet_ntoa(mysockaddr_in.sin_addr));
	}
	return nResult;
}

int phrawReceiveFrom(int m_hSocket, void* lpBuf, int nBufLen, struct sockaddr_in* psockaddr_in, int* pnsockaddr_inLen, int nFlags)
{
	int nResult;
	memset(psockaddr_in, 0, sizeof(struct sockaddr_in));

	*pnsockaddr_inLen = sizeof(struct sockaddr_in);
	
	nResult = recvfrom(m_hSocket, (char*)lpBuf, nBufLen, nFlags, (struct sockaddr*)psockaddr_in,(socklen_t *) pnsockaddr_inLen);

	return nResult;
}

int phSend(int m_hSocket, void* lpBuf, int nBufLen, int nFlag)
{
	int nResult;

	nResult = send(m_hSocket, (char*)lpBuf, nBufLen, nFlag);
	return nResult;
}

int phSendTo(int m_hSocket, const void* lpBuf, int nBufLen, unsigned short nHostPort, char * lpszHostAddress, int nFlags)
{
	struct sockaddr_in mysockaddr_in;

	int nsockaddr_inLen = sizeof(struct sockaddr_in);
	int nResult;
	struct hostent * lphost = NULL;

	memset(&mysockaddr_in, 0, sizeof(struct sockaddr_in));
	mysockaddr_in.sin_family = AF_INET;

	if (lpszHostAddress == NULL)
		mysockaddr_in.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	else
	{
		mysockaddr_in.sin_addr.s_addr = inet_addr(lpszHostAddress);
		if (mysockaddr_in.sin_addr.s_addr == INADDR_NONE)
		{
			lphost = gethostbyname(lpszHostAddress);
			if (lphost != NULL)
				mysockaddr_in.sin_addr.s_addr = ((struct in_addr *)lphost->h_addr)->s_addr;
			else
			{
				return -1;
			}
		}
	}

	mysockaddr_in.sin_port = htons((u_short)nHostPort);
	nResult = sendto(m_hSocket, (char*)lpBuf, nBufLen, nFlags, (const struct sockaddr*)&mysockaddr_in, nsockaddr_inLen);
	return nResult;
}

int phrawSendTo(int m_hSocket, const void* lpBuf, int nBufLen, struct sockaddr_in* psockaddr_in, int nsockaddr_inLen, int nFlags)
{
	int nResult;
	nResult = sendto(m_hSocket, (char*)lpBuf, nBufLen, nFlags, (const struct sockaddr*)psockaddr_in, nsockaddr_inLen);
	return nResult;
}

int phReadOneLine(int m_hSocket, char * lpszBuf, int nBufLen)
{
	int nReadCount = 0;
	int nRecv = 0;
	lpszBuf[0] = '\0';
	
	for (; ; )
	{
		if (phDataReadable(m_hSocket, 30) <= 0) return 0;
		nRecv = recv(m_hSocket, lpszBuf + nReadCount, 1, 0);
		//printf("recvoneline:%s,count:%d\n",lpszBuf,nRecv);
		//if (nRecv == 0) continue;
		if (nRecv > 0)
		{
			nReadCount += nRecv;
			
			if (lpszBuf[nReadCount - 1] == '\n')
			{
				if (nReadCount > 1 && lpszBuf[nReadCount - 2] == '\r')
				{
					lpszBuf[nReadCount - 2] = '\0';
				}
				else
				{
					lpszBuf[nReadCount - 1] = '\0';
				}
				break;
			}
			
			if (nReadCount == nBufLen)
			{
				break;
			}
		}
		else
		{
			nReadCount = -1;
			break;
		}
	}
	return nReadCount;
}

int phDataReadable(int m_hSocket, int nTimeout)
{
	fd_set  fdR;  
	int ret;
	struct  timeval timeout;  
	timeout.tv_sec = nTimeout;
	timeout.tv_usec = 0;
	
	FD_ZERO(&fdR);  
	FD_SET(m_hSocket, &fdR);  
	switch (ret = select(m_hSocket+1, &fdR, NULL,NULL, &timeout)) 
	{  
	case -1: 
		return -1;
	break;
	case 0:  
		return 0;
	break; 
	default:  
		return ret;
		break;
	}  
}


void phSetBlockingMode(int m_hSocket, BOOL bBlocking)
{
	unsigned long on = 1;
	if (bBlocking) on = 0;
	if (ioctlsocket(m_hSocket, FIONBIO, &on) < 0) {
		return;
	}
	return;
}