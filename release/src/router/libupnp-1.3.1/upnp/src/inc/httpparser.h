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

#ifndef GENLIB_NET_HTTP_HTTPPARSER_H
#define GENLIB_NET_HTTP_HTTPPARSER_H

#include "util.h"
#include "membuffer.h"
#include "uri.h"

#include "LinkedList.h"

////// private types ////////////


//////////////////////
// scanner
///////////////////////
// Used to represent different types of tokens in input
typedef enum // token_type_t
{
	TT_IDENTIFIER, 
	TT_WHITESPACE, 
	TT_CRLF, 
	TT_CTRL,				// needed ??
	TT_SEPARATOR,			// ??
	TT_QUOTEDSTRING,		// ??
} token_type_t;

typedef struct // scanner_t
{
	membuffer* msg;				// raw http msg
	size_t cursor;				// current position in buffer
	xboolean entire_msg_loaded;	// set this to TRUE if the entire msg is loaded in
								//   in 'msg'; else FALSE if only partial msg in 'msg'
								//   (default is FALSE)
} scanner_t;

typedef enum // parser_pos_t
{
	POS_REQUEST_LINE,
	POS_RESPONSE_LINE,
	POS_HEADERS,
	POS_ENTITY,
	POS_COMPLETE,
} parser_pos_t;


#define ENTREAD_DETERMINE_READ_METHOD	1
#define ENTREAD_USING_CLEN				2
#define ENTREAD_USING_CHUNKED			3
#define ENTREAD_UNTIL_CLOSE				4
#define ENTREAD_CHUNKY_BODY				5
#define ENTREAD_CHUNKY_HEADERS			6


// end of private section
//////////////////
// ##################################################################################

// method in a HTTP request
typedef enum // http_method_t
{
	HTTPMETHOD_POST, 
	HTTPMETHOD_MPOST, 
	HTTPMETHOD_SUBSCRIBE, 
	HTTPMETHOD_UNSUBSCRIBE, 
	HTTPMETHOD_NOTIFY, 
	HTTPMETHOD_GET,
	HTTPMETHOD_HEAD, 
	HTTPMETHOD_MSEARCH, 
	HTTPMETHOD_UNKNOWN,
    SOAPMETHOD_POST,	 //murari
	HTTPMETHOD_SIMPLEGET
} http_method_t;

// different types of HTTP headers
#define HDR_UNKNOWN				-1
#define HDR_CACHE_CONTROL		1
#define HDR_CALLBACK			2
#define HDR_CONTENT_LENGTH		3
#define HDR_CONTENT_TYPE		4
#define HDR_DATE				5
#define HDR_EXT					6
#define HDR_HOST				7
//#define HDR_IF_MODIFIED_SINCE	8
//#define HDR_IF_UNMODIFIED_SINCE	9
//#define HDR_LAST_MODIFIED		10
#define HDR_LOCATION			11
#define HDR_MAN					12
#define HDR_MX					13
#define HDR_NT					14
#define HDR_NTS					15
#define HDR_SERVER				16
#define HDR_SEQ					17
#define HDR_SID					18
#define HDR_SOAPACTION			19
#define HDR_ST					20
#define HDR_TIMEOUT				21
#define HDR_TRANSFER_ENCODING	22
#define HDR_USN					23
#define HDR_USER_AGENT			24

//Adding new header difinition//Beg_Murari
#define HDR_ACCEPT              25
#define HDR_ACCEPT_ENCODING     26
#define HDR_ACCEPT_CHARSET      27
#define HDR_ACCEPT_LANGUAGE     28
#define HDR_ACCEPT_RANGE        29
#define HDR_CONTENT_ENCODING    30
#define HDR_CONTENT_LANGUAGE    31
#define HDR_CONTENT_LOCATION    32
#define HDR_CONTENT_RANGE       33
#define HDR_IF_RANGE            34
#define HDR_RANGE               35
#define HDR_TE                  36
//End_Murari

// status of parsing
typedef enum // parse_status_t
{
	PARSE_SUCCESS = 0,	// msg was parsed successfully
	PARSE_INCOMPLETE,	// need more data to continue
	PARSE_INCOMPLETE_ENTITY,	// for responses that don't have length specified
	PARSE_FAILURE,		// parse failed; check status code for details
	PARSE_OK,			// done partial
	PARSE_NO_MATCH,		// token not matched

	// private
	PARSE_CONTINUE_1
} parse_status_t;

