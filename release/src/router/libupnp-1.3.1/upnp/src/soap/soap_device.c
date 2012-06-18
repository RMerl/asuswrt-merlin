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
#ifdef INCLUDE_DEVICE_APIS
#if EXCLUDE_SOAP == 0

#define SOAP_BODY "Body"
#define SOAP_URN "http://schemas.xmlsoap.org/soap/envelope/"

#define QUERY_STATE_VAR_URN "urn:schemas-upnp-org:control-1-0"

#include "upnpapi.h"
#include "parsetools.h"
#include "statcodes.h"
#include "httpparser.h"
#include "httpreadwrite.h"
#include "unixutil.h"
#include "soaplib.h"
#include "ssdplib.h"

// timeout duration in secs for transmission/reception
#define SOAP_TIMEOUT UPNP_TIMEOUT

#define SREQ_HDR_NOT_FOUND	 -1
#define SREQ_BAD_HDR_FORMAT	 -2

#define SOAP_INVALID_ACTION 401
#define SOAP_INVALID_ARGS	402
#define SOAP_OUT_OF_SYNC	403
#define SOAP_INVALID_VAR	404
#define SOAP_ACTION_FAILED	501

static const char *Soap_Invalid_Action = "Invalid Action";

//static const char* Soap_Invalid_Args = "Invalid Args";
static const char *Soap_Action_Failed = "Action Failed";
static const char *Soap_Invalid_Var = "Invalid Var";


/****************************************************************************
*	Function :	get_request_type
*
*	Parameters :
*			IN http_message_t* request :	HTTP request
*			OUT memptr* action_name :	SOAP action name
*
*	Description :	This function retrives the name of the SOAP action
*
*	Return : int
*		0 if successful else returns appropriate error.
*	Note :
****************************************************************************/
static XINLINE int
get_request_type( IN http_message_t * request,
                  OUT memptr * action_name )
{
    memptr value;
    memptr ns_value,
      dummy_quote;
    http_header_t *hdr;
    char save_char;
    char *s;
    membuffer soap_action_name;

    // find soapaction header
    //
    if( request->method == SOAPMETHOD_POST ) {
        if( httpmsg_find_hdr( request, HDR_SOAPACTION, &value )
            == NULL ) {
            return SREQ_HDR_NOT_FOUND;
        }
    } else                      // M-POST
    {
        // get NS value from MAN header
        hdr = httpmsg_find_hdr( request, HDR_MAN, &value );
        if( hdr == NULL ) {
            return SREQ_HDR_NOT_FOUND;
        }

        if( matchstr( value.buf, value.length, "%q%i ; ns = %s",
                      &dummy_quote, &ns_value ) != 0 ) {
            return SREQ_BAD_HDR_FORMAT;
        }
        // create soapaction name header
        membuffer_init( &soap_action_name );
        if( ( membuffer_assign( &soap_action_name,
                                ns_value.buf, ns_value.length )
              == UPNP_E_OUTOF_MEMORY ) ||
            ( membuffer_append_str( &soap_action_name,
                                    "-SOAPACTION" ) ==
              UPNP_E_OUTOF_MEMORY )
             ) {
            membuffer_destroy( &soap_action_name );
            return UPNP_E_OUTOF_MEMORY;
        }

        hdr = httpmsg_find_hdr_str( request, soap_action_name.buf );
        membuffer_destroy( &soap_action_name );
        if( hdr == NULL ) {
            return SREQ_HDR_NOT_FOUND;
        }

        value.buf = hdr->value.buf;
        value.length = hdr->value.length;
    }

    // determine type
    //
    save_char = value.buf[value.length];
    value.buf[value.length] = '\0';

    s = strchr( value.buf, '#' );
    if( s == NULL ) {
        value.buf[value.length] = save_char;
        return SREQ_BAD_HDR_FORMAT;
    }

    s++;                        // move to value

    if( matchstr( s, value.length - ( s - value.buf ), "%s",
                  action_name ) != PARSE_OK ) {
        value.buf[value.length] = save_char;
        return SREQ_BAD_HDR_FORMAT;
    }
    // action name or variable ?
    if( memptr_cmp( action_name, "QueryStateVariable" ) == 0 ) {
        // query variable
        action_name->buf = NULL;
        action_name->length = 0;
    }

    value.buf[value.length] = save_char;    // restore
    return 0;
}

