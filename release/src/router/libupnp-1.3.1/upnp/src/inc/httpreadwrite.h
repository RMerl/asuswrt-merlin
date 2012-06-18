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

#ifndef GENLIB_NET_HTTP_HTTPREADWRITE_H
#define GENLIB_NET_HTTP_HTTPREADWRITE_H

#include "config.h"
#include "util.h"
#include "sock.h"
#include "httpparser.h"

// timeout in secs
#define HTTP_DEFAULT_TIMEOUT	30



#ifdef __cplusplus
#extern "C" {
#endif

/************************************************************************
* Function: http_FixUrl													
*																		
* Parameters:															
*	IN uri_type* url ;			URL to be validated and fixed
*	OUT uri_type* fixed_url ;	URL after being fixed.
*																		
* Description: Validates URL											
*																		
* Returns:																
*	 UPNP_E_INVALID_URL													
* 	 UPNP_E_SUCCESS														
************************************************************************/
int http_FixUrl( IN uri_type* url, OUT uri_type* fixed_url );

/************************************************************************
* Function: http_FixStrUrl												
*																		
* Parameters:															
*	IN char* urlstr ; 			Character string as a URL				
*	IN int urlstrlen ; 			Length of the character string								
*	OUT uri_type* fixed_url	;	Fixed and corrected URL
*																		
* Description: Parses URL and then validates URL						
*																		
* Returns:																
*	 UPNP_E_INVALID_URL													
* 	 UPNP_E_SUCCESS														
************************************************************************/
int http_FixStrUrl( IN char* urlstr, IN int urlstrlen, OUT uri_type* fixed_url );


/************************************************************************
* Function: http_Connect												
*																		
* Parameters:															
*	IN uri_type* destination_url ; URL containing destination information					
*	OUT uri_type *url ;			   Fixed and corrected URL
*																		
* Description: Gets destination address from URL and then connects to the 
*	remote end
*																		
* Returns:																
*	socket descriptor on sucess											
*	UPNP_E_OUTOF_SOCKET													
*	UPNP_E_SOCKET_CONNECT on error										
************************************************************************/
int http_Connect( IN uri_type* destination_url, OUT uri_type *url );

/************************************************************************
* Function: http_RecvMessage											
*																		
* Parameters:															
*	IN SOCKINFO *info ;					Socket information object
*	OUT http_parser_t* parser,			HTTP parser object
*	IN http_method_t request_method ;	HTTP request method					
*	IN OUT int* timeout_secs ;			time out											
*	OUT int* http_error_code ;			HTTP error code returned
*																		
* Description: Get the data on the socket and take actions based on the 
*	read data to modify the parser objects buffer. If an error is reported 
*	while parsing the data, the error code is passed in the http_errr_code 
*	parameter
*																		
* Returns:																
*	 UPNP_E_BAD_HTTPMSG													
* 	 UPNP_E_SUCCESS														
************************************************************************/
int http_RecvMessage( IN SOCKINFO *info, OUT http_parser_t* parser,
		IN http_method_t request_method, 
		IN OUT int* timeout_secs,
		OUT int* http_error_code );


/************************************************************************
* Function: http_SendMessage											
*																		
* Parameters:															
*	IN SOCKINFO *info ;		Socket information object
*	IN OUT int * TimeOut ; 	time out value											
*	IN const char* fmt, ...	 Pattern format to take actions upon								
*																		
* Description: Sends a message to the destination based on the			
*	IN const char* fmt parameter										
*	fmt types:															
*		'f':	arg = const char * file name							
*		'm':	arg1 = const char * mem_buffer; arg2= size_t buf_length	
*	E.g.:																
*		char *buf = "POST /xyz.cgi http/1.1\r\n\r\n";					
*		char *filename = "foo.dat";										
*		int status = http_SendMessage( tcpsock, "mf",					
*					buf, strlen(buf),	// args for memory buffer		
*					filename );			// arg for file					
*																		
* Returns:																
*	UPNP_E_OUTOF_MEMORY													
* 	UPNP_E_FILE_READ_ERROR												
*	UPNP_E_SUCCESS														
************************************************************************/
int http_SendMessage( IN SOCKINFO *info, IN OUT int* timeout_secs, 
					  IN const char* fmt, ... );

