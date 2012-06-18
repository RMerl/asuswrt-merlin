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
* Purpose: This file defines the functions for services. It defines 
* functions for adding and removing services to and from the service table, 
* adding and accessing subscription and other attributes pertaining to the 
* service 
************************************************************************/

#include "config.h"
#include "service_table.h"

#ifdef INCLUDE_DEVICE_APIS

/************************************************************************
*	Function :	copy_subscription
*
*	Parameters :
*		subscription *in ;	Source subscription
*		subscription *out ;	Destination subscription
*
*	Description :	Makes a copy of the subscription
*
*	Return : int ;
*		HTTP_SUCCESS - On Sucess
*
*	Note :
************************************************************************/
int
copy_subscription( subscription * in,
                   subscription * out )
{
    int return_code = HTTP_SUCCESS;

    memcpy( out->sid, in->sid, SID_SIZE );
    out->sid[SID_SIZE] = 0;
    out->eventKey = in->eventKey;
    out->ToSendEventKey = in->ToSendEventKey;
    out->expireTime = in->expireTime;
    out->active = in->active;
    if( ( return_code =
          copy_URL_list( &in->DeliveryURLs, &out->DeliveryURLs ) )
        != HTTP_SUCCESS )
        return return_code;
    out->next = NULL;
    return HTTP_SUCCESS;
}

/************************************************************************
*	Function :	RemoveSubscriptionSID
*
*	Parameters :
*		Upnp_SID sid ;	subscription ID
*		service_info * service ;	service object providing the list of
*						subscriptions
*
*	Description :	Remove the subscription represented by the
*		const Upnp_SID sid parameter from the service table and update 
*		the service table.
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
RemoveSubscriptionSID( Upnp_SID sid,
                       service_info * service )
{
    subscription *finger = service->subscriptionList;
    subscription *previous = NULL;

    while( finger ) {
        if( !( strcmp( sid, finger->sid ) ) ) {
            if( previous )
                previous->next = finger->next;
            else
                service->subscriptionList = finger->next;
            finger->next = NULL;
            freeSubscriptionList( finger );
            finger = NULL;
            service->TotalSubscriptions--;
        } else {
            previous = finger;
            finger = finger->next;
        }
    }

}

/************************************************************************
*	Function :	GetSubscriptionSID
*
*	Parameters :
*		Upnp_SID sid ;	subscription ID
*		service_info * service ;	service object providing the list of
*						subscriptions
*
*	Description :	Return the subscription from the service table 
*		that matches const Upnp_SID sid value. 
*
*	Return : subscription * - Pointer to the matching subscription 
*		node;
*
*	Note :
************************************************************************/
subscription *
GetSubscriptionSID( Upnp_SID sid,
                    service_info * service )
{
    subscription *next = service->subscriptionList;
    subscription *previous = NULL;
    subscription *found = NULL;

    time_t current_time;

    while( ( next ) && ( found == NULL ) ) {
        if( !strcmp( next->sid, sid ) )
            found = next;
        else {
            previous = next;
            next = next->next;
        }
    }
    if( found ) {
        //get the current_time
        time( &current_time );
        if( ( found->expireTime != 0 )
            && ( found->expireTime < current_time ) ) {
            if( previous )
                previous->next = found->next;
            else
                service->subscriptionList = found->next;
            found->next = NULL;
            freeSubscriptionList( found );
            found = NULL;
            service->TotalSubscriptions--;
        }
    }
    return found;

}

/************************************************************************
*	Function :	GetNextSubscription
*
*	Parameters :
*		service_info * service ; service object providing the list of
*						subscriptions
*		subscription *current ;	current subscription object
*
*	Description :	Get current and valid subscription from the service 
*		table.
*
*	Return : subscription * - Pointer to the next subscription node;
*
*	Note :
************************************************************************/
subscription *
GetNextSubscription( service_info * service,
                     subscription * current )
{
    time_t current_time;
    subscription *next = NULL;
    subscription *previous = NULL;
    int notDone = 1;

    //get the current_time
    time( &current_time );
    while( ( notDone ) && ( current ) ) {
        previous = current;
        current = current->next;

        if( current == NULL ) {
            notDone = 0;
            next = current;
        } else
            if( ( current->expireTime != 0 )
                && ( current->expireTime < current_time ) ) {
            previous->next = current->next;
            current->next = NULL;
            freeSubscriptionList( current );
            current = previous;
            service->TotalSubscriptions--;
        } else if( current->active ) {
            notDone = 0;
            next = current;
        }
    }
    return next;
}

