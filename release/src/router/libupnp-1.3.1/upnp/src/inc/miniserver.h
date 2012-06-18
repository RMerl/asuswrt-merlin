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

#ifndef MINISERVER_H
#define MINISERVER_H

#include "sock.h"
#include "httpparser.h"

extern SOCKET gMiniServerStopSock;

typedef struct MServerSockArray {
  int miniServerSock;     //socket for listening for miniserver
                          //requests
  int miniServerStopSock; //socket for stopping miniserver 
  int ssdpSock; //socket for incoming advertisments and search requests

  int stopPort;
  int miniServerPort;

  CLIENTONLY(int ssdpReqSock;) //socket for sending search 
       //requests and receiving
       // search replies
      
} MiniServerSockArray;

//typedef void (*MiniServerCallback) ( const char* document, int sockfd );

typedef void (*MiniServerCallback) ( IN http_parser_t *parser,
									 IN http_message_t* request, 
									 IN SOCKINFO *info );

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************
*	Function :	SetHTTPGetCallback
*
*	Parameters :
*		MiniServerCallback callback ; - HTTP Callback to be invoked 
*
*	Description :	Set HTTP Get Callback
*
*	Return :	void
*
*	Note :
************************************************************************/
void SetHTTPGetCallback( MiniServerCallback callback );

/************************************************************************
*	Function :	SetSoapCallback
*
*	Parameters :
*		MiniServerCallback callback ; - SOAP Callback to be invoked 
*
*	Description :	Set SOAP Callback
*
*	Return :	void
*
*	Note :
************************************************************************/
void SetSoapCallback( MiniServerCallback callback );

/************************************************************************
*	Function :	SetGenaCallback
*
*	Parameters :
*		MiniServerCallback callback ; - GENA Callback to be invoked
*
*	Description :	Set GENA Callback
*
*	Return :	void
*
*	Note :
************************************************************************/
void SetGenaCallback( MiniServerCallback callback );

/************************************************************************
*	Function :	StartMiniServer
*
*	Parameters :
*		unsigned short listen_port ; Port on which the server listens for 
*									incoming connections
*
*	Description :	Initialize the sockets functionality for the 
*		Miniserver. Initialize a thread pool job to run the MiniServer
*		and the job to the thread pool. If listen port is 0, port is 
*		dynamically picked
*
*		Use timer mechanism to start the MiniServer, failure to meet the 
*		allowed delay aborts the attempt to launch the MiniServer.
*
*	Return : int ;
*		Actual port socket is bound to - On Success: 
*		A negative number UPNP_E_XXX - On Error   			
*	Note :
************************************************************************/
int StartMiniServer( unsigned short listen_port );

/************************************************************************
*	Function :	StopMiniServer
*
*	Parameters :
*		void ;	
*
*	Description :	Stop and Shutdown the MiniServer and free socket 
*		resources.
*
*	Return : int ;
*		Always returns 0 
*
*	Note :
************************************************************************/
int StopMiniServer( void );


#ifdef __cplusplus
}   /* extern C */
#endif

#endif /* MINISERVER_H */
