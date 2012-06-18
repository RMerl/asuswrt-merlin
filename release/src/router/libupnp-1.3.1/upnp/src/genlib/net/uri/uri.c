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
* Purpose: This file contains functions for uri, url parsing utility. 
************************************************************************/

#include "config.h"
#include "uri.h"

/************************************************************************
*	Function :	is_reserved
*
*	Parameters :
*		char in ;	char to be matched for RESERVED characters 
*
*	Description : Returns a 1 if a char is a RESERVED char as defined in 
*		http://www.ietf.org/rfc/rfc2396.txt RFC explaining URIs)
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
is_reserved( char in )
{
    if( strchr( RESERVED, in ) )
        return 1;
    else
        return 0;
}

/************************************************************************
*	Function :	is_mark
*
*	Parameters :
*		char in ; character to be matched for MARKED characters
*
*	Description : Returns a 1 if a char is a MARK char as defined in 
*		http://www.ietf.org/rfc/rfc2396.txt (RFC explaining URIs)
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
is_mark( char in )
{
    if( strchr( MARK, in ) )
        return 1;
    else
        return 0;
}

/************************************************************************
*	Function :	is_unreserved
*
*	Parameters :
*		char in ;	character to be matched for UNRESERVED characters
*
*	Description : Returns a 1 if a char is an unreserved char as defined in 
*		http://www.ietf.org/rfc/rfc2396.txt (RFC explaining URIs)	
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
is_unreserved( char in )
{
    if( isalnum( in ) || ( is_mark( in ) ) )
        return 1;
    else
        return 0;
}

/************************************************************************
*	Function :	is_escaped
*
*	Parameters :
*		char * in ;	character to be matched for ESCAPED characters
*
*	Description : Returns a 1 if a char[3] sequence is escaped as defined 
*		in http://www.ietf.org/rfc/rfc2396.txt (RFC explaining URIs)
*               size of array is NOT checked (MUST be checked by caller)	
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
is_escaped( char *in )
{

    if( ( in[0] == '%' ) && ( isxdigit( in[1] ) ) && isxdigit( in[2] ) ) {

        return 1;
    } else
        return 0;
}

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
int
replace_escaped( char *in,
                 int index,
                 int *max )
{
    int tempInt = 0;
    char tempChar = 0;
    int i = 0;
    int j = 0;

    if( ( in[index] == '%' ) && ( isxdigit( in[index + 1] ) )
        && isxdigit( in[index + 2] ) ) {
        //Note the "%2x", makes sure that we convert a maximum of two
        //characters.
        if( sscanf( &in[index + 1], "%2x", &tempInt ) != 1 )
            return 0;

        tempChar = ( char )tempInt;

        for( i = index + 3, j = index; j < ( *max ); i++, j++ ) {
            in[j] = tempChar;
            if( i < ( *max ) )
                tempChar = in[i];
            else
                tempChar = 0;
        }
        ( *max ) -= 2;
        return 1;
    } else
        return 0;
}

/************************************************************************
*	Function :	parse_uric
*
*	Parameters :
*		char *in ;	string of characters
*		int max ;	maximum limit
*		token *out ; token object where the string of characters is 
*					 copied
*
*	Description : Parses a string of uric characters starting at in[0]
*		as defined in http://www.ietf.org/rfc/rfc2396.txt (RFC explaining 
*		URIs)	
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
parse_uric( char *in,
            int max,
            token * out )
{
    int i = 0;

    while( ( i < max )
           && ( ( is_unreserved( in[i] ) ) || ( is_reserved( in[i] ) )
                || ( ( i + 2 < max ) && ( is_escaped( &in[i] ) ) ) ) ) {
        i++;
    }

    out->size = i;
    out->buff = in;
    return i;
}

/************************************************************************
*	Function :	copy_sockaddr_in
*
*	Parameters :
*		const struct sockaddr_in *in ;	Source socket address object
*		struct sockaddr_in *out ;		Destination socket address object
*
*	Description : Copies one socket address into another
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
copy_sockaddr_in( const struct sockaddr_in *in,
                  struct sockaddr_in *out )
{
    memset( out->sin_zero, 0, 8 );
    out->sin_family = in->sin_family;
    out->sin_port = in->sin_port;
    out->sin_addr.s_addr = in->sin_addr.s_addr;
}

/************************************************************************
*	Function :	copy_token
*
*	Parameters :
*		const token *in ;		source token	
*		const char * in_base ;	
*		token * out ;			destination token
*		char * out_base ;	
*
*	Description : Tokens are generally pointers into other strings
*		this copies the offset and size from a token (in) relative to 
*		one string (in_base) into a token (out) relative to another 
*		string (out_base)
*
*	Return : void ;
*
*	Note :
************************************************************************/
static void
copy_token( const token * in,
            const char *in_base,
            token * out,
            char *out_base )
{
    out->size = in->size;
    out->buff = out_base + ( in->buff - in_base );
}

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
int
copy_URL_list( URL_list * in,
               URL_list * out )
{
    int len = strlen( in->URLs ) + 1;
    int i = 0;

    out->URLs = NULL;
    out->parsedURLs = NULL;
    out->size = 0;

    out->URLs = ( char * )malloc( len );
    out->parsedURLs =
        ( uri_type * ) malloc( sizeof( uri_type ) * in->size );

    if( ( out->URLs == NULL ) || ( out->parsedURLs == NULL ) )
        return UPNP_E_OUTOF_MEMORY;

    memcpy( out->URLs, in->URLs, len );

    for( i = 0; i < in->size; i++ ) {
        //copy the parsed uri
        out->parsedURLs[i].type = in->parsedURLs[i].type;
        copy_token( &in->parsedURLs[i].scheme, in->URLs,
                    &out->parsedURLs[i].scheme, out->URLs );

        out->parsedURLs[i].path_type = in->parsedURLs[i].path_type;
        copy_token( &in->parsedURLs[i].pathquery, in->URLs,
                    &out->parsedURLs[i].pathquery, out->URLs );
        copy_token( &in->parsedURLs[i].fragment, in->URLs,
                    &out->parsedURLs[i].fragment, out->URLs );
        copy_token( &in->parsedURLs[i].hostport.text,
                    in->URLs, &out->parsedURLs[i].hostport.text,
                    out->URLs );

        copy_sockaddr_in( &in->parsedURLs[i].hostport.IPv4address,
                          &out->parsedURLs[i].hostport.IPv4address );
    }
    out->size = in->size;
    return HTTP_SUCCESS;

}

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
void
free_URL_list( URL_list * list )
{
    if( list->URLs )
        free( list->URLs );
    if( list->parsedURLs )
        free( list->parsedURLs );
    list->size = 0;
}

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
DBGONLY( void print_uri( uri_type * in ) {
         print_token( &in->scheme );
         print_token( &in->hostport.text );
         print_token( &in->pathquery ); print_token( &in->fragment );} )