/****************************************************************************
*	Function :	send_error_response
*
*	Parameters :
*			IN SOCKINFO *info :	socket info
*			IN int error_code :	error code
*			IN const char* err_msg :	error message
*			IN http_message_t* hmsg :	HTTP request
*
*	Description :	This function sends SOAP error response
*
*	Return : void
*
*	Note :
****************************************************************************/
static void
send_error_response( IN SOCKINFO * info,
                     IN int error_code,
                     IN const char *err_msg,
                     IN http_message_t * hmsg )
{
    int content_length;
    int timeout_secs = SOAP_TIMEOUT;
    int major,
      minor;
    const char *start_body =
        "<s:Envelope\n"
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\n"
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n"
        "<s:Fault>\n"
        "<faultcode>s:Client</faultcode>\n"
        "<faultstring>UPnPError</faultstring>\n"
        "<detail>\n"
        "<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">\n"
        "<errorCode>";

    const char *mid_body = "</errorCode>\n" "<errorDescription>";

    const char *end_body =
        "</errorDescription>\n"
        "</UPnPError>\n"
        "</detail>\n" "</s:Fault>\n" "</s:Body>\n" "</s:Envelope>\n";

    char err_code_str[30];

    membuffer headers;

    sprintf( err_code_str, "%d", error_code );

    // calc body len
    content_length = strlen( start_body ) + strlen( err_code_str ) +
        strlen( mid_body ) + strlen( err_msg ) + strlen( end_body );

    http_CalcResponseVersion( hmsg->major_version, hmsg->minor_version,
                              &major, &minor );

    // make headers
    membuffer_init( &headers );
/* -- PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld at users.sourceforge.net> */    
    if( http_MakeMessage( &headers, major, minor,
                          "RNsDsSXc" "sssss",
                          500,
                          content_length,
                          ContentTypeHeader,
                          "EXT:\r\n",
                          X_USER_AGENT,
                          start_body, err_code_str, mid_body, err_msg,
                          end_body ) != 0 ) {
        membuffer_destroy( &headers );
        return;                 // out of mem
    }
/*-- PATCH END - */
    // send err msg
    http_SendMessage( info, &timeout_secs, "b",
                      headers.buf, headers.length );

    membuffer_destroy( &headers );
}

/****************************************************************************
*	Function :	send_var_query_response
*
*	Parameters :
*			IN SOCKINFO *info :	socket info
*			IN const char* var_value :	value of the state variable
*			IN http_message_t* hmsg :	HTTP request
*
*	Description :	This function sends response of get var status
*
*	Return : void
*
*	Note :
****************************************************************************/
static XINLINE void
send_var_query_response( IN SOCKINFO * info,
                         IN const char *var_value,
                         IN http_message_t * hmsg )
{
    int content_length;
    int timeout_secs = SOAP_TIMEOUT;
    int major,
      minor;
    const char *start_body =
        "<s:Envelope\n"
        "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\n"
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n"
        "<u:QueryStateVariableResponse "
        "xmlns:u=\"urn:schemas-upnp-org:control-1-0\">\n" "<return>";

    const char *end_body =
        "</return>\n"
        "</u:QueryStateVariableResponse>\n"
        "</s:Body>\n" "</s:Envelope>\n";

    membuffer response;

    http_CalcResponseVersion( hmsg->major_version, hmsg->minor_version,
                              &major, &minor );

    content_length = strlen( start_body ) + strlen( var_value ) +
        strlen( end_body );

    // make headers
    membuffer_init( &response );
    
/* -- PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld at users.sourceforge.net> */
    if( http_MakeMessage( &response, major, minor,
                          "RNsDsSXcc" "sss",
                          HTTP_OK,
                          content_length,
                          ContentTypeHeader,
                          "EXT:\r\n",
                          X_USER_AGENT,
                          start_body, var_value, end_body ) != 0 ) {
        membuffer_destroy( &response );
        return;                 // out of mem
    }
/* -- PATCH END - */
    
    // send msg
    http_SendMessage( info, &timeout_secs, "b",
                      response.buf, response.length );

    membuffer_destroy( &response );
}

