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
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "upnp.h"
#include "util.h"
#include "webserver.h"
#include "uri.h"
#include "membuffer.h"
#include "urlconfig.h"

/************************************************************************
*	Function :	addrToString
*
*	Parameters :
*		IN const struct sockaddr_in* addr ;	socket address object with 
*					the IP Address and port information
*		OUT char ipaddr_port[] ;	character array which will hold the 
*					IP Address  in a string format.
*
*	Description : Converts an Internet address to a string and stores it 
*		a buffer.
*
*	Return : void ;
*
*	Note :
************************************************************************/
static XINLINE void
addrToString( IN const struct sockaddr_in *addr,
              OUT char ipaddr_port[] )
{
    sprintf( ipaddr_port, "%s:%d", inet_ntoa( addr->sin_addr ),
             ntohs( addr->sin_port ) );
}

/************************************************************************
*	Function :	calc_alias
*
*	Parameters :
*		IN const char* alias ;	String containing the alias
*		IN const char* rootPath ;	String containing the root path
*		OUT char** newAlias ;	String pointer to hold the modified new
*					alias
*
*	Description : Determine alias based urlbase's root path.
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success.
*		UPNP_E_OUTOF_MEMORY - On Failure to allocate memory for new alias
*
*	Note : 'newAlias' should be freed using free()
************************************************************************/
static XINLINE int
calc_alias( IN const char *alias,
            IN const char *rootPath,
            OUT char **newAlias )
{
    const char *aliasPtr;
    size_t root_len;
    char *temp_str;
    size_t new_alias_len;
    char *alias_temp;

    assert( rootPath );
    assert( alias );

    // add / suffix, if missing
    root_len = strlen( rootPath );
    if( root_len == 0 || rootPath[root_len - 1] != '/' ) {
        temp_str = "/";
    } else {
        temp_str = "";          // suffix already present
    }

    // discard / prefix, if present
    if( alias[0] == '/' ) {
        aliasPtr = alias + 1;
    } else {
        aliasPtr = alias;
    }

    new_alias_len = root_len + strlen( temp_str ) + strlen( aliasPtr );
    alias_temp = ( char * )malloc( new_alias_len + 1 );
    if( alias_temp == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    strcpy( alias_temp, rootPath );
    strcat( alias_temp, temp_str );
    strcat( alias_temp, aliasPtr );

    *newAlias = alias_temp;
    return UPNP_E_SUCCESS;
}

/************************************************************************
*	Function :	calc_descURL
*
*	Parameters :
*		IN const char* ipPortStr ;	String containing the port number
*		IN const char* alias ;		String containing the alias
*		OUT char descURL[LINE_SIZE] ;	buffer to hold the calculated 
*					description URL
*
*	Description : Determines the description URL
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success
*		UPNP_E_URL_TOO_BIG - length of the URL is determined to be to
*		exceeding the limit.
*
*	Note :
************************************************************************/
static XINLINE int
calc_descURL( IN const char *ipPortStr,
              IN const char *alias,
              OUT char descURL[LINE_SIZE] )
{
    size_t len;
    const char *http_scheme = "http://";

    assert( ipPortStr != NULL && strlen( ipPortStr ) > 0 );
    assert( alias != NULL && strlen( alias ) > 0 );

    len = strlen( http_scheme ) + strlen( ipPortStr ) + strlen( alias );

    if( len > ( LINE_SIZE - 1 ) ) {
        return UPNP_E_URL_TOO_BIG;
    }
    strcpy( descURL, http_scheme );
    strcat( descURL, ipPortStr );
    strcat( descURL, alias );

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "desc url: %s\n", descURL );
         )

        return UPNP_E_SUCCESS;
}

