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

/************************************************************************
* Purpose: This file defines the functionality making use of the http 
* It defines functions to receive messages, process messages, send 
* messages
************************************************************************/

#include "config.h"

#include <assert.h>
#include <stdarg.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/utsname.h>
#include "unixutil.h"
#include "upnp.h"
#include "upnpapi.h"
#include "membuffer.h"
#include "uri.h"
#include "statcodes.h"
#include "httpreadwrite.h"
#include "sock.h"
#include "webserver.h"

#define DOSOCKET_READ	1
#define DOSOCKET_WRITE	0

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
int
http_FixUrl( IN uri_type * url,
             OUT uri_type * fixed_url )
{
    char *temp_path = "/";

    *fixed_url = *url;

    if( token_string_casecmp( &fixed_url->scheme, "http" ) != 0 ) {
        return UPNP_E_INVALID_URL;
    }

    if( fixed_url->hostport.text.size == 0 ) {
        return UPNP_E_INVALID_URL;
    }
    // set pathquery to "/" if it is empty
    if( fixed_url->pathquery.size == 0 ) {
        fixed_url->pathquery.buff = temp_path;
        fixed_url->pathquery.size = 1;
    }

    return UPNP_E_SUCCESS;
}

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
int
http_FixStrUrl( IN char *urlstr,
                IN int urlstrlen,
                OUT uri_type * fixed_url )
{
    uri_type url;

    if( parse_uri( urlstr, urlstrlen, &url ) != HTTP_SUCCESS ) {
        return UPNP_E_INVALID_URL;
    }

    return http_FixUrl( &url, fixed_url );
}

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
int
http_Connect( IN uri_type * destination_url,
              OUT uri_type * url )
{
    int connfd;

    http_FixUrl( destination_url, url );

    connfd = socket( AF_INET, SOCK_STREAM, 0 );
    if( connfd == -1 ) {
        return UPNP_E_OUTOF_SOCKET;
    }

    if( connect( connfd, ( struct sockaddr * )&url->hostport.IPv4address,
                 sizeof( struct sockaddr_in ) ) == -1 ) {
        shutdown( connfd, SD_BOTH );
        UpnpCloseSocket( connfd );
        return UPNP_E_SOCKET_CONNECT;
    }

    return connfd;
}

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
int
http_RecvMessage( IN SOCKINFO * info,
                  OUT http_parser_t * parser,
                  IN http_method_t request_method,
                  IN OUT int *timeout_secs,
                  OUT int *http_error_code )
{
    parse_status_t status;
    int num_read;
    xboolean ok_on_close = FALSE;
    char buf[2 * 1024];

    if( request_method == HTTPMETHOD_UNKNOWN ) {
        parser_request_init( parser );
    } else {
        parser_response_init( parser, request_method );
    }

    while( TRUE ) {
        num_read = sock_read( info, buf, sizeof( buf ), timeout_secs );
        if( num_read > 0 ) {
            // got data
            status = parser_append( parser, buf, num_read );

            if( status == PARSE_SUCCESS ) {
                DBGONLY( UpnpPrintf
                         ( UPNP_INFO, HTTP, __FILE__, __LINE__,
                           "<<< (RECVD) <<<\n%s\n-----------------\n",
                           parser->msg.msg.buf );
                         //print_http_headers( &parser->msg );
                     )

                    if( parser->content_length >
                        ( unsigned int )g_maxContentLength ) {
                    *http_error_code = HTTP_REQ_ENTITY_TOO_LARGE;
                    return UPNP_E_OUTOF_BOUNDS;
                }

                return 0;
            } else if( status == PARSE_FAILURE ) {
                *http_error_code = parser->http_error_code;
                return UPNP_E_BAD_HTTPMSG;
            } else if( status == PARSE_INCOMPLETE_ENTITY ) {
                // read until close
                ok_on_close = TRUE;
            } else if( status == PARSE_CONTINUE_1 ) //Web post request. murari
            {
                return PARSE_SUCCESS;
            }
        } else if( num_read == 0 ) {
            if( ok_on_close ) {
                DBGONLY( UpnpPrintf
                         ( UPNP_INFO, HTTP, __FILE__, __LINE__,
                           "<<< (RECVD) <<<\n%s\n-----------------\n",
                           parser->msg.msg.buf );
                         //print_http_headers( &parser->msg );
                     )

                    return 0;
            } else {
                // partial msg
                *http_error_code = HTTP_BAD_REQUEST;    // or response
                return UPNP_E_BAD_HTTPMSG;
            }
        } else {
            *http_error_code = parser->http_error_code;
            return num_read;
        }
    }
}

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
int
http_SendMessage( IN SOCKINFO * info,
                  IN OUT int *TimeOut,
                  IN const char *fmt,
                  ... )
{
#define CHUNK_HEADER_SIZE 10
#define CHUNK_TAIL_SIZE 10

    char c;
    char *buf = NULL;
    size_t buf_length;
    char *filename = NULL;
    FILE *Fp;
    int num_read,
      num_written,
      amount_to_be_read = 0;
    va_list argp;
    char *file_buf = NULL,
     *ChunkBuf = NULL;
    struct SendInstruction *Instr = NULL;
    char Chunk_Header[10];
    int RetVal = 0;

    // 10 byte allocated for chunk header.
    int Data_Buf_Size = WEB_SERVER_BUF_SIZE;

    va_start( argp, fmt );

    while( ( c = *fmt++ ) != 0 ) {
        if( c == 'I' ) {
            Instr = ( struct SendInstruction * )
                va_arg( argp, struct SendInstruction * );

            assert( Instr );

            if( Instr->ReadSendSize >= 0 )
                amount_to_be_read = Instr->ReadSendSize;
            else
                amount_to_be_read = Data_Buf_Size;

            if( amount_to_be_read < WEB_SERVER_BUF_SIZE )
                Data_Buf_Size = amount_to_be_read;

            ChunkBuf = ( char * )malloc( Data_Buf_Size +
                                         CHUNK_HEADER_SIZE +
                                         CHUNK_TAIL_SIZE );
            if( !ChunkBuf )
                return UPNP_E_OUTOF_MEMORY;

            file_buf = ChunkBuf + 10;
        }

        if( c == 'f' ) {        // file name

            filename = ( char * )va_arg( argp, char * );

            if( Instr && Instr->IsVirtualFile )
                Fp = virtualDirCallback.open( filename, UPNP_READ );
            else
                Fp = fopen( filename, "rb" );

            if( Fp == NULL ) {
                free( ChunkBuf );
                return UPNP_E_FILE_READ_ERROR;
            }

            assert( Fp );

            if( Instr && Instr->IsRangeActive && Instr->IsVirtualFile ) {
                if( virtualDirCallback.seek( Fp, Instr->RangeOffset,
                                             SEEK_CUR ) != 0 ) {
                    free( ChunkBuf );
                    return UPNP_E_FILE_READ_ERROR;
                }
            } else if( Instr && Instr->IsRangeActive ) {
                if( fseek( Fp, Instr->RangeOffset, SEEK_CUR ) != 0 ) {
                    free( ChunkBuf );
                    return UPNP_E_FILE_READ_ERROR;
                }
            }

            while( amount_to_be_read ) {
                if( Instr ) {
                    if( amount_to_be_read >= Data_Buf_Size ) {
                        if( Instr->IsVirtualFile )
                            num_read = virtualDirCallback.read( Fp,
                                                                file_buf,
                                                                Data_Buf_Size );
                        else
                            num_read = fread( file_buf, 1, Data_Buf_Size,
                                              Fp );
                    } else {
                        if( Instr->IsVirtualFile )
                            num_read = virtualDirCallback.read( Fp,
                                                                file_buf,
                                                                amount_to_be_read );
                        else
                            num_read = fread( file_buf, 1,
                                              amount_to_be_read, Fp );
                    }

                    amount_to_be_read = amount_to_be_read - num_read;

                    if( Instr->ReadSendSize < 0 ) {
                        //read until close
                        amount_to_be_read = Data_Buf_Size;
                    }
                } else {
                    num_read = fread( file_buf, 1, Data_Buf_Size, Fp );
                }

                if( num_read == 0 ) // EOF so no more to send.
                {
                    if( Instr && Instr->IsChunkActive ) {
                        num_written = sock_write( info, "0\r\n\r\n",
                                                  strlen( "0\r\n\r\n" ),
                                                  TimeOut );
                    } else {
                        RetVal = UPNP_E_FILE_READ_ERROR;
                    }
                    goto Cleanup_File;
                }
                // Create chunk for the current buffer.
                if( Instr && Instr->IsChunkActive ) {
                    //Copy CRLF at the end of the chunk
                    memcpy( file_buf + num_read, "\r\n", 2 );

                    //Hex length for the chunk size.
                    sprintf( Chunk_Header, "%x", num_read );

                    //itoa(num_read,Chunk_Header,16); 
                    strcat( Chunk_Header, "\r\n" );

                    //Copy the chunk size header 
                    memcpy( file_buf - strlen( Chunk_Header ),
                            Chunk_Header, strlen( Chunk_Header ) );

                    // on the top of the buffer.
                    //file_buf[num_read+strlen(Chunk_Header)] = NULL;
                    //printf("Sending %s\n",file_buf-strlen(Chunk_Header));
                    num_written = sock_write( info,
                                              file_buf -
                                              strlen( Chunk_Header ),
                                              num_read +
                                              strlen( Chunk_Header ) + 2,
                                              TimeOut );

                    if( num_written !=
                        num_read + ( int )strlen( Chunk_Header )
                        + 2 ) {
                        goto Cleanup_File;  //Send error nothing we can do.
                    }
                } else {
                    // write data
                    num_written = sock_write( info, file_buf, num_read,
                                              TimeOut );

                    DBGONLY( UpnpPrintf
                             ( UPNP_INFO, HTTP, __FILE__, __LINE__,
                               ">>> (SENT) >>>\n%.*s\n------------\n",
                               ( int )num_written, file_buf );
                         )

                        //Send error nothing we can do
                        if( num_written != num_read ) {
                        goto Cleanup_File;
                    }
                }
            }                   //While
          Cleanup_File:
            va_end( argp );
            if( Instr && Instr->IsVirtualFile )
                virtualDirCallback.close( Fp );
            else
                fclose( Fp );
            free( ChunkBuf );
            return RetVal;

        } else if( c == 'b' ) { // memory buffer

            buf = ( char * )va_arg( argp, char * );

            buf_length = ( size_t ) va_arg( argp, size_t );
            if( buf_length > 0 ) {
                num_written = sock_write( info, buf, buf_length, TimeOut );
                if( ( size_t ) num_written != buf_length )
                    goto end;
                DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                                     ">>> (SENT) >>>\n%.*s\n------------\n",
                                     ( int )buf_length, buf );
                     )
            }
        }
    }

  end:
    va_end( argp );
    free( ChunkBuf );
    return 0;
}

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
int
http_RequestAndResponse( IN uri_type * destination,
                         IN const char *request,
                         IN size_t request_length,
                         IN http_method_t req_method,
                         IN int timeout_secs,
                         OUT http_parser_t * response )
{
    int tcp_connection;
    int ret_code;
    int http_error_code;
    SOCKINFO info;

    tcp_connection = socket( AF_INET, SOCK_STREAM, 0 );
    if( tcp_connection == -1 ) {
        parser_response_init( response, req_method );
        return UPNP_E_SOCKET_ERROR;
    }
    if( sock_init( &info, tcp_connection ) != UPNP_E_SUCCESS )
    {
        sock_destroy( &info, SD_BOTH );
        parser_response_init( response, req_method );
        return UPNP_E_SOCKET_ERROR;
    }
    // connect
    ret_code = connect( info.socket,
                        ( struct sockaddr * )&destination->hostport.
                        IPv4address, sizeof( struct sockaddr_in ) );

    if( ret_code == -1 ) {
        sock_destroy( &info, SD_BOTH );
        parser_response_init( response, req_method );
        return UPNP_E_SOCKET_CONNECT;
    }
    // send request
    ret_code = http_SendMessage( &info, &timeout_secs, "b",
                                 request, request_length );
    if( ret_code != 0 ) {
        sock_destroy( &info, SD_BOTH );
        parser_response_init( response, req_method );
        return ret_code;
    }
    // recv response
    ret_code = http_RecvMessage( &info, response, req_method,
                                 &timeout_secs, &http_error_code );

    sock_destroy( &info, SD_BOTH ); //should shutdown completely

    return ret_code;
}

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
int
http_Download( IN const char *url_str,
               IN int timeout_secs,
               OUT char **document,
               OUT int *doc_length,
               OUT char *content_type )
{
    int ret_code;
    uri_type url;
    char *msg_start,
     *entity_start,
     *hoststr,
     *temp;
    http_parser_t response;
    size_t msg_length,
      hostlen;
    memptr ctype;
    size_t copy_len;
    membuffer request;
    char *urlPath = alloca( strlen( url_str ) + 1 );

    //ret_code = parse_uri( (char*)url_str, strlen(url_str), &url );
    DBGONLY( UpnpPrintf
             ( UPNP_INFO, HTTP, __FILE__, __LINE__, "DOWNLOAD URL : %s\n",
               url_str );
         )
        ret_code =
        http_FixStrUrl( ( char * )url_str, strlen( url_str ), &url );
    if( ret_code != UPNP_E_SUCCESS ) {
        return ret_code;
    }
    // make msg
    membuffer_init( &request );

    strcpy( urlPath, url_str );
    hoststr = strstr( urlPath, "//" );
    if( hoststr == NULL ) {
        return UPNP_E_INVALID_URL;
    }

    hoststr += 2;
    temp = strchr( hoststr, '/' );
    if( temp == NULL ) {
        return UPNP_E_INVALID_URL;
    }

    *temp = '\0';
    hostlen = strlen( hoststr );
    *temp = '/';
    DBGONLY( UpnpPrintf
             ( UPNP_INFO, HTTP, __FILE__, __LINE__,
               "HOSTNAME : %s Length : %d\n", hoststr, hostlen );
         )

        ret_code = http_MakeMessage( &request, 1, 1, "QsbcDCUc",
                                     HTTPMETHOD_GET, url.pathquery.buff,
                                     url.pathquery.size, "HOST: ", hoststr,
                                     hostlen );
    if( ret_code != 0 ) {
        DBGONLY( UpnpPrintf
                 ( UPNP_INFO, HTTP, __FILE__, __LINE__,
                   "HTTP Makemessage failed\n" );
             )
            membuffer_destroy( &request );
        return ret_code;
    }

    DBGONLY( UpnpPrintf
             ( UPNP_INFO, HTTP, __FILE__, __LINE__,
               "HTTP Buffer:\n %s\n----------END--------\n", request.buf );
         )
        // get doc msg
        ret_code =
        http_RequestAndResponse( &url, request.buf, request.length,
                                 HTTPMETHOD_GET, timeout_secs, &response );

    if( ret_code != 0 ) {
        httpmsg_destroy( &response.msg );
        membuffer_destroy( &request );
        return ret_code;
    }

    DBGONLY( UpnpPrintf
             ( UPNP_INFO, HTTP, __FILE__, __LINE__, "Response\n" );
         )
        DBGONLY( print_http_headers( &response.msg );
         )

        // optional content-type
        if( content_type ) {
        if( httpmsg_find_hdr( &response.msg, HDR_CONTENT_TYPE, &ctype ) ==
            NULL ) {
            *content_type = '\0';   // no content-type
        } else {
            // safety
            copy_len = ctype.length < LINE_SIZE - 1 ?
                ctype.length : LINE_SIZE - 1;

            memcpy( content_type, ctype.buf, copy_len );
            content_type[copy_len] = '\0';
        }
    }
    //
    // extract doc from msg
    //

    if( ( *doc_length = ( int )response.msg.entity.length ) == 0 ) {
        // 0-length msg
        *document = NULL;
    } else if( response.msg.status_code == HTTP_OK )    //LEAK_FIX_MK
    {
        // copy entity
        entity_start = response.msg.entity.buf; // what we want
        msg_length = response.msg.msg.length;   // save for posterity   
        msg_start = membuffer_detach( &response.msg.msg );  // whole msg

        // move entity to the start; copy null-terminator too
        memmove( msg_start, entity_start, *doc_length + 1 );

        // save mem for body only
        *document = realloc( msg_start, *doc_length + 1 );  //LEAK_FIX_MK
        //*document = Realloc( msg_start,msg_length, *doc_length + 1 );//LEAK_FIX_MK

        // shrink can't fail
        assert( ( int )msg_length > *doc_length );
        assert( *document != NULL );
    }

    if( response.msg.status_code == HTTP_OK ) {
        ret_code = 0;           // success
    } else {
        // server sent error msg (not requested doc)
        ret_code = response.msg.status_code;
    }

    httpmsg_destroy( &response.msg );
    membuffer_destroy( &request );

    return ret_code;
}

