///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef GENLIB_NET_SOCK_H
#define GENLIB_NET_SOCK_H

#include "util.h"

#include <netinet/in.h>

//Following variable is not defined under winsock.h
#ifndef SD_RECEIVE
#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02
#endif


typedef struct 
{
	int socket;		// handle/descriptor to a socket

    // the following two fields are filled only in incoming requests;
    struct in_addr foreign_ip_addr;
    unsigned short foreign_ip_port;
    
} SOCKINFO;

#ifdef __cplusplus
#extern "C" {
#endif

/************************************************************************
*	Function :	sock_init
*
*	Parameters :
*		OUT SOCKINFO* info ;	Socket Information Object
*		IN int sockfd ;			Socket Descriptor
*
*	Description :	Assign the passed in socket descriptor to socket 
*		descriptor in the SOCKINFO structure.
*
*	Return : int;
*		UPNP_E_SUCCESS	
*		UPNP_E_OUTOF_MEMORY
*		UPNP_E_SOCKET_ERROR
*	Note :
************************************************************************/
int sock_init( OUT SOCKINFO* info, IN int sockfd );

/************************************************************************
*	Function :	sock_init_with_ip
*
*	Parameters :
*		OUT SOCKINFO* info ;				Socket Information Object
*		IN int sockfd ;						Socket Descriptor
*		IN struct in_addr foreign_ip_addr ;	Remote IP Address
*		IN unsigned short foreign_ip_port ;	Remote Port number
*
*	Description :	Calls the sock_init function and assigns the passed in
*		IP address and port to the IP address and port in the SOCKINFO
*		structure.
*
*	Return : int;
*		UPNP_E_SUCCESS	
*		UPNP_E_OUTOF_MEMORY
*		UPNP_E_SOCKET_ERROR
*
*	Note :
************************************************************************/
int sock_init_with_ip( OUT SOCKINFO* info, IN int sockfd, 
        IN struct in_addr foreign_ip_addr, IN unsigned short foreign_ip_port );

/************************************************************************
*	Function :	sock_read
*
*	Parameters :
*		IN SOCKINFO *info ;	Socket Information Object
*		OUT char* buffer ;	Buffer to get data to  
*		IN size_t bufsize ;	Size of the buffer
*	    IN int *timeoutSecs ;	timeout value
*
*	Description :	Reads data on socket in sockinfo
*
*	Return : int;
*		numBytes - On Success, no of bytes received		
*		UPNP_E_TIMEDOUT - Timeout
*		UPNP_E_SOCKET_ERROR - Error on socket calls
*
*	Note :
************************************************************************/
int sock_read( IN SOCKINFO *info, OUT char* buffer, IN size_t bufsize,
		    		 INOUT int *timeoutSecs );

/************************************************************************
*	Function :	sock_write
*
*	Parameters :
*		IN SOCKINFO *info ;	Socket Information Object
*		IN char* buffer ;	Buffer to send data from 
*		IN size_t bufsize ;	Size of the buffer
*	    IN int *timeoutSecs ;	timeout value
*
*	Description :	Writes data on the socket in sockinfo
*
*	Return : int;
*		numBytes - On Success, no of bytes sent		
*		UPNP_E_TIMEDOUT - Timeout
*		UPNP_E_SOCKET_ERROR - Error on socket calls
*
*	Note :
************************************************************************/
int sock_write( IN SOCKINFO *info, IN char* buffer, IN size_t bufsize,
		    		 INOUT int *timeoutSecs );

/************************************************************************
*	Function :	sock_destroy
*
*	Parameters :
*		INOUT SOCKINFO* info ;	Socket Information Object
*		int ShutdownMethod ;	How to shutdown the socket. Used by  
*								sockets's shutdown() 
*
*	Description :	Shutsdown the socket using the ShutdownMethod to 
*		indicate whether sends and receives on the socket will be 
*		dis-allowed. After shutting down the socket, closesocket is called
*		to release system resources used by the socket calls.
*
*	Return : int;
*		UPNP_E_SOCKET_ERROR on failure
*		UPNP_E_SUCCESS on success
*
*	Note :
************************************************************************/
int sock_destroy( INOUT SOCKINFO* info,int );

#ifdef __cplusplus
}	// #extern "C"
#endif


#endif // GENLIB_NET_SOCK_H