/************************************************************************
*	Function :	config_description_doc
*
*	Parameters :
*		INOUT IXML_Document *doc ;IMXL description document to be 
*					configured	
*		IN const char* ip_str ;	string containing the IP port number
*		OUT char** root_path_str ;	buffer to hold the root path
*					of the configured description document
*		INOUT IXML_Document *doc :	Description document
*		IN const char* ip_str :	ipaddress string
*		OUT char** root_path_str :	root path string
*
*	Description : Configure the description document. Add the standard 
*		format and then add information from the root device and any
*		child nodes.
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success
*		UPNP_E_OUTOF_MEMORY - Default Error
*		UPNP_E_INVALID_DESC - Invalid child node		
*		UPNP_E_INVALID_URL - Invalid node information
*
*	Note :
************************************************************************/
static int
config_description_doc( INOUT IXML_Document * doc,
                        IN const char *ip_str,
                        OUT char **root_path_str )
{
    xboolean addNew = FALSE;
    IXML_NodeList *baseList;
    IXML_Element *element = NULL;
    IXML_Element *newElement = NULL;
    IXML_Node *textNode = NULL;
    IXML_Node *rootNode = NULL;
    IXML_Node *urlbase_node = NULL;
    char *urlBaseStr = "URLBase";
    DOMString domStr = NULL;
    uri_type uri;
    int err_code;
    int len;
    membuffer url_str;
    membuffer root_path;

    membuffer_init( &url_str );
    membuffer_init( &root_path );

    err_code = UPNP_E_OUTOF_MEMORY; // default error

    baseList = ixmlDocument_getElementsByTagName( doc, urlBaseStr );
    if( baseList == NULL ) {
        // urlbase not found -- create new one
        addNew = TRUE;
        element = ixmlDocument_createElement( doc, urlBaseStr );
        if( element == NULL ) {
            goto error_handler;
        }

        if( membuffer_append_str( &url_str, "http://" ) != 0 ||
            membuffer_append_str( &url_str, ip_str ) != 0 ||
            membuffer_append_str( &url_str, "/" ) != 0 ||
            membuffer_append_str( &root_path, "/" ) != 0 ) {
            goto error_handler;
        }

        rootNode = ixmlNode_getFirstChild( ( IXML_Node * ) doc );
        if( rootNode == NULL ) {
            err_code = UPNP_E_INVALID_DESC;
            goto error_handler;
        }

        err_code =
            ixmlNode_appendChild( rootNode, ( IXML_Node * ) element );
        if( err_code != IXML_SUCCESS ) {
            goto error_handler;
        }

        textNode =
            ixmlDocument_createTextNode( doc, ( char * )url_str.buf );
        if( textNode == NULL ) {
            goto error_handler;
        }

        err_code =
            ixmlNode_appendChild( ( IXML_Node * ) element, textNode );
        if( err_code != IXML_SUCCESS ) {
            goto error_handler;
        }

    } else {
        // urlbase found
        urlbase_node = ixmlNodeList_item( baseList, 0 );
        assert( urlbase_node != NULL );

        textNode = ixmlNode_getFirstChild( urlbase_node );
        if( textNode == NULL ) {
            err_code = UPNP_E_INVALID_DESC;
            goto error_handler;
        }

        domStr = ixmlNode_getNodeValue( textNode );
        if( domStr == NULL ) {
            err_code = UPNP_E_INVALID_URL;
            goto error_handler;
        }

        len = parse_uri( domStr, strlen( domStr ), &uri );
        if( len < 0 || uri.type != ABSOLUTE ) {
            err_code = UPNP_E_INVALID_URL;
            goto error_handler;
        }

        if( membuffer_assign( &url_str, uri.scheme.buff,
                              uri.scheme.size ) != 0 ||
            membuffer_append_str( &url_str, "://" ) != 0 ||
            membuffer_append_str( &url_str, ip_str ) != 0 ) {
            goto error_handler;
        }
        // add leading '/' if missing from relative path
        if( ( uri.pathquery.size > 0 && uri.pathquery.buff[0] != '/' ) ||
            ( uri.pathquery.size == 0 )
             ) {
            if( membuffer_append_str( &url_str, "/" ) != 0 ||
                membuffer_append_str( &root_path, "/" ) != 0 ) {
                goto error_handler;
            }
        }

        if( membuffer_append( &url_str, uri.pathquery.buff,
                              uri.pathquery.size ) != 0 ||
            membuffer_append( &root_path, uri.pathquery.buff,
                              uri.pathquery.size ) != 0 ) {
            goto error_handler;
        }
        // add trailing '/' if missing
        if( url_str.buf[url_str.length - 1] != '/' ) {
            if( membuffer_append( &url_str, "/", 1 ) != 0 ) {
                goto error_handler;
            }
        }

        err_code = ixmlNode_setNodeValue( textNode, url_str.buf );
        if( err_code != IXML_SUCCESS ) {
            goto error_handler;
        }
    }

    *root_path_str = membuffer_detach( &root_path );    // return path
    err_code = UPNP_E_SUCCESS;

  error_handler:
    if( err_code != UPNP_E_SUCCESS ) {
        ixmlElement_free( newElement );
    }

    ixmlNodeList_free( baseList );

    membuffer_destroy( &root_path );
    membuffer_destroy( &url_str );

    return err_code;
}