typedef struct HTTPPOSTHANDLE {
    SOCKINFO sock_info;
    int contentLength;
} http_post_handle_t;

/************************************************************************
* Function: MakePostMessage												
*																		
* Parameters:															
*	const char *url_str ;		String as a URL
*	membuffer *request ;		Buffer containing the request									
*	uri_type *url ; 			URI object containing the scheme, path 
*								query token, etc.
*	int contentLength ;			length of content
*	const char *contentType	;	Type of content
*																		
* Description: Makes the message for the HTTP POST message				
*																		
* Returns:																
*	UPNP_E_INVALID_URL													
* 	UPNP_E_INVALID_PARAM												
*	UPNP_E_SUCCESS														
************************************************************************/
int
MakePostMessage( const char *url_str,
                 membuffer * request,
                 uri_type * url,
                 int contentLength,
                 const char *contentType )
{
    int ret_code = 0;
    char *urlPath = alloca( strlen( url_str ) + 1 );
    int hostlen = 0;
    char *hoststr,
     *temp;

    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "DOWNLOAD URL : %s\n", url_str );
         )

        ret_code =
        http_FixStrUrl( ( char * )url_str, strlen( url_str ), url );

    if( ret_code != UPNP_E_SUCCESS ) {
        return ret_code;
    }
    // make msg
    membuffer_init( request );

    strcpy( urlPath, url_str );
    hoststr = strstr( urlPath, "//" );
    if( hoststr == NULL ) {
        return UPNP_E_INVALID_URL;
    }

    hoststr += 2;
    temp = strchr( hoststr, '/' );
    if( temp == NULL ) {
        return UPNP_E_INVALID_URL;
    }

    *temp = '\0';
    hostlen = strlen( hoststr );
    *temp = '/';
    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "HOSTNAME : %s Length : %d\n", hoststr, hostlen );
         )

        if( contentLength >= 0 ) {
        ret_code = http_MakeMessage( request, 1, 1, "QsbcDCUTNc",
                                     HTTPMETHOD_POST, url->pathquery.buff,
                                     url->pathquery.size, "HOST: ",
                                     hoststr, hostlen, contentType,
                                     contentLength );
    } else if( contentLength == UPNP_USING_CHUNKED ) {
        ret_code = http_MakeMessage( request, 1, 1, "QsbcDCUTKc",
                                     HTTPMETHOD_POST, url->pathquery.buff,
                                     url->pathquery.size, "HOST: ",
                                     hoststr, hostlen, contentType );
    } else if( contentLength == UPNP_UNTIL_CLOSE ) {
        ret_code = http_MakeMessage( request, 1, 1, "QsbcDCUTc",
                                     HTTPMETHOD_POST, url->pathquery.buff,
                                     url->pathquery.size, "HOST: ",
                                     hoststr, hostlen, contentType );
    } else {
        ret_code = UPNP_E_INVALID_PARAM;
    }

    if( ret_code != 0 ) {
        DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                             "HTTP Makemessage failed\n" );
             )
            membuffer_destroy( request );
        return ret_code;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "HTTP Buffer:\n %s\n" "----------END--------\n",
                         request->buf );
         )

        return UPNP_E_SUCCESS;
}

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
int
http_WriteHttpPost( IN void *Handle,
                    IN char *buf,
                    IN unsigned int *size,
                    IN int timeout )
{
    http_post_handle_t *handle = ( http_post_handle_t * ) Handle;
    char *tempbuf = NULL;
    int tempbufSize = 0;
    int freeTempbuf = 0;
    int numWritten = 0;

    if( ( !handle ) || ( !size ) || ( ( ( *size ) > 0 ) && !buf )
        || ( ( *size ) < 0 ) ) {
        if(size) ( *size ) = 0;
        return UPNP_E_INVALID_PARAM;
    }
    if( handle->contentLength == UPNP_USING_CHUNKED ) {
        if( ( *size ) ) {
            int tempSize = 0;

            tempbuf =
                ( char * )malloc( ( *size ) + CHUNK_HEADER_SIZE +
                                  CHUNK_TAIL_SIZE );

            if ( tempbuf == NULL) return UPNP_E_OUTOF_MEMORY;

            sprintf( tempbuf, "%x\r\n", ( *size ) );    //begin chunk
            tempSize = strlen( tempbuf );
            memcpy( tempbuf + tempSize, buf, ( *size ) );
            memcpy( tempbuf + tempSize + ( *size ), "\r\n", 2 );    //end of chunk
            tempbufSize = tempSize + ( *size ) + 2;
            freeTempbuf = 1;
        }
    } else {
        tempbuf = buf;
        tempbufSize = ( *size );
    }

    numWritten =
        sock_write( &handle->sock_info, tempbuf, tempbufSize, &timeout );
    //(*size) = sock_write(&handle->sock_info,tempbuf,tempbufSize,&timeout);

    if( freeTempbuf ) {
        free( tempbuf );
    }
    if( numWritten < 0 ) {
        ( *size ) = 0;
        return numWritten;
    } else {
        ( *size ) = numWritten;
        return UPNP_E_SUCCESS;
    }
}

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
int
http_CloseHttpPost( IN void *Handle,
                    IN OUT int *httpStatus,
                    IN int timeout )
{
    int retc = 0;
    http_parser_t response;
    int http_error_code;

    http_post_handle_t *handle = Handle;

    if( ( !handle ) || ( !httpStatus ) ) {
        return UPNP_E_INVALID_PARAM;
    }

    if( handle->contentLength == UPNP_USING_CHUNKED ) {
        retc = sock_write( &handle->sock_info, "0\r\n\r\n", strlen( "0\r\n\r\n" ), &timeout );  //send last chunk
    }
    //read response
    parser_response_init( &response, HTTPMETHOD_POST );

    retc =
        http_RecvMessage( &handle->sock_info, &response, HTTPMETHOD_POST,
                          &timeout, &http_error_code );

    ( *httpStatus ) = http_error_code;

    sock_destroy( &handle->sock_info, SD_BOTH );    //should shutdown completely

    httpmsg_destroy( &response.msg );
    free( handle );

    return retc;
}

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
int
http_OpenHttpPost( IN const char *url_str,
                   IN OUT void **Handle,
                   IN const char *contentType,
                   IN int contentLength,
                   IN int timeout )
{
    int ret_code;
    int tcp_connection;
    membuffer request;
    http_post_handle_t *handle = NULL;
    uri_type url;

    if( ( !url_str ) || ( !Handle ) || ( !contentType ) ) {
        return UPNP_E_INVALID_PARAM;
    }

    ( *Handle ) = handle;

    if( ( ret_code =
          MakePostMessage( url_str, &request, &url, contentLength,
                           contentType ) ) != UPNP_E_SUCCESS ) {
        return ret_code;
    }

    handle =
        ( http_post_handle_t * ) malloc( sizeof( http_post_handle_t ) );

    if( handle == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    handle->contentLength = contentLength;

    tcp_connection = socket( AF_INET, SOCK_STREAM, 0 );
    if( tcp_connection == -1 ) {
        ret_code = UPNP_E_SOCKET_ERROR;
        goto errorHandler;
    }

    if( sock_init( &handle->sock_info, tcp_connection ) != UPNP_E_SUCCESS )
    {
        sock_destroy( &handle->sock_info, SD_BOTH );
        ret_code = UPNP_E_SOCKET_ERROR;
        goto errorHandler;
    }

    ret_code = connect( handle->sock_info.socket,
                        ( struct sockaddr * )&url.hostport.IPv4address,
                        sizeof( struct sockaddr_in ) );

    if( ret_code == -1 ) {
        sock_destroy( &handle->sock_info, SD_BOTH );
        ret_code = UPNP_E_SOCKET_CONNECT;
        goto errorHandler;
    }
    // send request
    ret_code = http_SendMessage( &handle->sock_info, &timeout, "b",
                                 request.buf, request.length );
    if( ret_code != 0 ) {
        sock_destroy( &handle->sock_info, SD_BOTH );
    }

  errorHandler:
    membuffer_destroy( &request );
    ( *Handle ) = handle;
    return ret_code;
}

typedef struct HTTPGETHANDLE {
    http_parser_t response;
    SOCKINFO sock_info;
    int entity_offset;
} http_get_handle_t;

/************************************************************************
* Function: MakeGetMessage												
*																		
* Parameters:															
*	const char *url_str ;	String as a URL
*	membuffer *request ;	Buffer containing the request									
*	uri_type *url ; 		URI object containing the scheme, path 
*							query token, etc.
*																		
* Description: Makes the message for the HTTP GET method				
*																		
* Returns:																
*	UPNP_E_INVALID_URL													
* 	Error Codes returned by http_MakeMessage							
*	UPNP_E_SUCCESS														
************************************************************************/
int
MakeGetMessage( const char *url_str,
                membuffer * request,
                uri_type * url )
{
    int ret_code;
    char *urlPath = alloca( strlen( url_str ) + 1 );
    int hostlen = 0;
    char *hoststr,
     *temp;

    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "DOWNLOAD URL : %s\n", url_str );
         )

        ret_code =
        http_FixStrUrl( ( char * )url_str, strlen( url_str ), url );

    if( ret_code != UPNP_E_SUCCESS ) {
        return ret_code;
    }
    // make msg
    membuffer_init( request );

    strcpy( urlPath, url_str );
    hoststr = strstr( urlPath, "//" );
    if( hoststr == NULL ) {
        return UPNP_E_INVALID_URL;
    }

    hoststr += 2;
    temp = strchr( hoststr, '/' );
    if( temp == NULL ) {
        return UPNP_E_INVALID_URL;
    }

    *temp = '\0';
    hostlen = strlen( hoststr );
    *temp = '/';
    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "HOSTNAME : %s Length : %d\n", hoststr, hostlen );
         )

        ret_code = http_MakeMessage( request, 1, 1, "QsbcDCUc",
                                     HTTPMETHOD_GET, url->pathquery.buff,
                                     url->pathquery.size, "HOST: ",
                                     hoststr, hostlen );

    if( ret_code != 0 ) {
        DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                             "HTTP Makemessage failed\n" );
             )
            membuffer_destroy( request );
        return ret_code;
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "HTTP Buffer:\n %s\n" "----------END--------\n",
                         request->buf );
         )

        return UPNP_E_SUCCESS;
}