/************************************************************************
*	Function :	GetFirstSubscription
*
*	Parameters :
*		service_info *service ;	service object providing the list of
*						subscriptions
*
*	Description :	Gets pointer to the first subscription node in the 
*		service table.
*
*	Return : subscription * - pointer to the first subscription node ;
*
*	Note :
************************************************************************/
subscription *
GetFirstSubscription( service_info * service )
{
    subscription temp;
    subscription *next = NULL;

    temp.next = service->subscriptionList;
    next = GetNextSubscription( service, &temp );
    service->subscriptionList = temp.next;
    //  service->subscriptionList=next;
    return next;
}

/************************************************************************
*	Function :	freeSubscription
*
*	Parameters :
*		subscription * sub ;	subscription to be freed
*
*	Description :	Free's the memory allocated for storing the URL of 
*		the subscription.
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
freeSubscription( subscription * sub )
{
    if( sub ) {
        free_URL_list( &sub->DeliveryURLs );
    }
}

/************************************************************************
*	Function :	freeSubscriptionList
*
*	Parameters :
*		subscription * head ;	head of the subscription list
*
*	Description :	Free's memory allocated for all the subscriptions 
*		in the service table. 
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
freeSubscriptionList( subscription * head )
{
    subscription *next = NULL;

    while( head ) {
        next = head->next;
        freeSubscription( head );
        free( head );
        head = next;
    }
}

/************************************************************************
*	Function :	FindServiceId
*
*	Parameters :
*		service_table *table ;	service table
*		const char * serviceId ;string representing the service id 
*								to be found among those in the table	
*		const char * UDN ;		string representing the UDN 
*								to be found among those in the table	
*
*	Description :	Traverses through the service table and returns a 
*		pointer to the service node that matches a known service  id 
*		and a known UDN
*
*	Return : service_info * - pointer to the matching service_info node;
*
*	Note :
************************************************************************/
service_info *
FindServiceId( service_table * table,
               const char *serviceId,
               const char *UDN )
{
    service_info *finger = NULL;

    if( table ) {
        finger = table->serviceList;
        while( finger ) {
            if( ( !strcmp( serviceId, finger->serviceId ) ) &&
                ( !strcmp( UDN, finger->UDN ) ) ) {
                return finger;
            }
            finger = finger->next;
        }
    }

    return NULL;
}

/************************************************************************
*	Function :	FindServiceEventURLPath
*
*	Parameters :
*		service_table *table ;	service table
*		char * eventURLPath ;	event URL path used to find a service 
*								from the table  
*
*	Description :	Traverses the service table and finds the node whose
*		event URL Path matches a know value 
*
*	Return : service_info * - pointer to the service list node from the 
*		service table whose event URL matches a known event URL;
*
*	Note :
************************************************************************/
service_info *
FindServiceEventURLPath( service_table * table,
                         char *eventURLPath )
{
    service_info *finger = NULL;
    uri_type parsed_url;
    uri_type parsed_url_in;

    if( ( table )
        &&
        ( parse_uri
          ( eventURLPath, strlen( eventURLPath ), &parsed_url_in ) ) ) {

        finger = table->serviceList;
        while( finger ) {
            if( finger->eventURL )
                if( ( parse_uri
                      ( finger->eventURL, strlen( finger->eventURL ),
                        &parsed_url ) ) ) {

                    if( !token_cmp
                        ( &parsed_url.pathquery,
                          &parsed_url_in.pathquery ) )
                        return finger;

                }
            finger = finger->next;
        }
    }

    return NULL;
}

