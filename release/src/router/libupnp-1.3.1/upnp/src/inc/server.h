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

#ifndef GENLIB_NET_HTTP_SERVER_H
#define GENLIB_NET_HTTP_SERVER_H

#ifdef __cplusplus

#include <genlib/net/http/parseutil.h>

int http_ServerCallback( IN HttpMessage& request, IN int sockfd );

// adds 'entity' to the alias list; the entity is referred using
//
// aliasRelURL: relative url for given entity
// entity: entity to be served
// actualAlias: [possibly] modified version of aliasResURL to resolve conflicts
// returns:
//   0 : success
//   HTTP_E_OUT_OF_MEMORY
int http_AddAlias( IN const char* aliasRelURL, IN HttpEntity* entity,
    OUT xstring& actualAlias );

extern "C" {
#endif /* __cplusplus */

void http_OldServerCallback( IN const char* msg, int sockfd );

void http_SetRootDir( const char* httpRootDir );

// removes a previously added entity
// returns:
//  0: success -- alias removed
// -1: alias not found
int http_RemoveAlias( IN const char* alias );

#ifdef __cplusplus
}   /* extern C */
#endif

#endif /* GENLIB_NET_HTTP_SERVER_H */
