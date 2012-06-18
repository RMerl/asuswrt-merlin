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

#ifndef URLCONFIG_H
#define URLCONFIG_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// functions available only if the web server is included 

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************
*	Function :	configure_urlbase
*
*	Parameters :
*		INOUT IXML_Document *doc ;	IXML Description document
*		IN const struct sockaddr_in* serverAddr ;	socket address object
*					providing the IP address and port information
*		IN const char* alias ;	string containing the alias
*		IN time_t last_modified ;	time when the XML document was 
*					downloaded
*		OUT char docURL[LINE_SIZE] ;	buffer to hold the URL of the 
*					document.
*
*	Description : Configure the full URL for the description document.
*		Create the URL document and add alias, description information.
*		The doc is added to the web server to be served using the given 
*		alias.
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success
*		UPNP_E_OUTOF_MEMORY - Default Error
*
*	Note :
****************************************************************************/
int configure_urlbase( INOUT IXML_Document *doc,
    IN const struct sockaddr_in* serverAddr,
    IN const char* alias,
	IN time_t last_modified,
    OUT char docURL[LINE_SIZE] );


#ifdef __cplusplus
} // extern C
#endif

#endif /* URLCONFIG_H */

