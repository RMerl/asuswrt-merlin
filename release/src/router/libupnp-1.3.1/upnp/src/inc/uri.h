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

#ifndef GENLIB_NET_URI_H
#define GENLIB_NET_URI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <malloc.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>

#include "upnp.h"
//#include <upnp_debug.h>


#define HTTP_DATE_LENGTH 37 // length for HTTP DATE: 
                            //"DATE: Sun, 01 Jul 2000 08:15:23 GMT<cr><lf>"
#define SEPARATORS "()<>@,;:\\\"/[]?={} \t"
#define MARK "-_.!~*'()"
#define RESERVED ";/?:@&=+$,{}" //added {} for compatibility
#define HTTP_SUCCESS 1

#define FALSE 0
#define TAB 9
#define CR 13
#define LF 10
#define SOCKET_BUFFER_SIZE 5000

enum hostType { HOSTNAME, IPv4address };
enum pathType { ABS_PATH, REL_PATH, OPAQUE_PART };
enum uriType  { ABSOLUTE, RELATIVE };

/*	Buffer used in parsinghttp messages, urls, etc. generally this simply
*	holds a pointer into a larger array									*/
typedef struct TOKEN {
   char * buff;
  int size;
} token;


/*	Represents a host port:e.g. :"127.127.0.1:80"
*	text is a token pointing to the full string representation */
typedef struct HOSTPORT {
  token text; //full host port
  struct sockaddr_in IPv4address; //Network Byte Order  
} hostport_type;

/*	Represents a URI used in parse_uri and elsewhere */
typedef struct URI{
  enum uriType type;
  token scheme;
  enum pathType path_type;
  token pathquery;
  token fragment;
  hostport_type hostport;
} uri_type;

/* Represents a list of URLs as in the "callback" header of SUBSCRIBE
*	message in GENA 
*	char * URLs holds dynamic memory								*/
typedef struct URL_LIST {
  int size;
  char * URLs; //all the urls, delimited by <>
  uri_type *parsedURLs;
} URL_list;


/************************************************************************
*	Function :	replace_escaped
*
*	Parameters :
*		char * in ;	string of characters
*		int index ;	index at which to start checking the characters
*		int *max ;	
*
*	Description : Replaces an escaped sequence with its unescaped version 
*		as in http://www.ietf.org/rfc/rfc2396.txt  (RFC explaining URIs)
*       Size of array is NOT checked (MUST be checked by caller)
*
*	Return : int ;
*
*	Note : This function modifies the string. If the sequence is an 
*		escaped sequence it is replaced, the other characters in the 
*		string are shifted over, and NULL characters are placed at the 
*		end of the string.
************************************************************************/
int replace_escaped(char * in, int index, int *max);

/************************************************************************
*	Function :	copy_URL_list
*
*	Parameters :
*		URL_list *in ;	Source URL list
*		URL_list *out ;	Destination URL list
*
*	Description : Copies one URL_list into another. This includes 
*		dynamically allocating the out->URLs field (the full string),
*       and the structures used to hold the parsedURLs. This memory MUST 
*		be freed by the caller through: free_URL_list(&out)
*
*	Return : int ;
*		HTTP_SUCCESS - On Success
*		UPNP_E_OUTOF_MEMORY - On Failure to allocate memory
*
*	Note :
************************************************************************/
int copy_URL_list( URL_list *in, URL_list *out);

/************************************************************************
*	Function :	free_URL_list
*
*	Parameters :
*		URL_list * list ;	URL List object
*
*	Description : Frees the memory associated with a URL_list. Frees the 
*		dynamically allocated members of of list. Does NOT free the 
*		pointer to the list itself ( i.e. does NOT free(list))
*
*	Return : void ;
*
*	Note :
************************************************************************/
void free_URL_list(URL_list * list);

/************************************************************************
*	Function :	print_uri
*
*	Parameters :
*		uri_type *in ;	URI object
*
*	Description : Function useful in debugging for printing a parsed uri.
*		Compiled out with DBGONLY macro. 
*
*	Return : void ;
*
*	Note :
************************************************************************/
DBGONLY(void print_uri( uri_type *in);)

/************************************************************************
*	Function :	print_token
*
*	Parameters :
*		token * in ;	
*
*	Description : Function useful in debugging for printing a token.
*		Compiled out with DBGONLY macro. 
*
*	Return : void ;
*
*	Note :
************************************************************************/
void print_token(  token * in);

/************************************************************************
*	Function :	token_string_casecmp
*
*	Parameters :
*		token * in1 ;	Token object whose buffer is to be compared
*		char * in2 ;	string of characters to compare with
*
*	Description :	Compares buffer in the token object with the buffer 
*		in in2
*
*	Return : int ;
*		< 0 string1 less than string2 
*		0 string1 identical to string2 
*		> 0 string1 greater than string2 
*
*	Note :
************************************************************************/
int token_string_casecmp( token * in1,  char * in2);

