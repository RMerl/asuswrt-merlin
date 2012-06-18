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
* Purpose: This file defines the functions for clients. It defines 
* functions for adding and removing clients to and from the client table, 
* adding and accessing subscription and other attributes pertaining to the 
* client  
************************************************************************/

#include "config.h"
#include "client_table.h"

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
CLIENTONLY( int copy_client_subscription( client_subscription * in,
                                          client_subscription * out ) {
            int len = strlen( in->ActualSID ) + 1;
            int len1 = strlen( in->EventURL ) + 1;
            memcpy( out->sid, in->sid, SID_SIZE );
            out->sid[SID_SIZE] = 0;
            out->ActualSID = ( char * )malloc( len );
            if( out->ActualSID == NULL )
                return UPNP_E_OUTOF_MEMORY;
            out->EventURL = ( char * )malloc( len1 );
            if( out->EventURL == NULL ) {
                free(out->ActualSID);
                return UPNP_E_OUTOF_MEMORY;
            }
            memcpy( out->ActualSID, in->ActualSID, len );
            memcpy( out->EventURL, in->EventURL, len1 );
            //copies do not get RenewEvent Ids or next
            out->RenewEventId = -1; out->next = NULL; return HTTP_SUCCESS;}

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
            void free_client_subscription( client_subscription * sub ) {
            upnp_timeout * event; ThreadPoolJob tempJob; if( sub ) {
            if( sub->ActualSID )
            free( sub->ActualSID ); if( sub->EventURL )
            free( sub->EventURL ); if( sub->RenewEventId != -1 )    //do not remove timer event of copy
            //invalid timer event id
            {
            if( TimerThreadRemove
                ( &gTimerThread, sub->RenewEventId, &tempJob ) == 0 ) {
            event = ( upnp_timeout * ) tempJob.arg;
            free_upnp_timeout( event );}
            }

            sub->RenewEventId = -1;}
            }

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
            void freeClientSubList( client_subscription * list ) {
            client_subscription * next; while( list ) {
            free_client_subscription( list );
            next = list->next; free( list ); list = next;}
            }

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
            void RemoveClientSubClientSID( client_subscription ** head,
                                           const Upnp_SID sid ) {
            client_subscription * finger = ( *head );
            client_subscription * previous = NULL; while( finger ) {
            if( !( strcmp( sid, finger->sid ) ) ) {
            if( previous )
            previous->next = finger->next;
            else
            ( *head ) = finger->next;
            finger->next = NULL;
            freeClientSubList( finger ); finger = NULL;}
            else
            {
            previous = finger; finger = finger->next;}
            }
            }

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
            client_subscription *
            GetClientSubClientSID( client_subscription * head,
                                   const Upnp_SID sid ) {
            client_subscription * next = head; while( next ) {
            if( !strcmp( next->sid, sid ) )
            break;
            else
            {
            next = next->next;}
            }
            return next;}

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
            client_subscription *
            GetClientSubActualSID( client_subscription * head,
                                   token * sid ) {
            client_subscription * next = head; while( next ) {

            if( !memcmp( next->ActualSID, sid->buff, sid->size ) )
            break;
            else
            {
            next = next->next;}
            }
            return next;}

 )
