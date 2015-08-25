#ifndef __PHSOCKET__H__
#define __PHSOCKET__H__

#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>

#endif
#ifndef BOOL
#define BOOL unsigned int
#endif

#ifndef TRUE	
#define TRUE 1
#endif

#ifndef FALSE	
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

int phReadOneLine(int m_hSocket, char * lpszBuf, int nBufLen);
void phSetBlockingMode(int m_hSocket, BOOL bBlocking);

BOOL	phAccept(int m_hSocket, int *rSocket, struct sockaddr_in* lpSockAddr, int* lpSockAddrLen);
BOOL	phBind(int m_hSocket, unsigned short nSocketPort, char *lpszSocketAddress);
void	phClose(int *m_hSocket);
BOOL	phConnect(int m_hSocket, char *lpszHostAddress, unsigned short nHostPort, int *nAddressIndex, char *szSelectedAddress);
BOOL	phrawConnect(int m_hSocket, struct sockaddr_in* pSockAddr, int nSockAddrLen);
BOOL	phListen(int m_hSocket, int nConnectionBacklog);
int		phReceive(int m_hSocket, void* lpBuf, int nBufLen, int nFlag);
int		phReceiveFrom(int m_hSocket, void* lpBuf, int nBufLen, char *rSocketAddress, unsigned short *rSocketPort, int nFlags);
int		phrawReceiveFrom(int m_hSocket, void* lpBuf, int nBufLen, struct sockaddr_in* pSockAddr, int* pnSockAddrLen, int nFlags);
int		phSend(int m_hSocket, void* lpBuf, int nBufLen, int nFlag);
int		phSendTo(int m_hSocket, const void* lpBuf, int nBufLen, unsigned short nHostPort, char *lpszHostAddress, int nFlags);
int		phrawSendTo(int m_hSocket, const void* lpBuf, int nBufLen, struct sockaddr_in* pSockAddr, int nSockAddrLen, int nFlags);
BOOL	phCreate(int *m_hSocket, unsigned short nSocketPort, int nSocketType, char *lpszSocketAddress);
int		phDataReadable(int m_hSocket, int nTimeout); //in Seconds

#endif