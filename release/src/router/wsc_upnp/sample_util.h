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

#ifndef SAMPLE_UTIL_H
#define SAMPLE_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <upnptools.h>
#include <ixml.h>

typedef enum {
	STATE_UPDATE = 0,
	DEVICE_ADDED =1,
	DEVICE_REMOVED=2,
	GET_VAR_COMPLETE=3
} eventType;


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
char * SampleUtil_GetElementValue(IN IXML_Element *element);

/********************************************************************************
 * SampleUtil_GetFirstServiceList
 *
 * Description: 
 *       Given a DOM node representing a UPnP Device Description Document,
 *       this routine parses the document and finds the first service list
 *       (i.e., the service list for the root device).  The service list
 *       is returned as a DOM node list. The NodeList must be freed using
 *       NodeList_free.
 *
 * Parameters:
 *   node -- The DOM node from which to extract the service list
 *
 ********************************************************************************/

IXML_NodeList *SampleUtil_GetFirstServiceList(IN IXML_Document * doc); 


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
char * SampleUtil_GetFirstDocumentItem(IN IXML_Document *doc, IN const char *item); 


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
char * SampleUtil_GetFirstElementItem(IN IXML_Element *element, IN const char *item); 


/********************************************************************************
 * SampleUtil_FindAndParseService
 *
 * Description: 
 *       This routine finds the first occurance of a service in a DOM representation
 *       of a description document and parses it.  Note that this function currently
 *       assumes that the eventURL and controlURL values in the service definitions
 *       are full URLs.  Relative URLs are not handled here.
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
int SampleUtil_FindAndParseService (
	IN IXML_Document *DescDoc,
	IN char* location,
	IN char *serviceType,
	OUT char **serviceId,
	OUT char **SCDPURL,
	OUT char **eventURL,
	OUT char **controlURL);

#ifdef __cplusplus
};
#endif

#endif /* UPNPSDK_UTIL_H */