typedef struct // http_header_t
{
	memptr name;		// header name as a string
	int name_id;		// header name id (for a selective group of headers only)
	membuffer value;	// raw-value; could be multi-lined; min-length = 0

    // private
    membuffer name_buf;
} http_header_t;

typedef struct // http_message_t
{
    int initialized;
	// request only
	http_method_t method;
	uri_type uri;

	// response only
	http_method_t request_method;
	int status_code;
	membuffer status_msg;

	// fields used in both request or response messages
	xboolean is_request;	// if TRUE, msg is a request, else response

	int major_version;		// http major.minor version
	int minor_version;


	LinkedList headers;
//NNS:	dlist headers;			// dlist<http_header_t *>
	memptr entity;			// message body(entity)

	// private fields
	membuffer msg;		// entire raw message
	char *urlbuf;	// storage for url string
} http_message_t;

typedef struct // http_parser_t
{
	http_message_t msg;
	int http_error_code;	// read-only; in case of parse error, this
							//  contains the HTTP error code (4XX or 5XX)

    // read-only; this is set to true if a NOTIFY request has no content-length.
    //  used to read valid sspd notify msg.
    xboolean valid_ssdp_notify_hack;

	// private data -- don't touch
	parser_pos_t position;
	int ent_position;
	unsigned int content_length;
	int chunk_size;
	size_t entity_start_position;
	scanner_t scanner;
} http_parser_t;


//--------------------------------------------------
//////////////// functions /////////////////////////
//--------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


/************************************************************************
*	Function :	httpmsg_init
*
*	Parameters :
*		INOUT http_message_t* msg ;	HTTP Message Object
*
*	Description :	Initialize and allocate memory for http message
*
*	Return : void ;
*
*	Note :
************************************************************************/
void httpmsg_init( INOUT http_message_t* msg );

/************************************************************************
*	Function :	httpmsg_destroy
*
*	Parameters :
*		INOUT http_message_t* msg ;	HTTP Message Object
*
*	Description :	Free memory allocated for the http message
*
*	Return : void ;
*
*	Note :
************************************************************************/
void httpmsg_destroy( INOUT http_message_t* msg );

/************************************************************************
*	Function :	httpmsg_find_hdr_str
*
*	Parameters :
*		IN http_message_t* msg ;	HTTP Message Object
*		IN const char* header_name ; Header name to be compared with	
*
*	Description :	Compares the header name with the header names stored 
*		in	the linked list of messages
*
*	Return : http_header_t* - Pointer to a header on success;
*			 NULL on failure														
*
*	Note :
************************************************************************/
http_header_t* httpmsg_find_hdr_str( IN http_message_t* msg,
			IN const char* header_name );

/************************************************************************
*	Function :	httpmsg_find_hdr
*
*	Parameters :
*		IN http_message_t* msg ; HTTP Message Object
*		IN int header_name_id ;	 Header Name ID to be compared with
*		OUT memptr* value ;		 Buffer to get the ouput to.
*
*	Description :	Finds header from a list, with the given 'name_id'.
*
*	Return : http_header_t*  - Pointer to a header on success;										*
*			 NULL on failure														
*
*	Note :
************************************************************************/
http_header_t* httpmsg_find_hdr( IN http_message_t* msg, 
			IN int header_name_id, OUT memptr* value );

/************************************************************************
* Function: parser_request_init											
*																		
* Parameters:															
*	OUT http_parser_t* parser ; HTTP Parser object									
*																
* Description: Initializes parser object for a request					
*																		
* Returns:																
*	 void																
************************************************************************/
void parser_request_init( OUT http_parser_t* parser );

/************************************************************************
* Function: parser_response_init										
*																		
* Parameters:															
*	OUT http_parser_t* parser	;	  HTTP Parser object
*	IN http_method_t request_method	; Request method 					
*																		
* Description: Initializes parser object for a response					
*																		
* Returns:																
*	 void																
************************************************************************/
void parser_response_init( OUT http_parser_t* parser, 
			   IN http_method_t request_method );

/************************************************************************
* Function: parser_parse												
*																		
* Parameters:															
*	INOUT http_parser_t* parser ; HTTP Parser object					
*																		
* Description: The parser function. Depending on the position of the 	
*	parser object the actual parsing function is invoked				
*																		
* Returns:																
*	 void																
************************************************************************/
parse_status_t parser_parse(INOUT http_parser_t * parser);