/************************************************************************
*	Function :	configure_urlbase
*
*	Parameters :
*		INOUT IXML_Document *doc ;	IXML Description document
*		IN const struct sockaddr_in* serverAddr ;	socket address object
*					providing the IP address and port information
*		IN const char* alias ;	string containing the alias
*		IN time_t last_modified ;	time when the XML document was 
*					downloaded
*		OUT char docURL[LINE_SIZE] ;	buffer to hold the URL of the 
*					document.
*		INOUT IXML_Document *doc:dom document whose urlbase is to be modified
*		IN const struct sockaddr_in* serverAddr : ip address and port of 
*													the miniserver
*		IN const char* alias : a name to be used for the temp; e.g.:"foo.xml"
*		IN time_t last_modified :	time
*		OUT char docURL[LINE_SIZE] :	document URL
*
*	Description : Configure the full URL for the description document.
*		Create the URL document and add alias, description information.
*		The doc is added to the web server to be served using the given 
*		alias.
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success
*		UPNP_E_OUTOF_MEMORY - Default Error
*
*	Note :
************************************************************************/
int
configure_urlbase( INOUT IXML_Document * doc,
                   IN const struct sockaddr_in *serverAddr,
                   IN const char *alias,
                   IN time_t last_modified,
                   OUT char docURL[LINE_SIZE] )
{
    char *root_path = NULL;
    char *new_alias = NULL;
    char *xml_str = NULL;
    int err_code;
    char ipaddr_port[LINE_SIZE];

    err_code = UPNP_E_OUTOF_MEMORY; // default error

    // get IP address and port
    addrToString( serverAddr, ipaddr_port );

    // config url-base in 'doc'
    err_code = config_description_doc( doc, ipaddr_port, &root_path );
    if( err_code != UPNP_E_SUCCESS ) {
        goto error_handler;
    }
    // calc alias
    err_code = calc_alias( alias, root_path, &new_alias );
    if( err_code != UPNP_E_SUCCESS ) {
        goto error_handler;
    }
    // calc full url for desc doc
    err_code = calc_descURL( ipaddr_port, new_alias, docURL );
    if( err_code != UPNP_E_SUCCESS ) {
        goto error_handler;
    }
    // xml doc to str
    xml_str = ixmlPrintDocument( ( IXML_Node * ) doc );
    if( xml_str == NULL ) {
        goto error_handler;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                         "desc url: %s\n", docURL );
         )
        DBGONLY( UpnpPrintf( UPNP_INFO, API, __FILE__, __LINE__,
                             "doc = %s\n", xml_str );
         )
        // store in web server
        err_code =
        web_server_set_alias( new_alias, xml_str, strlen( xml_str ),
                              last_modified );

  error_handler:
    free( root_path );
    free( new_alias );

    if( err_code != UPNP_E_SUCCESS ) {
        ixmlFreeDOMString( xml_str );
    }
    return err_code;
}