/****************************************************************************
*	Function :	get_action_node
*
*	Parameters :
*		IN IXML_Document *TempDoc :	The root DOM node.
*		IN char *NodeName :	IXML_Node name to be searched.
*		OUT IXML_Document ** RespNode :	Response/Output node.
*
*	Description :	This function separates the action node from 
*	the root DOM node.
*
*	Return :	static XINLINE int
*		0 if successful, or -1 if fails.
*
*	Note :
****************************************************************************/
static XINLINE int
get_action_node( IN IXML_Document * TempDoc,
                 IN char *NodeName,
                 OUT IXML_Document ** RespNode )
{
    IXML_Node *EnvpNode = NULL;
    IXML_Node *BodyNode = NULL;
    IXML_Node *ActNode = NULL;
    DOMString ActNodeName = NULL;
    const DOMString nodeName;
    int ret_code = -1;          // error, by default
    IXML_NodeList *nl = NULL;

    DBGONLY( UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "get_action_node(): node name =%s\n ", NodeName );
         )

        * RespNode = NULL;

    // Got the Envelope node here
    EnvpNode = ixmlNode_getFirstChild( ( IXML_Node * ) TempDoc );
    if( EnvpNode == NULL ) {
        goto error_handler;
    }

    nl = ixmlElement_getElementsByTagNameNS( ( IXML_Element * ) EnvpNode,
                                             "*", "Body" );

    if( nl == NULL ) {
        goto error_handler;
    }

    BodyNode = ixmlNodeList_item( nl, 0 );

    if( BodyNode == NULL ) {
        goto error_handler;
    }
    // Got action node here
    ActNode = ixmlNode_getFirstChild( BodyNode );
    if( ActNode == NULL ) {
        goto error_handler;
    }
    //Test whether this is the action node
    nodeName = ixmlNode_getNodeName( ActNode );
    if( nodeName == NULL ) {
        goto error_handler;
    }

    if( strstr( nodeName, NodeName ) == NULL ) {
        goto error_handler;
    } else {
        ActNodeName = ixmlPrintDocument( ActNode );
        if( ActNodeName == NULL ) {
            goto error_handler;
        }

        ret_code = ixmlParseBufferEx( ActNodeName, RespNode );
        if( ret_code != IXML_SUCCESS ) {
            ixmlFreeDOMString( ActNodeName );
            ret_code = -1;
            goto error_handler;
        }
    }

    ret_code = 0;               // success

  error_handler:

    ixmlFreeDOMString( ActNodeName );

    if( nl )
        ixmlNodeList_free( nl );
    return ret_code;
}

