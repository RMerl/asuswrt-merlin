/*
  Copyright (c) Fabasoft R&D Software GmbH & Co KG, 2003
  oss@fabasoft.com
  Author: Bernhard Penz <bernhard.penz@fabasoft.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  *  Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  *  Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  *  The name of Fabasoft R&D Software GmbH & Co KG or any of its subsidiaries, 
     brand or product names may not be used to endorse or promote products 
     derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
  OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef WIN32

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>
#include <net-snmp/library/snmp_assert.h>
#include <net-snmp/library/winpipe.h>
#include <io.h>

static int InitUPDSocket(SOCKET *sock, struct sockaddr_in *socketaddress)
{
	*sock = 0;
	memset(socketaddress, 0, sizeof(struct sockaddr_in));

	if( (*sock = socket(AF_INET, SOCK_DGRAM, 0)) == SOCKET_ERROR)
	{
		netsnmp_assert(GetLastError() != WSANOTINITIALISED);
		return -1;
	}
	socketaddress->sin_family = AF_INET;
	socketaddress->sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
	socketaddress->sin_port = 0;

	if(bind(*sock, (struct sockaddr *) socketaddress, sizeof(struct sockaddr)) == SOCKET_ERROR)
	{
		return -1;
	}

	return 0;
}

static int ConnectUDPSocket(SOCKET *sock, struct sockaddr_in *socketaddress, SOCKET *remotesocket)
{
	int size = sizeof(struct sockaddr);
	if (getsockname(*sock, (struct sockaddr *) socketaddress, &size) == SOCKET_ERROR)
	{
		return -1;
	}

	if(size != sizeof(struct sockaddr))
	{
		return -1;
	}

	if (connect(*remotesocket, (struct sockaddr *) socketaddress, sizeof(struct sockaddr)) == SOCKET_ERROR)
	{
		return -1;
	}

	return 0;
}

static int TestUDPSend(SOCKET *sock, struct sockaddr_in *socketaddress)
{
	unsigned short port = socketaddress->sin_port;

	int bytessent = sendto(*sock, (char *) &port, sizeof(port), 0, NULL, 0);
	if(bytessent != sizeof(port))
	{
		return -1;
	}

	return 0;
}

static int TestUDPReceive(SOCKET *sock, SOCKET *remotesocket, struct sockaddr_in *remotesocketaddress)
{
	struct sockaddr_in recvfromaddress;
	unsigned short readbuffer[2];
	int size = sizeof(struct sockaddr);

	int bytesreceived = recvfrom(*sock,(char *) &readbuffer, sizeof(readbuffer), 0, (struct sockaddr *) &recvfromaddress, &size) ;
	if(bytesreceived != sizeof(unsigned short) || size != sizeof(struct sockaddr) || readbuffer[0] != (unsigned short) remotesocketaddress->sin_port || recvfromaddress.sin_family != remotesocketaddress->sin_family || recvfromaddress.sin_addr.S_un.S_addr != remotesocketaddress->sin_addr.S_un.S_addr || recvfromaddress.sin_port != remotesocketaddress->sin_port)
	{
		return -1;
	}

	return 0;
}

static void CloseUDPSocketPair(SOCKET *socketpair)
{
	if(socketpair[0] != INVALID_SOCKET)
	{
		closesocket(socketpair[0]);
	}
	if(socketpair[1] != INVALID_SOCKET)
	{
		closesocket(socketpair[1]);
	}
}

/*
	Windows unnamed pipe emulation, used to enable select()
	on a Windows machine for the CALLBACK (pipe-based) transport domain.
*/
int create_winpipe_transport(int *pipefds)
{
	SOCKET socketpair[2];
	struct sockaddr_in socketaddress[2];

	struct timeval waittime = {0, 200000};
	fd_set readset;

	if (InitUPDSocket(&socketpair[0], &socketaddress[0]))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}
	if (InitUPDSocket(&socketpair[1], &socketaddress[1]))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}

	/*
		I have two UDP sockets - now lets connect them to each other.
	*/

	if (ConnectUDPSocket(&socketpair[0], &socketaddress[0], &socketpair[1]))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}
	if(ConnectUDPSocket(&socketpair[1], &socketaddress[1], &socketpair[0]))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}

	/*
		The two sockets are connected to each other, now lets test the connection
		by sending the own port number.
	*/
	if(TestUDPSend(&socketpair[0], &socketaddress[0]))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}
	if(TestUDPSend(&socketpair[1], &socketaddress[1]))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}

	/*
		Port numbers sent, now lets select() on the socketpair and check that 
		both messages got through
	*/
	FD_ZERO(&readset);
	FD_SET(socketpair[0], &readset);
	FD_SET(socketpair[1], &readset);

	/*
		For some unknown reason the timeout setting in the select call does not have
		the desired effect, and for yet another unknown reason a Sleep(1) solves this
		problem.
	*/
	Sleep(1);
	if(select(0, &readset, NULL, NULL, &waittime) != 2 || !FD_ISSET(socketpair[0], &readset) || !FD_ISSET(socketpair[1], &readset))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}

	/*
		Check if the packets I receive were really sent by me, and nobody else
		tried to sneak.
	*/
    if(TestUDPReceive(&socketpair[0], &socketpair[1], &socketaddress[1]))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}
	if(TestUDPReceive(&socketpair[1], &socketpair[0], &socketaddress[0]))
	{
		CloseUDPSocketPair(socketpair);
		return -1;
	}

	/*
		All sanity checks passed, I can return a "UDP pipe"
	*/
	pipefds[0] = (int) socketpair[0];
	pipefds[1] = (int) socketpair[1];

	return 0;
}

#endif /* WIN32 */