/************************************************************************
*	Function :	token_string_cmp
*
*	Parameters :
*		token * in1 ;	Token object whose buffer is to be compared
*		char * in2 ;	string of characters to compare with
*
*	Description : Compares a null terminated string to a token (exact)	
*
*	Return : int ;
*		< 0 string1 less than string2 
*		0 string1 identical to string2 
*		> 0 string1 greater than string2 
*
*	Note :
************************************************************************/
int token_string_cmp( token * in1,  char * in2);

/************************************************************************
*	Function :	token_cmp
*
*	Parameters :
*		token *in1 ;	First token object whose buffer is to be compared
*		token *in2 ;	Second token object used for the comparison
*
*	Description : Compares two tokens	
*
*	Return : int ;
*		< 0 string1 less than string2 
*		0 string1 identical to string2 
*		> 0 string1 greater than string2 
*
*	Note :
************************************************************************/
int token_cmp( token *in1,  token *in2);

/************************************************************************
*	Function :	parse_port
*
*	Parameters :
*		int max ;	sets a maximum limit
*		char * port ;	port to be parsed.
*		unsigned short * out ;	 out parameter where the port is parsed 
*							and converted into network format
*
*	Description : parses a port (i.e. '4000') and converts it into a 
*		network ordered unsigned short int.
*
*	Return : int ;
*
*	Note :
************************************************************************/
int parse_port(int max,   char * port, unsigned short int * out);

/************************************************************************
*	Function :	parse_hostport
*
*	Parameters :
*		char *in ;	string of characters representing host and port
*		int max ;	sets a maximum limit
*		hostport_type *out ;	out parameter where the host and port
*					are represented as an internet address
*
*	Description : Parses a string representing a host and port
*		(e.g. "127.127.0.1:80" or "localhost") and fills out a 
*		hostport_type struct with internet address and a token 
*		representing the full host and port.  uses gethostbyname.
*
*	Return : int ;
*
*	Note :
************************************************************************/
int parse_hostport(  char* in, int max, hostport_type *out );

/************************************************************************
*	Function :	remove_escaped_chars
*
*	Parameters :
*		INOUT char *in ;	string of characters to be modified
*		INOUT int *size ;	size limit for the number of characters
*
*	Description : removes http escaped characters such as: "%20" and 
*		replaces them with their character representation. i.e. 
*		"hello%20foo" -> "hello foo". The input IS MODIFIED in place. 
*		(shortened). Extra characters are replaced with NULL.
*
*	Return : int ;
*		UPNP_E_SUCCESS
*
*	Note :
************************************************************************/
int remove_escaped_chars(char *in,int *size);

/************************************************************************
*	Function :	remove_dots
*
*	Parameters :
*		char *in ;	string of characters from which "dots" have to be 
*					removed
*		int size ;	size limit for the number of characters
*
*	Description : Removes ".", and ".." from a path. If a ".." can not
*		be resolved (i.e. the .. would go past the root of the path) an 
*		error is returned. The input IS modified in place.)
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success
*		UPNP_E_OUTOF_MEMORY - On failure to allocate memory
*		UPNP_E_INVALID_URL - Failure to resolve URL
*
*	Note :
*		Examples
*       char path[30]="/../hello";
*       remove_dots(path, strlen(path)) -> UPNP_E_INVALID_URL
*       char path[30]="/./hello";
*       remove_dots(path, strlen(path)) -> UPNP_E_SUCCESS, 
*       in = "/hello"
*       char path[30]="/./hello/foo/../goodbye" -> 
*       UPNP_E_SUCCESS, in = "/hello/goodbye"

************************************************************************/
int remove_dots(char * in, int size);

/************************************************************************
*	Function :	resolve_rel_url
*
*	Parameters :
*		char * base_url ;	Base URL
*		char * rel_url ;	Relative URL
*
*	Description : resolves a relative url with a base url returning a NEW 
*		(dynamically allocated with malloc) full url. If the base_url is 
*		NULL, then a copy of the  rel_url is passed back if the rel_url 
*		is absolute then a copy of the rel_url is passed back if neither 
*		the base nor the rel_url are Absolute then NULL is returned.
*		otherwise it tries and resolves the relative url with the base 
*		as described in: http://www.ietf.org/rfc/rfc2396.txt (RFCs 
*		explaining URIs) 
*       : resolution of '..' is NOT implemented, but '.' is resolved 
*
*	Return : char * ;
*
*	Note :
************************************************************************/
char * resolve_rel_url( char * base_url,  char * rel_url);

