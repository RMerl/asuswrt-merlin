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
* Purpose: This file contains functions for scanner and parser for http 
* messages.
************************************************************************/

#include "config.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include "strintmap.h"
#include "httpparser.h"
#include "statcodes.h"
#include "unixutil.h"

// entity positions

#define NUM_HTTP_METHODS 9
static str_int_entry Http_Method_Table[NUM_HTTP_METHODS] = {
    {"GET", HTTPMETHOD_GET},
    {"HEAD", HTTPMETHOD_HEAD},
    {"M-POST", HTTPMETHOD_MPOST},
    {"M-SEARCH", HTTPMETHOD_MSEARCH},
    {"NOTIFY", HTTPMETHOD_NOTIFY},
    {"POST", HTTPMETHOD_POST},
    {"SUBSCRIBE", HTTPMETHOD_SUBSCRIBE},
    {"UNSUBSCRIBE", HTTPMETHOD_UNSUBSCRIBE},
    {"POST", SOAPMETHOD_POST},

};

#define NUM_HTTP_HEADER_NAMES 33
str_int_entry Http_Header_Names[NUM_HTTP_HEADER_NAMES] = {
    {"ACCEPT", HDR_ACCEPT},
    {"ACCEPT-CHARSET", HDR_ACCEPT_CHARSET},
    {"ACCEPT-ENCODING", HDR_ACCEPT_ENCODING},
    {"ACCEPT-LANGUAGE", HDR_ACCEPT_LANGUAGE},
    {"ACCEPT-RANGES", HDR_ACCEPT_RANGE},
    {"CACHE-CONTROL", HDR_CACHE_CONTROL},
    {"CALLBACK", HDR_CALLBACK},
    {"CONTENT-ENCODING", HDR_CONTENT_ENCODING},
    {"CONTENT-LANGUAGE", HDR_CONTENT_LANGUAGE},
    {"CONTENT-LENGTH", HDR_CONTENT_LENGTH},
    {"CONTENT-LOCATION", HDR_CONTENT_LOCATION},
    {"CONTENT-RANGE", HDR_CONTENT_RANGE},
    {"CONTENT-TYPE", HDR_CONTENT_TYPE},
    {"DATE", HDR_DATE},
    {"EXT", HDR_EXT},
    {"HOST", HDR_HOST},
    {"IF-RANGE", HDR_IF_RANGE},
    {"LOCATION", HDR_LOCATION},
    {"MAN", HDR_MAN},
    {"MX", HDR_MX},
    {"NT", HDR_NT},
    {"NTS", HDR_NTS},
    {"RANGE", HDR_RANGE},
    {"SEQ", HDR_SEQ},
    {"SERVER", HDR_SERVER},
    {"SID", HDR_SID},
    {"SOAPACTION", HDR_SOAPACTION},
    {"ST", HDR_ST},
    {"TE", HDR_TE},
    {"TIMEOUT", HDR_TIMEOUT},
    {"TRANSFER-ENCODING", HDR_TRANSFER_ENCODING},
    {"USER-AGENT", HDR_USER_AGENT},
    {"USN", HDR_USN}
};

/***********************************************************************/

/*************				 scanner					  **************/

/***********************************************************************/

#define TOKCHAR_CR		0xD
#define TOKCHAR_LF		0xA

/************************************************************************
*	Function :	scanner_init
*
*	Parameters :
*		OUT scanner_t* scanner ; Scanner Object to be initialized
*		IN membuffer* bufptr ;	 Buffer to be copied
*
*	Description :	Intialize scanner
*
*	Return : void ;
*
*	Note :
************************************************************************/
static XINLINE void
scanner_init( OUT scanner_t * scanner,
              IN membuffer * bufptr )
{
    scanner->cursor = 0;
    scanner->msg = bufptr;
    scanner->entire_msg_loaded = FALSE;
}

/************************************************************************
*	Function :	is_separator_char
*
*	Parameters :
*		IN char c ;	character to be tested against used separator values
*
*	Description :	Finds the separator character.
*
*	Return : xboolean ;
*
*	Note :
************************************************************************/
static XINLINE xboolean
is_separator_char( IN char c )
{
    return strchr( " \t()<>@,;:\\\"/[]?={}", c ) != NULL;
}

/************************************************************************
*	Function :	is_identifier_char
*
*	Parameters :
*		IN char c ;	character to be tested for separator values
*
*	Description :	Calls the function to indentify separator character 
*
*	Return : xboolean ;
*
*	Note :
************************************************************************/
static XINLINE xboolean
is_identifier_char( IN char c )
{
    return ( c >= 32 && c <= 126 ) && !is_separator_char( c );
}

/************************************************************************
*	Function :	is_control_char
*
*	Parameters :
*		IN char c ;	character to be tested for a control character
*
*	Description :	Determines if the passed value is a control character
*
*	Return : xboolean ;
*
*	Note :
************************************************************************/
static XINLINE xboolean
is_control_char( IN char c )
{
    return ( ( c >= 0 && c <= 31 ) || ( c == 127 ) );
}

