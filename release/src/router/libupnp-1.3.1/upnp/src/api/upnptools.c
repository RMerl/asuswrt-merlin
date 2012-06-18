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

#include "config.h"
#if EXCLUDE_DOM == 0
#include <stdarg.h>
#include "upnptools.h"
#include "uri.h"
#define HEADER_LENGTH 2000

//Structure to maintain a error code and string associated with the 
// error code
struct ErrorString {
    int rc;                     /* error code */
    const char *rcError;        /* error description */

};

//Intializing the array of error structures. 
struct ErrorString ErrorMessages[] = { {UPNP_E_SUCCESS, "UPNP_E_SUCCESS"},
{UPNP_E_INVALID_HANDLE, "UPNP_E_INVALID_HANDLE"},
{UPNP_E_INVALID_PARAM, "UPNP_E_INVALID_PARAM"},
{UPNP_E_OUTOF_HANDLE, "UPNP_E_OUTOF_HANDLE"},
{UPNP_E_OUTOF_CONTEXT, "UPNP_E_OUTOF_CONTEXT"},
{UPNP_E_OUTOF_MEMORY, "UPNP_E_OUTOF_MEMOR"},
{UPNP_E_INIT, "UPNP_E_INIT"},
{UPNP_E_BUFFER_TOO_SMALL, "UPNP_E_BUFFER_TOO_SMALL"},
{UPNP_E_INVALID_DESC, "UPNP_E_INVALID_DESC"},
{UPNP_E_INVALID_URL, "UPNP_E_INVALID_URL"},
{UPNP_E_INVALID_SID, "UPNP_E_INVALID_SID"},
{UPNP_E_INVALID_DEVICE, "UPNP_E_INVALID_DEVICE"},
{UPNP_E_INVALID_SERVICE, "UPNP_E_INVALID_SERVICE"},
{UPNP_E_BAD_RESPONSE, "UPNP_E_BAD_RESPONSE"},
{UPNP_E_BAD_REQUEST, "UPNP_E_BAD_REQUEST"},
{UPNP_E_INVALID_ACTION, "UPNP_E_INVALID_ACTION"},
{UPNP_E_FINISH, "UPNP_E_FINISH"},
{UPNP_E_INIT_FAILED, "UPNP_E_INIT_FAILED"},
{UPNP_E_BAD_HTTPMSG, "UPNP_E_BAD_HTTPMSG"},
{UPNP_E_NETWORK_ERROR, "UPNP_E_NETWORK_ERROR"},
{UPNP_E_SOCKET_WRITE, "UPNP_E_SOCKET_WRITE"},
{UPNP_E_SOCKET_READ, "UPNP_E_SOCKET_READ"},
{UPNP_E_SOCKET_BIND, "UPNP_E_SOCKET_BIND"},
{UPNP_E_SOCKET_CONNECT, "UPNP_E_SOCKET_CONNECT"},
{UPNP_E_OUTOF_SOCKET, "UPNP_E_OUTOF_SOCKET"},
{UPNP_E_LISTEN, "UPNP_E_LISTEN"},
{UPNP_E_EVENT_PROTOCOL, "UPNP_E_EVENT_PROTOCOL"},
{UPNP_E_SUBSCRIBE_UNACCEPTED, "UPNP_E_SUBSCRIBE_UNACCEPTED"},
{UPNP_E_UNSUBSCRIBE_UNACCEPTED, "UPNP_E_UNSUBSCRIBE_UNACCEPTED"},
{UPNP_E_NOTIFY_UNACCEPTED, "UPNP_E_NOTIFY_UNACCEPTED"},
{UPNP_E_INTERNAL_ERROR, "UPNP_E_INTERNAL_ERROR"},
{UPNP_E_INVALID_ARGUMENT, "UPNP_E_INVALID_ARGUMENT"},
{UPNP_E_OUTOF_BOUNDS, "UPNP_E_OUTOF_BOUNDS"}
};

/************************************************************************
* Function : UpnpGetErrorMessage											
*																	
* Parameters:														
*	IN int rc: error code
*																	
* Description:														
*	This functions returns the error string mapped to the error code 
* Returns: const char *
*	return either the right string or "Unknown Error"
***************************************************************************/
const char *
UpnpGetErrorMessage( IN int rc )
{
    int i;

    for( i = 0; i < sizeof( ErrorMessages ) / sizeof( ErrorMessages[0] );
         i++ ) {
        if( rc == ErrorMessages[i].rc )
            return ErrorMessages[i].rcError;

    }

    return "Unknown Error";

}

