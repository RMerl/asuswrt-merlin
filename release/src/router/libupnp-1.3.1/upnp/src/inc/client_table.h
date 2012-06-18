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

#ifndef _CLIENT_TABLE
#define _CLIENT_TABLE

#ifdef __cplusplus
extern "C" {
#endif

#include "upnp.h"

#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include "uri.h"
#include "service_table.h"

#include "TimerThread.h"
#include "upnp_timeout.h"

extern TimerThread gTimerThread;

CLIENTONLY(
typedef struct CLIENT_SUBSCRIPTION {
  Upnp_SID sid;
  char * ActualSID;
  char * EventURL;
  int RenewEventId;
  struct CLIENT_SUBSCRIPTION * next;
} client_subscription;

/************************************************************************
*	Function :	copy_client_subscription
*
*	Parameters :
*		client_subscription * in ;	- source client subscription
*		client_subscription * out ;	- destination client subscription
*
*	Description :	Make a copy of the client subscription data
*
*	Return : int ;
*		UPNP_E_OUTOF_MEMORY - On Failure to allocate memory
*		HTTP_SUCCESS - On Success
*
*	Note :
************************************************************************/
int copy_client_subscription(client_subscription * in, client_subscription * out);

/************************************************************************
*	Function :	free_client_subscription
*
*	Parameters :
*		client_subscription * sub ;	- Client subscription to be freed
*
*	Description :	Free memory allocated for client subscription data.
*		Remove timer thread associated with this subscription event.
*
*	Return : void ;
*
*	Note :
************************************************************************/
void free_client_subscription(client_subscription * sub);


/************************************************************************
*	Function :	freeClientSubList
*
*	Parameters :
*		client_subscription * list ; Client subscription 
*
*	Description :	Free the client subscription table.
*
*	Return : void ;
*
*	Note :
************************************************************************/
void freeClientSubList(client_subscription * list);

/************************************************************************
*	Function :	RemoveClientSubClientSID
*
*	Parameters :
*		client_subscription **head ; Head of the subscription list	
*		const Upnp_SID sid ;		 Subscription ID to be mactched
*
*	Description :	Remove the client subscription matching the 
*		subscritpion id represented by the const Upnp_SID sid parameter 
*		from the table and update the table.
*
*	Return : void ;
*
*	Note :
************************************************************************/
void RemoveClientSubClientSID(client_subscription **head, 
				       const Upnp_SID sid);

/************************************************************************
*	Function :	GetClientSubClientSID
*
*	Parameters :
*		client_subscription *head ; Head of the subscription list	
*		const Upnp_SID sid ;		Subscription ID to be matched
*
*	Description :	Return the client subscription from the client table 
*		that matches const Upnp_SID sid subscrition id value. 
*
*	Return : client_subscription * ; The matching subscription
*
*	Note :
************************************************************************/
client_subscription * GetClientSubClientSID(client_subscription *head
						     , const Upnp_SID sid);

/************************************************************************
*	Function :	GetClientSubActualSID
*
*	Parameters :
*		client_subscription *head ;	Head of the subscription list		
*		token * sid ;				Subscription ID to be matched
*
*	Description :	Returns the client subscription from the client 
*		subscription table that has the matching token * sid buffer
*		value.
*
*	Return : client_subscription * ; The matching subscription
*
*	Note :
************************************************************************/
client_subscription * GetClientSubActualSID(client_subscription *head
						     , token * sid);
)

#ifdef __cplusplus
}
#endif

#endif /*	_CLIENT_TABLE */