/****************************************************************************
*	Function :	check_soap_body
*
*	Parameters :
*		IN IXML_Document *doc :	soap body xml document
*		    IN const char *urn : 
*		    IN const char *actionName : Name of the requested action 	
*
*	Description :	This function checks the soap body xml came in the
*		SOAP request.
*
*	Return : int
*		UPNP_E_SUCCESS if successful else returns appropriate error
*
*	Note :
****************************************************************************/
static int
check_soap_body( IN IXML_Document * doc,
                 IN const char *urn,
                 IN const char *actionName )
{
    IXML_NodeList *nl = NULL;
    IXML_Node *bodyNode = NULL;
    IXML_Node *actionNode = NULL;
    const DOMString ns = NULL;
    const DOMString name = NULL;

    int ret_code = UPNP_E_INVALID_ACTION;

    nl = ixmlDocument_getElementsByTagNameNS( doc, SOAP_URN, SOAP_BODY );

    if( nl ) {
        bodyNode = ixmlNodeList_item( nl, 0 );
        if( bodyNode ) {
            actionNode = ixmlNode_getFirstChild( bodyNode );
            if( actionNode ) {
                ns = ixmlNode_getNamespaceURI( actionNode );
                name = ixmlNode_getLocalName( actionNode );

                if( ( !strcmp( actionName, name ) )
                    && ( !strcmp( urn, ns ) ) ) {
                    ret_code = UPNP_E_SUCCESS;
                }
            }
        }
        ixmlNodeList_free( nl );
    }
    return ret_code;

}

/****************************************************************************
*	Function :	check_soap_action_header
*
*	Parameters :
*		IN http_message_t *request : HTTP request
*		IN const char *urn :
*		OUT char **actionName :	 name of the SOAP action
*
*	Description :	This function checks the HTTP header of the SOAP request
*		coming from the control point
*
*	Return :	static int
*		UPNP_E_SUCCESS if successful else returns appropriate error
*
*	Note :
****************************************************************************/
static int
check_soap_action_header( IN http_message_t * request,
                          IN const char *urn,
                          OUT char **actionName )
{
    memptr header_name;
    http_header_t *soap_action_header = NULL;
    char *ns_compare = NULL;
    int tempSize = 0;
    int ret_code = UPNP_E_SUCCESS;
    char *temp_header_value = NULL;
    char *temp = NULL;
    char *temp2 = NULL;

    //check soap action header

    soap_action_header = httpmsg_find_hdr( request, HDR_SOAPACTION,
                                           &header_name );

    if( !soap_action_header ) {
        ret_code = UPNP_E_INVALID_ACTION;
        return ret_code;
    }

    if( soap_action_header->value.length <= 0 ) {
        ret_code = UPNP_E_INVALID_ACTION;
        return ret_code;
    }

    temp_header_value =
        ( char * )malloc( soap_action_header->value.length + 1 );

    if( !temp_header_value ) {
        ret_code = UPNP_E_OUTOF_MEMORY;
        free( temp_header_value );
        return ret_code;
    }

    strncpy( temp_header_value, soap_action_header->value.buf,
             soap_action_header->value.length );
    temp_header_value[soap_action_header->value.length] = 0;

    temp = strchr( temp_header_value, '#' );
    if( !temp ) {
        free( temp_header_value );
        ret_code = UPNP_E_INVALID_ACTION;
        return ret_code;
    }

    ( *temp ) = 0;              //temp make string

    //check to see if it is Query State Variable or
    //Service Action

    tempSize = strlen( urn ) + 2;

    ns_compare = ( char * )malloc( tempSize );

    if( !ns_compare ) {
        ret_code = UPNP_E_OUTOF_MEMORY;
        free( temp_header_value );
        return ret_code;
    }

    snprintf( ns_compare, tempSize, "\"%s", urn );

    if( strcmp( temp_header_value, ns_compare ) ) {
        ret_code = UPNP_E_INVALID_ACTION;
    } else {
        ret_code = UPNP_E_SUCCESS;
        temp++;
        temp2 = strchr( temp, '\"' );

        if( temp2 )             //remove ending " if present
        {
            ( *temp2 ) = 0;
        }

        if( *temp )
            ( *actionName ) = strdup( temp );
        if( !*actionName ) {
            ret_code = UPNP_E_OUTOF_MEMORY;
        }
    }

    free( temp_header_value );
    free( ns_compare );
    return ret_code;
}