/************************************************************************
*	Function :	is_qdtext_char
*
*	Parameters :
*		IN char cc ; character to be tested for CR/LF
*
*	Description :	Checks to see if the passed in value is CR/LF
*
*	Return : xboolean ;
*
*	Note :
************************************************************************/
static XINLINE xboolean
is_qdtext_char( IN char cc )
{
    unsigned char c = ( unsigned char )cc;

    // we don't check for this; it's checked in get_token()
    assert( c != '"' );

    if( ( c >= 32 && c != 127 ) ||
        ( c == TOKCHAR_CR || c == TOKCHAR_LF || c == '\t' )
         ) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/************************************************************************
*	Function :	scanner_get_token
*
*	Parameters :
*		INOUT scanner_t* scanner ;	Scanner Object
*		OUT memptr* token ;			Token 
*		OUT token_type_t* tok_type ; Type of token
*
*	Description :	reads next token from the input stream				
*		note: 0 and is used as a marker, and will not be valid in a quote
*
*	Return : parse_status_t ;
*		PARSE_OK															
*		PARSE_INCOMPLETE		-- not enuf chars to get a token			
*		PARSE_FAILURE			-- bad msg format							
*
*	Note :
************************************************************************/
static parse_status_t
scanner_get_token( INOUT scanner_t * scanner,
                   OUT memptr * token,
                   OUT token_type_t * tok_type )
{
    char *cursor;
    char *null_terminator;      // point to null-terminator in buffer
    char c;
    token_type_t token_type;
    xboolean got_end_quote;

    assert( scanner );
    assert( token );
    assert( tok_type );

    // point to next char in buffer
    cursor = scanner->msg->buf + scanner->cursor;
    null_terminator = scanner->msg->buf + scanner->msg->length;

    // not enough chars in input to parse
    if( cursor == null_terminator ) {
        return PARSE_INCOMPLETE;
    }

    c = *cursor;
    if( is_identifier_char( c ) ) {
        // scan identifier

        token->buf = cursor++;
        token_type = TT_IDENTIFIER;

        while( is_identifier_char( *cursor ) ) {
            cursor++;
        }

        if( !scanner->entire_msg_loaded && cursor == null_terminator ) {
            // possibly more valid chars
            return PARSE_INCOMPLETE;
        }
        // calc token length
        token->length = cursor - token->buf;
    } else if( c == ' ' || c == '\t' ) {
        token->buf = cursor++;
        token_type = TT_WHITESPACE;

        while( *cursor == ' ' || *cursor == '\t' ) {
            cursor++;
        }

        if( !scanner->entire_msg_loaded && cursor == null_terminator ) {
            // possibly more chars
            return PARSE_INCOMPLETE;
        }
        token->length = cursor - token->buf;
    } else if( c == TOKCHAR_CR ) {
        // scan CRLF

        token->buf = cursor++;
        if( cursor == null_terminator ) {
            // not enuf info to determine CRLF
            return PARSE_INCOMPLETE;
        }
        if( *cursor != TOKCHAR_LF ) {
            // couldn't match CRLF; match as CR
            token_type = TT_CTRL;   // ctrl char
            token->length = 1;
        } else {
            // got CRLF
            token->length = 2;
            token_type = TT_CRLF;
            cursor++;
        }
    } else if( c == TOKCHAR_LF )    // accept \n as CRLF
    {
        token->buf = cursor++;
        token->length = 1;
        token_type = TT_CRLF;
    } else if( c == '"' ) {
        // quoted text
        token->buf = cursor++;
        token_type = TT_QUOTEDSTRING;
        got_end_quote = FALSE;

        while( cursor < null_terminator ) {
            c = *cursor++;
            if( c == '"' ) {
                got_end_quote = TRUE;
                break;
            } else if( c == '\\' ) {
                if( cursor < null_terminator ) {
                    c = *cursor++;
                    //if ( !(c > 0 && c <= 127) )
                    if( c == 0 ) {
                        return PARSE_FAILURE;
                    }
                }
                // else, while loop handles incomplete buf
            } else if( is_qdtext_char( c ) ) {
                // just accept char
            } else {
                // bad quoted text
                return PARSE_FAILURE;
            }
        }
        if( got_end_quote ) {
            token->length = cursor - token->buf;
        } else                  // incomplete
        {
            assert( cursor == null_terminator );
            return PARSE_INCOMPLETE;
        }
    } else if( is_separator_char( c ) ) {
        // scan separator

        token->buf = cursor++;
        token_type = TT_SEPARATOR;
        token->length = 1;
    } else if( is_control_char( c ) ) {
        // scan ctrl char

        token->buf = cursor++;
        token_type = TT_CTRL;
        token->length = 1;
    } else {
        return PARSE_FAILURE;
    }

    scanner->cursor += token->length;   // move to next token
    *tok_type = token_type;
    return PARSE_OK;
}

/************************************************************************
*	Function :	scanner_get_str
*
*	Parameters :
*		IN scanner_t* scanner ;	Scanner Object
*
*	Description :	returns ptr to next char in string
*
*	Return : char* ;
*
*	Note :
************************************************************************/
static XINLINE char *
scanner_get_str( IN scanner_t * scanner )
{
    return scanner->msg->buf + scanner->cursor;
}

/************************************************************************
*	Function :	scanner_pushback
*
*	Parameters :
*		INOUT scanner_t* scanner ;	Scanner Object
*		IN size_t pushback_bytes ;	Bytes to be moved back
*
*	Description :	Move back by a certain number of bytes.				
*		This is used to put back one or more tokens back into the input		
*
*	Return : void ;
*
*	Note :
************************************************************************/
static XINLINE void
scanner_pushback( INOUT scanner_t * scanner,
                  IN size_t pushback_bytes )
{
    scanner->cursor -= pushback_bytes;
}

/***********************************************************************/

/*************				end of scanner				  **************/

/***********************************************************************/

/***********************************************************************/

/*************					parser					  **************/

/***********************************************************************/

/***********************************************************************/

/*************				  http_message_t			  **************/

/***********************************************************************/

/************************************************************************
*	Function :	httpmsg_compare
*
*	Parameters :
*		void* param1 ;	
*		void* param2 ;	
*
*	Description :	Compares name id in the http headers.
*
*	Return : int ;
*
*	Note :
************************************************************************/
static int
httpmsg_compare( void *param1,
                 void *param2 )
{
    assert( param1 != NULL );
    assert( param2 != NULL );

    return ( ( http_header_t * ) param1 )->name_id ==
        ( ( http_header_t * ) param2 )->name_id;
}

/************************************************************************
*	Function :	httpheader_free
*
*	Parameters :
*		void *msg ;	
*
*	Description :	Free memory allocated for the http header
*
*	Return : void ;
*
*	Note :
************************************************************************/
static void
httpheader_free( void *msg )
{
    http_header_t *hdr = ( http_header_t * ) msg;

    membuffer_destroy( &hdr->name_buf );
    membuffer_destroy( &hdr->value );
    free( hdr );
}

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
void
httpmsg_init( INOUT http_message_t * msg )
{
    msg->initialized = 1;
    msg->entity.buf = NULL;
    msg->entity.length = 0;
    ListInit( &msg->headers, httpmsg_compare, httpheader_free );
    membuffer_init( &msg->msg );
    membuffer_init( &msg->status_msg );
}

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
void
httpmsg_destroy( INOUT http_message_t * msg )
{
    assert( msg != NULL );

    if( msg->initialized == 1 ) {
        ListDestroy( &msg->headers, 1 );
        membuffer_destroy( &msg->msg );
        membuffer_destroy( &msg->status_msg );
        free( msg->urlbuf );
        msg->initialized = 0;
    }
}

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
http_header_t *
httpmsg_find_hdr_str( IN http_message_t * msg,
                      IN const char *header_name )
{
    http_header_t *header;

    ListNode *node;

    node = ListHead( &msg->headers );
    while( node != NULL ) {

        header = ( http_header_t * ) node->item;

        if( memptr_cmp_nocase( &header->name, header_name ) == 0 ) {
            return header;
        }

        node = ListNext( &msg->headers, node );
    }
    return NULL;
}

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
http_header_t *
httpmsg_find_hdr( IN http_message_t * msg,
                  IN int header_name_id,
                  OUT memptr * value )
{
    http_header_t header;       // temp header for searching

    ListNode *node;

    http_header_t *data;

    header.name_id = header_name_id;

    node = ListFind( &msg->headers, NULL, &header );

    if( node == NULL ) {
        return NULL;
    }

    data = ( http_header_t * ) node->item;

    if( value != NULL ) {
        value->buf = data->value.buf;
        value->length = data->value.length;
    }

    return data;
}

/***********************************************************************/

/*************				  http_parser_t				  **************/

/***********************************************************************/

/************************************************************************
*	Function :	skip_blank_lines
*
*	Parameters :
*		INOUT scanner_t* scanner ;	Scanner Object
*
*	Description :	skips blank lines at the start of a msg.
*
*	Return : int ;
*
*	Note :
************************************************************************/
static XINLINE int
skip_blank_lines( INOUT scanner_t * scanner )
{
    memptr token;
    token_type_t tok_type;
    parse_status_t status;

    // skip ws, crlf
    do {
        status = scanner_get_token( scanner, &token, &tok_type );
    } while( status == PARSE_OK &&
             ( tok_type == TT_WHITESPACE || tok_type == TT_CRLF ) );

    if( status == PARSE_OK ) {
        // pushback a non-whitespace token
        scanner->cursor -= token.length;
        //scanner_pushback( scanner, token.length );
    }

    return status;
}

/************************************************************************
*	Function :	skip_lws
*
*	Parameters :
*		INOUT scanner_t* scanner ;	Scanner Object
*
*	Description :	skip linear whitespace.
*
*	Return : int ;
*		PARSE_OK: (LWS)* removed from input								
*		PARSE_FAILURE: bad input											
*		PARSE_INCOMPLETE: incomplete input									
*
*	Note :
************************************************************************/
static XINLINE int
skip_lws( INOUT scanner_t * scanner )
{
    memptr token;
    token_type_t tok_type;
    parse_status_t status;
    size_t save_pos;
    xboolean matched;

    do {
        save_pos = scanner->cursor;
        matched = FALSE;

        // get CRLF or WS
        status = scanner_get_token( scanner, &token, &tok_type );
        if( status == PARSE_OK ) {
            if( tok_type == TT_CRLF ) {
                // get WS
                status = scanner_get_token( scanner, &token, &tok_type );
            }

            if( status == PARSE_OK && tok_type == TT_WHITESPACE ) {
                matched = TRUE;
            } else {
                // did not match LWS; pushback token(s)
                scanner->cursor = save_pos;
            }
        }
    } while( matched );

    // if entire msg is loaded, ignore an 'incomplete' warning
    if( status == PARSE_INCOMPLETE && scanner->entire_msg_loaded ) {
        status = PARSE_OK;
    }

    return status;
}

/************************************************************************
*	Function :	match_non_ws_string
*
*	Parameters :
*		INOUT scanner_t* scanner ;	Scanner Object
*		OUT memptr* str ;	Buffer to get the scanner buffer contents.
*
*	Description :	Match a string without whitespace or CRLF (%S)
*
*	Return : XINLINE parse_status_t ;
*		PARSE_OK															
*		PARSE_NO_MATCH														
*		PARSE_FAILURE														
*		PARSE_INCOMPLETE													
*
*	Note :
************************************************************************/
static XINLINE parse_status_t
match_non_ws_string( INOUT scanner_t * scanner,
                     OUT memptr * str )
{
    memptr token;
    token_type_t tok_type;
    parse_status_t status;
    xboolean done = FALSE;
    size_t save_cursor;

    save_cursor = scanner->cursor;

    str->length = 0;
    str->buf = scanner_get_str( scanner );  // point to next char in input

    while( !done ) {
        status = scanner_get_token( scanner, &token, &tok_type );
        if( status == PARSE_OK &&
            tok_type != TT_WHITESPACE && tok_type != TT_CRLF ) {
            // append non-ws token
            str->length += token.length;
        } else {
            done = TRUE;
        }
    }

    if( status == PARSE_OK ) {
        // last token was WS; push it back in
        scanner->cursor -= token.length;
    }
    // tolerate 'incomplete' msg
    if( status == PARSE_OK ||
        ( status == PARSE_INCOMPLETE && scanner->entire_msg_loaded )
         ) {
        if( str->length == 0 ) {
            // no strings found
            return PARSE_NO_MATCH;
        } else {
            return PARSE_OK;
        }
    } else {
        // error -- pushback tokens
        scanner->cursor = save_cursor;
        return status;
    }
}

/************************************************************************
*	Function :	match_raw_value
*
*	Parameters :
*		INOUT scanner_t* scanner ;	Scanner Object
*		OUT memptr* raw_value ;	    Buffer to get the scanner buffer 
*									contents
*
*	Description :	Matches a raw value in a the input; value's length 
*		can	be 0 or more. Whitespace after value is trimmed. On success,
*		scanner points the CRLF that ended the value						
*
*	Return : parse_status_t ;
*		PARSE_OK													
*		PARSE_INCOMPLETE													
*		PARSE_FAILURE														
*
*	Note :
************************************************************************/
static XINLINE parse_status_t
match_raw_value( INOUT scanner_t * scanner,
                 OUT memptr * raw_value )
{
    memptr token;
    token_type_t tok_type;
    parse_status_t status;
    xboolean done = FALSE;
    xboolean saw_crlf = FALSE;
    size_t pos_at_crlf = 0;
    size_t save_pos;
    char c;

    save_pos = scanner->cursor;

    // value points to start of input
    raw_value->buf = scanner_get_str( scanner );
    raw_value->length = 0;

    while( !done ) {
        status = scanner_get_token( scanner, &token, &tok_type );
        if( status == PARSE_OK ) {
            if( !saw_crlf ) {
                if( tok_type == TT_CRLF ) {
                    // CRLF could end value
                    saw_crlf = TRUE;

                    // save input position at start of CRLF
                    pos_at_crlf = scanner->cursor - token.length;
                }
                // keep appending value
                raw_value->length += token.length;
            } else              // already seen CRLF
            {
                if( tok_type == TT_WHITESPACE ) {
                    // start again; forget CRLF
                    saw_crlf = FALSE;
                    raw_value->length += token.length;
                } else {
                    // non-ws means value ended just before CRLF
                    done = TRUE;

                    // point to the crlf which ended the value
                    scanner->cursor = pos_at_crlf;
                }
            }
        } else {
            // some kind of error; restore scanner position
            scanner->cursor = save_pos;
            done = TRUE;
        }
    }

    if( status == PARSE_OK ) {
        // trim whitespace on right side of value
        while( raw_value->length > 0 ) {
            // get last char
            c = raw_value->buf[raw_value->length - 1];

            if( c != ' ' && c != '\t' &&
                c != TOKCHAR_CR && c != TOKCHAR_LF ) {
                // done; no more whitespace
                break;
            }
            // remove whitespace
            raw_value->length--;
        }
    }

    return status;
}

/************************************************************************
* Function: match_int													
*																		
* Parameters:															
*	INOUT scanner_t* scanner ;  Scanner Object											
*	IN int base :				Base of number in the string; 
*								valid values: 10 or 16	
*	OUT int* value ;			Number stored here									
*																		
* Description: Matches an unsigned integer value in the input. The		
*	integer is returned in 'value'. Except for PARSE_OK result, the		
*	scanner's cursor is moved back to its original position on error.	
*																		
* Returns:																
*   PARSE_OK															
*   PARSE_NO_MATCH		-- got different kind of token					
*   PARSE_FAILURE		-- bad input									
*   PARSE_INCOMPLETE													
************************************************************************/
static XINLINE int
match_int( INOUT scanner_t * scanner,
           IN int base,
           OUT int *value )
{
    memptr token;
    token_type_t   tok_type;
    parse_status_t status;
    long           num;
    char          *end_ptr;
    size_t         save_pos;

    save_pos = scanner->cursor;

    status = scanner_get_token( scanner, &token, &tok_type );
    if( status == PARSE_OK ) {
        if( tok_type == TT_IDENTIFIER ) {
            errno = 0;

            num = strtol( token.buf, &end_ptr, base );
            if( ( num < 0 )
                // all and only those chars in token should be used for num
                || ( end_ptr != token.buf + token.length )
                || ( ( num == LONG_MIN || num == LONG_MAX )
                     && ( errno == ERANGE ) )
                 ) {
                status = PARSE_NO_MATCH;
            }

            *value = num;       // save result
        } else {
            status = PARSE_NO_MATCH;    // token must be an identifier
        }
    }

    if( status != PARSE_OK ) {
        // restore scanner position for bad values
        scanner->cursor = save_pos;
    }

    return status;
}

/************************************************************************
* Function: read_until_crlf												
*																		
* Parameters:															
*	INOUT scanner_t* scanner ;	Scanner Object											
*	OUT memptr* str ;			Buffer to copy scanner buffer contents to
*																		
* Description: Reads data until end of line; the crlf at the end of		
*	line is not consumed. On error, scanner is not restored. On			
*	success, 'str' points to a string that runs until eol				
*																		
* Returns:																
*   PARSE_OK															
*   PARSE_FAILURE														
*   PARSE_INCOMPLETE													
************************************************************************/
static XINLINE int
read_until_crlf( INOUT scanner_t * scanner,
                 OUT memptr * str )
{
    memptr token;
    token_type_t tok_type;
    parse_status_t status;
    size_t start_pos;

    start_pos = scanner->cursor;
    str->buf = scanner_get_str( scanner );

    // read until we hit a crlf
    do {
        status = scanner_get_token( scanner, &token, &tok_type );
    } while( status == PARSE_OK && tok_type != TT_CRLF );

    if( status == PARSE_OK ) {
        // pushback crlf in stream
        scanner->cursor -= token.length;

        // str should include all strings except crlf at the end
        str->length = scanner->cursor - start_pos;
    }

    return status;
}

/************************************************************************
* Function: skip_to_end_of_header										
*																		
* Parameters:															
*	INOUT scanner_t* scanner ; Scanner Object
*																		
* Description: Skip to end of header									
*																		
* Returns:																
*   PARSE_OK															
*   PARSE_FAILURE														
*   PARSE_INCOMPLETE													
************************************************************************/
static XINLINE int
skip_to_end_of_header( INOUT scanner_t * scanner )
{
    memptr dummy_raw_value;
    parse_status_t status;

    status = match_raw_value( scanner, &dummy_raw_value );
    return status;
}

/************************************************************************
* Function: match_char													
*																		
* Parameters:															
*	INOUT scanner_t* scanner ;  Scanner Object											
*	IN char c ;					Character to be compared with 															
*	IN xboolean case_sensitive; Flag indicating whether comparison should 
*								be case sensitive
*																		
* Description: Compares a character to the next char in the scanner;	
*	on error, scanner chars are not restored							
*																		
* Returns:																
*   PARSE_OK															
*   PARSE_NO_MATCH														
*   PARSE_INCOMPLETE													
************************************************************************/
static XINLINE parse_status_t
match_char( INOUT scanner_t * scanner,
            IN char c,
            IN xboolean case_sensitive )
{
    char scan_char;

    if( scanner->cursor >= scanner->msg->length ) {
        return PARSE_INCOMPLETE;
    }
    // read next char from scanner
    scan_char = scanner->msg->buf[scanner->cursor++];

    if( case_sensitive ) {
        return c == scan_char ? PARSE_OK : PARSE_NO_MATCH;
    } else {
        return tolower( c ) == tolower( scan_char ) ?
            PARSE_OK : PARSE_NO_MATCH;
    }
}

////////////////////////////////////////////////////////////////////////
// args for ...
//   %d,    int *     (31-bit positive integer)
//   %x,    int *     (31-bit postive number encoded as hex)
//   %s,    memptr*  (simple identifier)
//   %q,    memptr*  (quoted string)
//   %S,    memptr*  (non-whitespace string)
//   %R,    memptr*  (raw value)
//   %U,    uri_type* (url)
//   %L,    memptr*  (string until end of line)
//   %P,    int * (current index of the string being scanned)
//
// no args for
//   ' '    LWS*
//   \t     whitespace
//   "%%"   matches '%'
//   "% "   matches ' '
//   %c     matches CRLF
//   %i     ignore case in literal matching
//   %n     case-sensitive matching in literals
//   %w     optional whitespace; (similar to '\t', 
//                  except whitespace is optional)
//   %0     (zero) match null-terminator char '\0' 
//              (can only be used as last char in fmt)
//              use only in matchstr(), not match()
//   other chars match literally
//
// returns:
//   PARSE_OK
//   PARSE_INCOMPLETE
//   PARSE_FAILURE      -- bad input
//   PARSE_NO_MATCH     -- input does not match pattern

/************************************************************************
*	Function :	vfmatch
*
*	Parameters :
*		INOUT scanner_t* scanner ;  Scanner Object	
*		IN const char* fmt ;		Pattern Format 
*		va_list argp ;				List of variable arguments
*
*	Description :	Extracts variable parameters depending on the passed 
*		in format parameter. Parses data also based on the passed in 
*		format parameter.
*
*	Return : int ;
*		PARSE_OK
*		PARSE_INCOMPLETE
*		PARSE_FAILURE		- bad input
*		PARSE_NO_MATCH		- input does not match pattern
*
*	Note :
************************************************************************/
static int
vfmatch( INOUT scanner_t * scanner,
         IN const char *fmt,
         va_list argp )
{
    char c;
    const char *fmt_ptr = fmt;
    parse_status_t status;
    memptr *str_ptr;
    memptr temp_str;
    int *int_ptr;
    uri_type *uri_ptr;
    size_t save_pos;
    int stat;
    xboolean case_sensitive = TRUE;
    memptr token;
    token_type_t tok_type;
    int base;

    assert( scanner != NULL );
    assert( fmt != NULL );

    // save scanner pos; to aid error recovery
    save_pos = scanner->cursor;

    status = PARSE_OK;
    while( ( ( c = *fmt_ptr++ ) != 0 ) && ( status == PARSE_OK )
         ) {
        if( c == '%' ) {
            c = *fmt_ptr++;

            switch ( c ) {

                case 'R':      // raw value
                    str_ptr = va_arg( argp, memptr * );
                    assert( str_ptr != NULL );
                    status = match_raw_value( scanner, str_ptr );
                    break;

                case 's':      // simple identifier
                    str_ptr = va_arg( argp, memptr * );
                    assert( str_ptr != NULL );
                    status = scanner_get_token( scanner, str_ptr,
                                                &tok_type );
                    if( status == PARSE_OK && tok_type != TT_IDENTIFIER ) {
                        // not an identifier
                        status = PARSE_NO_MATCH;
                    }
                    break;

                case 'c':      // crlf
                    status = scanner_get_token( scanner,
                                                &token, &tok_type );
                    if( status == PARSE_OK && tok_type != TT_CRLF ) {
                        // not CRLF token
                        status = PARSE_NO_MATCH;
                    }
                    break;

                case 'd':      // integer
                case 'x':      // hex number
                    int_ptr = va_arg( argp, int * );

                    assert( int_ptr != NULL );
                    base = ( c == 'd' ? 10 : 16 );
                    status = match_int( scanner, base, int_ptr );
                    break;

                case 'S':      // non-whitespace string
                case 'U':      // uri
                    if( c == 'S' ) {
                        str_ptr = va_arg( argp, memptr * );
                    } else {
                        str_ptr = &temp_str;
                    }
                    assert( str_ptr != NULL );
                    status = match_non_ws_string( scanner, str_ptr );
                    if( c == 'U' && status == PARSE_OK ) {
                        uri_ptr = va_arg( argp, uri_type * );
                        assert( uri_ptr != NULL );
                        stat = parse_uri( str_ptr->buf, str_ptr->length,
                                          uri_ptr );
                        if( stat != HTTP_SUCCESS ) {
                            status = PARSE_NO_MATCH;
                        }
                    }
                    break;

                case 'L':      // string till eol
                    str_ptr = va_arg( argp, memptr * );
                    assert( str_ptr != NULL );
                    status = read_until_crlf( scanner, str_ptr );
                    break;

                case ' ':      // match space
                case '%':      // match percentage symbol
                    status = match_char( scanner, c, case_sensitive );
                    break;

                case 'n':      // case-sensitive match
                    case_sensitive = TRUE;
                    break;

                case 'i':      // ignore case
                    case_sensitive = FALSE;
                    break;

                case 'q':      // quoted string
                    str_ptr = ( memptr * ) va_arg( argp, memptr * );
                    status =
                        scanner_get_token( scanner, str_ptr, &tok_type );
                    if( status == PARSE_OK && tok_type != TT_QUOTEDSTRING ) {
                        status = PARSE_NO_MATCH;    // not a quoted string
                    }
                    break;

                case 'w':      // optional whitespace
                    status = scanner_get_token( scanner,
                                                &token, &tok_type );
                    if( status == PARSE_OK && tok_type != TT_WHITESPACE ) {
                        // restore non-whitespace token
                        scanner->cursor -= token.length;
                    }
                    break;

                case 'P':      // current pos of scanner
                    int_ptr = va_arg( argp, int * );

                    assert( int_ptr != NULL );
                    *int_ptr = scanner->cursor;
                    break;

                    // valid only in matchstr()
                case '0':      // end of msg?
                    // check that we are 1 beyond last char
                    if( scanner->cursor == scanner->msg->length &&
                        scanner->msg->buf[scanner->cursor] == '\0' ) {
                        status = PARSE_OK;
                    } else {
                        status = PARSE_NO_MATCH;
                    }
                    break;

                default:
                    assert( 0 );    // unknown option
            }
        } else {
            switch ( c ) {
                case ' ':      // LWS*
                    status = skip_lws( scanner );
                    break;

                case '\t':     // Whitespace
                    status = scanner_get_token( scanner,
                                                &token, &tok_type );
                    if( status == PARSE_OK && tok_type != TT_WHITESPACE ) {
                        // not whitespace token
                        status = PARSE_NO_MATCH;
                    }
                    break;

                default:       // match characters
                    {
                        status = match_char( scanner, c, case_sensitive );
                    }
            }
        }
    }

    if( status != PARSE_OK ) {
        // on error, restore original scanner pos
        scanner->cursor = save_pos;
    }

    return status;
}

/************************************************************************
* Function: match														
*																		
* Parameters:															
*	INOUT scanner_t* scanner ; Scanner Object											
*	IN const char* fmt;			Pattern format													
*	...																	
*																		
* Description: matches a variable parameter list and takes necessary	
*	actions based on the data type specified.							
*																		
* Returns:																
*   PARSE_OK															
*   PARSE_NO_MATCH														
*   PARSE_INCOMPLETE													
************************************************************************/
static int
match( INOUT scanner_t * scanner,
       IN const char *fmt,
       ... )
{
    int ret_code;
    va_list args;

    va_start( args, fmt );
    ret_code = vfmatch( scanner, fmt, args );
    va_end( args );

    return ret_code;
}

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
int
matchstr( IN char *str,
          IN size_t slen,
          IN const char *fmt,
          ... )
{
    int ret_code;
    char save_char;
    scanner_t scanner;
    membuffer buf;
    va_list arg_list;

    // null terminate str
    save_char = str[slen];
    str[slen] = '\0';

    membuffer_init( &buf );

    // under no circumstances should this buffer be modifed because its memory
    //  might have not come from malloc()
    membuffer_attach( &buf, str, slen );

    scanner_init( &scanner, &buf );
    scanner.entire_msg_loaded = TRUE;

    va_start( arg_list, fmt );
    ret_code = vfmatch( &scanner, fmt, arg_list );
    va_end( arg_list );

    // restore str
    str[slen] = save_char;

    // don't destroy buf

    return ret_code;
}

/************************************************************************
* Function: parser_init													
*																		
* Parameters:															
*	OUT http_parser_t* parser ; HTTP Parser object
*																		
* Description: Initializes the parser object.							
*																		
* Returns:																
*	void																
************************************************************************/
static XINLINE void
parser_init( OUT http_parser_t * parser )
{
    memset( parser, 0, sizeof( http_parser_t ) );

    parser->http_error_code = HTTP_BAD_REQUEST; // err msg by default
    parser->ent_position = ENTREAD_DETERMINE_READ_METHOD;
    parser->valid_ssdp_notify_hack = FALSE;

    httpmsg_init( &parser->msg );
    scanner_init( &parser->scanner, &parser->msg.msg );
}

/************************************************************************
* Function: parser_parse_requestline									
*																		
* Parameters:															
*	INOUT http_parser_t* parser ; HTTP Parser  object						
*																		
* Description: Get HTTP Method, URL location and version information.	
*																		
* Returns:																
*	PARSE_OK															
*	PARSE_SUCCESS														
*	PARSE_FAILURE														
************************************************************************/
static parse_status_t
parser_parse_requestline( INOUT http_parser_t * parser )
{
    parse_status_t status;
    http_message_t *hmsg = &parser->msg;
    memptr method_str;
    memptr version_str;
    int index;
    char save_char;
    int num_scanned;
    memptr url_str;

    assert( parser->position == POS_REQUEST_LINE );

    status = skip_blank_lines( &parser->scanner );
    if( status != PARSE_OK ) {
        return status;
    }
    //simple get http 0.9 as described in http 1.0 spec

    status =
        match( &parser->scanner, "%s\t%S%w%c", &method_str, &url_str );

    if( status == PARSE_OK ) {

        index =
            map_str_to_int( method_str.buf, method_str.length,
                            Http_Method_Table, NUM_HTTP_METHODS, TRUE );

        if( index < 0 ) {
            // error; method not found
            parser->http_error_code = HTTP_NOT_IMPLEMENTED;
            return PARSE_FAILURE;
        }

        if( Http_Method_Table[index].id != HTTPMETHOD_GET ) {
            parser->http_error_code = HTTP_BAD_REQUEST;
            return PARSE_FAILURE;
        }

        hmsg->method = HTTPMETHOD_SIMPLEGET;

        // store url
        hmsg->urlbuf = str_alloc( url_str.buf, url_str.length );
        if( hmsg->urlbuf == NULL ) {
            // out of mem
            parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
            return PARSE_FAILURE;
        }
        if( parse_uri( hmsg->urlbuf, url_str.length, &hmsg->uri ) !=
            HTTP_SUCCESS ) {
            return PARSE_FAILURE;
        }

        parser->position = POS_COMPLETE;    // move to headers

        return PARSE_SUCCESS;
    }

    status = match( &parser->scanner,
                    "%s\t%S\t%ihttp%w/%w%L%c", &method_str, &url_str,
                    &version_str );
    if( status != PARSE_OK ) {
        return status;
    }
    // store url
    hmsg->urlbuf = str_alloc( url_str.buf, url_str.length );
    if( hmsg->urlbuf == NULL ) {
        // out of mem
        parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
        return PARSE_FAILURE;
    }
    if( parse_uri( hmsg->urlbuf, url_str.length, &hmsg->uri ) !=
        HTTP_SUCCESS ) {
        return PARSE_FAILURE;
    }
    // scan version
    save_char = version_str.buf[version_str.length];
    version_str.buf[version_str.length] = '\0'; // null-terminate
    num_scanned = sscanf( version_str.buf, "%d . %d",
                          &hmsg->major_version, &hmsg->minor_version );
    version_str.buf[version_str.length] = save_char;    // restore
    if( num_scanned != 2 ||
        hmsg->major_version < 0 || hmsg->minor_version < 0 ) {
        // error; bad http version
        return PARSE_FAILURE;
    }

    index =
        map_str_to_int( method_str.buf, method_str.length,
                        Http_Method_Table, NUM_HTTP_METHODS, TRUE );
    if( index < 0 ) {
        // error; method not found
        parser->http_error_code = HTTP_NOT_IMPLEMENTED;
        return PARSE_FAILURE;
    }

    hmsg->method = Http_Method_Table[index].id;
    parser->position = POS_HEADERS; // move to headers

    return PARSE_OK;
}

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
parse_status_t
parser_parse_responseline( INOUT http_parser_t * parser )
{
    parse_status_t status;
    http_message_t *hmsg = &parser->msg;
    memptr line;
    char save_char;
    int num_scanned;
    int i;
    char *p;

    assert( parser->position == POS_RESPONSE_LINE );

    status = skip_blank_lines( &parser->scanner );
    if( status != PARSE_OK ) {
        return status;
    }
    // response line
    //status = match( &parser->scanner, "%ihttp%w/%w%d\t.\t%d\t%d\t%L%c",
    //  &hmsg->major_version, &hmsg->minor_version,
    //  &hmsg->status_code, &hmsg->status_msg );

    status = match( &parser->scanner, "%ihttp%w/%w%L%c", &line );
    if( status != PARSE_OK ) {
        return status;
    }

    save_char = line.buf[line.length];
    line.buf[line.length] = '\0';   // null-terminate

    // scan http version and ret code
    num_scanned = sscanf( line.buf, "%d . %d %d",
                          &hmsg->major_version, &hmsg->minor_version,
                          &hmsg->status_code );

    line.buf[line.length] = save_char;  // restore

    if( num_scanned != 3 ||
        hmsg->major_version < 0 ||
        hmsg->minor_version < 0 || hmsg->status_code < 0 ) {
        // bad response line
        return PARSE_FAILURE;
    }
    //
    // point to status msg
    //

    p = line.buf;

    // skip 3 ints
    for( i = 0; i < 3; i++ ) {
        // go to start of num
        while( !isdigit( *p ) ) {
            p++;
        }

        // skip int
        while( isdigit( *p ) ) {
            p++;
        }
    }

    // whitespace must exist after status code
    if( *p != ' ' && *p != '\t' ) {
        return PARSE_FAILURE;
    }
    // skip whitespace
    while( *p == ' ' || *p == '\t' ) {
        p++;
    }

    // now, p is at start of status msg
    if( membuffer_assign( &hmsg->status_msg, p,
                          line.length - ( p - line.buf ) ) != 0 ) {
        // out of mem
        parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
        return PARSE_FAILURE;
    }

    parser->position = POS_HEADERS; // move to headers

    return PARSE_OK;
}

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
parse_status_t
parser_parse_headers( INOUT http_parser_t * parser )
{
    parse_status_t status;
    memptr token;
    memptr hdr_value;
    token_type_t tok_type;
    scanner_t *scanner = &parser->scanner;
    size_t save_pos;
    http_header_t *header;
    int header_id;
    int ret = 0;
    int index;
    http_header_t *orig_header;
    char save_char;
    int ret2;

    assert( parser->position == POS_HEADERS ||
            parser->ent_position == ENTREAD_CHUNKY_HEADERS );

    while( TRUE ) {
        save_pos = scanner->cursor;

        //
        // check end of headers
        //
        status = scanner_get_token( scanner, &token, &tok_type );
        if( status != PARSE_OK ) {
            return status;
        }

        if( tok_type == TT_CRLF ) {

            // end of headers
            if( ( parser->msg.is_request )
                && ( parser->msg.method == HTTPMETHOD_POST ) ) {
                parser->position = POS_COMPLETE;    //post entity parsing
                //is handled separately 
                return PARSE_SUCCESS;
            }

            parser->position = POS_ENTITY;  // read entity next
            return PARSE_OK;
        }
        //
        // not end; read header
        //
        if( tok_type != TT_IDENTIFIER ) {
            return PARSE_FAILURE;   // didn't see header name
        }

        status = match( scanner, " : %R%c", &hdr_value );
        if( status != PARSE_OK ) {
            // pushback tokens; useful only on INCOMPLETE error
            scanner->cursor = save_pos;
            return status;
        }
        //
        // add header
        //

        // find header
        index = map_str_to_int( token.buf, token.length, Http_Header_Names,
                                NUM_HTTP_HEADER_NAMES, FALSE );
        if( index != -1 ) {

            //Check if it is a soap header
            if( Http_Header_Names[index].id == HDR_SOAPACTION ) {
                parser->msg.method = SOAPMETHOD_POST;
            }

            header_id = Http_Header_Names[index].id;
            orig_header =
                httpmsg_find_hdr( &parser->msg, header_id, NULL );
        } else {
            header_id = HDR_UNKNOWN;

            save_char = token.buf[token.length];
            token.buf[token.length] = '\0';

            orig_header = httpmsg_find_hdr_str( &parser->msg, token.buf );

            token.buf[token.length] = save_char;    // restore
        }

        if( orig_header == NULL ) {
            //
            // add new header
            //

            header = ( http_header_t * ) malloc( sizeof( http_header_t ) );
            if( header == NULL ) {
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }
            membuffer_init( &header->name_buf );
            membuffer_init( &header->value );

            // value can be 0 length
            if( hdr_value.length == 0 ) {
                hdr_value.buf = "\0";
                hdr_value.length = 1;
            }
            // save in header in buffers
            if( membuffer_assign
                ( &header->name_buf, token.buf, token.length ) != 0
                || membuffer_assign( &header->value, hdr_value.buf,
                                     hdr_value.length ) != 0 ) {
                // not enuf mem
                free (header);
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }

            header->name.buf = header->name_buf.buf;
            header->name.length = header->name_buf.length;
            header->name_id = header_id;

            ListAddTail( &parser->msg.headers, header );

            //NNS:          ret = dlist_append( &parser->msg.headers, header );
            if( ret == UPNP_E_OUTOF_MEMORY ) {
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }
        } else if( hdr_value.length > 0 ) {
            //
            // append value to existing header
            //

            // append space
            ret = membuffer_append_str( &orig_header->value, ", " );

            // append continuation of header value
            ret2 = membuffer_append( &orig_header->value,
                                     hdr_value.buf, hdr_value.length );

            if( ret == UPNP_E_OUTOF_MEMORY || ret2 == UPNP_E_OUTOF_MEMORY ) {
                // not enuf mem
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }
        }
    }                           // end while

}

////////////////////////////////////////////////////////////////////////
#ifdef HIGHLY_UNLIKELY
// **************
static parse_status_t
parser_parse_headers_old( INOUT http_parser_t * parser )
{
    parse_status_t status;
    memptr token;
    memptr hdr_value;
    token_type_t tok_type;
    scanner_t *scanner = &parser->scanner;
    size_t save_pos;
    http_header_t *header;
    int header_id;
    int ret = 0;
    int index;
    http_header_t *orig_header;
    char save_char;
    int ret2,
      ret3;

    assert( parser->position == POS_HEADERS ||
            parser->ent_position == ENTREAD_CHUNKY_HEADERS );

    while( TRUE ) {
        save_pos = scanner->cursor;

        //
        // check end of headers
        //
        status = scanner_get_token( scanner, &token, &tok_type );
        if( status != PARSE_OK ) {
            return status;
        }

        if( tok_type == TT_CRLF ) {
            // end of headers
            parser->position = POS_ENTITY;  // read entity next
            return PARSE_OK;
        }
        //
        // not end; read header
        //
        if( tok_type != TT_IDENTIFIER ) {
            return PARSE_FAILURE;   // didn't see header name
        }

        status = match( scanner, " : %R%c", &hdr_value );
        if( status != PARSE_OK ) {
            // pushback tokens; useful only on INCOMPLETE error
            scanner->cursor = save_pos;
            return status;
        }

        //
        // add header
        //

        // find header
        index = map_str_to_int( token.buf, token.length, Http_Header_Names,
                                NUM_HTTP_HEADER_NAMES, FALSE );
        if( index != -1 ) {
            header_id = Http_Header_Names[index].id;

            orig_header =
                httpmsg_find_hdr( &parser->msg, header_id, NULL );
        } else {
            header_id = HDR_UNKNOWN;

            save_char = token.buf[token.length];
            token.buf[token.length] = '\0';

            orig_header = httpmsg_find_hdr_str( &parser->msg, token.buf );

            token.buf[token.length] = save_char;    // restore
        }

        if( orig_header == NULL ) {
            //
            // add new header
            //

            header = ( http_header_t * ) malloc( sizeof( http_header_t ) );
            if( header == NULL ) {
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }
            membuffer_init( &header->multi_hdr_buf );

            header->name = token;
            header->value = hdr_value;
            header->name_id = header_id;

            ret = dlist_append( &parser->msg.headers, header );
            if( ret == UPNP_E_OUTOF_MEMORY ) {
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }
        } else if( hdr_value.length > 0 ) {
            //
            // append value to existing header
            //

            if( orig_header->multi_hdr_buf.buf == NULL ) {
                // store in buffer
                ret = membuffer_append( &orig_header->multi_hdr_buf,
                                        orig_header->value.buf,
                                        orig_header->value.length );
            }
            // append space
            ret2 =
                membuffer_append( &orig_header->multi_hdr_buf, ", ", 2 );

            // append continuation of header value
            ret3 = membuffer_append( &orig_header->multi_hdr_buf,
                                     hdr_value.buf, hdr_value.length );

            if( ret == UPNP_E_OUTOF_MEMORY ||
                ret2 == UPNP_E_OUTOF_MEMORY ||
                ret3 == UPNP_E_OUTOF_MEMORY ) {
                // not enuf mem
                parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                return PARSE_FAILURE;
            }
            // header value points to allocated buf
            orig_header->value.buf = orig_header->multi_hdr_buf.buf;
            orig_header->value.length = orig_header->multi_hdr_buf.length;
        }
    }                           // end while

}
#endif
// ******************************

/************************************************************************
* Function: parser_parse_entity_using_clen								
*																		
* Parameters:															
*	INOUT http_parser_t* parser ; HTTP Parser object
*																		
* Description: reads entity using content-length						
*																		
* Returns:																
*	 PARSE_INCOMPLETE													
*	 PARSE_FAILURE -- entity length > content-length value				
*	 PARSE_SUCCESS														
************************************************************************/
static XINLINE parse_status_t
parser_parse_entity_using_clen( INOUT http_parser_t * parser )
{
    //int entity_length;

    assert( parser->ent_position == ENTREAD_USING_CLEN );

    // determine entity (i.e. body) length so far
    //entity_length = parser->msg.msg.length - parser->entity_start_position;
    parser->msg.entity.length =
        parser->msg.msg.length - parser->entity_start_position;

    if( parser->msg.entity.length < parser->content_length ) {
        // more data to be read
        return PARSE_INCOMPLETE;
    } else {
        if( parser->msg.entity.length > parser->content_length ) {
            // silently discard extra data
            parser->msg.msg.buf[parser->entity_start_position +
                                parser->content_length] = '\0';
        }
        // save entity length
        parser->msg.entity.length = parser->content_length;

        // save entity start ptr; (the very last thing to do)
        parser->msg.entity.buf = parser->msg.msg.buf +
            parser->entity_start_position;

        // done reading entity
        parser->position = POS_COMPLETE;
        return PARSE_SUCCESS;
    }
}

/************************************************************************
* Function: parser_parse_chunky_body									
*																		
* Parameters:															
*	INOUT http_parser_t* parser	; HTTP Parser object
*																		
* Description: Read data in the chunks									
*																		
* Returns:																
*	 PARSE_INCOMPLETE													
*	 PARSE_FAILURE -- entity length > content-length value				
*	 PARSE_SUCCESS														
************************************************************************/
static XINLINE parse_status_t
parser_parse_chunky_body( INOUT http_parser_t * parser )
{
    parse_status_t status;
    size_t save_pos;

    // if 'chunk_size' of bytes have been read; read next chunk
    if( ( int )( parser->msg.msg.length - parser->scanner.cursor ) >=
        parser->chunk_size ) {
        // move to next chunk
        parser->scanner.cursor += parser->chunk_size;
        save_pos = parser->scanner.cursor;

        //discard CRLF
        status = match( &parser->scanner, "%c" );
        if( status != PARSE_OK ) {
            parser->scanner.cursor -= parser->chunk_size;   //move back
            //parser->scanner.cursor = save_pos;
            return status;
        }

        membuffer_delete( &parser->msg.msg, save_pos,
                          ( parser->scanner.cursor - save_pos ) );
        parser->scanner.cursor = save_pos;
        parser->msg.entity.length += parser->chunk_size;    //update temp 
        parser->ent_position = ENTREAD_USING_CHUNKED;
        return PARSE_CONTINUE_1;
    } else {
        return PARSE_INCOMPLETE;    // need more data for chunk
    }
}

/************************************************************************
* Function: parser_parse_chunky_headers									
*																		
* Parameters:															
*	INOUT http_parser_t* parser	; HTTP Parser object						
*																		
* Description: Read headers at the end of the chunked entity			
*																		
* Returns:																
*	 PARSE_INCOMPLETE													
*	 PARSE_FAILURE -- entity length > content-length value				
*	 PARSE_SUCCESS														
************************************************************************/
static XINLINE parse_status_t
parser_parse_chunky_headers( INOUT http_parser_t * parser )
{
    parse_status_t status;
    size_t save_pos;

    save_pos = parser->scanner.cursor;
    status = parser_parse_headers( parser );
    if( status == PARSE_OK ) {
        // finally, done with the whole msg
        parser->position = POS_COMPLETE;

        // save entity start ptr as the very last thing to do
        parser->msg.entity.buf = parser->msg.msg.buf +
            parser->entity_start_position;

        membuffer_delete( &parser->msg.msg, save_pos,
                          ( parser->scanner.cursor - save_pos ) );
        parser->scanner.cursor = save_pos;

        return PARSE_SUCCESS;
    } else {
        return status;
    }
}

/************************************************************************
* Function: parser_parse_chunky_entity									
*																		
* Parameters:															
*	INOUT http_parser_t* parser	- HTTP Parser Object									
*																		
* Description: Read headers at the end of the chunked entity			
*																		
* Returns:																
*	 PARSE_INCOMPLETE													
*	 PARSE_FAILURE -- entity length > content-length value				
*	 PARSE_SUCCESS														
*	 PARSE_CONTINUE_1													
************************************************************************/
static XINLINE parse_status_t
parser_parse_chunky_entity( INOUT http_parser_t * parser )
{
    scanner_t *scanner = &parser->scanner;
    parse_status_t status;
    size_t save_pos;
    memptr dummy;

    assert( parser->ent_position == ENTREAD_USING_CHUNKED );

    save_pos = scanner->cursor;

    // get size of chunk, discard extension, discard CRLF
    status = match( scanner, "%x%L%c", &parser->chunk_size, &dummy );
    if( status != PARSE_OK ) {
        scanner->cursor = save_pos;
        DBGONLY( UpnpPrintf
                 ( UPNP_INFO, HTTP, __FILE__, __LINE__,
                   "CHUNK COULD NOT BE PARSED\n" ); )
            return status;
    }
    // remove chunk info just matched; just retain data
    membuffer_delete( &parser->msg.msg, save_pos,
                      ( scanner->cursor - save_pos ) );
    scanner->cursor = save_pos; // adjust scanner too

    if( parser->chunk_size == 0 ) {
        // done reading entity; determine length of entity
        parser->msg.entity.length = parser->scanner.cursor -
            parser->entity_start_position;

        // read entity headers
        parser->ent_position = ENTREAD_CHUNKY_HEADERS;
    } else {
        // read chunk body
        parser->ent_position = ENTREAD_CHUNKY_BODY;
    }

    return PARSE_CONTINUE_1;    // continue to reading body
}

/************************************************************************
* Function: parser_parse_entity_until_close								
*																		
* Parameters:															
*	INOUT http_parser_t* parser	; HTTP Parser object
*																		
* Description: Read headers at the end of the chunked entity			
*																		
* Returns:																
*	 PARSE_INCOMPLETE_ENTITY											
************************************************************************/
static XINLINE parse_status_t
parser_parse_entity_until_close( INOUT http_parser_t * parser )
{
    size_t cursor;

    assert( parser->ent_position == ENTREAD_UNTIL_CLOSE );

    // eat any and all data
    cursor = parser->msg.msg.length;

    // update entity length
    parser->msg.entity.length = cursor - parser->entity_start_position;

    // update pointer
    parser->msg.entity.buf =
        parser->msg.msg.buf + parser->entity_start_position;

    parser->scanner.cursor = cursor;

    return PARSE_INCOMPLETE_ENTITY; // add anything
}

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
XINLINE parse_status_t
parser_get_entity_read_method( INOUT http_parser_t * parser )
{
    http_message_t *hmsg = &parser->msg;
    int response_code;
    memptr hdr_value;

    assert( parser->ent_position == ENTREAD_DETERMINE_READ_METHOD );

    // entity points to start of msg body
    parser->msg.entity.buf = scanner_get_str( &parser->scanner );
    parser->msg.entity.length = 0;

    // remember start of body
    parser->entity_start_position = parser->scanner.cursor;

    // std http rules for determining content length

    // * no body for 1xx, 204, 304 and HEAD, GET,
    //      SUBSCRIBE, UNSUBSCRIBE
    if( hmsg->is_request ) {
        switch ( hmsg->method ) {
            case HTTPMETHOD_HEAD:
            case HTTPMETHOD_GET:
                //case HTTPMETHOD_POST:
            case HTTPMETHOD_SUBSCRIBE:
            case HTTPMETHOD_UNSUBSCRIBE:
            case HTTPMETHOD_MSEARCH:
                // no body; mark as done
                parser->position = POS_COMPLETE;
                return PARSE_SUCCESS;
                break;

            default:
                ;               // do nothing
        }
    } else                      // response
    {
        response_code = hmsg->status_code;

        if( response_code == 204 ||
            response_code == 304 ||
            ( response_code >= 100 && response_code <= 199 ) ||
            hmsg->request_method == HTTPMETHOD_HEAD ||
            hmsg->request_method == HTTPMETHOD_MSEARCH ||
            hmsg->request_method == HTTPMETHOD_SUBSCRIBE ||
            hmsg->request_method == HTTPMETHOD_UNSUBSCRIBE ||
            hmsg->request_method == HTTPMETHOD_NOTIFY ) {
            parser->position = POS_COMPLETE;
            return PARSE_SUCCESS;
        }
    }

    // * transfer-encoding -- used to indicate chunked data
    if( httpmsg_find_hdr( hmsg, HDR_TRANSFER_ENCODING, &hdr_value ) ) {
        if( raw_find_str( &hdr_value, "chunked" ) >= 0 ) {
            // read method to use chunked transfer encoding
            parser->ent_position = ENTREAD_USING_CHUNKED;
            DBGONLY( UpnpPrintf
                     ( UPNP_INFO, HTTP, __FILE__, __LINE__,
                       "Found Chunked Encoding ....\n" ); )

                return PARSE_CONTINUE_1;
        }
    }
    // * use content length
    if( httpmsg_find_hdr( hmsg, HDR_CONTENT_LENGTH, &hdr_value ) ) {
        parser->content_length = raw_to_int( &hdr_value, 10 );
        if( parser->content_length < 0 ) {
            // bad content-length
            return PARSE_FAILURE;
        }
        parser->ent_position = ENTREAD_USING_CLEN;
        return PARSE_CONTINUE_1;
    }
    // * multi-part/byteranges not supported (yet)

    // * read until connection is closed
    if( hmsg->is_request ) {
        // set hack flag for NOTIFY methods; if set to true this is
        //  a valid SSDP notify msg
        if( hmsg->method == HTTPMETHOD_NOTIFY ) {
            parser->valid_ssdp_notify_hack = TRUE;
        }

        parser->http_error_code = HTTP_LENGTH_REQUIRED;
        return PARSE_FAILURE;
    }

    parser->ent_position = ENTREAD_UNTIL_CLOSE;
    return PARSE_CONTINUE_1;
}

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
XINLINE parse_status_t
parser_parse_entity( INOUT http_parser_t * parser )
{
    parse_status_t status = PARSE_OK;

    assert( parser->position == POS_ENTITY );

    do {
        switch ( parser->ent_position ) {
            case ENTREAD_USING_CLEN:
                status = parser_parse_entity_using_clen( parser );
                break;

            case ENTREAD_USING_CHUNKED:
                status = parser_parse_chunky_entity( parser );
                break;

            case ENTREAD_CHUNKY_BODY:
                status = parser_parse_chunky_body( parser );
                break;

            case ENTREAD_CHUNKY_HEADERS:
                status = parser_parse_chunky_headers( parser );
                break;

            case ENTREAD_UNTIL_CLOSE:
                status = parser_parse_entity_until_close( parser );
                break;

            case ENTREAD_DETERMINE_READ_METHOD:
                status = parser_get_entity_read_method( parser );
                break;

            default:
                assert( 0 );
        }

    } while( status == PARSE_CONTINUE_1 );

    return status;
}

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
void
parser_request_init( OUT http_parser_t * parser )
{
    parser_init( parser );
    parser->msg.is_request = TRUE;
    parser->position = POS_REQUEST_LINE;
}

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
void
parser_response_init( OUT http_parser_t * parser,
                      IN http_method_t request_method )
{
    parser_init( parser );
    parser->msg.is_request = FALSE;
    parser->msg.request_method = request_method;
    parser->position = POS_RESPONSE_LINE;
}

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
parse_status_t
parser_parse( INOUT http_parser_t * parser )
{
    parse_status_t status;

    //takes an http_parser_t with memory already allocated 
    //in the message 
    assert( parser != NULL );

    do {
        switch ( parser->position ) {
            case POS_ENTITY:
                status = parser_parse_entity( parser );

                break;

            case POS_HEADERS:
                status = parser_parse_headers( parser );

                break;

            case POS_REQUEST_LINE:
                status = parser_parse_requestline( parser );

                break;

            case POS_RESPONSE_LINE:
                status = parser_parse_responseline( parser );

                break;

            default:
                {
                    status = PARSE_FAILURE;
                    assert( 0 );
                }
        }

    } while( status == PARSE_OK );

    return status;

}

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
parse_status_t
parser_append( INOUT http_parser_t * parser,
               IN const char *buf,
               IN size_t buf_length )
{
    int ret_code;

    assert( parser != NULL );
    assert( buf != NULL );

    // append data to buffer
    ret_code = membuffer_append( &parser->msg.msg, buf, buf_length );
    if( ret_code != 0 ) {
        // set failure status
        parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
        return PARSE_FAILURE;
    }

    return parser_parse( parser );
}

/************************************************************************
**********					end of parser					  ***********
************************************************************************/

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
int
raw_to_int( IN memptr * raw_value,
            IN int base )
{
    long  num;
    char *end_ptr;

    if( raw_value->length == 0 ) {
        return -1;
    }

    errno = 0;
    num = strtol( raw_value->buf, &end_ptr, base );
    if( ( num < 0 )
        // all and only those chars in token should be used for num
        || ( end_ptr != raw_value->buf + raw_value->length )
        || ( ( num == LONG_MIN || num == LONG_MAX )
             && ( errno == ERANGE ) )
         ) {
        return -1;
    }
    return num;

}

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
int
raw_find_str( IN memptr * raw_value,
              IN const char *str )
{
    char c;
    char *ptr;

    c = raw_value->buf[raw_value->length];  // save
    raw_value->buf[raw_value->length] = 0;  // null-terminate

    ptr = strstr( raw_value->buf, str );

    raw_value->buf[raw_value->length] = c;  // restore

    if( ptr == 0 ) {
        return -1;
    }

    return ptr - raw_value->buf;    // return index
}

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
const char *
method_to_str( IN http_method_t method )
{
    int index;

    index = map_int_to_str( method, Http_Method_Table, NUM_HTTP_METHODS );

    assert( index != -1 );

    return index == -1 ? NULL : Http_Method_Table[index].name;
}

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
void
print_http_headers( http_message_t * hmsg )
{

    ListNode *node;

    //NNS:  dlist_node *node;
    http_header_t *header;

    // print start line
    if( hmsg->is_request ) {
        //printf( "method = %d, version = %d.%d, url = %.*s\n", 
        //  hmsg->method, hmsg->major_version, hmsg->minor_version,
        //  hmsg->uri.pathquery.size, hmsg->uri.pathquery.buff);
    } else {
        //  printf( "resp status = %d, version = %d.%d, status msg = %.*s\n",
        //  hmsg->status_code, hmsg->major_version, hmsg->minor_version,
        //  (int)hmsg->status_msg.length, hmsg->status_msg.buf);
    }

    // print headers

    node = ListHead( &hmsg->headers );
    //NNS: node = dlist_first_node( &hmsg->headers );
    while( node != NULL ) {

        header = ( http_header_t * ) node->item;
        //NNS: header = (http_header_t *)node->data;
        //printf( "hdr name: %.*s, value: %.*s\n", 
        //  (int)header->name.length, header->name.buf,
        //  (int)header->value.length, header->value.buf );

        node = ListNext( &hmsg->headers, node );

        //NNS: node = dlist_next( &hmsg->headers, node );
    }
}