/************************************************************************
*	Function :	FindServiceControlURLPath
*
*	Parameters :
*		service_table * table ;	service table
*		char * controlURLPath ;	control URL path used to find a service 
*								from the table  
*
*	Description :	Traverses the service table and finds the node whose
*		control URL Path matches a know value 
*
*	Return : service_info * - pointer to the service list node from the 
*		service table whose control URL Path matches a known value;
*
*	Note :
************************************************************************/
service_info *
FindServiceControlURLPath( service_table * table,
                           char *controlURLPath )
{
    service_info *finger = NULL;
    uri_type parsed_url;
    uri_type parsed_url_in;

    if( ( table )
        &&
        ( parse_uri
          ( controlURLPath, strlen( controlURLPath ),
            &parsed_url_in ) ) ) {
        finger = table->serviceList;
        while( finger ) {
            if( finger->controlURL )
                if( ( parse_uri
                      ( finger->controlURL, strlen( finger->controlURL ),
                        &parsed_url ) ) ) {
                    if( !token_cmp
                        ( &parsed_url.pathquery,
                          &parsed_url_in.pathquery ) )
                        return finger;
                }
            finger = finger->next;
        }
    }

    return NULL;

}

/************************************************************************
*	Function :	printService
*
*	Parameters :
*		service_info *service ;Service whose information is to be printed
*		Dbg_Level level ; Debug level specified to the print function
*		Dbg_Module module ;	Debug module specified to the print function
*
*	Description :	For debugging purposes prints information from the 
*		service passed into the function.
*
*	Return : void ;
*
*	Note :
************************************************************************/
DBGONLY( void printService( service_info * service, Dbg_Level level,
                            Dbg_Module module ) {
         if( service ) {
         if( service->serviceType )
         UpnpPrintf( level, module, __FILE__, __LINE__,
                     "serviceType: %s\n", service->serviceType );
         if( service->serviceId )
         UpnpPrintf( level, module, __FILE__, __LINE__, "serviceId: %s\n",
                     service->serviceId ); if( service->SCPDURL )
         UpnpPrintf( level, module, __FILE__, __LINE__, "SCPDURL: %s\n",
                     service->SCPDURL ); if( service->controlURL )
         UpnpPrintf( level, module, __FILE__, __LINE__, "controlURL: %s\n",
                     service->controlURL ); if( service->eventURL )
         UpnpPrintf( level, module, __FILE__, __LINE__, "eventURL: %s\n",
                     service->eventURL ); if( service->UDN )
         UpnpPrintf( level, module, __FILE__, __LINE__, "UDN: %s\n\n",
                     service->UDN ); if( service->active )
         UpnpPrintf( level, module, __FILE__, __LINE__,
                     "Service is active\n" );
         else
         UpnpPrintf( level, module, __FILE__, __LINE__,
                     "Service is inactive\n" );}
         }

 )

/************************************************************************
*	Function :	printServiceList
*
*	Parameters :
*		service_info *service ;	Service whose information is to be printed
*		Dbg_Level level ;	Debug level specified to the print function
*		Dbg_Module module ;	Debug module specified to the print function
*
*	Description :	For debugging purposes prints information of each 
*		service from the service table passed into the function.
*
*	Return : void ;
*
*	Note :
************************************************************************/
    DBGONLY( void printServiceList( service_info * service,
                                    Dbg_Level level,
                                    Dbg_Module module ) {
             while( service ) {
             if( service->serviceType )
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "serviceType: %s\n", service->serviceType );
             if( service->serviceId )
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "serviceId: %s\n", service->serviceId );
             if( service->SCPDURL )
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "SCPDURL: %s\n", service->SCPDURL );
             if( service->controlURL )
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "controlURL: %s\n", service->controlURL );
             if( service->eventURL )
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "eventURL: %s\n", service->eventURL );
             if( service->UDN )
             UpnpPrintf( level, module, __FILE__, __LINE__, "UDN: %s\n\n",
                         service->UDN ); if( service->active )
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "Service is active\n" );
             else
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "Service is inactive\n" );
             service = service->next;}
             }
 )