/************************************************************************
*	Function :	ReadResponseLineAndHeaders
*
*	Parameters :
*		IN SOCKINFO *info ;				Socket information object
*		IN OUT http_parser_t *parser ;	HTTP Parser object
*		IN OUT int *timeout_secs ;		time out value
*		IN OUT int *http_error_code ;	HTTP errror code returned
*
*	Description : Parses already exiting data. If not complete reads more 
*		data on the connected socket. The read data is then parsed. The 
*		same methid is carried out for headers.
*
*	Return : int ;
*		PARSE_OK - On Success
*		PARSE_FAILURE - Failure to parse data correctly
*		UPNP_E_BAD_HTTPMSG - Socker read() returns an error
*
*	Note :
************************************************************************/
int
ReadResponseLineAndHeaders( IN SOCKINFO * info,
                            IN OUT http_parser_t * parser,
                            IN OUT int *timeout_secs,
                            IN OUT int *http_error_code )
{
    parse_status_t status;
    int num_read;
    char buf[2 * 1024];
    int done = 0;
    int ret_code = 0;

    //read response line

    status = parser_parse_responseline( parser );
    if( status == PARSE_OK ) {
        done = 1;
    } else if( status == PARSE_INCOMPLETE ) {
        done = 0;
    } else {
        //error
        return status;
    }

    while( !done ) {
        num_read = sock_read( info, buf, sizeof( buf ), timeout_secs );
        if( num_read > 0 ) {
            // append data to buffer
            ret_code = membuffer_append( &parser->msg.msg, buf, num_read );
            if( ret_code != 0 ) {
                // set failure status
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }
            status = parser_parse_responseline( parser );
            if( status == PARSE_OK ) {
                done = 1;
            } else if( status == PARSE_INCOMPLETE ) {
                done = 0;
            } else {
                //error
                return status;
            }
        } else if( num_read == 0 ) {

            // partial msg
            *http_error_code = HTTP_BAD_REQUEST;    // or response
            return UPNP_E_BAD_HTTPMSG;

        } else {
            *http_error_code = parser->http_error_code;
            return num_read;
        }
    }

    done = 0;

    status = parser_parse_headers( parser );
    if( ( status == PARSE_OK ) && ( parser->position == POS_ENTITY ) ) {

        done = 1;
    } else if( status == PARSE_INCOMPLETE ) {
        done = 0;
    } else {
        //error
        return status;
    }

    //read headers
    while( !done ) {
        num_read = sock_read( info, buf, sizeof( buf ), timeout_secs );
        if( num_read > 0 ) {
            // append data to buffer
            ret_code = membuffer_append( &parser->msg.msg, buf, num_read );
            if( ret_code != 0 ) {
                // set failure status
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }
            status = parser_parse_headers( parser );
            if( ( status == PARSE_OK )
                && ( parser->position == POS_ENTITY ) ) {

                done = 1;
            } else if( status == PARSE_INCOMPLETE ) {
                done = 0;
            } else {
                //error
                return status;
            }
        } else if( num_read == 0 ) {

            // partial msg
            *http_error_code = HTTP_BAD_REQUEST;    // or response
            return UPNP_E_BAD_HTTPMSG;

        } else {
            *http_error_code = parser->http_error_code;
            return num_read;
        }
    }

    return PARSE_OK;
}

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
int
http_ReadHttpGet( IN void *Handle,
                  IN OUT char *buf,
                  IN OUT unsigned int *size,
                  IN int timeout )
{
    http_get_handle_t *handle = Handle;

    parse_status_t status;
    int num_read;
    xboolean ok_on_close = FALSE;
    char tempbuf[2 * 1024];

    int ret_code = 0;

    if( ( !handle ) || ( !size ) || ( ( ( *size ) > 0 ) && !buf )
        || ( ( *size ) < 0 ) ) {
        if(size) ( *size ) = 0;
        return UPNP_E_INVALID_PARAM;
    }
    //first parse what has already been gotten
    if( handle->response.position != POS_COMPLETE ) {
        status = parser_parse_entity( &handle->response );
    } else {
        status = PARSE_SUCCESS;
    }

    if( status == PARSE_INCOMPLETE_ENTITY ) {
        // read until close
        ok_on_close = TRUE;
    } else if( ( status != PARSE_SUCCESS )
               && ( status != PARSE_CONTINUE_1 )
               && ( status != PARSE_INCOMPLETE ) ) {
        //error
        ( *size ) = 0;
        return UPNP_E_BAD_RESPONSE;
    }
    //read more if necessary entity
    while( ( ( handle->entity_offset + ( *size ) ) >
             handle->response.msg.entity.length )
           && ( handle->response.position != POS_COMPLETE ) ) {
        num_read =
            sock_read( &handle->sock_info, tempbuf, sizeof( tempbuf ),
                       &timeout );
        if( num_read > 0 ) {
            // append data to buffer
            ret_code = membuffer_append( &handle->response.msg.msg,
                                         tempbuf, num_read );
            if( ret_code != 0 ) {
                // set failure status
                handle->response.http_error_code =
                    HTTP_INTERNAL_SERVER_ERROR;
                ( *size ) = 0;
                return PARSE_FAILURE;
            }
            status = parser_parse_entity( &handle->response );
            if( status == PARSE_INCOMPLETE_ENTITY ) {
                // read until close
                ok_on_close = TRUE;
            } else if( ( status != PARSE_SUCCESS )
                       && ( status != PARSE_CONTINUE_1 )
                       && ( status != PARSE_INCOMPLETE ) ) {
                //error
                ( *size ) = 0;
                return UPNP_E_BAD_RESPONSE;
            }
        } else if( num_read == 0 ) {
            if( ok_on_close ) {
                DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                                     "<<< (RECVD) <<<\n%s\n-----------------\n",
                                     handle->response.msg.msg.buf );
                         //print_http_headers( &parser->msg );
                     )
                    handle->response.position = POS_COMPLETE;
            } else {
                // partial msg
                ( *size ) = 0;
                handle->response.http_error_code = HTTP_BAD_REQUEST;    // or response
                return UPNP_E_BAD_HTTPMSG;
            }
        } else {
            ( *size ) = 0;
            return num_read;
        }
    }

    if( ( handle->entity_offset + ( *size ) ) >
        handle->response.msg.entity.length ) {
        ( *size ) =
            handle->response.msg.entity.length - handle->entity_offset;
    }

    memcpy( buf,
            &handle->response.msg.msg.buf[handle->
                                          response.entity_start_position +
                                          handle->entity_offset],
            ( *size ) );
    handle->entity_offset += ( *size );
    return UPNP_E_SUCCESS;
}

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
int
http_CloseHttpGet( IN void *Handle )
{
    http_get_handle_t *handle = Handle;

    if( !handle ) {
        return UPNP_E_INVALID_PARAM;
    }

    sock_destroy( &handle->sock_info, SD_BOTH );    //should shutdown completely
    httpmsg_destroy( &handle->response.msg );
    handle->entity_offset = 0;
    free( handle );
    return UPNP_E_SUCCESS;
}

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
int
http_OpenHttpGet( IN const char *url_str,
                  IN OUT void **Handle,
                  IN OUT char **contentType,
                  OUT int *contentLength,
                  OUT int *httpStatus,
                  IN int timeout )
{
    int ret_code;
    int http_error_code;
    memptr ctype;
    int tcp_connection;
    membuffer request;
    http_get_handle_t *handle = NULL;
    uri_type url;
    parse_status_t status;

    if( ( !url_str ) || ( !Handle ) || ( !contentType )
        || ( !httpStatus ) ) {
        return UPNP_E_INVALID_PARAM;
    }

    ( *httpStatus ) = 0;
    ( *Handle ) = handle;
    ( *contentType ) = NULL;
    ( *contentLength ) = 0;

    if( ( ret_code =
          MakeGetMessage( url_str, &request, &url ) ) != UPNP_E_SUCCESS ) {
        return ret_code;
    }

    handle = ( http_get_handle_t * ) malloc( sizeof( http_get_handle_t ) );

    if( handle == NULL ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    handle->entity_offset = 0;
    parser_response_init( &handle->response, HTTPMETHOD_GET );

    tcp_connection = socket( AF_INET, SOCK_STREAM, 0 );
    if( tcp_connection == -1 ) {
        ret_code = UPNP_E_SOCKET_ERROR;
        goto errorHandler;
    }

    if( sock_init( &handle->sock_info, tcp_connection ) != UPNP_E_SUCCESS )
    {
        sock_destroy( &handle->sock_info, SD_BOTH );
        ret_code = UPNP_E_SOCKET_ERROR;
        goto errorHandler;
    }

    ret_code = connect( handle->sock_info.socket,
                        ( struct sockaddr * )&url.hostport.IPv4address,
                        sizeof( struct sockaddr_in ) );

    if( ret_code == -1 ) {
        sock_destroy( &handle->sock_info, SD_BOTH );
        ret_code = UPNP_E_SOCKET_CONNECT;
        goto errorHandler;
    }
    // send request
    ret_code = http_SendMessage( &handle->sock_info, &timeout, "b",
                                 request.buf, request.length );
    if( ret_code != 0 ) {
        sock_destroy( &handle->sock_info, SD_BOTH );
        goto errorHandler;
    }

    status =
        ReadResponseLineAndHeaders( &handle->sock_info, &handle->response,
                                    &timeout, &http_error_code );

    if( status != PARSE_OK ) {
        ret_code = UPNP_E_BAD_RESPONSE;
        goto errorHandler;
    }

    status = parser_get_entity_read_method( &handle->response );

    if( ( status != PARSE_CONTINUE_1 ) && ( status != PARSE_SUCCESS ) ) {
        ret_code = UPNP_E_BAD_RESPONSE;
        goto errorHandler;
    }

    ( *httpStatus ) = handle->response.msg.status_code;
    ret_code = UPNP_E_SUCCESS;

    if( httpmsg_find_hdr( &handle->response.msg, HDR_CONTENT_TYPE, &ctype )
        == NULL ) {
        *contentType = NULL;    // no content-type
    } else {
        *contentType = ctype.buf;
    }

    if( handle->response.position == POS_COMPLETE ) {
        ( *contentLength ) = 0;
    } else if( handle->response.ent_position == ENTREAD_USING_CHUNKED ) {
        ( *contentLength ) = UPNP_USING_CHUNKED;
    } else if( handle->response.ent_position == ENTREAD_USING_CLEN ) {
        ( *contentLength ) = handle->response.content_length;
    } else if( handle->response.ent_position == ENTREAD_UNTIL_CLOSE ) {
        ( *contentLength ) = UPNP_UNTIL_CLOSE;
    }

  errorHandler:

    ( *Handle ) = handle;

    membuffer_destroy( &request );

    if( ret_code != UPNP_E_SUCCESS ) {
        httpmsg_destroy( &handle->response.msg );
    }
    return ret_code;
}

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
int
http_SendStatusResponse( IN SOCKINFO * info,
                         IN int http_status_code,
                         IN int request_major_version,
                         IN int request_minor_version )
{
    int response_major,
      response_minor;
    membuffer membuf;
    int ret;
    int timeout;

    http_CalcResponseVersion( request_major_version, request_minor_version,
                              &response_major, &response_minor );

    membuffer_init( &membuf );
    membuf.size_inc = 70;

    ret = http_MakeMessage( &membuf, response_major, response_minor, "RSCB", http_status_code,  // response start line
                            http_status_code ); // body
    if( ret == 0 ) {
        timeout = HTTP_DEFAULT_TIMEOUT;
        ret = http_SendMessage( info, &timeout, "b",
                                membuf.buf, membuf.length );
    }

    membuffer_destroy( &membuf );

    return ret;
}

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
* --- PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld@users.sourceforge.net>
*       'X':    arg = const char useragent; "redsonic" HTTP X-User-Agent: useragent
* --- PATCH END ---
*
*	Return : int;
*		0 - On Success
*		UPNP_E_OUTOF_MEMORY
*		UPNP_E_INVALID_URL;
*
*	Note :
************************************************************************/
int
http_MakeMessage( INOUT membuffer * buf,
                  IN int http_major_version,
                  IN int http_minor_version,
                  IN const char *fmt,
                  ... )
{
    char c;
    char *s = NULL;
    int num;
    size_t length;
    time_t *loc_time;
    time_t curr_time;
    struct tm *date;
    char *start_str,
     *end_str;
    int status_code;
    const char *status_msg;
    http_method_t method;
    const char *method_str;
    const char *url_str;
    const char *temp_str;
    uri_type url;
    uri_type *uri_ptr;
    int error_code = UPNP_E_OUTOF_MEMORY;

    va_list argp;
    char tempbuf[200];
    const char *weekday_str = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";
    const char *month_str = "Jan\0Feb\0Mar\0Apr\0May\0Jun\0"
        "Jul\0Aug\0Sep\0Oct\0Nov\0Dec";

    va_start( argp, fmt );

    while( ( c = *fmt++ ) != 0 ) {

        if( c == 's' )          // C string
        {
            s = ( char * )va_arg( argp, char * );

            assert( s );

            //DBGONLY(UpnpPrintf(UPNP_ALL,HTTP,__FILE__,__LINE__,"Adding a string : %s\n", s);) 
            if( membuffer_append( buf, s, strlen( s ) ) != 0 ) {
                goto error_handler;
            }
        } else if( c == 'K' )   // Add Chunky header
        {
            if( membuffer_append
                ( buf, "TRANSFER-ENCODING: chunked\r\n",
                  strlen( "Transfer-Encoding: chunked\r\n" ) ) != 0 ) {
                goto error_handler;
            }
        } else if( c == 'G' )   // Add Range header
        {
            struct SendInstruction *RespInstr;
            RespInstr =
                ( struct SendInstruction * )va_arg( argp,
                                                    struct SendInstruction
                                                    * );
            assert( RespInstr );
            // connection header
            if( membuffer_append
                ( buf, RespInstr->RangeHeader,
                  strlen( RespInstr->RangeHeader ) ) != 0 ) {
                goto error_handler;
            }

        } else if( c == 'b' )   // mem buffer
        {
            s = ( char * )va_arg( argp, char * );

            //DBGONLY(UpnpPrintf(UPNP_ALL,HTTP,__FILE__,__LINE__,"Adding a char Buffer starting with: %c\n", s[0]);)
            assert( s );
            length = ( size_t ) va_arg( argp, size_t );
            if( membuffer_append( buf, s, length ) != 0 ) {
                goto error_handler;
            }
        }

        else if( c == 'c' )     // crlf
        {
            if( membuffer_append( buf, "\r\n", 2 ) != 0 ) {
                goto error_handler;
            }
        }

        else if( c == 'd' )     // integer
        {
            num = ( int )va_arg( argp, int );

            sprintf( tempbuf, "%d", num );
            if( membuffer_append( buf, tempbuf, strlen( tempbuf ) ) != 0 ) {
                goto error_handler;
            }
        }

        else if( c == 't' || c == 'D' ) // date
        {
            if( c == 'D' ) {
                // header
                start_str = "DATE: ";
                end_str = "\r\n";
                curr_time = time( NULL );
                date = gmtime( &curr_time );
            } else {
                // date value only
                start_str = end_str = "";
                loc_time = ( time_t * ) va_arg( argp, time_t * );
                assert( loc_time );
                date = gmtime( loc_time );
            }

            sprintf( tempbuf, "%s%s, %02d %s %d %02d:%02d:%02d GMT%s",
                     start_str,
                     &weekday_str[date->tm_wday * 4], date->tm_mday,
                     &month_str[date->tm_mon * 4], date->tm_year + 1900,
                     date->tm_hour, date->tm_min, date->tm_sec, end_str );

            if( membuffer_append( buf, tempbuf, strlen( tempbuf ) ) != 0 ) {
                goto error_handler;
            }
        }

        else if( c == 'C' ) {
            if( ( http_major_version > 1 ) ||
                ( http_major_version == 1 && http_minor_version == 1 )
                 ) {
                // connection header
                if( membuffer_append_str( buf, "CONNECTION: close\r\n" ) !=
                    0 ) {
                    goto error_handler;
                }
            }
        }

        else if( c == 'N' ) {
            // content-length header
            num = ( int )va_arg( argp, int );

            assert( num >= 0 );
            if( http_MakeMessage
                ( buf, http_major_version, http_minor_version, "sdc",
                  "CONTENT-LENGTH: ", num ) != 0 ) {
                goto error_handler;
            }
        }

        else if( c == 'S' || c == 'U' ) {
            // SERVER or USER-AGENT header

            temp_str = ( c == 'S' ) ? "SERVER: " : "USER-AGENT: ";
            get_sdk_info( tempbuf );
            if( http_MakeMessage
                ( buf, http_major_version, http_minor_version, "ss",
                  temp_str, tempbuf ) != 0 ) {
                goto error_handler;
            }
        }

/* --- PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld@users.sourceforge.net> */
	else if( c == 'X' )          // C string
        {
            s = ( char * )va_arg( argp, char * );

            assert( s );

            if( membuffer_append_str( buf, "X-User-Agent: ") != 0 ) {
                goto error_handler;
            }
            if( membuffer_append( buf, s, strlen( s ) ) != 0 ) {
                goto error_handler;
            }
        }
        
/* --- PATCH END --- */
    

        else if( c == 'R' ) {
            // response start line
            //   e.g.: 'HTTP/1.1 200 OK'
            //

            // code
            status_code = ( int )va_arg( argp, int );

            assert( status_code > 0 );
            sprintf( tempbuf, "HTTP/%d.%d %d ",
                     http_major_version, http_minor_version, status_code );

            // str
            status_msg = http_get_code_text( status_code );
            if( http_MakeMessage
                ( buf, http_major_version, http_minor_version, "ssc",
                  tempbuf, status_msg ) != 0 ) {
                goto error_handler;
            }
        }

        else if( c == 'B' ) {
            // body of a simple reply
            // 

            status_code = ( int )va_arg( argp, int );

            sprintf( tempbuf, "%s%d %s%s",
                     "<html><body><h1>",
                     status_code, http_get_code_text( status_code ),
                     "</h1></body></html>" );
            num = strlen( tempbuf );

            if( http_MakeMessage( buf, http_major_version, http_minor_version, "NTcs", num, // content-length
                                  "text/html",  // content-type
                                  tempbuf ) != 0 )  // body
            {
                goto error_handler;
            }
        }

        else if( c == 'Q' ) {
            // request start line
            // GET /foo/bar.html HTTP/1.1\r\n
            //

            method = ( http_method_t ) va_arg( argp, http_method_t );
            method_str = method_to_str( method );
            url_str = ( const char * )va_arg( argp, const char * );
            num = ( int )va_arg( argp, int );   // length of url_str

            if( http_MakeMessage( buf, http_major_version, http_minor_version, "ssbsdsdc", method_str,  // method
                                  " ", url_str, num,    // url
                                  " HTTP/", http_major_version, ".",
                                  http_minor_version ) != 0 ) {
                goto error_handler;
            }
        }

        else if( c == 'q' ) {
            // request start line and HOST header

            method = ( http_method_t ) va_arg( argp, http_method_t );

            uri_ptr = ( uri_type * ) va_arg( argp, uri_type * );
            assert( uri_ptr );
            if( http_FixUrl( uri_ptr, &url ) != 0 ) {
                error_code = UPNP_E_INVALID_URL;
                goto error_handler;
            }

            if( http_MakeMessage
                ( buf, http_major_version, http_minor_version, "Q" "sbc",
                  method, url.pathquery.buff, url.pathquery.size, "HOST: ",
                  url.hostport.text.buff, url.hostport.text.size ) != 0 ) {
                goto error_handler;
            }
        }

        else if( c == 'T' ) {
            // content type header
            temp_str = ( const char * )va_arg( argp, const char * );    // type/subtype format

            if( http_MakeMessage
                ( buf, http_major_version, http_minor_version, "ssc",
                  "CONTENT-TYPE: ", temp_str ) != 0 ) {
                goto error_handler;
            }
        }

        else {
            assert( 0 );
        }
    }

    return 0;

  error_handler:
    va_end( argp );
    membuffer_destroy( buf );
    return error_code;
}

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
void
http_CalcResponseVersion( IN int request_major_vers,
                          IN int request_minor_vers,
                          OUT int *response_major_vers,
                          OUT int *response_minor_vers )
{
    if( ( request_major_vers > 1 ) ||
        ( request_major_vers == 1 && request_minor_vers >= 1 )
         ) {
        *response_major_vers = 1;
        *response_minor_vers = 1;
    } else {
        *response_major_vers = request_major_vers;
        *response_minor_vers = request_minor_vers;
    }
}

/************************************************************************
* Function: MakeGetMessageEx												
*																		
* Parameters:															
*	const char *url_str ;	String as a URL
*	membuffer *request ;	Buffer containing the request									
*	uri_type *url ; 		URI object containing the scheme, path 
*							query token, etc.
*																		
* Description: Makes the message for the HTTP GET method				
*																		
* Returns:																
*	UPNP_E_INVALID_URL													
* 	Error Codes returned by http_MakeMessage							
*	UPNP_E_SUCCESS														
************************************************************************/
int
MakeGetMessageEx( const char *url_str,
                  membuffer * request,
                  uri_type * url,
                  struct SendInstruction *pRangeSpecifier )
{
    int errCode = UPNP_E_SUCCESS;
    char *urlPath = NULL;
    int hostlen = 0;
    char *hoststr,
     *temp;

    do {
        DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                             "DOWNLOAD URL : %s\n", url_str );
             )

            if( ( errCode = http_FixStrUrl( ( char * )url_str,
                                            strlen( url_str ),
                                            url ) ) != UPNP_E_SUCCESS ) {
            break;
        }
        // make msg
        membuffer_init( request );

        urlPath = alloca( strlen( url_str ) + 1 );
        if( !urlPath ) {
            errCode = UPNP_E_OUTOF_MEMORY;
            break;
        }

        memset( urlPath, 0, strlen( url_str ) + 1 );

        strcpy( urlPath, url_str );

        hoststr = strstr( urlPath, "//" );
        if( hoststr == NULL ) {
            errCode = UPNP_E_INVALID_URL;
            break;
        }

        hoststr += 2;
        temp = strchr( hoststr, '/' );
        if( temp == NULL ) {
            errCode = UPNP_E_INVALID_URL;
            break;
        }

        *temp = '\0';
        hostlen = strlen( hoststr );
        *temp = '/';

        DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                             "HOSTNAME : %s Length : %d\n", hoststr,
                             hostlen );
             )

            errCode = http_MakeMessage( request,
                                        1,
                                        1,
                                        "QsbcGDCUc",
                                        HTTPMETHOD_GET,
                                        url->pathquery.buff,
                                        url->pathquery.size,
                                        "HOST: ",
                                        hoststr,
                                        hostlen, pRangeSpecifier );

        if( errCode != 0 ) {
            DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                                 "HTTP Makemessage failed\n" );
                 )
                membuffer_destroy( request );
            return errCode;
        }
    } while( 0 );

    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "HTTP Buffer:\n %s\n" "----------END--------\n",
                         request->buf );
         )

        return errCode;
}