/************************************************************************
* Function: http_RequestAndResponse										
*																		
* Parameters:															
*	IN uri_type* destination ;		Destination URI object which contains 
*									remote IP address among other elements
*	IN const char* request ;		Request to be sent
*	IN size_t request_length ;		Length of the request
*	IN http_method_t req_method ;	HTTP Request method
*	IN int timeout_secs ;			time out value
*	OUT http_parser_t* response	;	Parser object to receive the repsonse
*																		
* Description: Initiates socket, connects to the destination, sends a	
*	request and waits for the response from the remote end				
*																		
* Returns:																
*	UPNP_E_SOCKET_ERROR													
* 	UPNP_E_SOCKET_CONNECT												
*	Error Codes returned by http_SendMessage							
*	Error Codes returned by http_RecvMessage							
************************************************************************/
int http_RequestAndResponse( IN uri_type* destination,
						   IN const char* request, IN size_t request_length,
						   IN http_method_t req_method,
						   IN int timeout_secs, 
						   OUT http_parser_t* response );

/************************************************************************
*  return codes:
*		0 -- success
*		UPNP_E_OUTOF_MEMORY
*		UPNP_E_TIMEDOUT
*		UPNP_E_BAD_REQUEST
*		UPNP_E_BAD_RESPONSE
*		UPNP_E_INVALID_URL
*		UPNP_E_SOCKET_READ
*		UPNP_E_SOCKET_WRITE
************************************************************************/

/************************************************************************
*	Function :	http_Download
*
*	Parameters :
*		IN const char* url_str :	String as a URL
*		IN int timeout_secs :		time out value
*		OUT char** document :		buffer to store the document extracted
*									from the donloaded message.
*		OUT int* doc_length :		length of the extracted document
*	    OUT char* content_type :	Type of content
*
*	Description :	Download the document message and extract the document 
*		from the message.
*
*	Return : int;
*		UPNP_E_SUCCESS;
*		UPNP_E_INVALID_URL;
*			
*
*	Note :
************************************************************************/
int http_Download( IN const char* url, 
				   IN int timeout_secs,
				   OUT char** document, OUT int* doc_length,
				   OUT char* content_type );


/************************************************************************
*	Function :	http_WriteHttpPost
*
*	Parameters :
*		IN void *Handle :		Handle to the http post object
*		IN char *buf :			Buffer to send to peer, if format used
*								is not UPNP_USING_CHUNKED, 
*		IN unsigned int *size :	Size of the data to be sent.
*		IN int timeout :		time out value
*
*	Description :	Formats data if format used is UPNP_USING_CHUNKED.
*		Writes data on the socket connected to the peer.
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success
*		UPNP_E_INVALID_PARAM - Invalid Parameter
*		-1 - On Socket Error.
*
*	Note :
************************************************************************/
int http_WriteHttpPost(IN void *Handle,
		       IN char *buf,
		       IN unsigned int *size,
		       IN int timeout);

/************************************************************************
*	Function :	http_CloseHttpPost
*
*	Parameters :
*		IN void *Handle :			Handle to the http post object
*		IN OUT int *httpStatus :	HTTP status returned on receiving a
*									response message
*		IN int timeout :			time out value
*
*	Description :	Sends remaining data if using  UPNP_USING_CHUNKED 
*		format. Receives any more messages. Destroys socket and any socket
*		associated memory. Frees handle associated with the HTTP POST msg.
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Sucess ;
*		UPNP_E_INVALID_PARAM  - Invalid Parameter;
*
*	Note :
************************************************************************/
int http_CloseHttpPost(IN void *Handle, 
		       IN OUT int *httpStatus,
		       IN int timeout);