/****************************************************************************
*	Function :	get_device_info
*
*	Parameters :
*		IN http_message_t* request :	HTTP request
*		IN int isQuery :	flag for a querry
*		IN IXML_Document *actionDoc :	action request document
*		OUT char device_udn[LINE_SIZE] :	Device UDN string
*		OUT char service_id[LINE_SIZE] :	Service ID string
*		OUT Upnp_FunPtr *callback :	callback function of the device 
*									application
*		OUT void** cookie :	cookie stored by device application 
*							
*	Description :	This function retrives all the information needed to 
*		process the incoming SOAP request. It finds the device and service info
*		and also the callback function to hand-over the request to the device
*		application.
*
*	Return : int
*		UPNP_E_SUCCESS if successful else returns appropriate error
*
*	Note :
****************************************************************************/
static int
get_device_info( IN http_message_t * request,
                 IN int isQuery,
                 IN IXML_Document * actionDoc,
                 OUT char device_udn[LINE_SIZE],
                 OUT char service_id[LINE_SIZE],
                 OUT Upnp_FunPtr * callback,
                 OUT void **cookie )
{
    struct Handle_Info *device_info;
    int device_hnd;
    service_info *serv_info;
    char save_char;
    int ret_code = -1;          // error by default
    char *control_url;
    char *actionName = NULL;

    // null-terminate pathquery of url
    control_url = request->uri.pathquery.buff;
    save_char = control_url[request->uri.pathquery.size];
    control_url[request->uri.pathquery.size] = '\0';

    HandleLock(  );

    if( GetDeviceHandleInfo( &device_hnd, &device_info ) != HND_DEVICE ) {
        goto error_handler;
    }

    if( ( serv_info =
          FindServiceControlURLPath( &device_info->ServiceTable,
                                     control_url ) ) == NULL ) {
        goto error_handler;
    }

    if( isQuery ) {
        ret_code = check_soap_action_header( request, QUERY_STATE_VAR_URN,
                                             &actionName );
        if( ( ret_code != UPNP_E_SUCCESS )
            && ( ret_code != UPNP_E_OUTOF_MEMORY ) ) {
            ret_code = UPNP_E_INVALID_ACTION;
            goto error_handler;
        }
        //check soap body
        ret_code =
            check_soap_body( actionDoc, QUERY_STATE_VAR_URN, actionName );
        free( actionName );
        if( ret_code != UPNP_E_SUCCESS ) {
            goto error_handler;
        }
    } else {
        ret_code = check_soap_action_header( request,
                                             serv_info->serviceType,
                                             &actionName );
        if( ( ret_code != UPNP_E_SUCCESS )
            && ( ret_code != UPNP_E_OUTOF_MEMORY ) ) {
            ret_code = UPNP_E_INVALID_SERVICE;
            goto error_handler;
        }
        //check soap body
        ret_code =
            check_soap_body( actionDoc, serv_info->serviceType,
                             actionName );
        free( actionName );
        if( ret_code != UPNP_E_SUCCESS ) {
            ret_code = UPNP_E_INVALID_SERVICE;
            goto error_handler;
        }
    }

    namecopy( service_id, serv_info->serviceId );
    namecopy( device_udn, serv_info->UDN );
    *callback = device_info->Callback;
    *cookie = device_info->Cookie;

    ret_code = 0;

  error_handler:
    control_url[request->uri.pathquery.size] = save_char;   // restore
    HandleUnlock(  );
    return ret_code;
}