/************************************************************************
*	Function :	printServiceTable
*
*	Parameters :
*		service_table * table ;	Service table to be printed
*		Dbg_Level level ;	Debug level specified to the print function
*		Dbg_Module module ;	Debug module specified to the print function
*
*	Description :	For debugging purposes prints the URL base of the table
*		and information of each service from the service table passed into 
*		the function.
*
*	Return : void ;
*
*	Note :
************************************************************************/
    DBGONLY( void printServiceTable( service_table * table,
                                     Dbg_Level level,
                                     Dbg_Module module ) {
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "URL_BASE: %s\n", table->URLBase );
             UpnpPrintf( level, module, __FILE__, __LINE__,
                         "Services: \n" );
             printServiceList( table->serviceList, level, module );}
 )

/************************************************************************
*	Function :	freeService
*
*	Parameters :
*		service_info *in ;	service information that is to be freed
*
*	Description :	Free's memory allocated for the various components 
*		of the service entry in the service table.
*
*	Return : void ;
*
*	Note :
************************************************************************/
     void freeService( service_info * in )
{
    if( in ) {
        if( in->serviceType )
            ixmlFreeDOMString( in->serviceType );

        if( in->serviceId )
            ixmlFreeDOMString( in->serviceId );

        if( in->SCPDURL )
            free( in->SCPDURL );

        if( in->controlURL )
            free( in->controlURL );

        if( in->eventURL )
            free( in->eventURL );

        if( in->UDN )
            ixmlFreeDOMString( in->UDN );

        if( in->subscriptionList )
            freeSubscriptionList( in->subscriptionList );

        in->TotalSubscriptions = 0;
        free( in );
    }
}

/************************************************************************
*	Function :	freeServiceList
*
*	Parameters :
*		service_info * head ;	Head of the service list to be freed
*
*	Description :	Free's memory allocated for the various components 
*		of each service entry in the service table.
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
freeServiceList( service_info * head )
{
    service_info *next = NULL;

    while( head ) {
        if( head->serviceType )
            ixmlFreeDOMString( head->serviceType );
        if( head->serviceId )
            ixmlFreeDOMString( head->serviceId );
        if( head->SCPDURL )
            free( head->SCPDURL );
        if( head->controlURL )
            free( head->controlURL );
        if( head->eventURL )
            free( head->eventURL );
        if( head->UDN )
            ixmlFreeDOMString( head->UDN );
        if( head->subscriptionList )
            freeSubscriptionList( head->subscriptionList );

        head->TotalSubscriptions = 0;
        next = head->next;
        free( head );
        head = next;
    }
}

/************************************************************************
*	Function :	freeServiceTable
*
*	Parameters :
*		service_table * table ;	Service table whose memory needs to be 
*								freed
*
*	Description : Free's dynamic memory in table.
*		(does not free table, only memory within the structure)
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
freeServiceTable( service_table * table )
{
    ixmlFreeDOMString( table->URLBase );
    freeServiceList( table->serviceList );
    table->serviceList = NULL;
    table->endServiceList = NULL;
}

/************************************************************************
*	Function :	getElementValue
*
*	Parameters :
*		IXML_Node *node ;	Input node which provides the list of child 
*							nodes
*
*	Description :	Returns the clone of the element value
*
*	Return : DOMString ;
*
*	Note : value must be freed with DOMString_free
************************************************************************/
DOMString
getElementValue( IXML_Node * node )
{
    IXML_Node *child = ( IXML_Node * ) ixmlNode_getFirstChild( node );
    DOMString temp = NULL;

    if( ( child != 0 ) && ( ixmlNode_getNodeType( child ) == eTEXT_NODE ) ) {
        temp = ixmlNode_getNodeValue( child );
        return ixmlCloneDOMString( temp );
    } else {
        return NULL;
    }
}

