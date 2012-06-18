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

/************************************************************************
* Purpose: This file implements the sockets functionality 
************************************************************************/

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include "sock.h"
#include "upnp.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "unixutil.h"

#ifndef MSG_NOSIGNAL
 #define MSG_NOSIGNAL 0
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
*
*	Note :
************************************************************************/
int
sock_init( OUT SOCKINFO * info,
           IN int sockfd )
{
    assert( info );

    memset( info, 0, sizeof( SOCKINFO ) );

    info->socket = sockfd;

    return UPNP_E_SUCCESS;
}

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
int
sock_init_with_ip( OUT SOCKINFO * info,
                   IN int sockfd,
                   IN struct in_addr foreign_ip_addr,
                   IN unsigned short foreign_ip_port )
{
    int ret;

    ret = sock_init( info, sockfd );
    if( ret != UPNP_E_SUCCESS ) {
        return ret;
    }

    info->foreign_ip_addr = foreign_ip_addr;
    info->foreign_ip_port = foreign_ip_port;

    return UPNP_E_SUCCESS;
}

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
int
sock_destroy( INOUT SOCKINFO * info,
              int ShutdownMethod )
{
    shutdown( info->socket, ShutdownMethod );
    if( UpnpCloseSocket( info->socket ) == -1 ) {
        return UPNP_E_SOCKET_ERROR;
    }

    return UPNP_E_SUCCESS;
}

/************************************************************************
*	Function :	sock_read_write
*
*	Parameters :
*		IN SOCKINFO *info ;	Socket Information Object
*		OUT char* buffer ;	Buffer to get data to or send data from 
*		IN size_t bufsize ;	Size of the buffer
*	    IN int *timeoutSecs ;	timeout value
*		IN xboolean bRead ;	Boolean value specifying read or write option
*
*	Description :	Receives or sends data. Also returns the time taken
*		to receive or send data.
*
*	Return :int ;
*		numBytes - On Success, no of bytes received or sent		
*		UPNP_E_TIMEDOUT - Timeout
*		UPNP_E_SOCKET_ERROR - Error on socket calls
*
*	Note :
************************************************************************/
static int
sock_read_write( IN SOCKINFO * info,
                 OUT char *buffer,
                 IN size_t bufsize,
                 IN int *timeoutSecs,
                 IN xboolean bRead )
{
    int retCode;
    fd_set readSet;
    fd_set writeSet;
    struct timeval timeout;
    int numBytes;
    time_t start_time = time( NULL );
    int sockfd = info->socket;
    long bytes_sent = 0,
      byte_left = 0,
      num_written;

    if( *timeoutSecs < 0 ) {
        return UPNP_E_TIMEDOUT;
    }

    FD_ZERO( &readSet );
    FD_ZERO( &writeSet );
    if( bRead ) {
        FD_SET( ( unsigned )sockfd, &readSet );
    } else {
        FD_SET( ( unsigned )sockfd, &writeSet );
    }

    timeout.tv_sec = *timeoutSecs;
    timeout.tv_usec = 0;

    while( TRUE ) {
        if( *timeoutSecs == 0 ) {
            retCode =
                select( sockfd + 1, &readSet, &writeSet, NULL, NULL );
        } else {
            retCode =
                select( sockfd + 1, &readSet, &writeSet, NULL, &timeout );
        }

        if( retCode == 0 ) {
            return UPNP_E_TIMEDOUT;
        }
        if( retCode == -1 ) {
            if( errno == EINTR )
                continue;
            return UPNP_E_SOCKET_ERROR; // error
        } else {
            break;              // read or write
        }
    }

    if( bRead ) {
        // read data
        numBytes = recv( sockfd, buffer, bufsize, MSG_NOSIGNAL );
    } else {
        byte_left = bufsize;
        bytes_sent = 0;
        while( byte_left > 0 ) {
            // write data
            num_written =
                send( sockfd, buffer + bytes_sent, byte_left,
                      MSG_DONTROUTE | MSG_NOSIGNAL );
            if( num_written == -1 ) {
                return num_written;
            }

            byte_left = byte_left - num_written;
            bytes_sent += num_written;
        }

        numBytes = bytes_sent;
    }

    if( numBytes < 0 ) {
        return UPNP_E_SOCKET_ERROR;
    }
    // subtract time used for reading/writing
    if( *timeoutSecs != 0 ) {
        *timeoutSecs -= time( NULL ) - start_time;
    }

    return numBytes;
}

/************************************************************************
*	Function :	sock_read
*
*	Parameters :
*		IN SOCKINFO *info ;	Socket Information Object
*		OUT char* buffer ;	Buffer to get data to  
*		IN size_t bufsize ;	Size of the buffer
*	    IN int *timeoutSecs ;	timeout value
*
*	Description :	Calls sock_read_write() for reading data on the 
*		socket
*
*	Return : int;
*		Values returned by sock_read_write() 
*
*	Note :
************************************************************************/
int
sock_read( IN SOCKINFO * info,
           OUT char *buffer,
           IN size_t bufsize,
           INOUT int *timeoutSecs )
{
    return sock_read_write( info, buffer, bufsize, timeoutSecs, TRUE );
}

/************************************************************************
*	Function :	sock_write
*
*	Parameters :
*		IN SOCKINFO *info ;	Socket Information Object
*		IN char* buffer ;	Buffer to send data from 
*		IN size_t bufsize ;	Size of the buffer
*	    IN int *timeoutSecs ;	timeout value
*
*	Description :	Calls sock_read_write() for writing data on the 
*		socket
*
*	Return : int;
*		sock_read_write()
*
*	Note :
************************************************************************/
int
sock_write( IN SOCKINFO * info,
            IN char *buffer,
            IN size_t bufsize,
            INOUT int *timeoutSecs )
{
    return sock_read_write( info, buffer, bufsize, timeoutSecs, FALSE );
}
