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
#ifdef INCLUDE_CLIENT_APIS
#if EXCLUDE_SOAP == 0

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "miniserver.h"
#include "membuffer.h"
#include "httpparser.h"
#include "httpreadwrite.h"
#include "statcodes.h"
#include "parsetools.h"
#include "upnpapi.h"
#include "soaplib.h"
#include "uri.h"
#include "upnp.h"

#include "unixutil.h"

#define SOAP_ACTION_RESP	1
#define SOAP_VAR_RESP		2
//#define SOAP_ERROR_RESP       3
#define SOAP_ACTION_RESP_ERROR  3
#define SOAP_VAR_RESP_ERROR   4

/****************************************************************************
*	Function :	dom_cmp_name
*
*	Parameters :
*			IN char *name :	lookup name
*			IN IXML_Node *node : xml node
*
*	Description : This function compares 'name' and node's name	
*
*	Return : int
*		0 if both are equal; 1 if not equal, and UPNP_E_OUTOF_MEMORY
*
*	Note :
****************************************************************************/
static int
dom_cmp_name( IN char *name,
              IN IXML_Node * node )
{
    const DOMString node_name = NULL;
    memptr nameptr,
      dummy;
    int ret_code;

    assert( name );
    assert( node );

    node_name = ixmlNode_getNodeName( node );
    if( node_name == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    if( strcmp( name, node_name ) == 0 ) {
        ret_code = 0;
    } else if( matchstr( ( char * )node_name, strlen( node_name ),
                         "%s:%s%0", &dummy, &nameptr ) == PARSE_OK &&
               strcmp( nameptr.buf, name ) == 0 ) {
        ret_code = 0;
    } else {
        ret_code = 1;           // names are not the same
    }

    return ret_code;
}

/****************************************************************************
*	Function :	dom_find_node
*
*	Parameters :
*			IN char* node_name : name of the node	
*			IN IXML_Node *start_node :	complete xml node
*			OUT IXML_Node ** matching_node : matched node
*
*	Description :	This function goes thru each child of 'start_node' 
*		looking for a node having the name 'node_name'. 
*
*	Return : int
*		return UPNP_E_SUCCESS if successful else returns appropriate error
*
*	Note :
****************************************************************************/
static int
dom_find_node( IN char *node_name,
               IN IXML_Node * start_node,
               OUT IXML_Node ** matching_node )
{
    IXML_Node *node;

    // invalid args
    if( node_name == NULL || start_node == NULL ) {
        return UPNP_E_NOT_FOUND;
    }

    node = ixmlNode_getFirstChild( start_node );
    while( node != NULL ) {
        // match name
        if( dom_cmp_name( node_name, node ) == 0 ) {
            *matching_node = node;
            return UPNP_E_SUCCESS;
        }
        // free and next node
        node = ixmlNode_getNextSibling( node ); // next node
    }

    return UPNP_E_NOT_FOUND;
}

/****************************************************************************
*	Function :	dom_find_deep_node
*
*	Parameters :
*			IN char* names[] : array of names
*			IN int num_names :	size of array
*			IN IXML_Node *start_node : Node from where it should should be 
*										searched	
*			OUT IXML_Node ** matching_node : Node that matches the last name
*											of the array
*
*	Description :	This function searches for the node specifed by the last 
*		name in the 'name' array.
*
*	Return : int
*		return UPNP_E_SUCCESS if successful else returns appropriate error
*	Note :
****************************************************************************/
static int
dom_find_deep_node( IN char *names[],
                    IN int num_names,
                    IN IXML_Node * start_node,
                    OUT IXML_Node ** matching_node )
{
    int i;
    IXML_Node *node;
    IXML_Node *match_node;

    assert( num_names > 0 );

    node = start_node;
    if( dom_cmp_name( names[0], start_node ) == 0 ) {
        if( num_names == 1 ) {
            *matching_node = start_node;
            return UPNP_E_SUCCESS;
        }
    }

    for( i = 1; i < num_names; i++ ) {
        if( dom_find_node( names[i], node, &match_node ) !=
            UPNP_E_SUCCESS ) {
            return UPNP_E_NOT_FOUND;
        }

        if( i == num_names - 1 ) {
            *matching_node = match_node;
            return UPNP_E_SUCCESS;
        }

        node = match_node;      // try again
    }

    return UPNP_E_NOT_FOUND;    // this line not reached
}

/****************************************************************************
*	Function :	get_node_value
*
*	Parameters :
*			IN IXML_Node *node : input node	
*
*	Description :	This function returns the value of the text node
*
*	Return : DOMString
*		string containing the node value
*
*	Note :The given node must have a text node as its first child
****************************************************************************/
static DOMString
get_node_value( IN IXML_Node * node )
{
    IXML_Node *text_node = NULL;
    DOMString text_value = NULL;

    text_node = ixmlNode_getFirstChild( node );
    if( text_node == NULL ) {
        return NULL;
    }

    text_value = ixmlNode_getNodeValue( text_node );
    return text_value;
}

/****************************************************************************
*	Function :	get_host_and_path
*
*	Parameters :
*			IN char *ctrl_url :	URL 
*			OUT memptr *host :	host string
*			OUT memptr *path :	path string
*			OUT uri_type* url :	URL type
*
*	Description :	This function retrives the host and path from the 
*		control URL
*
*	Return : int
*		returns 0 on sucess; -1 on error
*
*	Note :
****************************************************************************/
static XINLINE int
get_host_and_path( IN char *ctrl_url,
                   OUT memptr * host,
                   OUT memptr * path,
                   OUT uri_type * url )
{
    if( parse_uri( ctrl_url, strlen( ctrl_url ), url ) != HTTP_SUCCESS ) {
        return -1;
    }
    host->buf = url->hostport.text.buff;
    host->length = url->hostport.text.size;

    path->buf = url->pathquery.buff;
    path->length = url->pathquery.size;

    return 0;
}

/****************************************************************************
*	Function :	get_action_name
*
*	Parameters :
*			IN char* action :	string containing action name
*			OUT memptr* name : name of the action	
*
*	Description :	This functions retirves the action name in the buffer
*
*	Return : int
*		returns 0 on success; -1 on error
*
*	Note :
****************************************************************************/
static XINLINE int
get_action_name( IN char *action,
                 OUT memptr * name )
{
    memptr dummy;
    int ret_code;

    ret_code =
        matchstr( action, strlen( action ), " <%s:%s", &dummy, name );

    return ret_code == PARSE_OK ? 0 : -1;
}

/****************************************************************************
*	Function :	add_man_header
*
*	Parameters :
*			INOUT membuffer* headers :	HTTP header
*
*	Description :	This function adds "MAN" field in the HTTP header
*
*	Return : int
*		returns 0 on success; UPNP_E_OUTOFMEMORY on error
*
*	Note :
****************************************************************************/
static XINLINE int
add_man_header( INOUT membuffer * headers )
{
    char *soap_action_hdr;
    char *man_hdr = "MAN: \"http://schemas.xmlsoap.org/soap/envelope/\"; "
        "ns=01\r\n01-";

    // change POST to M-POST
    if( membuffer_insert( headers, "M-", 2, 0 ) != 0 ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    soap_action_hdr = strstr( headers->buf, "SOAPACTION:" );
    assert( soap_action_hdr != NULL );  // can't fail

    // insert MAN header
    if( membuffer_insert( headers, man_hdr, strlen( man_hdr ),
                          soap_action_hdr - headers->buf ) != 0 ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    return 0;
}

/****************************************************************************
*	Function :	soap_request_and_response
*
*	Parameters :
*		IN membuffer* request :	request that will be sent to the device
*		IN uri_type* destination_url :	destination address string
*		OUT http_parser_t *response :	response from the device
*
*	Description :	This function sends the control point's request to the 
*		device and receives a response from it.
*
*	Return : int
*
*	Note :
****************************************************************************/
static int
soap_request_and_response( IN membuffer * request,
                           IN uri_type * destination_url,
                           OUT http_parser_t * response )
{
    int ret_code;

    ret_code = http_RequestAndResponse( destination_url, request->buf,
                                        request->length,
                                        SOAPMETHOD_POST,
                                        UPNP_TIMEOUT, response );
    if( ret_code != 0 ) {
        httpmsg_destroy( &response->msg );
        return ret_code;
    }
    // method-not-allowed error
    if( response->msg.status_code == HTTP_METHOD_NOT_ALLOWED ) {
        ret_code = add_man_header( request );   // change to M-POST msg
        if( ret_code != 0 ) {
            return ret_code;
        }

        httpmsg_destroy( &response->msg );  // about to reuse response

        // try again
        ret_code = http_RequestAndResponse( destination_url, request->buf,
                                            HTTPMETHOD_MPOST,
                                            request->length, UPNP_TIMEOUT,
                                            response );
        if( ret_code != 0 ) {
            httpmsg_destroy( &response->msg );
        }

    }

    return ret_code;
}

/****************************************************************************
*	Function :	get_response_value
*
*	Parameters :
*			IN http_message_t* hmsg :	HTTP response message
*			IN int code :	return code in the HTTP response
*			IN char*name :	name of the action
*			OUT int *upnp_error_code :	UPnP error code
*			OUT IXML_Node ** action_value :	SOAP response node 
*			OUT DOMString * str_value : state varible value ( in the case of 
*							querry state variable request)	
*
*	Description :	This function handles the response coming back from the 
*		device. This function parses the response and gives back the SOAP 
*		response node.
*
*	Return : int
*		return the type of the SOAP message if successful else returns 
*	appropriate error.
*
*	Note :
****************************************************************************/
static int
get_response_value( IN http_message_t * hmsg,
                    IN int code,
                    IN char *name,
                    OUT int *upnp_error_code,
                    OUT IXML_Node ** action_value,
                    OUT DOMString * str_value )
{
    IXML_Node *node = NULL;
    IXML_Node *root_node = NULL;
    IXML_Node *error_node = NULL;
    IXML_Document *doc = NULL;
    char *node_str = NULL;
    char *temp_str = NULL;
    DOMString error_node_str = NULL;
    int err_code;
    xboolean done = FALSE;
    char *names[5];
    DOMString nodeValue;

    err_code = UPNP_E_BAD_RESPONSE; // default error

    // only 200 and 500 status codes are relevant
    if( ( hmsg->status_code != HTTP_OK &&
          hmsg->status_code != HTTP_INTERNAL_SERVER_ERROR ) ||
        !has_xml_content_type( hmsg ) ) {

        goto error_handler;
    }

    if( ixmlParseBufferEx( hmsg->entity.buf, &doc ) != IXML_SUCCESS ) {

        goto error_handler;
    }

    root_node = ixmlNode_getFirstChild( ( IXML_Node * ) doc );
    if( root_node == NULL ) {

        goto error_handler;
    }

    if( code == SOAP_ACTION_RESP ) {
        //
        // try reading soap action response
        //
        assert( action_value != NULL );

        *action_value = NULL;

        names[0] = "Envelope";
        names[1] = "Body";
        names[2] = name;
        if( dom_find_deep_node( names, 3, root_node, &node ) ==
            UPNP_E_SUCCESS ) {
            node_str = ixmlPrintDocument( node );
            if( node_str == NULL ) {
                err_code = UPNP_E_OUTOF_MEMORY;
                goto error_handler;
            }

            if( ixmlParseBufferEx( node_str,
                                   ( IXML_Document ** ) action_value ) !=
                IXML_SUCCESS ) {
                err_code = UPNP_E_BAD_RESPONSE;
                goto error_handler;
            }

            err_code = SOAP_ACTION_RESP;
            done = TRUE;
        }
    } else if( code == SOAP_VAR_RESP ) {
        // try reading var response
        assert( str_value != NULL );
        *str_value = NULL;

        names[0] = "Envelope";
        names[1] = "Body";
        names[2] = "QueryStateVariableResponse";
        names[3] = "return";
        if( dom_find_deep_node( names, 4, root_node, &node )
            == UPNP_E_SUCCESS ) {
            nodeValue = get_node_value( node );
            if( nodeValue == NULL ) {
                goto error_handler;
            }

            *str_value = ixmlCloneDOMString( nodeValue );
            err_code = SOAP_VAR_RESP;
            done = TRUE;
        }
    }

    if( !done ) {
        // not action or var resp; read error code and description
        *str_value = NULL;

        names[0] = "Envelope";
        names[1] = "Body";
        names[2] = "Fault";
        names[3] = "detail";
        names[4] = "UPnPError";
        if( dom_find_deep_node( names, 5, root_node, &error_node )
            != UPNP_E_SUCCESS ) {
            goto error_handler;
        }

        if( dom_find_node( "errorCode", error_node, &node )
            != UPNP_E_SUCCESS ) {
            goto error_handler;
        }

        temp_str = get_node_value( node );
        if( temp_str == NULL ) {
            goto error_handler;
        }

        *upnp_error_code = atoi( temp_str );
        if( *upnp_error_code < 400 ) {
            err_code = *upnp_error_code;
            goto error_handler; // bad SOAP error code
        }

        if( code == SOAP_VAR_RESP ) {
            if( dom_find_node( "errorDescription", error_node, &node )
                != UPNP_E_SUCCESS ) {
                goto error_handler;
            }

            nodeValue = get_node_value( node );
            if( nodeValue == NULL ) {
                goto error_handler;
            }
            *str_value = ixmlCloneDOMString( nodeValue );
            if( *str_value == NULL ) {
                goto error_handler;
            }
            err_code = SOAP_VAR_RESP_ERROR;
        }

        else if( code == SOAP_ACTION_RESP ) {
            error_node_str = ixmlPrintDocument( error_node );
            if( error_node_str == NULL ) {
                err_code = UPNP_E_OUTOF_MEMORY;
                goto error_handler;
            }

            if( ixmlParseBufferEx( error_node_str,
                                   ( IXML_Document ** ) action_value ) !=
                IXML_SUCCESS ) {
                err_code = UPNP_E_BAD_RESPONSE;

                goto error_handler;
            }
            err_code = SOAP_ACTION_RESP_ERROR;
        }
    }

  error_handler:

    ixmlDocument_free( doc );
    ixmlFreeDOMString( node_str );
    ixmlFreeDOMString( error_node_str );
    return err_code;
}

/****************************************************************************
*	Function :	SoapSendAction
*
*	Parameters :
*		IN char* action_url :	device contrl URL 
*		IN char *service_type :	device service type
*		IN IXML_Document *action_node : SOAP action node	
*		OUT IXML_Document **response_node :	SOAP response node
*
*	Description :	This function is called by UPnP API to send the SOAP 
*		action request and waits till it gets the response from the device
*		pass the response to the API layer
*
*	Return :	int
*		returns UPNP_E_SUCCESS if successful else returns appropriate error
*	Note :
****************************************************************************/
int
SoapSendAction( IN char *action_url,
                IN char *service_type,
                IN IXML_Document * action_node,
                OUT IXML_Document ** response_node )
{
    char *action_str = NULL;
    memptr name;
    membuffer request;
    membuffer responsename;
    int err_code;
    int ret_code;
    http_parser_t response;
    uri_type url;
    int upnp_error_code;
    char *upnp_error_str;
    xboolean got_response = FALSE;

    char *xml_start =
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\n"
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>";
    char *xml_end = "</s:Body>\n" "</s:Envelope>\n";
    int xml_start_len;
    int xml_end_len;
    int action_str_len;

    *response_node = NULL;      // init

    err_code = UPNP_E_OUTOF_MEMORY; // default error

    DBGONLY( UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "Inside SoapSendAction():" );
         )
        // init
        membuffer_init( &request );
    membuffer_init( &responsename );

    // print action
    action_str = ixmlPrintDocument( ( IXML_Node * ) action_node );
    if( action_str == NULL ) {
        goto error_handler;
    }
    // get action name
    if( get_action_name( action_str, &name ) != 0 ) {
        err_code = UPNP_E_INVALID_ACTION;
        goto error_handler;
    }
    // parse url
    if( http_FixStrUrl( action_url, strlen( action_url ), &url ) != 0 ) {
        err_code = UPNP_E_INVALID_URL;
        goto error_handler;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "path=%.*s, hostport=%.*s\n",
                         url.pathquery.size, url.pathquery.buff,
                         url.hostport.text.size,
                         url.hostport.text.buff ); )

        xml_start_len = strlen( xml_start );
    xml_end_len = strlen( xml_end );
    action_str_len = strlen( action_str );

    // make request msg
    request.size_inc = 50;
    if( http_MakeMessage( &request, 1, 1, "q" "N" "s" "sssbs" "U" "c" "bbb", SOAPMETHOD_POST, &url, xml_start_len + action_str_len + xml_end_len,   // content-length
                          ContentTypeHeader,
                          "SOAPACTION: \"", service_type, "#", name.buf,
                          name.length, "\"\r\n", xml_start, xml_start_len,
                          action_str, action_str_len, xml_end,
                          xml_end_len ) != 0 ) {
        goto error_handler;
    }

    ret_code = soap_request_and_response( &request, &url, &response );
    got_response = TRUE;
    if( ret_code != UPNP_E_SUCCESS ) {
        err_code = ret_code;
        goto error_handler;
    }

    if( membuffer_append( &responsename, name.buf, name.length ) != 0 ||
        membuffer_append_str( &responsename, "Response" ) != 0 ) {
        goto error_handler;
    }
    // get action node from the response
    ret_code = get_response_value( &response.msg, SOAP_ACTION_RESP,
                                   responsename.buf, &upnp_error_code,
                                   ( IXML_Node ** ) response_node,
                                   &upnp_error_str );

    if( ret_code == SOAP_ACTION_RESP ) {
        err_code = UPNP_E_SUCCESS;
    } else if( ret_code == SOAP_ACTION_RESP_ERROR ) {
        err_code = upnp_error_code;
    } else {
        err_code = ret_code;
    }

  error_handler:
    ixmlFreeDOMString( action_str );
    membuffer_destroy( &request );
    membuffer_destroy( &responsename );
    if( got_response ) {
        httpmsg_destroy( &response.msg );
    }

    return err_code;
}

/****************************************************************************
*	Function :	SoapSendActionEx
*
*	Parameters :
*		IN char* action_url :	device contrl URL 
*		IN char *service_type :	device service type
		IN IXML_Document *Header: Soap header
*		IN IXML_Document *action_node : SOAP action node ( SOAP body)	
*		OUT IXML_Document **response_node :	SOAP response node
*
*	Description :	This function is called by UPnP API to send the SOAP 
*		action request and waits till it gets the response from the device
*		pass the response to the API layer. This action is similar to the 
*		the SoapSendAction with only difference that it allows users to 
*		pass the SOAP header along the SOAP body ( soap action request)
*
*	Return :	int
*		returns UPNP_E_SUCCESS if successful else returns appropriate error
*	Note :
****************************************************************************/
int
SoapSendActionEx( IN char *action_url,
                  IN char *service_type,
                  IN IXML_Document * header,
                  IN IXML_Document * action_node,
                  OUT IXML_Document ** response_node )
{
    char *xml_header_str = NULL;
    char *action_str = NULL;
    memptr name;
    membuffer request;
    membuffer responsename;
    int err_code;
    int ret_code;
    http_parser_t response;
    uri_type url;
    int upnp_error_code;
    char *upnp_error_str;
    xboolean got_response = FALSE;

    char *xml_start =
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\n"
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n";
    char *xml_body_start = "<s:Body>";
    char *xml_end = "</s:Body>\n" "</s:Envelope>\n";
    int xml_start_len;
    int xml_end_len;
    char *xml_header_start = "<s:Header>\n";
    char *xml_header_end = "</s:Header>\n";
    int xml_header_start_len;
    int xml_header_end_len;
    int xml_header_str_len;
    int action_str_len;
    int xml_body_start_len;

    *response_node = NULL;      // init

    err_code = UPNP_E_OUTOF_MEMORY; // default error

    DBGONLY( UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "Inside SoapSendActionEx():" );
         )
        // init
        membuffer_init( &request );
    membuffer_init( &responsename );

    // header string
    xml_header_str = ixmlPrintDocument( ( IXML_Node * ) header );
    if( xml_header_str == NULL ) {
        goto error_handler;
    }
    // print action
    action_str = ixmlPrintDocument( ( IXML_Node * ) action_node );
    if( action_str == NULL ) {
        goto error_handler;
    }
    // get action name
    if( get_action_name( action_str, &name ) != 0 ) {
        err_code = UPNP_E_INVALID_ACTION;
        goto error_handler;
    }
    // parse url
    if( http_FixStrUrl( action_url, strlen( action_url ), &url ) != 0 ) {
        err_code = UPNP_E_INVALID_URL;
        goto error_handler;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, SOAP, __FILE__, __LINE__,
                         "path=%.*s, hostport=%.*s\n",
                         url.pathquery.size, url.pathquery.buff,
                         url.hostport.text.size,
                         url.hostport.text.buff ); )

        xml_start_len = strlen( xml_start );
    xml_body_start_len = strlen( xml_body_start );
    xml_end_len = strlen( xml_end );
    action_str_len = strlen( action_str );

    xml_header_start_len = strlen( xml_header_start );
    xml_header_end_len = strlen( xml_header_end );
    xml_header_str_len = strlen( xml_header_str );

    // make request msg
    request.size_inc = 50;
    if( http_MakeMessage( &request, 1, 1, "q" "N" "s" "sssbs" "U" "c" "bbbbbbb", SOAPMETHOD_POST, &url, xml_start_len + xml_header_start_len + xml_header_str_len + xml_header_end_len + xml_body_start_len + action_str_len + xml_end_len, // content-length
                          ContentTypeHeader,
                          "SOAPACTION: \"", service_type, "#", name.buf,
                          name.length, "\"\r\n",
                          xml_start, xml_start_len,
                          xml_header_start, xml_header_start_len,
                          xml_header_str, xml_header_str_len,
                          xml_header_end, xml_header_end_len,
                          xml_body_start, xml_body_start_len,
                          action_str, action_str_len,
                          xml_end, xml_end_len ) != 0 ) {
        goto error_handler;
    }

    ret_code = soap_request_and_response( &request, &url, &response );
    got_response = TRUE;
    if( ret_code != UPNP_E_SUCCESS ) {
        err_code = ret_code;
        goto error_handler;
    }

    if( membuffer_append( &responsename, name.buf, name.length ) != 0 ||
        membuffer_append_str( &responsename, "Response" ) != 0 ) {
        goto error_handler;
    }
    // get action node from the response
    ret_code = get_response_value( &response.msg, SOAP_ACTION_RESP,
                                   responsename.buf, &upnp_error_code,
                                   ( IXML_Node ** ) response_node,
                                   &upnp_error_str );

    if( ret_code == SOAP_ACTION_RESP ) {
        err_code = UPNP_E_SUCCESS;
    } else if( ret_code == SOAP_ACTION_RESP_ERROR ) {
        err_code = upnp_error_code;
    } else {
        err_code = ret_code;
    }

  error_handler:

    ixmlFreeDOMString( action_str );
    ixmlFreeDOMString( xml_header_str );
    membuffer_destroy( &request );
    membuffer_destroy( &responsename );
    if( got_response ) {
        httpmsg_destroy( &response.msg );
    }

    return err_code;
}