/************************************************************************
*	Function :	getSubElement
*
*	Parameters :
*		const char *element_name ;	sub element name to be searched for
*		IXML_Node *node ;	Input node which provides the list of child 
*							nodes
*		IXML_Node **out ;	Ouput node to which the matched child node is
*							returned.
*
*	Description :	Traverses through a list of XML nodes to find the 
*		node with the known element name.
*
*	Return : int ;
*		1 - On Success
*		0 - On Failure
*
*	Note :
************************************************************************/
int
getSubElement( const char *element_name,
               IXML_Node * node,
               IXML_Node ** out )
{

    const DOMString NodeName = NULL;
    int found = 0;

    IXML_Node *child = ( IXML_Node * ) ixmlNode_getFirstChild( node );

    ( *out ) = NULL;

    while( ( child != NULL ) && ( !found ) ) {

        switch ( ixmlNode_getNodeType( child ) ) {
            case eELEMENT_NODE:

                NodeName = ixmlNode_getNodeName( child );
                if( !strcmp( NodeName, element_name ) ) {
                    ( *out ) = child;
                    found = 1;
                    return found;
                }
                break;

            default:
                break;
        }

        child = ( IXML_Node * ) ixmlNode_getNextSibling( child );
    }

    return found;
}

/************************************************************************
*	Function :	getServiceList
*
*	Parameters :
*		IXML_Node *node ;	XML node information
*		service_info **end ; service added is returned to the output
*							parameter
*		char * URLBase ;	provides Base URL to resolve relative URL 
*
*	Description :	Returns pointer to service info after getting the 
*		sub-elements of the service info. 
*
*	Return : service_info * - pointer to the service info node ;
*
*	Note :
************************************************************************/
service_info *
getServiceList( IXML_Node * node,
                service_info ** end,
                char *URLBase )
{
    IXML_Node *serviceList = NULL;
    IXML_Node *current_service = NULL;
    IXML_Node *UDN = NULL;

    IXML_Node *serviceType = NULL;
    IXML_Node *serviceId = NULL;
    IXML_Node *SCPDURL = NULL;
    IXML_Node *controlURL = NULL;
    IXML_Node *eventURL = NULL;
    DOMString tempDOMString = NULL;
    service_info *head = NULL;
    service_info *current = NULL;
    service_info *previous = NULL;
    IXML_NodeList *serviceNodeList = NULL;
    int NumOfServices = 0;
    int i = 0;
    int fail = 0;

    if( getSubElement( "UDN", node, &UDN ) &&
        getSubElement( "serviceList", node, &serviceList ) ) {

        serviceNodeList = ixmlElement_getElementsByTagName( ( IXML_Element
                                                              * )
                                                            serviceList,
                                                            "service" );

        if( serviceNodeList != NULL ) {
            NumOfServices = ixmlNodeList_length( serviceNodeList );
            for( i = 0; i < NumOfServices; i++ ) {
                current_service = ixmlNodeList_item( serviceNodeList, i );
                fail = 0;

                if( current ) {
                    current->next =
                        ( service_info * )
                        malloc( sizeof( service_info ) );

                    previous = current;
                    current = current->next;
                } else {
                    head =
                        ( service_info * )
                        malloc( sizeof( service_info ) );
                    current = head;
                }

                if( !current ) {
                    freeServiceList( head );
                    return NULL;
                }

                current->next = NULL;
                current->controlURL = NULL;
                current->eventURL = NULL;
                current->serviceType = NULL;
                current->serviceId = NULL;
                current->SCPDURL = NULL;
                current->active = 1;
                current->subscriptionList = NULL;
                current->TotalSubscriptions = 0;

                if( !( current->UDN = getElementValue( UDN ) ) )
                    fail = 1;

                if( ( !getSubElement( "serviceType", current_service,
                                      &serviceType ) ) ||
                    ( !( current->serviceType =
                         getElementValue( serviceType ) ) ) )
                    fail = 1;

                if( ( !getSubElement( "serviceId", current_service,
                                      &serviceId ) ) ||
                    ( !
                      ( current->serviceId =
                        getElementValue( serviceId ) ) ) )
                    fail = 1;

                if( ( !
                      ( getSubElement
                        ( "SCPDURL", current_service, &SCPDURL ) ) )
                    || ( !( tempDOMString = getElementValue( SCPDURL ) ) )
                    ||
                    ( !
                      ( current->SCPDURL =
                        resolve_rel_url( URLBase, tempDOMString ) ) ) )
                    fail = 1;

                ixmlFreeDOMString( tempDOMString );
                tempDOMString = NULL;

                if( ( !
                      ( getSubElement
                        ( "controlURL", current_service, &controlURL ) ) )
                    ||
                    ( !( tempDOMString = getElementValue( controlURL ) ) )
                    ||
                    ( !
                      ( current->controlURL =
                        resolve_rel_url( URLBase, tempDOMString ) ) ) ) {
                    DBGONLY( UpnpPrintf
                             ( UPNP_INFO, GENA, __FILE__, __LINE__,
                               "BAD OR MISSING CONTROL URL" ) );
                    DBGONLY( UpnpPrintf
                             ( UPNP_INFO, GENA, __FILE__, __LINE__,
                               "CONTROL URL SET TO NULL IN SERVICE INFO" ) );
                    current->controlURL = NULL;
                    fail = 0;
                }

                ixmlFreeDOMString( tempDOMString );
                tempDOMString = NULL;

                if( ( !
                      ( getSubElement
                        ( "eventSubURL", current_service, &eventURL ) ) )
                    || ( !( tempDOMString = getElementValue( eventURL ) ) )
                    ||
                    ( !
                      ( current->eventURL =
                        resolve_rel_url( URLBase, tempDOMString ) ) ) ) {
                    DBGONLY( UpnpPrintf
                             ( UPNP_INFO, GENA, __FILE__, __LINE__,
                               "BAD OR MISSING EVENT URL" ) );
                    DBGONLY( UpnpPrintf
                             ( UPNP_INFO, GENA, __FILE__, __LINE__,
                               "EVENT URL SET TO NULL IN SERVICE INFO" ) );
                    current->eventURL = NULL;
                    fail = 0;
                }

                ixmlFreeDOMString( tempDOMString );
                tempDOMString = NULL;

                if( fail ) {
                    freeServiceList( current );

                    if( previous )
                        previous->next = NULL;
                    else
                        head = NULL;

                    current = previous;
                }

            }

            ixmlNodeList_free( serviceNodeList );
        }

        ( *end ) = current;

        return head;
    } else
        return NULL;

}