#define SIZE_RANGE_BUFFER 50

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
int
http_OpenHttpGetEx( IN const char *url_str,
                    IN OUT void **Handle,
                    IN OUT char **contentType,
                    OUT int *contentLength,
                    OUT int *httpStatus,
                    IN int lowRange,
                    IN int highRange,
                    IN int timeout )
{
    int http_error_code;
    memptr ctype;
    int tcp_connection;
    membuffer request;
    http_get_handle_t *handle = NULL;
    uri_type url;
    parse_status_t status;
    int errCode = UPNP_E_SUCCESS;

    //  char rangeBuf[SIZE_RANGE_BUFFER];
    struct SendInstruction rangeBuf;

    do {
        // Checking Input parameters
        if( ( !url_str ) || ( !Handle ) ||
            ( !contentType ) || ( !httpStatus ) ) {
            errCode = UPNP_E_INVALID_PARAM;
            break;
        }
        // Initialize output parameters
        ( *httpStatus ) = 0;
        ( *Handle ) = handle;
        ( *contentType ) = NULL;
        ( *contentLength ) = 0;

        if( lowRange > highRange ) {
            errCode = UPNP_E_INTERNAL_ERROR;
            break;
        }

        memset( &rangeBuf, 0, sizeof( rangeBuf ) );
        sprintf( rangeBuf.RangeHeader, "Range: bytes=%d-%d\r\n",
                 lowRange, highRange );

        membuffer_init( &request );

        if( ( errCode = MakeGetMessageEx( url_str,
                                          &request, &url, &rangeBuf ) )
            != UPNP_E_SUCCESS ) {
            break;
        }

        handle =
            ( http_get_handle_t * ) malloc( sizeof( http_get_handle_t ) );
        if( handle == NULL ) {
            errCode = UPNP_E_OUTOF_MEMORY;
            break;
        }

        memset( handle, 0, sizeof( *handle ) );

        handle->entity_offset = 0;
        parser_response_init( &handle->response, HTTPMETHOD_GET );

        tcp_connection = socket( AF_INET, SOCK_STREAM, 0 );
        if( tcp_connection == -1 ) {
            errCode = UPNP_E_SOCKET_ERROR;
            free( handle );
            break;
        }

        if( sock_init( &handle->sock_info, tcp_connection ) !=
            UPNP_E_SUCCESS ) {
            sock_destroy( &handle->sock_info, SD_BOTH );
            errCode = UPNP_E_SOCKET_ERROR;
            free( handle );
            break;
        }

        errCode = connect( handle->sock_info.socket,
                           ( struct sockaddr * )&url.hostport.IPv4address,
                           sizeof( struct sockaddr_in ) );
        if( errCode == -1 ) {
            sock_destroy( &handle->sock_info, SD_BOTH );
            errCode = UPNP_E_SOCKET_CONNECT;
            free( handle );
            break;
        }
        // send request
        errCode = http_SendMessage( &handle->sock_info,
                                    &timeout,
                                    "b", request.buf, request.length );

        if( errCode != UPNP_E_SUCCESS ) {
            sock_destroy( &handle->sock_info, SD_BOTH );
            free( handle );
            break;
        }

        status = ReadResponseLineAndHeaders( &handle->sock_info,
                                             &handle->response,
                                             &timeout, &http_error_code );

        if( status != PARSE_OK ) {
            errCode = UPNP_E_BAD_RESPONSE;
            free( handle );
            break;
        }

        status = parser_get_entity_read_method( &handle->response );
        if( ( status != PARSE_CONTINUE_1 ) && ( status != PARSE_SUCCESS ) ) {
            errCode = UPNP_E_BAD_RESPONSE;
            free( handle );
            break;
        }

        ( *httpStatus ) = handle->response.msg.status_code;
        errCode = UPNP_E_SUCCESS;

        if( httpmsg_find_hdr( &handle->response.msg,
                              HDR_CONTENT_TYPE, &ctype ) == NULL ) {
            *contentType = NULL;    // no content-type
        } else {
            *contentType = ctype.buf;
        }

        if( handle->response.position == POS_COMPLETE ) {
            ( *contentLength ) = 0;
        } else if( handle->response.ent_position == ENTREAD_USING_CHUNKED ) {
            ( *contentLength ) = UPNP_USING_CHUNKED;
        } else if( handle->response.ent_position == ENTREAD_USING_CLEN ) {
            ( *contentLength ) = handle->response.content_length;
        } else if( handle->response.ent_position == ENTREAD_UNTIL_CLOSE ) {
            ( *contentLength ) = UPNP_UNTIL_CLOSE;
        }

        ( *Handle ) = handle;

    } while( 0 );

    membuffer_destroy( &request );

    return errCode;
}

/************************************************************************
*	Function :	get_sdk_info
*
*	Parameters :
*		OUT char *info :	buffer to store the operating system 
*							information
*
*	Description :	Returns the server information for the operating 
*		system
*
*	Return :	XINLINE void
*
*	Note :
************************************************************************/
// 'info' should have a size of at least 100 bytes
void
get_sdk_info( OUT char *info )
{
    int ret_code;
    struct utsname sys_info;

    ret_code = uname( &sys_info );
    if( ret_code == -1 ) {
        *info = '\0';
    }

    sprintf( info, "%s/%s, UPnP/1.0, Portable SDK for UPnP devices/"
	     PACKAGE_VERSION "\r\n",
             sys_info.sysname, sys_info.release );
}