/************************************************************************
*	Function :	print_token
*
*	Parameters :
*		token * in ;	token
*
*	Description : Function useful in debugging for printing a token.
*		Compiled out with DBGONLY macro. 
*
*	Return : void ;
*
*	Note :
************************************************************************/
DBGONLY( void print_token( token * in ) {
         int i = 0;
         printf( "Token Size : %d\n\'", in->size );
         for( i = 0; i < in->size; i++ ) {
         putchar( in->buff[i] );}
         putchar( '\'' ); putchar( '\n' );}

 )

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
    int token_string_casecmp( token * in1,
                              char *in2 ) {
    int in2_length = strlen( in2 );

    if( in1->size != in2_length )
        return 1;
    else
        return strncasecmp( in1->buff, in2, in1->size );
}

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
int
token_string_cmp( token * in1,
                  char *in2 )
{
    int in2_length = strlen( in2 );

    if( in1->size != in2_length )
        return 1;
    else
        return strncmp( in1->buff, in2, in1->size );
}

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
int
token_cmp( token * in1,
           token * in2 )
{
    if( in1->size != in2->size )
        return 1;
    else
        return memcmp( in1->buff, in2->buff, in1->size );
}

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
int
parse_port( int max,
            char *port,
            unsigned short *out )
{

    char *finger = port;
    char *max_ptr = finger + max;
    unsigned short temp = 0;

    while( ( finger < max_ptr ) && ( isdigit( *finger ) ) ) {
        temp = temp * 10;
        temp += ( *finger ) - '0';
        finger++;
    }

    *out = htons( temp );
    return finger - port;
}

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
int
parse_hostport( char *in,
                int max,
                hostport_type * out )
{
#define BUFFER_SIZE 8192

    int i = 0;
    int begin_port;
    int hostport_size = 0;
    int host_size = 0;
    struct hostent h_buf;
    char temp_hostbyname_buff[BUFFER_SIZE];
    struct hostent *h = NULL;
    int errcode = 0;
    char *temp_host_name = NULL;
    int last_dot = -1;

    out->text.size = 0;
    out->text.buff = NULL;

    out->IPv4address.sin_port = htons( 80 );    //default port is 80
    memset( &out->IPv4address.sin_zero, 0, 8 );

    while( ( i < max ) && ( in[i] != ':' ) && ( in[i] != '/' )
           && ( ( isalnum( in[i] ) ) || ( in[i] == '.' )
                || ( in[i] == '-' ) ) ) {
        i++;
        if( in[i] == '.' ) {
            last_dot = i;
        }
    }

    host_size = i;

    if( ( i < max ) && ( in[i] == ':' ) ) {
        begin_port = i + 1;
        //convert port
        if( !( hostport_size = parse_port( max - begin_port,
                                           &in[begin_port],
                                           &out->IPv4address.sin_port ) ) )
        {
            return UPNP_E_INVALID_URL;
        }
        hostport_size += begin_port;
    } else
        hostport_size = host_size;

    //convert to temporary null terminated string
    temp_host_name = ( char * )malloc( host_size + 1 );

    if( temp_host_name == NULL )
        return UPNP_E_OUTOF_MEMORY;

    memcpy( temp_host_name, in, host_size );
    temp_host_name[host_size] = '\0';

    //check to see if host name is an ipv4 address
    if( ( last_dot != -1 ) && ( last_dot + 1 < host_size )
        && ( isdigit( temp_host_name[last_dot + 1] ) ) ) {
        //must be ipv4 address

        errcode = inet_pton( AF_INET,
                             temp_host_name, &out->IPv4address.sin_addr );
        if( errcode == 1 ) {
            out->IPv4address.sin_family = AF_INET;
        } else {
            out->IPv4address.sin_addr.s_addr = 0;
            out->IPv4address.sin_family = AF_INET;
            free( temp_host_name );
            temp_host_name = NULL;
            return UPNP_E_INVALID_URL;
        }
    } else {
        int errCode = 0;

        //call gethostbyname_r (reentrant form of gethostbyname)
        errCode = gethostbyname_r( temp_host_name,
                                   &h_buf,
                                   temp_hostbyname_buff,
                                   BUFFER_SIZE, &h, &errcode );

        if( errCode == 0 ) {
            if( h ) {
                if( ( h->h_addrtype == AF_INET ) && ( h->h_length == 4 ) ) {
                    out->IPv4address.sin_addr =
                        ( *( struct in_addr * )h->h_addr );
                    out->IPv4address.sin_family = AF_INET;

                }
            }
        } else {
            out->IPv4address.sin_addr.s_addr = 0;
            out->IPv4address.sin_family = AF_INET;
            free( temp_host_name );
            temp_host_name = NULL;
            return UPNP_E_INVALID_URL;
        }
    }

    if( temp_host_name ) {
        free( temp_host_name );
        temp_host_name = NULL;
    }

    out->text.size = hostport_size;
    out->text.buff = in;
    return hostport_size;

}

