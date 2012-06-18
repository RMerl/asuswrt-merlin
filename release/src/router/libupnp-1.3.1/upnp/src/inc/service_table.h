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

#ifndef _SERVICE_TABLE
#define _SERVICE_TABLE

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "uri.h"
#include "ixml.h"

#include "upnp.h"
#include <stdio.h>
#include <malloc.h>
#include <time.h>

#define SID_SIZE  41

DEVICEONLY(

typedef struct SUBSCRIPTION {
  Upnp_SID sid;
  int eventKey;
  int ToSendEventKey;
  time_t expireTime;
  int active;
  URL_list DeliveryURLs;
  struct SUBSCRIPTION *next;
} subscription;


typedef struct SERVICE_INFO {
  DOMString serviceType;
  DOMString serviceId;
  char		*SCPDURL ;
  char		*controlURL;
  char		*eventURL;
  DOMString UDN;
  int		active;
  int		TotalSubscriptions;
  subscription			*subscriptionList;
  struct SERVICE_INFO	 *next;
} service_info;

typedef struct SERVICE_TABLE {
  DOMString URLBase;
  service_info *serviceList;
  service_info *endServiceList;
} service_table;


/*			Functions for Subscriptions				*/

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
int copy_subscription(subscription *in, subscription *out);

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
void RemoveSubscriptionSID(Upnp_SID sid, service_info * service);

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
subscription * GetSubscriptionSID(Upnp_SID sid,service_info * service); 
  
//returns a pointer to the subscription with the SID, NULL if not found

subscription * CheckSubscriptionSID(Upnp_SID sid,service_info * service);

//returns a pointer to the first subscription
subscription * GetFirstSubscription(service_info *service);

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
subscription * GetNextSubscription(service_info * service, subscription *current);

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
void freeSubscription(subscription * sub);

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
void freeSubscriptionList(subscription * head);

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
service_info *FindServiceId( service_table * table, 
			     const char * serviceId, const char * UDN);

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
service_info * FindServiceEventURLPath( service_table *table,
					  char * eventURLPath
					 );

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
service_info * FindServiceControlURLPath( service_table *table,
					  char * controlURLPath);

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
DBGONLY(void printService(service_info *service,Dbg_Level
				   level,
				   Dbg_Module module));

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
DBGONLY(void printServiceList(service_info *service,
				       Dbg_Level level, Dbg_Module module));

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
DBGONLY(void printServiceTable(service_table *
					table,Dbg_Level
					level,Dbg_Module module));

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
void freeService(service_info * in);

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
void freeServiceList(service_info * head);


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
void freeServiceTable(service_table * table);

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
int removeServiceTable(IXML_Node *node,
				service_table *in);


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
int addServiceTable(IXML_Node *node, service_table *in, const char *DefaultURLBase);

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
int getServiceTable(IXML_Node *node, service_table * out, const char * DefaultURLBase);


/*						Misc helper functions						   */


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
DOMString getElementValue(IXML_Node *node);

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
int getSubElement(const char *element_name, IXML_Node *node, 
		  IXML_Node **out);


)	/* DEVICEONLY */

#ifdef __cplusplus
}
#endif

#endif /* _SERVICE_TABLE */