/************************************************************************
* Function : UpnpResolveURL											
*																	
* Parameters:														
*	IN char * BaseURL: Base URL string
*	IN char * RelURL: relative URL string
*	OUT char * AbsURL: Absolute URL string
* Description:														
*	This functions concatinates the base URL and relative URL to generate 
*	the absolute URL
* Returns: int
*	return either UPNP_E_SUCCESS or appropriate error
***************************************************************************/
int
UpnpResolveURL( IN const char *BaseURL,
                IN const char *RelURL,
                OUT char *AbsURL )
{
    // There is some unnecessary allocation and
    // deallocation going on here because of the way
    // resolve_rel_url was originally written and used
    // in the future it would be nice to clean this up

    char *tempRel;

    if( RelURL == NULL )
        return UPNP_E_INVALID_PARAM;

    tempRel = NULL;

    tempRel = resolve_rel_url((char*) BaseURL, (char*) RelURL );

    if( tempRel ) {
        strcpy( AbsURL, tempRel );
        free( tempRel );
    } else {
        return UPNP_E_INVALID_URL;
    }

    return UPNP_E_SUCCESS;

}

/************************************************************************
* Function : addToAction											
*																	
* Parameters:														
*	IN int response: flag to tell if the ActionDoc is for response 
*					or request
*	INOUT IXML_Document **ActionDoc: request or response document
*	IN char *ActionName: Name of the action request or response
*	IN char *ServType: Service type
*	IN char * ArgName: Name of the argument
*	IN char * ArgValue: Value of the argument
*
* Description:		
*	This function adds the argument in the action request or response. 
* This function creates the action request or response if it is a first
* argument else it will add the argument in the document
*
* Returns: int
*	returns UPNP_E_SUCCESS if successful else returns appropriate error
***************************************************************************/
static int
addToAction( IN int response,
             INOUT IXML_Document ** ActionDoc,
             IN const char *ActionName,
             IN const char *ServType,
             IN const char *ArgName,
             IN const char *ArgValue )
{
    char *ActBuff = NULL;
    IXML_Node *node = NULL;
    IXML_Element *Ele = NULL;
    IXML_Node *Txt = NULL;
    int rc = 0;

    if( ActionName == NULL || ServType == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    if( *ActionDoc == NULL ) {
        ActBuff = ( char * )malloc( HEADER_LENGTH );
        if( ActBuff == NULL ) {
            return UPNP_E_OUTOF_MEMORY;
        }

        if( response ) {
            sprintf( ActBuff,
                     "<u:%sResponse xmlns:u=\"%s\"></u:%sResponse>",
                     ActionName, ServType, ActionName );
        } else {
            sprintf( ActBuff, "<u:%s xmlns:u=\"%s\"></u:%s>",
                     ActionName, ServType, ActionName );
        }

        rc = ixmlParseBufferEx( ActBuff, ActionDoc );
        free( ActBuff );
        if( rc != IXML_SUCCESS ) {
            if( rc == IXML_INSUFFICIENT_MEMORY ) {
                return UPNP_E_OUTOF_MEMORY;
            } else {
                return UPNP_E_INVALID_DESC;
            }
        }
    }

    if( ArgName != NULL /*&& ArgValue != NULL */  ) {
        node = ixmlNode_getFirstChild( ( IXML_Node * ) * ActionDoc );
        Ele = ixmlDocument_createElement( *ActionDoc, ArgName );
        if( ArgValue ) {
            Txt = ixmlDocument_createTextNode( *ActionDoc, ArgValue );
            ixmlNode_appendChild( ( IXML_Node * ) Ele, Txt );
        }

        ixmlNode_appendChild( node, ( IXML_Node * ) Ele );
    }

    return UPNP_E_SUCCESS;
}

/************************************************************************
* Function : makeAction											
*																	
* Parameters:														
*	IN int response: flag to tell if the ActionDoc is for response 
*					or request
*	IN char * ActionName: Name of the action request or response
*	IN char * ServType: Service type
*	IN int NumArg :Number of arguments in the action request or response
*	IN char * Arg : pointer to the first argument
*	IN va_list ArgList: Argument list
*
* Description:		
*	This function creates the action request or response from the argument
* list.
* Returns: IXML_Document *
*	returns action request or response document if successful 
*	else returns NULL
***************************************************************************/
static IXML_Document *
makeAction( IN int response,
            IN const char *ActionName,
            IN const char *ServType,
            IN int NumArg,
            IN const char *Arg,
            IN va_list ArgList )
{
    const char *ArgName,
     *ArgValue;
    char *ActBuff;
    int Idx = 0;
    IXML_Document *ActionDoc;
    IXML_Node *node;
    IXML_Element *Ele;
    IXML_Node *Txt = NULL;

    if( ActionName == NULL || ServType == NULL ) {
        return NULL;
    }

    ActBuff = ( char * )malloc( HEADER_LENGTH );
    if( ActBuff == NULL ) {
        return NULL;
    }

    if( response ) {
        sprintf( ActBuff, "<u:%sResponse xmlns:u=\"%s\"></u:%sResponse>",
                 ActionName, ServType, ActionName );
    } else {
        sprintf( ActBuff, "<u:%s xmlns:u=\"%s\"></u:%s>",
                 ActionName, ServType, ActionName );
    }

    if( ixmlParseBufferEx( ActBuff, &ActionDoc ) != IXML_SUCCESS ) {
        free( ActBuff );
        return NULL;
    }

    free( ActBuff );

    if( ActionDoc == NULL ) {
        return NULL;
    }

    if( NumArg > 0 ) {
        //va_start(ArgList, Arg);
        ArgName = Arg;
        while( Idx++ != NumArg ) {
            ArgValue = va_arg( ArgList, const char * );

            if( ArgName != NULL ) {
                node = ixmlNode_getFirstChild( ( IXML_Node * ) ActionDoc );
                Ele = ixmlDocument_createElement( ActionDoc, ArgName );
                if( ArgValue ) {
                    Txt =
                        ixmlDocument_createTextNode( ActionDoc, ArgValue );
                    ixmlNode_appendChild( ( IXML_Node * ) Ele, Txt );
                }

                ixmlNode_appendChild( node, ( IXML_Node * ) Ele );
            }

            ArgName = va_arg( ArgList, const char * );
        }
        //va_end(ArgList);
    }

    return ActionDoc;
}

/************************************************************************
* Function : UpnpMakeAction											
*																	
* Parameters:														
*	IN char * ActionName: Name of the action request or response
*	IN char * ServType: Service type
*	IN int NumArg :Number of arguments in the action request or response
*	IN char * Arg : pointer to the first argument
*	IN ... : variable argument list
*	IN va_list ArgList: Argument list
*
* Description:		
*	This function creates the action request from the argument
* list. Its a wrapper function that calls makeAction function to create
* the action request.
*
* Returns: IXML_Document *
*	returns action request document if successful 
*	else returns NULL
***************************************************************************/
IXML_Document *
UpnpMakeAction( const char *ActionName,
                const char *ServType,
                int NumArg,
                const char *Arg,
                ... )
{
    va_list ArgList;
    IXML_Document *out = NULL;

    if( NumArg > 0 ) {
        va_start( ArgList, Arg );
    }

    out = makeAction( 0, ActionName, ServType, NumArg, Arg, ArgList );
    if( NumArg > 0 ) {
        va_end( ArgList );
    }

    return out;
}

/************************************************************************
* Function : UpnpMakeActionResponse											
*																	
* Parameters:														
*	IN char * ActionName: Name of the action request or response
*	IN char * ServType: Service type
*	IN int NumArg :Number of arguments in the action request or response
*	IN char * Arg : pointer to the first argument
*	IN ... : variable argument list
*	IN va_list ArgList: Argument list
*
* Description:		
*	This function creates the action response from the argument
* list. Its a wrapper function that calls makeAction function to create
* the action response.
*
* Returns: IXML_Document *
*	returns action response document if successful 
*	else returns NULL
***************************************************************************/
IXML_Document *
UpnpMakeActionResponse( const char *ActionName,
                        const char *ServType,
                        int NumArg,
                        const char *Arg,
                        ... )
{
    va_list ArgList;
    IXML_Document *out = NULL;

    if( NumArg > 0 ) {
        va_start( ArgList, Arg );
    }

    out = makeAction( 1, ActionName, ServType, NumArg, Arg, ArgList );
    if( NumArg > 0 ) {
        va_end( ArgList );
    }

    return out;
}

/************************************************************************
* Function : UpnpAddToActionResponse									
*																	
* Parameters:
*	INOUT IXML_Document **ActionResponse: action response document	
*	IN char * ActionName: Name of the action request or response
*	IN char * ServType: Service type
*	IN int ArgName :Name of argument to be added in the action response
*	IN char * ArgValue : value of the argument
*
* Description:		
*	This function adds the argument in the action response. Its a wrapper 
* function that calls addToAction function to add the argument in the 
* action response.
*
* Returns: int
*	returns UPNP_E_SUCCESS if successful 
*	else returns appropriate error
***************************************************************************/
int
UpnpAddToActionResponse( INOUT IXML_Document ** ActionResponse,
                         IN const char *ActionName,
                         IN const char *ServType,
                         IN const char *ArgName,
                         IN const char *ArgValue )
{
    return addToAction( 1, ActionResponse, ActionName, ServType, ArgName,
                        ArgValue );
}

/************************************************************************
* Function : UpnpAddToAction									
*																	
* Parameters:
*	INOUT IXML_Document **ActionDoc: action request document	
*	IN char * ActionName: Name of the action request or response
*	IN char * ServType: Service type
*	IN int ArgName :Name of argument to be added in the action response
*	IN char * ArgValue : value of the argument
*
* Description:		
*	This function adds the argument in the action request. Its a wrapper 
* function that calls addToAction function to add the argument in the 
* action request.
*
* Returns: int
*	returns UPNP_E_SUCCESS if successful 
*	else returns appropriate error
***************************************************************************/
int
UpnpAddToAction( IXML_Document ** ActionDoc,
                 const char *ActionName,
                 const char *ServType,
                 const char *ArgName,
                 const char *ArgValue )
{

    return addToAction( 0, ActionDoc, ActionName, ServType, ArgName,
                        ArgValue );
}

/************************************************************************
* Function : UpnpAddToPropertySet											
*																	
* Parameters:														
*	INOUT IXML_Document **PropSet: propertyset document
*	IN char *ArgName: Name of the argument
*	IN char *ArgValue: value of the argument
*
* Description:		
*	This function adds the argument in the propertyset node 
*
* Returns: int
*	returns UPNP_E_SUCCESS if successful else returns appropriate error
***************************************************************************/
int
UpnpAddToPropertySet( INOUT IXML_Document ** PropSet,
                      IN const char *ArgName,
                      IN const char *ArgValue )
{

    char BlankDoc[] = "<e:propertyset xmlns:e=\"urn:schemas"
        "-upnp-org:event-1-0\"></e:propertyset>";
    IXML_Node *node;
    IXML_Element *Ele;
    IXML_Element *Ele1;
    IXML_Node *Txt;
    int rc;

    if( ArgName == NULL ) {
        return UPNP_E_INVALID_PARAM;
    }

    if( *PropSet == NULL ) {
        rc = ixmlParseBufferEx( BlankDoc, PropSet );
        if( rc != IXML_SUCCESS ) {
            return UPNP_E_OUTOF_MEMORY;
        }
    }

    node = ixmlNode_getFirstChild( ( IXML_Node * ) * PropSet );

    Ele1 = ixmlDocument_createElement( *PropSet, "e:property" );
    Ele = ixmlDocument_createElement( *PropSet, ArgName );

    if( ArgValue ) {
        Txt = ixmlDocument_createTextNode( *PropSet, ArgValue );
        ixmlNode_appendChild( ( IXML_Node * ) Ele, Txt );
    }

    ixmlNode_appendChild( ( IXML_Node * ) Ele1, ( IXML_Node * ) Ele );
    ixmlNode_appendChild( node, ( IXML_Node * ) Ele1 );

    return UPNP_E_SUCCESS;
}

/************************************************************************
* Function : UpnpCreatePropertySet											
*																	
* Parameters:														
*	IN int NumArg: Number of argument that will go in the propertyset node
*	IN char * Args: argument strings
*
* Description:		
*	This function creates a propertyset node and put all the input 
*	parameters in the node as elements
*
* Returns: IXML_Document *
*	returns the document containing propertyset node.
***************************************************************************/
IXML_Document *
UpnpCreatePropertySet( IN int NumArg,
                       IN const char *Arg,
                       ... )
{
    va_list ArgList;
    int Idx = 0;
    char BlankDoc[] = "<e:propertyset xmlns:e=\"urn:schemas-"
        "upnp-org:event-1-0\"></e:propertyset>";
    const char *ArgName,
     *ArgValue;
    IXML_Node *node;
    IXML_Element *Ele;
    IXML_Element *Ele1;
    IXML_Node *Txt;
    IXML_Document *PropSet;

    if( ixmlParseBufferEx( BlankDoc, &PropSet ) != IXML_SUCCESS ) {
        return NULL;
    }

    if( NumArg < 1 ) {
        return NULL;
    }

    va_start( ArgList, Arg );
    ArgName = Arg;

    while( Idx++ != NumArg ) {
        ArgValue = va_arg( ArgList, const char * );

        if( ArgName != NULL /*&& ArgValue != NULL */  ) {
            node = ixmlNode_getFirstChild( ( IXML_Node * ) PropSet );
            Ele1 = ixmlDocument_createElement( PropSet, "e:property" );
            Ele = ixmlDocument_createElement( PropSet, ArgName );
            if( ArgValue ) {
                Txt = ixmlDocument_createTextNode( PropSet, ArgValue );
                ixmlNode_appendChild( ( IXML_Node * ) Ele, Txt );
            }

            ixmlNode_appendChild( ( IXML_Node * ) Ele1,
                                  ( IXML_Node * ) Ele );
            ixmlNode_appendChild( node, ( IXML_Node * ) Ele1 );
        }

        ArgName = va_arg( ArgList, const char * );

    }
    va_end( ArgList );
    return PropSet;
}

#endif