/****************************************************************************
*	Function :	send_action_response
*
*	Parameters :
*		IN SOCKINFO *info :	socket info
*		IN IXML_Document *action_resp : The response document	
*		IN http_message_t* request :	action request document
*
*	Description :	This function sends the SOAP response 
*
*	Return : void
*
*	Note :
****************************************************************************/
static XINLINE void
send_action_response( IN SOCKINFO * info,
                      IN IXML_Document * action_resp,
                      IN http_message_t * request )
{
    char *xml_response = NULL;
    membuffer headers;
    int major,
      minor;
    int err_code;
    int content_length;
    int ret_code;
    int timeout_secs = SOAP_TIMEOUT;
    static char *start_body =
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap."
        "org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap."
        "org/soap/encoding/\"><s:Body>\n";
    static char *end_body = "</s:Body> </s:Envelope>";

    // init
    http_CalcResponseVersion( request->major_version,
                              request->minor_version, &major, &minor );
    membuffer_init( &headers );
    err_code = UPNP_E_OUTOF_MEMORY; // one error only

    // get xml
    xml_response = ixmlPrintDocument( ( IXML_Node * ) action_resp );
    if( xml_response == NULL ) {
        goto error_handler;
    }

    content_length = strlen( start_body ) + strlen( xml_response ) +
        strlen( end_body );

    // make headers
/* -- PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld at users.sourceforge.net> */    
    if( http_MakeMessage( &headers, major, minor, "RNsDsSXcc", HTTP_OK,   // status code
                          content_length, ContentTypeHeader, "EXT:\r\n", X_USER_AGENT // EXT header
         ) != 0 ) {
        goto error_handler;
    }
/* -- PATCH END - */

    // send whole msg
    ret_code = http_SendMessage( info, &timeout_secs, "bbbb",
                                 headers.buf, headers.length,
                                 start_body, strlen( start_body ),
                                 xml_response, strlen( xml_response ),
                                 end_body, strlen( end_body ) );

    DBGONLY( if( ret_code != 0 ) {
             UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "Failed to send response: err code = %d\n",
                         ret_code );}
     )

        err_code = 0;

  error_handler:
    ixmlFreeDOMString( xml_response );
    membuffer_destroy( &headers );
    if( err_code != 0 ) {
        // only one type of error to worry about - out of mem
        send_error_response( info, SOAP_ACTION_FAILED, "Out of memory",
                             request );
    }
}

/****************************************************************************
*	Function :	get_var_name
*
*	Parameters :
*		IN IXML_Document *TempDoc :	Document containing variable request
*		OUT char* VarName :	Name of the state varible
*
*	Description :	This function finds the name of the state variable 
*				asked in the SOAP request.
*
*	Return :	int
*		returns 0 if successful else returns -1.
*	Note :
****************************************************************************/
static XINLINE int
get_var_name( IN IXML_Document * TempDoc,
              OUT char *VarName )
{
    IXML_Node *EnvpNode = NULL;
    IXML_Node *BodyNode = NULL;
    IXML_Node *StNode = NULL;
    IXML_Node *VarNameNode = NULL;
    IXML_Node *VarNode = NULL;
    const DOMString StNodeName = NULL;
    DOMString Temp = NULL;
    int ret_val = -1;

    // Got the Envelop node here
    EnvpNode = ixmlNode_getFirstChild( ( IXML_Node * ) TempDoc );
    if( EnvpNode == NULL ) {
        goto error_handler;
    }
    // Got Body here
    BodyNode = ixmlNode_getFirstChild( EnvpNode );
    if( BodyNode == NULL ) {
        goto error_handler;
    }
    // Got action node here
    StNode = ixmlNode_getFirstChild( BodyNode );
    if( StNode == NULL ) {
        goto error_handler;
    }
    //Test whether this is the action node
    StNodeName = ixmlNode_getNodeName( StNode );
    if( StNodeName == NULL || strstr( StNodeName,
                                      "QueryStateVariable" ) == NULL ) {
        goto error_handler;
    }

    VarNameNode = ixmlNode_getFirstChild( StNode );
    if( VarNameNode == NULL ) {
        goto error_handler;
    }

    VarNode = ixmlNode_getFirstChild( VarNameNode );
    Temp = ixmlNode_getNodeValue( VarNode );
    linecopy( VarName, Temp );

    DBGONLY( UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "Received query for variable  name %s\n",
                         VarName );
         )

        ret_val = 0;            // success

  error_handler:
    return ret_val;
}