/************************************************************************
*	Function :	http_OpenHttpPost
*
*	Parameters :
*		IN const char *url_str :		String as a URL	
*		IN OUT void **Handle :			Pointer to buffer to store HTTP
*										post handle
*		IN const char *contentType :	Type of content
*		IN int contentLength :			length of content
*		IN int timeout :				time out value
*
*	Description :	Makes the HTTP POST message, connects to the peer, 
*		sends the HTTP POST request. Adds the post handle to buffer of 
*		such handles
*
*	Return : int;
*		UPNP_E_SUCCESS - On Sucess ;
*		UPNP_E_INVALID_PARAM - Invalid Paramter ;
*		UPNP_E_OUTOF_MEMORY ;
*		UPNP_E_SOCKET_ERROR ;
*		UPNP_E_SOCKET_CONNECT ;
*
*	Note :
************************************************************************/
int http_OpenHttpPost(IN const char *url_str,
		      IN OUT void **Handle,
		      IN const char *contentType,
		      IN int contentLength,
		      IN int timeout);

/************************************************************************
*	Function :	http_ReadHttpGet
*
*	Parameters :
*		IN void *Handle :			Handle to the HTTP get object
*		IN OUT char *buf :			Buffer to get the read and parsed data
*		IN OUT unsigned int *size :	Size of tge buffer passed
*		IN int timeout :			time out value
*
*	Description :	Parses already existing data, then gets new data.
*		Parses and extracts information from the new data.
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Sucess ;
*		UPNP_E_INVALID_PARAM  - Invalid Parameter;
*		UPNP_E_BAD_RESPONSE ;
*		UPNP_E_BAD_HTTPMSG ;
*
*	Note :
************************************************************************/
int http_ReadHttpGet(IN void *Handle,
					IN OUT char *buf,
					IN OUT unsigned int *size,
					IN int timeout);

/************************************************************************
*	Function :	http_CloseHttpGet
*
*	Parameters :
*		IN void *Handle ;	Handle to HTTP get object
*
*	Description :	Clears the handle allocated for the HTTP GET operation
*		Clears socket states and memory allocated for socket operations. 
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success
*		UPNP_E_INVALID_PARAM - Invalid Parameter
*
*	Note :
************************************************************************/
int http_CloseHttpGet(IN void *Handle);

/************************************************************************
*	Function :	http_OpenHttpGet
*
*	Parameters :
*		IN const char *url_str :	String as a URL
*		IN OUT void **Handle :		Pointer to buffer to store HTTP
*									post handle
*		IN OUT char **contentType :	Type of content
*		OUT int *contentLength :	length of content
*		OUT int *httpStatus :		HTTP status returned on receiving a
*									response message
*		IN int timeout :			time out value
*
*	Description :	Makes the HTTP GET message, connects to the peer, 
*		sends the HTTP GET request, gets the response and parses the 
*		response.
*
*	Return : int;
*		UPNP_E_SUCCESS - On Success ;
*		UPNP_E_INVALID_PARAM - Invalid Paramters ;
*		UPNP_E_OUTOF_MEMORY ;
*		UPNP_E_SOCKET_ERROR ;
*		UPNP_E_BAD_RESPONSE ;
*
*	Note :
*
************************************************************************/
int http_OpenHttpGet(IN const char *url_str,
					IN OUT void **Handle,
					IN OUT char **contentType,
					OUT int *contentLength,
					OUT int *httpStatus,
					IN int timeout);


/************************************************************************
*	Function :	http_SendStatusResponse
*
*	Parameters :
*		IN SOCKINFO *info :				Socket information object
*		IN int http_status_code :		error code returned while making 
*										or sending the response message
*		IN int request_major_version :	request major version
*		IN int request_minor_version :	request minor version
*
*	Description :	Generate a response message for the status query and
*		send the status response.
*
*	Return : int;
*		0 -- success
*		UPNP_E_OUTOF_MEMORY
*		UPNP_E_SOCKET_WRITE
*		UPNP_E_TIMEDOUT
*
*	Note :
************************************************************************/
int http_SendStatusResponse( IN SOCKINFO *info, IN int http_status_code,
						  IN int request_major_version,
						  IN int request_minor_version );