/************************************************************************
*	Function :	getAllServiceList
*
*	Parameters :
*		IXML_Node *node ;	XML node information
*		char * URLBase ;	provides Base URL to resolve relative URL 
*		service_info **out_end ; service added is returned to the output
*							parameter
*
*	Description :	Returns pointer to service info after getting the 
*		sub-elements of the service info. 
*
*	Return : service_info * ;
*
*	Note :
************************************************************************/
service_info *
getAllServiceList( IXML_Node * node,
                   char *URLBase,
                   service_info ** out_end )
{
    service_info *head = NULL;
    service_info *end = NULL;
    service_info *next_end = NULL;
    IXML_NodeList *deviceList = NULL;
    IXML_Node *currentDevice = NULL;

    int NumOfDevices = 0;
    int i = 0;

    ( *out_end ) = NULL;

    deviceList =
        ixmlElement_getElementsByTagName( ( IXML_Element * ) node,
                                          "device" );
    if( deviceList != NULL ) {
        NumOfDevices = ixmlNodeList_length( deviceList );
        for( i = 0; i < NumOfDevices; i++ ) {
            currentDevice = ixmlNodeList_item( deviceList, i );
            if( head ) {
                end->next =
                    getServiceList( currentDevice, &next_end, URLBase );
                end = next_end;
            } else
                head = getServiceList( currentDevice, &end, URLBase );

        }

        ixmlNodeList_free( deviceList );
    }

    ( *out_end ) = end;
    return head;
}

