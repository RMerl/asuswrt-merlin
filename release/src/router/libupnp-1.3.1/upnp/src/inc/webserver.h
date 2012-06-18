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

#ifndef GENLIB_NET_HTTP_WEBSERVER_H
#define GENLIB_NET_HTTP_WEBSERVER_H

#include <time.h>
#include "sock.h"
#include "httpparser.h"

#ifdef __cplusplus
extern "C" {
#endif


struct SendInstruction
{
   int  IsVirtualFile;
   int  IsChunkActive;
   int  IsRangeActive;
   int  IsTrailers;
   char RangeHeader[200];
   long RangeOffset;
   long ReadSendSize;  // Read from local source and send on the network.
   long RecvWriteSize; // Recv from the network and write into local file.

   //Later few more member could be added depending on the requirement.
};

/************************************************************************
* Function: web_server_init												
*																		
* Parameters:															
*	none																
*																		
* Description: Initilialize the different documents. Initialize the		
*	memory for root directory for web server. Call to initialize global 
*	XML document. Sets bWebServerState to WEB_SERVER_ENABLED			
*																		
* Returns:																
*	0 - OK																
*	UPNP_E_OUTOF_MEMORY: note: alias_content is not freed here			
************************************************************************/
int web_server_init( void );

/************************************************************************
* Function: web_server_destroy											
*																		
* Parameters:															
*	none																
*																		
* Description: Release memory allocated for the global web server root	
*	directory and the global XML document								
*	Resets the flag bWebServerState to WEB_SERVER_DISABLED				
*																		
* Returns:																
*	void																
************************************************************************/
void web_server_destroy( void );

/************************************************************************
* Function: web_server_set_alias										
*																		
* Parameters:															
*	alias_name: webserver name of alias; created by caller and freed by 
*				caller (doesn't even have to be malloc()d .)					
*	alias_content:	the xml doc; this is allocated by the caller; and	
*					freed by the web server											
*	alias_content_length: length of alias body in bytes					
*	last_modified:	time when the contents of alias were last			
*					changed (local time)											
*																		
* Description: Replaces current alias with the given alias. To remove	
*	the current alias, set alias_name to NULL.							
*																		
* Returns:																
*	0 - OK																
*	UPNP_E_OUTOF_MEMORY: note: alias_content is not freed here			
************************************************************************/
int web_server_set_alias( IN const char* alias_name,
		IN const char* alias_content, IN size_t alias_content_length,
		IN time_t last_modified );

/************************************************************************
* Function: web_server_set_root_dir										
*																		
* Parameters:															
*	IN const char* root_dir ; String having the root directory for the 
*								document		 						
*																		
* Description: Assign the path specfied by the IN const char* root_dir	
*	parameter to the global Document root directory. Also check for		
*	path names ending in '/'											
*																		
* Returns:																
*	int																	
************************************************************************/
int web_server_set_root_dir( IN const char* root_dir );

/************************************************************************
* Function: web_server_callback											*
*																		*
* Parameters:															*
*	IN http_parser_t *parser,											*
*	INOUT http_message_t* req,											*
*	IN SOCKINFO *info													*
*																		*
* Description: main entry point into web server;						*
*	handles HTTP GET and HEAD requests									*
*																		*
* Returns:																*
*	void																*
************************************************************************/
void web_server_callback( IN http_parser_t *parser, IN http_message_t* req, INOUT SOCKINFO *info );


#ifdef __cplusplus
} // extern C
#endif


#endif // GENLIB_NET_HTTP_WEBSERVER_H
