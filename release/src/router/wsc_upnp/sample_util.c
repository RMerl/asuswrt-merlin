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

#include <stdarg.h>
#include <pthread.h>
#include "sample_util.h"

/********************************************************************************
 * SampleUtil_GetElementValue
 *
 * Description: 
 *       Given a DOM node such as <Channel>11</Channel>, this routine
 *       extracts the value (e.g., 11) from the node and returns it as 
 *       a string. The string must be freed by the caller using 
 *       free.
 *
 * Parameters:
 *   node -- The DOM node from which to extract the value
 *
 ********************************************************************************/

char *
SampleUtil_GetElementValue( IN IXML_Element * element )
{

    IXML_Node *child = ixmlNode_getFirstChild( ( IXML_Node * ) element );

    char *temp = NULL;

    if( ( child != 0 ) && ( ixmlNode_getNodeType( child ) == eTEXT_NODE ) ) {
        temp = strdup( ixmlNode_getNodeValue( child ) );
    }

    return temp;
}


/********************************************************************************
 * SampleUtil_GetFirstServiceList
 *
 * Description: 
 *       Given a DOM node representing a UPnP Device Description Document,
 *       this routine parses the document and finds the first service list
 *       (i.e., the service list for the root device).  The service list
 *       is returned as a DOM node list.
 *
 * Parameters:
 *   node -- The DOM node from which to extract the service list
 *
 ********************************************************************************/
IXML_NodeList *
SampleUtil_GetFirstServiceList(
	IN IXML_Document * doc)
{
    IXML_NodeList *ServiceList = NULL;
    IXML_NodeList *servlistnodelist = NULL;
    IXML_Node *servlistnode = NULL;

    servlistnodelist = ixmlDocument_getElementsByTagName(doc, "serviceList");
    if (servlistnodelist && ixmlNodeList_length(servlistnodelist))
	{

        /*
           we only care about the first service list, from the root device 
         */
		servlistnode = ixmlNodeList_item(servlistnodelist, 0);

        /*
           create as list of DOM nodes 
         */
		ServiceList = ixmlElement_getElementsByTagName((IXML_Element *)servlistnode, "service");
    }

	if (servlistnodelist)
		ixmlNodeList_free(servlistnodelist);

    return ServiceList;
}


/********************************************************************************
 * SampleUtil_GetFirstDocumentItem
 *
 * Description: 
 *       Given a document node, this routine searches for the first element
 *       named by the input string item, and returns its value as a string.
 *       String must be freed by caller using free.
 * Parameters:
 *   doc -- The DOM document from which to extract the value
 *   item -- The item to search for
 *
 ********************************************************************************/
char *
SampleUtil_GetFirstDocumentItem(
	IN IXML_Document * doc,
	IN const char *item)
{
    IXML_NodeList *nodeList = NULL;
    IXML_Node *textNode = NULL;
    IXML_Node *tmpNode = NULL;

    char *ret = NULL;

    nodeList = ixmlDocument_getElementsByTagName( doc, ( char * )item );

    if( nodeList ) {
        if( ( tmpNode = ixmlNodeList_item( nodeList, 0 ) ) ) {
            textNode = ixmlNode_getFirstChild( tmpNode );

            ret = strdup( ixmlNode_getNodeValue( textNode ) );
        }
    }

    if( nodeList )
        ixmlNodeList_free( nodeList );
    return ret;
}


/********************************************************************************
 * SampleUtil_GetFirstElementItem
 *
 * Description: 
 *       Given a DOM element, this routine searches for the first element
 *       named by the input string item, and returns its value as a string.
 *       The string must be freed using free.
 * Parameters:
 *   node -- The DOM element from which to extract the value
 *   item -- The item to search for
 *
 ********************************************************************************/