/************************************************************************
*	Function :	removeServiceTable
*
*	Parameters :
*		IXML_Node *node ;	XML node information
*		service_table *in ;	service table from which services will be 
*							removed
*
*	Description :	This function assumes that services for a particular 
*		root device are placed linearly in the service table, and in the 
*		order in which they are found in the description document
*		all services for this root device are removed from the list
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
removeServiceTable( IXML_Node * node,
                    service_table * in )
{
    IXML_Node *root = NULL;
    IXML_Node *currentUDN = NULL;
    DOMString UDN = NULL;
    IXML_NodeList *deviceList = NULL;
    IXML_Node *currentDevice = NULL;
    service_info *current_service = NULL;
    service_info *start_search = NULL;
    service_info *prev_service = NULL;
    int NumOfDevices = 0;
    int i = 0;

    if( getSubElement( "root", node, &root ) ) {
        current_service = in->serviceList;
        start_search = in->serviceList;
        deviceList =
            ixmlElement_getElementsByTagName( ( IXML_Element * ) root,
                                              "device" );
        if( deviceList != NULL ) {
            NumOfDevices = ixmlNodeList_length( deviceList );
            for( i = 0; i < NumOfDevices; i++ ) {
                currentDevice = ixmlNodeList_item( deviceList, i );
                if( ( start_search )
                    && ( ( getSubElement( "UDN", node, &currentUDN ) )
                         && ( UDN = getElementValue( currentUDN ) ) ) ) {
                    current_service = start_search;
                    //Services are put in the service table in the order in which they appear in the 
                    //description document, therefore we go through the list only once to remove a particular
                    //root device
                    while( ( current_service )
                           && ( strcmp( current_service->UDN, UDN ) ) ) {
                        current_service = current_service->next;
                        prev_service = current_service->next;
                    }
                    while( ( current_service )
                           && ( !strcmp( current_service->UDN, UDN ) ) ) {
                        if( prev_service ) {
                            prev_service->next = current_service->next;
                        } else {
                            in->serviceList = current_service->next;
                        }
                        if( current_service == in->endServiceList )
                            in->endServiceList = prev_service;
                        start_search = current_service->next;
                        freeService( current_service );
                        current_service = start_search;
                    }
                }
            }

            ixmlNodeList_free( deviceList );
        }
    }
    return 1;
}

/************************************************************************
*	Function :	addServiceTable
*
*	Parameters :
*		IXML_Node *node ;	XML node information 
*		service_table *in ;	service table that will be initialized with 
*							services
*		const char *DefaultURLBase ; Default base URL on which the URL 
*							will be returned to the service list.
*
*	Description :	Add Service to the table.
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
addServiceTable( IXML_Node * node,
                 service_table * in,
                 const char *DefaultURLBase )
{
    IXML_Node *root = NULL;
    IXML_Node *URLBase = NULL;

    service_info *tempEnd = NULL;

    if( in->URLBase ) {
        free( in->URLBase );
        in->URLBase = NULL;
    }

    if( getSubElement( "root", node, &root ) ) {
        if( getSubElement( "URLBase", root, &URLBase ) ) {
            in->URLBase = getElementValue( URLBase );
        } else {
            if( DefaultURLBase ) {
                in->URLBase = ixmlCloneDOMString( DefaultURLBase );
            } else {
                in->URLBase = ixmlCloneDOMString( "" );
            }
        }

        if( ( in->endServiceList->next =
              getAllServiceList( root, in->URLBase, &tempEnd ) ) ) {
            in->endServiceList = tempEnd;
            return 1;
        }

    }

    return 0;
}

/************************************************************************
*	Function :	getServiceTable
*
*	Parameters :
*		IXML_Node *node ;	XML node information
*		service_table *out ;	output parameter which will contain the 
*							service list and URL 
*		const char *DefaultURLBase ; Default base URL on which the URL 
*							will be returned.
*
*	Description :	Retrieve service from the table
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
getServiceTable( IXML_Node * node,
                 service_table * out,
                 const char *DefaultURLBase )
{
    IXML_Node *root = NULL;
    IXML_Node *URLBase = NULL;

    if( getSubElement( "root", node, &root ) ) {
        if( getSubElement( "URLBase", root, &URLBase ) ) {
            out->URLBase = getElementValue( URLBase );
        } else {
            if( DefaultURLBase ) {
                out->URLBase = ixmlCloneDOMString( DefaultURLBase );
            } else {
                out->URLBase = ixmlCloneDOMString( "" );
            }
        }

        if( ( out->serviceList = getAllServiceList( root, out->URLBase,
                                                    &out->
                                                    endServiceList ) ) ) {
            return 1;
        }

    }

    return 0;
}

#endif // INCLUDE_DEVICE_APIS