/****************************************************************************
*	Function :	SoapGetServiceVarStatus
*
*	Parameters :
*			IN  char * action_url :	Address to send this variable 
*									query message.
*			IN  char *var_name : Name of the variable.
*			OUT char **var_value :	Output value.
*
*	Description :	This function creates a status variable query message 
*		send it to the specified URL. It also collect the response.
*
*	Return :	int
*
*	Note :
****************************************************************************/
int
SoapGetServiceVarStatus( IN char *action_url,
                         IN char *var_name,
                         OUT char **var_value )
{
    memptr host;                // value for HOST header
    memptr path;                // ctrl path in first line in msg
    uri_type url;
    membuffer request;
    int ret_code;
    http_parser_t response;
    int upnp_error_code;

    char *xml_start =
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\n"
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "<s:Body>\n"
        "<u:QueryStateVariable xmlns:u=\"urn:schemas-upnp-org:control-1-0\">\n"
        "<u:varName>";

    char *xml_end = "</u:varName>\n"
        "</u:QueryStateVariable>\n" "</s:Body>\n" "</s:Envelope>\n";

    *var_value = NULL;          // return NULL in case of an error

    membuffer_init( &request );

    // get host hdr and url path
    if( get_host_and_path( action_url, &host, &path, &url ) == -1 ) {
        return UPNP_E_INVALID_URL;
    }
    // make headers
    request.size_inc = 50;
    if( http_MakeMessage( &request, 1, 1, "Q" "sbc" "N" "s" "s" "U" "c" "sss", SOAPMETHOD_POST, path.buf, path.length, "HOST: ", host.buf, host.length, strlen( xml_start ) + strlen( var_name ) + strlen( xml_end ),   // content-length
                          ContentTypeHeader,
                          "SOAPACTION: \"urn:schemas"
                          "-upnp-org:control-1-0#QueryStateVariable\"\r\n",
                          xml_start, var_name, xml_end ) != 0 ) {
        return UPNP_E_OUTOF_MEMORY;
    }
    // send msg and get reply
    ret_code = soap_request_and_response( &request, &url, &response );
    membuffer_destroy( &request );
    if( ret_code != UPNP_E_SUCCESS ) {
        return ret_code;
    }
    // get variable value from the response
    ret_code = get_response_value( &response.msg, SOAP_VAR_RESP, NULL,
                                   &upnp_error_code, NULL, var_value );

    httpmsg_destroy( &response.msg );

    if( ret_code == SOAP_VAR_RESP ) {
        return UPNP_E_SUCCESS;
    } else if( ret_code == SOAP_VAR_RESP_ERROR ) {
        return upnp_error_code;
    } else {
        return ret_code;
    }
}

#endif // EXCLUDE_SOAP
#endif // INCLUDE_CLIENT_APIS