/****************************************************************************
*	Function :	handle_query_variable
*
*	Parameters :
*		IN SOCKINFO *info :	Socket info
*		IN http_message_t* request : HTTP request	
*		IN IXML_Document *xml_doc :	Document containing the variable request 
*									SOAP message
*
*	Description :	This action handles the SOAP requests to querry the 
*				state variables. This functionality has been deprecated in 
*				the UPnP V1.0 architecture
*
*	Return :	void
*
*	Note :
****************************************************************************/
static XINLINE void
handle_query_variable( IN SOCKINFO * info,
                       IN http_message_t * request,
                       IN IXML_Document * xml_doc )
{
    Upnp_FunPtr soap_event_callback;
    void *cookie;
    char var_name[LINE_SIZE];
    struct Upnp_State_Var_Request variable;
    const char *err_str;
    int err_code;

    // get var name
    if( get_var_name( xml_doc, var_name ) != 0 ) {
        send_error_response( info, SOAP_INVALID_VAR,
                             Soap_Invalid_Var, request );
        return;
    }
    // get info for event
    if( get_device_info( request, 1, xml_doc, variable.DevUDN,
                         variable.ServiceID,
                         &soap_event_callback, &cookie ) != 0 ) {
        send_error_response( info, SOAP_INVALID_VAR,
                             Soap_Invalid_Var, request );
        return;
    }

    linecopy( variable.ErrStr, "" );
    variable.ErrCode = UPNP_E_SUCCESS;
    namecopy( variable.StateVarName, var_name );
    variable.CurrentVal = NULL;
    variable.CtrlPtIPAddr = info->foreign_ip_addr;

    // send event
    soap_event_callback( UPNP_CONTROL_GET_VAR_REQUEST, &variable, cookie );

    DBGONLY( UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "Return from callback for var request\n" ) );

    // validate, and handle result
    if( variable.CurrentVal == NULL ) {
        err_code = SOAP_ACTION_FAILED;
        err_str = Soap_Action_Failed;
        send_error_response( info, SOAP_INVALID_VAR,
                             Soap_Invalid_Var, request );
        return;
    }
    if( variable.ErrCode != UPNP_E_SUCCESS ) {
        if( strlen( variable.ErrStr ) > 0 ) {
            err_code = SOAP_INVALID_VAR;
            err_str = Soap_Invalid_Var;
        } else {
            err_code = variable.ErrCode;
            err_str = variable.ErrStr;
        }
        send_error_response( info, err_code, err_str, request );
        return;
    }
    // send response
    send_var_query_response( info, variable.CurrentVal, request );
    ixmlFreeDOMString( variable.CurrentVal );

}