/************************************************************************
* Function: parser_parse_responseline									
*																		
* Parameters:															
*	INOUT http_parser_t* parser	; HTTP Parser object					
*																		
* Description: Get HTTP Method, URL location and version information.	
*																		
* Returns:																
*	PARSE_OK															
*	PARSE_SUCCESS														
*	PARSE_FAILURE														
************************************************************************/
parse_status_t parser_parse_responseline(INOUT http_parser_t *parser);

/************************************************************************
* Function: parser_parse_headers									
*																		
* Parameters:															
*	INOUT http_parser_t* parser	; HTTP Parser object										
*													
* Description: Get HTTP Method, URL location and version information.	
*																		
* Returns:																
*	PARSE_OK															
*	PARSE_SUCCESS														
*	PARSE_FAILURE														
************************************************************************/
parse_status_t parser_parse_headers(INOUT http_parser_t *parser);

/************************************************************************
* Function: parser_parse_entity											
*																		
* Parameters:															
*	INOUT http_parser_t* parser	; HTTP Parser object					
*																		
* Description: Determines method to read entity							
*																		
* Returns:																
*	 PARSE_OK															
* 	 PARSE_FAILURE														
*	 PARSE_COMPLETE	-- no more reading to do							
************************************************************************/
parse_status_t parser_parse_entity(INOUT http_parser_t *parser);

/************************************************************************
* Function: parser_get_entity_read_method								
*																		
* Parameters:															
*	INOUT http_parser_t* parser	; HTTP Parser object					
*																		
* Description: Determines method to read entity							
*																		
* Returns:																
*	 PARSE_OK															
* 	 PARSE_FAILURE														
*	 PARSE_COMPLETE	-- no more reading to do							
************************************************************************/
parse_status_t parser_get_entity_read_method( INOUT http_parser_t* parser );

/************************************************************************
* Function: parser_append												
*																		
* Parameters:															
*	INOUT http_parser_t* parser ;	HTTP Parser Object					
*	IN const char* buf	;			buffer to be appended to the parser 
*									buffer							
*	IN size_t buf_length ;			Size of the buffer												
*																		
* Description: The parser function. Depending on the position of the 	
*	parser object the actual parsing function is invoked				
*																		
* Returns:																
*	 void																
************************************************************************/
parse_status_t parser_append( INOUT http_parser_t* parser, 
				 IN const char* buf,
				 IN size_t buf_length );

/************************************************************************
* Function: matchstr													
*																		
* Parameters:															
*	IN char *str ;		 String to be matched														
*	IN size_t slen ;     Length of the string														
*	IN const char* fmt ; Pattern format												
*	...																	
*																		
* Description: Matches a variable parameter list with a string			
*	and takes actions based on the data type specified.					
*																		
* Returns:																
*   PARSE_OK															
*   PARSE_NO_MATCH -- failure to match pattern 'fmt'					
*   PARSE_FAILURE	-- 'str' is bad input							
************************************************************************/
int matchstr( IN char *str, IN size_t slen, IN const char* fmt, ... );

// ====================================================
// misc functions


/************************************************************************
* Function: raw_to_int													
*																		
* Parameters:															
*	IN memptr* raw_value ;	Buffer to be converted 					
*	IN int base ;			Base  to use for conversion
*																		
* Description: Converts raw character data to long-integer value					
*																		
* Returns:																
*	 int																
************************************************************************/
int raw_to_int( IN memptr* raw_value, int base );

/************************************************************************
* Function: raw_find_str												
*																		
* Parameters:															
*	IN memptr* raw_value ; Buffer containg the string												
*	IN const char* str ;	Substring to be found													
*																		
* Description: Find a substring from raw character string buffer					
*																		
* Returns:																
*	 int - index at which the substring is found.						
************************************************************************/
int raw_find_str( IN memptr* raw_value, IN const char* str );

/************************************************************************
* Function: method_to_str												
*																		
* Parameters:															
* IN http_method_t method ; HTTP method						
*																		
* Description: A wrapper function that maps a method id to a method		
*	nameConverts a http_method id stored in the HTTP Method				
*																		
* Returns:																
*	 const char* ptr - Ptr to the HTTP Method																							*
************************************************************************/
const char* method_to_str( IN http_method_t method );

/************************************************************************
* Function: print_http_headers											
*																		
* Parameters:															
*	http_message_t* hmsg ; HTTP Message object									
*																		
* Description:															
*																		
* Returns:																
*	 void																
************************************************************************/
void print_http_headers( IN http_message_t* hmsg );

#ifdef __cplusplus
}		// extern "C"
#endif	// __cplusplus


#endif // GENLIB_NET_HTTP_HTTPPARSER_H
