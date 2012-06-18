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

/** @name Optional Tool APIs
 *  The Linux SDK for UPnP Devices contains some additional, optional 
 *  utility APIs that can be helpful in writing applications using the 
 *  SDK. These additional APIs can be compiled out in order to save code 
 *  size in the SDK. Refer to the README for details.
 */

//@{

#ifndef UPNP_TOOLS_H
#define UPNP_TOOLS_H

#include "upnp.h"

// Function declarations only if tools compiled into the library
#if UPNP_HAVE_TOOLS

#ifdef __cplusplus
extern "C" {
#endif

/** {\bf UpnpResolveURL} combines a base URL and a relative URL into
 *  a single absolute URL.  The memory for {\bf AbsURL} needs to be
 *  allocated by the caller and must be large enough to hold the
 *  {\bf BaseURL} and {\bf RelURL} combined.
 *
 *  @return [int] An integer representing one of the following:
 *    \begin{itemize}
 *      \item {\tt UPNP_E_SUCCESS}: The operation completed successfully.
 *      \item {\tt UPNP_E_INVALID_PARAM}: {\bf RelURL} is {\tt NULL}.
 *      \item {\tt UPNP_E_INVALID_URL}: The {\bf BaseURL} / {\bf RelURL} 
 *              combination does not form a valid URL.
 *      \item {\tt UPNP_E_OUTOF_MEMORY}: Insufficient resources exist to 
 *              complete this operation.
 *    \end{itemize}
 */

int UpnpResolveURL(
    IN const char * BaseURL,  /** The base URL to combine. */
    IN const char * RelURL,   /** The relative URL to {\bf BaseURL}. */
    OUT char * AbsURL   /** A pointer to a buffer to store the 
                            absolute URL. */
    );

/** {\bf UpnpMakeAction} creates an action request packet based on its input 
 *  parameters (status variable name and value pair). Any number of input 
 *  parameters can be passed to this function but every input variable name 
 *  should have a matching value argument. 
 *   
 *  @return [IXML_Document*] The action node of {\bf Upnp_Document} type or 
 *                      {\tt NULL} if the operation failed.
 */

IXML_Document* UpnpMakeAction(
    IN const char * ActionName, /** The action name. */
    IN const char * ServType,   /** The service type.  */
    IN int NumArg,              /** Number of argument pairs to be passed. */ 
    IN const char * Arg,        /** Status variable name and value pair. */
    IN ...                   /*  Other status variable name and value pairs. */
    );

/** {\bf UpnpAddToAction} creates an action request packet based on its input 
 *  parameters (status variable name and value pair). This API is specially 
 *  suitable inside a loop to add any number input parameters into an existing
 *  action. If no action document exists in the beginning then a 
 *  {\bf Upnp_Document} variable initialized with {\tt NULL} should be passed 
 *  as a parameter.
 *
 *  @return [int] An integer representing one of the following:
 *    \begin{itemize}
 *      \item {\tt UPNP_E_SUCCESS}: The operation completed successfully.
 *      \item {\tt UPNP_E_INVALID_PARAM}: One or more of the parameters 
 *                                        are invalid.
 *      \item {\tt UPNP_E_OUTOF_MEMORY}: Insufficient resources exist to 
 *              complete this operation.
 *    \end{itemize}
 */

int UpnpAddToAction(
        IN OUT IXML_Document ** ActionDoc, 
	                              /** A pointer to store the action 
				          document node. */
        IN const char * ActionName,   /** The action name. */
        IN const char * ServType,     /** The service type.  */
        IN const char * ArgName,      /** The status variable name. */
        IN const char * ArgVal        /** The status variable value.  */
        );

/** {\bf UpnpMakeActionResponse} creates an action response packet based 
 *  on its output parameters (status variable name and value pair). Any  
 *  number of input parameters can be passed to this function but every output
 *  variable name should have a matching value argument. 
 *   
 *  @return [IXML_Document*] The action node of {\bf Upnp_Document} type or 
 *                           {\tt NULL} if the operation failed.
 */

IXML_Document* UpnpMakeActionResponse(
    IN const char * ActionName, /** The action name. */
    IN const char * ServType,   /** The service type.  */
    IN int NumArg,              /** The number of argument pairs passed. */  
    IN const char * Arg,        /** The status variable name and value pair. */
    IN ...                   /*  Other status variable name and value pairs. */
    );

/** {\bf UpnpAddToActionResponse} creates an action response
 *  packet based on its output parameters (status variable name
 *  and value pair). This API is especially suitable inside
 *  a loop to add any number of input parameters into an existing action 
 *  response. If no action document exists in the beginning, a 
 *  {\bf Upnp_Document} variable initialized with {\tt NULL} should be passed 
 *  as a parameter.
 *
 *  @return [int] An integer representing one of the following:
 *    \begin{itemize}
 *      \item {\tt UPNP_E_SUCCESS}: The operation completed successfully.
 *      \item {\tt UPNP_E_INVALID_PARAM}: One or more of the parameters 
 *                                        are invalid.
 *      \item {\tt UPNP_E_OUTOF_MEMORY}: Insufficient resources exist to 
 *              complete this operation.
 *    \end{itemize}
 */

int UpnpAddToActionResponse(
        IN OUT IXML_Document ** ActionResponse, 
	                                   /** Pointer to a document to 
					       store the action document 
					       node. */
        IN const char * ActionName,        /** The action name. */
        IN const char * ServType,          /** The service type.  */
        IN const char * ArgName,           /** The status variable name. */
        IN const char * ArgVal             /** The status variable value.  */
        );

/** {\bf UpnpAddToPropertySet} can be used when an application needs to 
 *  transfer the status of many variables at once. It can be used 
 *  (inside a loop) to add some extra status variables into an existing
 *  property set. If the application does not already have a property
 *  set document, the application should create a variable initialized 
 *  with {\tt NULL} and pass that as the first parameter.
 *  
 *  @return [int] An integer representing one of the following:
 *    \begin{itemize}
 *      \item {\tt UPNP_E_SUCCESS}: The operation completed successfully.
 *      \item {\tt UPNP_E_INVALID_PARAM}: One or more of the parameters 
 *                                        are invalid.
 *      \item {\tt UPNP_E_OUTOF_MEMORY}: Insufficient resources exist to 
 *              complete this operation.
 *    \end{itemize}
 *
 */

int UpnpAddToPropertySet(
    IN OUT IXML_Document **PropSet,    
                                  /** A pointer to the document containing 
				      the property set document node. */
    IN const char * ArgName,      /** The status variable name. */  
    IN const char * ArgVal        /** The status variable value.  */
    );

/** {\bf UpnpCreatePropertySet} creates a property set  
 *  message packet. Any number of input parameters can be passed  
 *  to this function but every input variable name should have 
 *  a matching value input argument.
 *  
 *  @return [IXML_Document*] {\tt NULL} on failure, or the property-set 
 *                           document node.
 *
 */

IXML_Document* UpnpCreatePropertySet(
    IN int NumArg,        /** The number of argument pairs passed. */
    IN const char* Arg,   /** The status variable name and value pair. */
    IN ...
    );

/** {\bf UpnpGetErrorMessage} converts an SDK error code into a 
 *  string error message suitable for display.  The memory returned
 *  from this function should NOT be freed.
 *
 *  @return [char*] An ASCII text string representation of the error message 
 *                  associated with the error code. 
 */

const char * UpnpGetErrorMessage(
        int errorcode  /** The SDK error code to convert. */
        );

//@}

#ifdef __cplusplus
}
#endif

#endif // UPNP_HAVE_TOOLS

#endif // UPNP_TOOLS_H