/****************************************************************************
*	Function :	handle_invoke_action
*
*	Parameters :
*		IN SOCKINFO *info :	Socket info
*		IN http_message_t* request : HTTP Request	
*		IN memptr action_name :	 Name of the SOAP Action
*		IN IXML_Document *xml_doc :	document containing the SOAP action 
*									request
*
*	Description :	This functions handle the SOAP action request. It checks 
*		the integrity of the SOAP action request and gives the call back to 
*		the device application.
*
*	Return : void
*
*	Note :
****************************************************************************/
static void
handle_invoke_action( IN SOCKINFO * info,
                      IN http_message_t * request,
                      IN memptr action_name,
                      IN IXML_Document * xml_doc )
{
    char save_char;
    IXML_Document *resp_node = NULL;
    struct Upnp_Action_Request action;
    Upnp_FunPtr soap_event_callback;
    void *cookie = NULL;
    int err_code;
    const char *err_str;

    action.ActionResult = NULL;

    // null-terminate
    save_char = action_name.buf[action_name.length];
    action_name.buf[action_name.length] = '\0';

    // set default error
    err_code = SOAP_INVALID_ACTION;
    err_str = Soap_Invalid_Action;

    // get action node
    if( get_action_node( xml_doc, action_name.buf, &resp_node ) == -1 ) {
        goto error_handler;
    }
    // get device info for action event
    err_code = get_device_info( request, 0, xml_doc, action.DevUDN,
                                action.ServiceID, &soap_event_callback,
                                &cookie );

    if( err_code != UPNP_E_SUCCESS ) {
        goto error_handler;
    }

    namecopy( action.ActionName, action_name.buf );
    linecopy( action.ErrStr, "" );
    action.ActionRequest = resp_node;
    action.ActionResult = NULL;
    action.ErrCode = UPNP_E_SUCCESS;
    action.CtrlPtIPAddr = info->foreign_ip_addr;

    DBGONLY( UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "Calling Callback\n" ) );

    soap_event_callback( UPNP_CONTROL_ACTION_REQUEST, &action, cookie );

    if( action.ErrCode != UPNP_E_SUCCESS ) {
        if( strlen( action.ErrStr ) <= 0 ) {
            err_code = SOAP_ACTION_FAILED;
            err_str = Soap_Action_Failed;
        } else {
            err_code = action.ErrCode;
            err_str = action.ErrStr;
        }
        goto error_handler;
    }
    // validate, and handle action error
    if( action.ActionResult == NULL ) {
        err_code = SOAP_ACTION_FAILED;
        err_str = Soap_Action_Failed;
        goto error_handler;
    }
    // send response
    send_action_response( info, action.ActionResult, request );

    err_code = 0;

    // error handling and cleanup
  error_handler:
    ixmlDocument_free( action.ActionResult );
    ixmlDocument_free( resp_node );
    action_name.buf[action_name.length] = save_char;    // restore
    if( err_code != 0 ) {
        send_error_response( info, err_code, err_str, request );
    }
}

/****************************************************************************
*	Function :	soap_device_callback
*
*	Parameters :
*		  IN http_parser_t *parser : Parsed request received by the device
*		  IN http_message_t* request :	HTTP request 
*		  INOUT SOCKINFO *info :	socket info
*
*	Description :	This is a callback called by minisever after receiving 
*		the request from the control point. This function will start 
*		processing the request. It calls handle_invoke_action to handle the
*		SOAP action
*
*	Return :	void
*
*	Note :
****************************************************************************/
void
soap_device_callback( IN http_parser_t * parser,
                      IN http_message_t * request,
                      INOUT SOCKINFO * info )
{
    int err_code;
    const char *err_str;
    memptr action_name;
    IXML_Document *xml_doc = NULL;

    // set default error
    err_code = SOAP_INVALID_ACTION;
    err_str = Soap_Invalid_Action;

    // validate: content-type == text/xml
    if( !has_xml_content_type( request ) ) {
        goto error_handler;
    }
    // type of request
    if( get_request_type( request, &action_name ) != 0 ) {
        goto error_handler;
    }
    // parse XML
    err_code = ixmlParseBufferEx( request->entity.buf, &xml_doc );
    if( err_code != IXML_SUCCESS ) {
        if( err_code == IXML_INSUFFICIENT_MEMORY ) {
            err_code = UPNP_E_OUTOF_MEMORY;
        } else {
            err_code = SOAP_ACTION_FAILED;
        }

        err_str = "XML error";
        goto error_handler;
    }

    if( action_name.length == 0 ) {
        // query var
        handle_query_variable( info, request, xml_doc );
    } else {
        // invoke action
        handle_invoke_action( info, request, action_name, xml_doc );
    }

    err_code = 0;               // no error

  error_handler:
    ixmlDocument_free( xml_doc );
    if( err_code != 0 ) {
        send_error_response( info, err_code, err_str, request );
    }
}

#endif // EXCLUDE_SOAP

#endif // INCLUDE_DEVICE_APIS