/************************************************************************
*	Function :	parse_uri
*
*	Parameters :
*		char * in ;	character string containing uri information to be 
*					parsed
*		int max ;	maximum limit on the number of characters
*		uri_type * out ; out parameter which will have the parsed uri
*					information	
*
*	Description : parses a uri as defined in http://www.ietf.org/rfc/
*		rfc2396.txt (RFC explaining URIs)
*		Handles absolute, relative, and opaque uris. Parses into the 
*		following pieces: scheme, hostport, pathquery, fragment (path and
*		query are treated as one token)
*       Caller should check for the pieces they require.
*
*	Return : int ;
*
*	Note :
************************************************************************/
int parse_uri(  char * in, int max, uri_type * out);

/************************************************************************
*	Function :	parse_uri_and_unescape
*
*	Parameters :
*		char * in ;	
*		int max ;	
*		uri_type * out ;	
*
*	Description : Same as parse_uri, except that all strings are 
*		unescaped (%XX replaced by chars)
*
*	Return : int ;
*
*	Note: This modifies 'pathquery' and 'fragment' parts of the input
************************************************************************/
int parse_uri_and_unescape(char * in, int max, uri_type * out);

int parse_token( char * in, token * out, int max_size);

/************************************************************************
*				commented #defines, functions and typdefs				*
************************************************************************/

/************************************************************************
*						Commented #defines								*
************************************************************************/
//#define HTTP_E_BAD_URL UPNP_E_INVALID_URL
//#define HTTP_E_READ_SOCKET  UPNP_E_SOCKET_READ
//#define HTTP_E_BIND_SOCKET  UPNP_E_SOCKET_BIND
//#define HTTP_E_WRITE_SOCKET  UPNP_E_SOCKET_WRITE
//#define HTTP_E_CONNECT_SOCKET  UPNP_E_SOCKET_CONNECT
//#define HTTP_E_SOCKET    UPNP_E_OUTOF_SOCKET
//#define HTTP_E_BAD_RESPONSE UPNP_E_BAD_RESPONSE
//#define HTTP_E_BAD_REQUEST UPNP_E_BAD_REQUEST
//#define HTTP_E_BAD_IP_ADDRESS UPNP_E_INVALID_URL

//#define RESPONSE_TIMEOUT 30

/************************************************************************
*						Commented typedefs								*
************************************************************************/
//Buffer used to store data read from a socket during an http transfer
//in function read_bytes.
//typedef struct SOCKET_BUFFER{
//  char buff[SOCKET_BUFFER_SIZE];
//  int size;
//  struct SOCKET_BUFFER *next;
//} socket_buffer;

//typedef struct HTTP_HEADER {
//  token header;
//  token value;
//  struct HTTP_HEADER * next;
//} http_header;

//typedef struct HTTP_STATUS_LINE{
//  token http_version;
//  token status_code;
//  token reason_phrase;
//} http_status;

//typedef struct HTTP_REQUEST_LINE {
//  token http_version;
//  uri_type request_uri;
//  token method;
//} http_request;

//Represents a parsed HTTP_MESSAGE head_list is dynamically allocated
//typedef struct HTTP_MESSAGE {
//  http_status status;
//  http_request request;
//  http_header * header_list;
//  token content;
//} http_message;

/************************************************************************
*						Commented functions								*
************************************************************************

EXTERN_C int transferHTTP( char * request,  char * toSend, 
			  int toSendSize, char **out,  char * Url);


EXTERN_C int transferHTTPRaw( char * toSend, int toSendSize, 
			     char **out,  char *URL);

helper function
EXTERN_C int transferHTTPparsedURL( char * request, 
				    char * toSend, int toSendSize, 
				   char **out, uri_type *URL);

assumes that char * out has enough space ( 38 characters)
outputs the current time in the following null terminated string:
 "DATE: Sun, Jul 06 2000 08:53:01 GMT\r\n"
EXTERN_C void currentTmToHttpDate(char *out);

EXTERN_C int parse_http_response(  char * in, http_message * out, 
				  int max_len);

EXTERN_C int parse_http_request( char * in, http_message *out, 
				int max_len);

EXTERN_C void print_http_message( http_message * message);
EXTERN_C int search_for_header( http_message * in, 
			        char * header, token *out_value);

EXTERN_C void print_status_line(http_status *in);
EXTERN_C void print_request_line(http_request *in);
EXTERN_C int parse_http_line(  char * in, int max_size);
EXTERN_C int parse_not_LWS(  char *in, token *out, int max_size);
EXTERN_C int parse_LWS( char * in, int max_size);

EXTERN_C size_t write_bytes(int fd,   char * bytes, size_t n, 
						    int timeout);
EXTERN_C void free_http_message(http_message * message);

************************************************************************/

#ifdef __cplusplus
}
#endif

#endif // GENLIB_NET_URI_H