/************************************************************************
*	Function :	http_MakeMessage
*
*	Parameters :
*		INOUT membuffer* buf :		buffer with the contents of the 
*									message
*		IN int http_major_version :	HTTP major version
*		IN int http_minor_version :	HTTP minor version
*		IN const char* fmt :		Pattern format 
*		... :	
*
*	Description :	Generate an HTTP message based on the format that is 
*		specified in the input parameters.
*
*		fmt types:
*		's':	arg = const char* C_string
*		'b':	arg1 = const char* buf; arg2 = size_t buf_length 
*				memory ptr
*		'c':	(no args) appends CRLF "\r\n"
*		'd':	arg = int number		// appends decimal number
*		't':	arg = time_t * gmt_time	// appends time in RFC 1123 fmt
*		'D':	(no args) appends HTTP DATE: header
*		'S':	(no args) appends HTTP SERVER: header
*		'U':	(no args) appends HTTP USER-AGENT: header
*		'C':	(no args) appends a HTTP CONNECTION: close header 
*				depending on major,minor version
*		'N':	arg1 = int content_length	// content-length header
*		'Q':	arg1 = http_method_t; arg2 = char* url; 
*				arg3 = int url_length // start line of request
*		'R':	arg = int status_code // adds a response start line
*		'B':	arg = int status_code 
*				appends content-length, content-type and HTML body for given code
*		'T':	arg = char * content_type; format e.g: "text/html";	
*				 content-type header
*
*	Return : int;
*		0 - On Success
*		UPNP_E_OUTOF_MEMORY
*		UPNP_E_INVALID_URL;
*
*	Note :
************************************************************************/
int http_MakeMessage( INOUT membuffer* buf, 
			IN int http_major_version,
			IN int http_minor_version,
			IN const char* fmt, ... );

/************************************************************************
*	Function :	http_CalcResponseVersion
*
*	Parameters :
*		IN int request_major_vers :		Request major version
*		IN int request_minor_vers :		Request minor version
*		OUT int* response_major_vers :	Response mojor version
*		OUT int* response_minor_vers :	Response minor version
*
*	Description :	Calculate HTTP response versions based on the request
*		versions.
*
*	Return :	void
*
*	Note :
************************************************************************/
void http_CalcResponseVersion( 
		IN int request_major_vers, IN int request_minor_vers,
		OUT int* response_major_vers, OUT int* response_minor_vers );

/************************************************************************
*	Function :	http_OpenHttpGetEx
*
*	Parameters :
*		IN const char *url_str :	String as a URL
*		IN OUT void **Handle :		Pointer to buffer to store HTTP
*									post handle
*		IN OUT char **contentType :	Type of content
*		OUT int *contentLength :	length of content
*		OUT int *httpStatus :		HTTP status returned on receiving a
*									response message
*		IN int timeout :			time out value
*
*	Description :	Makes the HTTP GET message, connects to the peer, 
*		sends the HTTP GET request, gets the response and parses the 
*		response.
*
*	Return : int;
*		UPNP_E_SUCCESS - On Success ;
*		UPNP_E_INVALID_PARAM - Invalid Paramters ;
*		UPNP_E_OUTOF_MEMORY ;
*		UPNP_E_SOCKET_ERROR ;
*		UPNP_E_BAD_RESPONSE ;
*
*	Note :
*
************************************************************************/
int http_OpenHttpGetEx(IN const char *url_str,
		     IN OUT void **Handle,
		     IN OUT char **contentType,
		     OUT int *contentLength,
		     OUT int *httpStatus,
			 IN int lowRange,
			 IN int highRange,
		     IN int timeout);

/************************************************************************
*	Function :	get_sdk_info
*
*	Parameters :
*		OUT char *info ;	
*
*	Description :	Returns the server information for the operating 
*		system
*
*	Return : void ;
*
*	Note :
************************************************************************/
void get_sdk_info( OUT char *info );

#ifdef __cplusplus
}	// #extern "C"
#endif


#endif // GENLIB_NET_HTTP_HTTPREADWRITE_H
