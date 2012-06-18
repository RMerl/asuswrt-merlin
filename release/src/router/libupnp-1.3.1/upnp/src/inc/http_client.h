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

#ifndef _http_client_h_
#define _http_client_h_
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif
#include "genlib/closesocket/upnpclosesocket.h"
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <malloc.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include "tools/config.h"
#include "upnp.h"
//#include "upnp_debug.h"


#define HTTP_DATE_LENGTH 37 // length for HTTP DATE: 
                            //"DATE: Sun, 01 Jul 2000 08:15:23 GMT<cr><lf>"
#define SEPARATORS "()<>@,;:\\\"/[]?={} \t"
#define MARK "-_.!~*'()"
#define RESERVED ";/?:@&=+$,"
#define HTTP_SUCCESS 1
#define HTTP_E_BAD_URL UPNP_E_INVALID_URL
#define HTTP_E_READ_SOCKET  UPNP_E_SOCKET_READ
#define HTTP_E_BIND_SOCKET  UPNP_E_SOCKET_BIND
#define HTTP_E_WRITE_SOCKET  UPNP_E_SOCKET_WRITE
#define HTTP_E_CONNECT_SOCKET  UPNP_E_SOCKET_CONNECT
#define HTTP_E_SOCKET    UPNP_E_OUTOF_SOCKET
#define HTTP_E_BAD_RESPONSE UPNP_E_BAD_RESPONSE
#define HTTP_E_BAD_REQUEST UPNP_E_BAD_REQUEST
#define HTTP_E_BAD_IP_ADDRESS UPNP_E_INVALID_URL
#define FALSE 0
#define TAB 9
#define CR 13
#define LF 10
#define RESPONSE_TIMEOUT 30
#define SOCKET_BUFFER_SIZE 5000

enum hostType { HOSTNAME, IPv4address };
enum pathType { ABS_PATH, REL_PATH, OPAQUE_PART };
enum uriType  { ABSOLUTE, RELATIVE };

//Buffer used to store data read from 
//a socket during an http transfer
//in function read_bytes.
typedef struct SOCKET_BUFFER{
  char buff[SOCKET_BUFFER_SIZE];
  int size;
  struct SOCKET_BUFFER *next;
} socket_buffer;

//Buffer used in parsing
//http messages, urls, etc.
//generally this simply
//holds a pointer into a larger array
typedef struct TOKEN {
   char * buff;
  int size;
} token;


//Represents a host port:
//e.g. :"127.127.0.1:80"
//text is a token pointing to 
//the full string representation
typedef struct HOSTPORT {
  token text; //full host port
  struct sockaddr_in IPv4address; //Network Byte Order  
} hostport_type;

//Represents a URI
//used in parse_uri and elsewhere
typedef struct URI{
  enum uriType type;
  token scheme;
  enum pathType path_type;
  token pathquery;
  token fragment;
  hostport_type hostport;
} uri_type;

//Represents a list of URLs as in 
//the "callback" header of SUBSCRIBE
//message in GENA
//char * URLs holds dynamic memory
typedef struct URL_LIST {
  int size;
  char * URLs; //all the urls, delimited by <>
  uri_type *parsedURLs;
} URL_list;

typedef struct HTTP_HEADER {
  token header;
  token value;
  struct HTTP_HEADER * next;
} http_header;

typedef struct HTTP_STATUS_LINE{
  token http_version;
  token status_code;
  token reason_phrase;
} http_status;

typedef struct HTTP_REQUEST_LINE {
  token http_version;
  uri_type request_uri;
  token method;
} http_request;

//Represents a parsed HTTP_MESSAGE
//head_list is dynamically allocated
typedef struct HTTP_MESSAGE {
  http_status status;
  http_request request;
  http_header * header_list;
  token content;
} http_message;


EXTERN_C int transferHTTP( char * request,  char * toSend, 
			  int toSendSize, char **out,  char * Url);


EXTERN_C int transferHTTPRaw( char * toSend, int toSendSize, 
			     char **out,  char *URL);

//helper function
EXTERN_C int transferHTTPparsedURL( char * request, 
				    char * toSend, int toSendSize, 
				   char **out, uri_type *URL);

//assumes that char * out has enough space ( 38 characters)
//outputs the current time in the following null terminated string:
// "DATE: Sun, Jul 06 2000 08:53:01 GMT\r\n"
EXTERN_C void currentTmToHttpDate(char *out);

//returns dynamic memory or NULL on error
EXTERN_C char * resolve_rel_url( char * base_url,  char * rel_url);

EXTERN_C int parse_uri(  char * in, int max, uri_type * out);

EXTERN_C int token_cmp( token *in1,  token *in2);

EXTERN_C int token_string_casecmp( token * in1,  char * in2);

EXTERN_C int token_string_cmp( token * in1,  char * in2);

EXTERN_C int parse_http_response(  char * in, http_message * out, 
				  int max_len);

EXTERN_C int parse_http_request( char * in, http_message *out, 
				int max_len);

EXTERN_C int search_for_header( http_message * in, 
			        char * header, token *out_value);



EXTERN_C int parse_hostport(  char* in, int max, hostport_type *out );

EXTERN_C size_t write_bytes(int fd,   char * bytes, size_t n, 
			    int timeout);
EXTERN_C void free_http_message(http_message * message);
EXTERN_C int copy_URL_list( URL_list *in, URL_list *out);
EXTERN_C void free_URL_list(URL_list * list);
EXTERN_C int parse_port(int max,   char * port, unsigned short int * out);

EXTERN_C int parse_http_line(  char * in, int max_size);
EXTERN_C int parse_not_LWS(  char *in, token *out, int max_size);
EXTERN_C int parse_LWS( char * in, int max_size);
EXTERN_C int parse_token( char * in, token * out, int max_size);
EXTERN_C ssize_t readLine(int fd, char *out, int max, int *timeout);
EXTERN_C int remove_dots(char * in, int size);


DBGONLY(EXTERN_C void print_http_request(http_message
					 *message,Dbg_Level DLevel,
					 Dbg_Module Module,char *DbgFileName,
					 int DbgLineNo););

DBGONLY(EXTERN_C void print_http_response(http_message *message,
					  Dbg_Level DLevel,
					  Dbg_Module Module,char *DbgFileName,
					  int DbgLineNo););

DBGONLY(EXTERN_C void print_token(  token * in,
				    Dbg_Level DLevel,
				    Dbg_Module Module,
				    char *DbgFileName,
				    int DbgLineNo););

DBGONLY(EXTERN_C void print_status_line(http_status *in,
					Dbg_Level DLevel,
					Dbg_Module Module,
					char *DbgFileName,
					int DbgLineNo););

DBGONLY(EXTERN_C void print_request_line(http_request *in,
					 Dbg_Level DLevel,
					 Dbg_Module Module,
					 char *DbgFileName,int DbgLineNo));

DBGONLY(EXTERN_C void print_uri( uri_type *in,
				 Dbg_Level DLevel,
				 Dbg_Module Module,
				 char *DbgFileName,
				 int DbgLineNo););

#endif