/************************************************************************
*	Function :	parse_scheme
*
*	Parameters :
*		char * in ;	string of characters representing a scheme
*		int max ;	maximum number of characters
*		token * out ;	output parameter whose buffer is filled in with 
*					the scheme
*
*	Description : parses a uri scheme starting at in[0] as defined in 
*		http://www.ietf.org/rfc/rfc2396.txt (RFC explaining URIs)
*		(e.g. "http:" -> scheme= "http"). 
*		Note, string MUST include ':' within the max charcters
*
*	Return : int ;
*
*	Note :
************************************************************************/
int
parse_scheme( char *in,
              int max,
              token * out )
{
    int i = 0;

    out->size = 0;
    out->buff = NULL;

    if( ( max == 0 ) || ( !isalpha( in[0] ) ) )
        return FALSE;

    i++;
    while( ( i < max ) && ( in[i] != ':' ) ) {

        if( !( isalnum( in[i] ) || ( in[i] == '+' ) || ( in[i] == '-' )
               || ( in[i] == '.' ) ) )
            return FALSE;

        i++;
    }
    if( i < max ) {
        out->size = i;
        out->buff = &in[0];
        return i;
    }

    return FALSE;

}

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
int
remove_escaped_chars( INOUT char *in,
                      INOUT int *size )
{
    int i = 0;

    for( i = 0; i < *size; i++ ) {
        replace_escaped( in, i, size );
    }
    return UPNP_E_SUCCESS;
}

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
int
remove_dots( char *in,
             int size )
{
    char *copyTo = in;
    char *copyFrom = in;
    char *max = in + size;
    char **Segments = NULL;
    int lastSegment = -1;

    Segments = malloc( sizeof( char * ) * size );

    if( Segments == NULL )
        return UPNP_E_OUTOF_MEMORY;

    Segments[0] = NULL;
    DBGONLY( UpnpPrintf
             ( UPNP_ALL, API, __FILE__, __LINE__,
               "REMOVE_DOTS: before: %s\n", in ) );
    while( ( copyFrom < max ) && ( *copyFrom != '?' )
           && ( *copyFrom != '#' ) ) {

        if( ( ( *copyFrom ) == '.' )
            && ( ( copyFrom == in ) || ( *( copyFrom - 1 ) == '/' ) ) ) {
            if( ( copyFrom + 1 == max )
                || ( *( copyFrom + 1 ) == '/' ) ) {

                copyFrom += 2;
                continue;
            } else if( ( *( copyFrom + 1 ) == '.' )
                       && ( ( copyFrom + 2 == max )
                            || ( *( copyFrom + 2 ) == '/' ) ) ) {
                copyFrom += 3;

                if( lastSegment > 0 ) {
                    copyTo = Segments[--lastSegment];
                } else {
                    free( Segments );
                    //TRACE("ERROR RESOLVING URL, ../ at ROOT");
                    return UPNP_E_INVALID_URL;
                }
                continue;
            }
        }

        if( ( *copyFrom ) == '/' ) {

            lastSegment++;
            Segments[lastSegment] = copyTo + 1;
        }
        ( *copyTo ) = ( *copyFrom );
        copyTo++;
        copyFrom++;
    }
    if( copyFrom < max ) {
        while( copyFrom < max ) {
            ( *copyTo ) = ( *copyFrom );
            copyTo++;
            copyFrom++;
        }
    }
    ( *copyTo ) = 0;
    free( Segments );
    DBGONLY( UpnpPrintf
             ( UPNP_ALL, API, __FILE__, __LINE__,
               "REMOVE_DOTS: after: %s\n", in ) );
    return UPNP_E_SUCCESS;
}

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
char *
resolve_rel_url( char *base_url,
                 char *rel_url )
{
    uri_type base;
    uri_type rel;
    char temp_path = '/';

    int i = 0;
    char *finger = NULL;

    char *last_slash = NULL;

    char *out = NULL;
    char *out_finger = NULL;

    if( base_url && rel_url ) {
        out =
            ( char * )malloc( strlen( base_url ) + strlen( rel_url ) + 2 );
        out_finger = out;
    } else {
        if( rel_url )
            return strdup( rel_url );
        else
            return NULL;
    }

    if( out == NULL ) {
        return NULL;
    }

    if( ( parse_uri( rel_url, strlen( rel_url ), &rel ) ) == HTTP_SUCCESS ) {

        if( rel.type == ABSOLUTE ) {

            strcpy( out, rel_url );
        } else {

            if( ( parse_uri( base_url, strlen( base_url ), &base ) ==
                  HTTP_SUCCESS )
                && ( base.type == ABSOLUTE ) ) {

                if( strlen( rel_url ) == 0 ) {
                    strcpy( out, base_url );
                } else {
                    memcpy( out, base.scheme.buff, base.scheme.size );
                    out_finger += base.scheme.size;
                    ( *out_finger ) = ':';
                    out_finger++;

                    if( rel.hostport.text.size > 0 ) {
                        sprintf( out_finger, "%s", rel_url );
                    } else {
                        if( base.hostport.text.size > 0 ) {
                            memcpy( out_finger, "//", 2 );
                            out_finger += 2;
                            memcpy( out_finger, base.hostport.text.buff,
                                    base.hostport.text.size );
                            out_finger += base.hostport.text.size;
                        }

                        if( rel.path_type == ABS_PATH ) {
                            strcpy( out_finger, rel_url );

                        } else {

                            if( base.pathquery.size == 0 ) {
                                base.pathquery.size = 1;
                                base.pathquery.buff = &temp_path;
                            }

                            finger = out_finger;
                            last_slash = finger;
                            i = 0;

                            while( ( i < base.pathquery.size ) &&
                                   ( base.pathquery.buff[i] != '?' ) ) {
                                ( *finger ) = base.pathquery.buff[i];
                                if( base.pathquery.buff[i] == '/' )
                                    last_slash = finger + 1;
                                i++;
                                finger++;

                            }
                            i = 0;
                            strcpy( last_slash, rel_url );
                            if( remove_dots( out_finger,
                                             strlen( out_finger ) ) !=
                                UPNP_E_SUCCESS ) {
                                free( out );
                                //free(rel_url);
                                return NULL;
                            }
                        }

                    }
                }
            } else {
                free( out );
                //free(rel_url);
                return NULL;
            }
        }
    } else {
        free( out );
        //free(rel_url);            
        return NULL;
    }

    //free(rel_url);
    return out;
}

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
int
parse_uri( char *in,
           int max,
           uri_type * out )
{
    int begin_path = 0;
    int begin_hostport = 0;
    int begin_fragment = 0;

    if( ( begin_hostport = parse_scheme( in, max, &out->scheme ) ) ) {
        out->type = ABSOLUTE;
        out->path_type = OPAQUE_PART;
        begin_hostport++;
    } else {
        out->type = RELATIVE;
        out->path_type = REL_PATH;
    }

    if( ( ( begin_hostport + 1 ) < max ) && ( in[begin_hostport] == '/' )
        && ( in[begin_hostport + 1] == '/' ) ) {
        begin_hostport += 2;

        if( ( begin_path = parse_hostport( &in[begin_hostport],
                                           max - begin_hostport,
                                           &out->hostport ) ) >= 0 ) {
            begin_path += begin_hostport;
        } else
            return begin_path;

    } else {
        out->hostport.IPv4address.sin_port = 0;
        out->hostport.IPv4address.sin_addr.s_addr = 0;
        out->hostport.text.size = 0;
        out->hostport.text.buff = 0;
        begin_path = begin_hostport;
    }

    begin_fragment =
        parse_uric( &in[begin_path], max - begin_path,
                    &out->pathquery ) + begin_path;

    if( ( out->pathquery.size ) && ( out->pathquery.buff[0] == '/' ) ) {
        out->path_type = ABS_PATH;
    }

    if( ( begin_fragment < max ) && ( in[begin_fragment] == '#' ) ) {
        begin_fragment++;
        parse_uric( &in[begin_fragment], max - begin_fragment,
                    &out->fragment );
    } else {
        out->fragment.buff = NULL;
        out->fragment.size = 0;
    }
    return HTTP_SUCCESS;
}

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
int
parse_uri_and_unescape( char *in,
                        int max,
                        uri_type * out )
{
    int ret;

    if( ( ret = parse_uri( in, max, out ) ) != HTTP_SUCCESS )
        return ret;
    if( out->pathquery.size > 0 )
        remove_escaped_chars( out->pathquery.buff, &out->pathquery.size );
    if( out->fragment.size > 0 )
        remove_escaped_chars( out->fragment.buff, &out->fragment.size );
    return HTTP_SUCCESS;
}