char *
SampleUtil_GetFirstElementItem(
	IN IXML_Element * element,
	IN const char *item)
{
    IXML_NodeList *nodeList = NULL;
    IXML_Node *textNode = NULL;
    IXML_Node *tmpNode = NULL;

    char *ret = NULL;

    nodeList = ixmlElement_getElementsByTagName( element, ( char * )item );

    if( nodeList == NULL ) {
        return NULL;
    }

    if( ( tmpNode = ixmlNodeList_item( nodeList, 0 ) ) == NULL ) {
        ixmlNodeList_free( nodeList );
        return NULL;
    }

    textNode = ixmlNode_getFirstChild( tmpNode );

    ret = strdup( ixmlNode_getNodeValue( textNode ) );

    if( !ret ) {
        ixmlNodeList_free( nodeList );
        return NULL;
    }

    ixmlNodeList_free( nodeList );

    return ret;
}


/********************************************************************************
 * SampleUtil_FindAndParseService
 *
 * Description: 
 *       This routine finds the first occurance of a service in a DOM representation
 *       of a description document and parses it.  
 *
 * Parameters:
 *   DescDoc -- The DOM description document
 *   location -- The location of the description document
 *   serviceSearchType -- The type of service to search for
 *   serviceId -- OUT -- The service ID
 *   eventURL -- OUT -- The event URL for the service
 *   controlURL -- OUT -- The control URL for the service
 *
 ********************************************************************************/
int
SampleUtil_FindAndParseService(
	IN IXML_Document * DescDoc,
                                IN char *location,
                                IN char *serviceType,
                                OUT char **serviceId,
	OUT char **SCPDURL,
                                OUT char **eventURL,
	OUT char **controlURL)
{
    int i, length, found = 0;
    int ret;
    char *tempServiceType = NULL;
    char *baseURL = NULL;
    char *base;
    char *relcontrolURL = NULL, *releventURL = NULL, *relSCPDURL = NULL;
    IXML_NodeList *serviceList = NULL;
    IXML_Element *service = NULL;

    baseURL = SampleUtil_GetFirstDocumentItem(DescDoc, "URLBase");

    if( baseURL )
        base = baseURL;
    else
        base = location;


    serviceList = SampleUtil_GetFirstServiceList(DescDoc);
    length = ixmlNodeList_length(serviceList);
    for (i = 0; i < length; i++)
	{
        service = (IXML_Element *)ixmlNodeList_item(serviceList, i);
        tempServiceType = SampleUtil_GetFirstElementItem((IXML_Element *)service, "serviceType");

        if (strcmp(tempServiceType, serviceType) == 0) 
		{
			*serviceId = SampleUtil_GetFirstElementItem(service, "serviceId");
			relSCPDURL = SampleUtil_GetFirstElementItem(service, "SCPDURL");
			relcontrolURL = SampleUtil_GetFirstElementItem(service, "controlURL");
			releventURL = SampleUtil_GetFirstElementItem(service, "eventSubURL");
			
			*SCPDURL = malloc(strlen(base) + strlen(relSCPDURL) + 1);
			if(*SCPDURL)
				ret = UpnpResolveURL(base, relSCPDURL, *SCPDURL);

			*controlURL = malloc(strlen(base) + strlen(relcontrolURL) + 1);
			if(*controlURL)
				ret = UpnpResolveURL(base, relcontrolURL, *controlURL);

			*eventURL = malloc(strlen(base) + strlen(releventURL) + 1);
			if(*eventURL)
				ret = UpnpResolveURL(base, releventURL, *eventURL);

			if(relSCPDURL)
				free(relSCPDURL);
			if(relcontrolURL)
				free(relcontrolURL);
			if(releventURL)
				free(releventURL);
            relSCPDURL = relcontrolURL = releventURL = NULL;

            found = 1;
            break;
        }

        if (tempServiceType)
            free(tempServiceType);
        tempServiceType = NULL;
    }

    if( tempServiceType )
        free( tempServiceType );
    if( serviceList )
        ixmlNodeList_free( serviceList );
    if( baseURL )
        free( baseURL );

    return ( found );
}


