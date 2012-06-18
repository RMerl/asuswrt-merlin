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
* Purpose: This file defines the Web Server and has functions to carry out 
* operations of the Web Server.										
************************************************************************/

#include "config.h"
#include <assert.h>
#include "util.h"
#include "strintmap.h"
#include "membuffer.h"
#include "httpparser.h"
#include "httpreadwrite.h"
#include "statcodes.h"
#include "webserver.h"
#include "upnp.h"
#include "upnpapi.h"
#include "ssdplib.h"

#include <unistd.h>
#include <sys/stat.h>
#include "ithread.h"
#include "unixutil.h"

/*
   Response Types 
 */
enum resp_type { RESP_FILEDOC, RESP_XMLDOC, RESP_HEADERS, RESP_WEBDOC,
        RESP_POST };

// mapping of file extension to content-type of document
struct document_type_t {
    const char *file_ext;
    const char *content_type;
    const char *content_subtype;
};

struct xml_alias_t {
    membuffer name;             // name of DOC from root; e.g.: /foo/bar/mydesc.xml
    membuffer doc;              // the XML document contents
    time_t last_modified;
    int *ct;
};

static const char *gMediaTypes[] = {
    NULL,                       // 0
    "audio",                    // 1
    "video",                    // 2
    "image",                    // 3
    "application",              // 4
    "text"                      // 5
};

/*
   Defines 
 */

// index into 'gMediaTypes'
#define AUDIO_STR		"\1"
#define VIDEO_STR		"\2"
#define IMAGE_STR		"\3"
#define APPLICATION_STR "\4"
#define TEXT_STR		"\5"

// int index
#define APPLICATION_INDEX	4
#define TEXT_INDEX			5

// general
#define NUM_MEDIA_TYPES 69
#define NUM_HTTP_HEADER_NAMES 33

// sorted by file extension; must have 'NUM_MEDIA_TYPES' extensions
static const char *gEncodedMediaTypes =
    "aif\0" AUDIO_STR "aiff\0"
    "aifc\0" AUDIO_STR "aiff\0"
    "aiff\0" AUDIO_STR "aiff\0"
    "asf\0" VIDEO_STR "x-ms-asf\0"
    "asx\0" VIDEO_STR "x-ms-asf\0"
    "au\0" AUDIO_STR "basic\0"
    "avi\0" VIDEO_STR "msvideo\0"
    "bmp\0" IMAGE_STR "bmp\0"
    "dcr\0" APPLICATION_STR "x-director\0"
    "dib\0" IMAGE_STR "bmp\0"
    "dir\0" APPLICATION_STR "x-director\0"
    "dxr\0" APPLICATION_STR "x-director\0"
    "gif\0" IMAGE_STR "gif\0"
    "hta\0" TEXT_STR "hta\0"
    "htm\0" TEXT_STR "html\0"
    "html\0" TEXT_STR "html\0"
    "jar\0" APPLICATION_STR "java-archive\0"
    "jfif\0" IMAGE_STR "pjpeg\0"
    "jpe\0" IMAGE_STR "jpeg\0"
    "jpeg\0" IMAGE_STR "jpeg\0"
    "jpg\0" IMAGE_STR "jpeg\0"
    "js\0" APPLICATION_STR "x-javascript\0"
    "kar\0" AUDIO_STR "midi\0"
    "m3u\0" AUDIO_STR "mpegurl\0"
    "mid\0" AUDIO_STR "midi\0"
    "midi\0" AUDIO_STR "midi\0"
    "mov\0" VIDEO_STR "quicktime\0"
    "mp2v\0" VIDEO_STR "x-mpeg2\0"
    "mp3\0" AUDIO_STR "mpeg\0"
    "mpe\0" VIDEO_STR "mpeg\0"
    "mpeg\0" VIDEO_STR "mpeg\0"
    "mpg\0" VIDEO_STR "mpeg\0"
    "mpv\0" VIDEO_STR "mpeg\0"
    "mpv2\0" VIDEO_STR "x-mpeg2\0"
    "pdf\0" APPLICATION_STR "pdf\0"
    "pjp\0" IMAGE_STR "jpeg\0"
    "pjpeg\0" IMAGE_STR "jpeg\0"
    "plg\0" TEXT_STR "html\0"
    "pls\0" AUDIO_STR "scpls\0"
    "png\0" IMAGE_STR "png\0"
    "qt\0" VIDEO_STR "quicktime\0"
    "ram\0" AUDIO_STR "x-pn-realaudio\0"
    "rmi\0" AUDIO_STR "mid\0"
    "rmm\0" AUDIO_STR "x-pn-realaudio\0"
    "rtf\0" APPLICATION_STR "rtf\0"
    "shtml\0" TEXT_STR "html\0"
    "smf\0" AUDIO_STR "midi\0"
    "snd\0" AUDIO_STR "basic\0"
    "spl\0" APPLICATION_STR "futuresplash\0"
    "ssm\0" APPLICATION_STR "streamingmedia\0"
    "swf\0" APPLICATION_STR "x-shockwave-flash\0"
    "tar\0" APPLICATION_STR "tar\0"
    "tcl\0" APPLICATION_STR "x-tcl\0"
    "text\0" TEXT_STR "plain\0"
    "tif\0" IMAGE_STR "tiff\0"
    "tiff\0" IMAGE_STR "tiff\0"
    "txt\0" TEXT_STR "plain\0"
    "ulw\0" AUDIO_STR "basic\0"
    "wav\0" AUDIO_STR "wav\0"
    "wax\0" AUDIO_STR "x-ms-wax\0"
    "wm\0" VIDEO_STR "x-ms-wm\0"
    "wma\0" AUDIO_STR "x-ms-wma\0"
    "wmv\0" VIDEO_STR "x-ms-wmv\0"
    "wvx\0" VIDEO_STR "x-ms-wvx\0"
    "xbm\0" IMAGE_STR "x-xbitmap\0"
    "xml\0" TEXT_STR "xml\0"
    "xsl\0" TEXT_STR "xml\0"
    "z\0" APPLICATION_STR "x-compress\0"
    "zip\0" APPLICATION_STR "zip\0" "\0";
    // *** end ***

/***********************************************************************/
/*
   module variables - Globals, static and externs                      
 */

/***********************************************************************/
static struct document_type_t gMediaTypeList[NUM_MEDIA_TYPES];
membuffer gDocumentRootDir;     // a local dir which serves as webserver root
static struct xml_alias_t gAliasDoc;    // XML document
static ithread_mutex_t gWebMutex;
extern str_int_entry Http_Header_Names[NUM_HTTP_HEADER_NAMES];

/************************************************************************
* Function: has_xml_content_type										
*																		
* Parameters:															
*	none																
*																		
* Description: decodes list and stores it in gMediaTypeList				
*																		
* Returns:																
*	 void																
************************************************************************/
static XINLINE void
media_list_init( void )
{
    int i;
    const char *s = gEncodedMediaTypes;
    struct document_type_t *doc_type;

    for( i = 0; *s != '\0'; i++ ) {
        doc_type = &gMediaTypeList[i];

        doc_type->file_ext = s;

        s += strlen( s ) + 1;   // point to type
        doc_type->content_type = gMediaTypes[( int )*s];    // set cont-type

        s++;                    // point to subtype
        doc_type->content_subtype = s;

        s += strlen( s ) + 1;   // next entry
    }
    assert( i == NUM_MEDIA_TYPES );
}

/************************************************************************
* Function: has_xml_content_type										
*																		
* Parameters:															
*	IN const char* extension ; 											
*	OUT const char** con_type,											
*	OUT const char** con_subtype										
*																		
* Description: Based on the extension, returns the content type and 	
*	content subtype														
*																		
* Returns:																
*	 0 on success;														
*	-1 on error															
************************************************************************/
static XINLINE int
search_extension( IN const char *extension,
                  OUT const char **con_type,
                  OUT const char **con_subtype )
{
    int top,
      mid,
      bot;
    int cmp;

    top = 0;
    bot = NUM_MEDIA_TYPES - 1;

    while( top <= bot ) {
        mid = ( top + bot ) / 2;
        cmp = strcasecmp( extension, gMediaTypeList[mid].file_ext );

        if( cmp > 0 ) {
            top = mid + 1;      // look below mid
        } else if( cmp < 0 ) {
            bot = mid - 1;      // look above mid
        } else                  // cmp == 0
        {
            *con_type = gMediaTypeList[mid].content_type;
            *con_subtype = gMediaTypeList[mid].content_subtype;
            return 0;
        }
    }

    return -1;
}

/************************************************************************
* Function: get_content_type											
*																		
* Parameters:															
*	IN const char* filename,											
*	OUT DOMString* content_type											
*																		
* Description: Based on the extension, clones an XML string based on	
*	type and content subtype. If content type and sub type are not		
*	found, unknown types are used										
*																		
* Returns:																
*	 0 - On Sucess														
*	 UPNP_E_OUTOF_MEMORY - on memory allocation failures				
************************************************************************/
XINLINE int
get_content_type( IN const char *filename,
                  OUT DOMString * content_type )
{
    const char *extension;
    const char *type,
     *subtype;
    xboolean ctype_found = FALSE;
    char *temp = NULL;
    int length = 0;

    ( *content_type ) = NULL;

    // get ext
    extension = strrchr( filename, '.' );
    if( extension != NULL ) {
        if( search_extension( extension + 1, &type, &subtype ) == 0 ) {
            ctype_found = TRUE;
        }
    }

    if( !ctype_found ) {
        // unknown content type
        type = gMediaTypes[APPLICATION_INDEX];
        subtype = "octet-stream";
    }

    length = strlen( type ) + strlen( "/" ) + strlen( subtype ) + 1;
    temp = ( char * )malloc( length );

    if( !temp ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    sprintf( temp, "%s/%s", type, subtype );
    ( *content_type ) = ixmlCloneDOMString( temp );

    free( temp );

    if( !content_type ) {
        return UPNP_E_OUTOF_MEMORY;
    }

    return 0;
}

/************************************************************************
* Function: glob_alias_init												
*																		
* Parameters:															
*	none																
*																		
* Description: Initialize the global XML document. Allocate buffers		
*	for the XML document												
*																		
* Returns:																
*	 void																
************************************************************************/
static XINLINE void
glob_alias_init( void )
{
    struct xml_alias_t *alias = &gAliasDoc;

    membuffer_init( &alias->doc );
    membuffer_init( &alias->name );
    alias->ct = NULL;
    alias->last_modified = 0;
}

/************************************************************************
* Function: is_valid_alias												
*																		
* Parameters:															
*	IN const struct xml_alias_t* alias ; XML alias object				
*																		
* Description: Check for the validity of the XML object buffer													
*																		
* Returns:																
*	 BOOLEAN															
************************************************************************/
static XINLINE xboolean
is_valid_alias( IN const struct xml_alias_t *alias )
{
    return alias->doc.buf != NULL;
}

/************************************************************************
* Function: alias_grab													
*																		
* Parameters:															
*	OUT struct xml_alias_t* alias ; XML alias object										
*																		
* Description: Copy the contents of the global XML document into the	
*	local OUT parameter																							
*																		
* Returns:																
*	 void																
************************************************************************/
static void
alias_grab( OUT struct xml_alias_t *alias )
{
    ithread_mutex_lock( &gWebMutex );

    assert( is_valid_alias( &gAliasDoc ) );

    memcpy( alias, &gAliasDoc, sizeof( struct xml_alias_t ) );
    *alias->ct = *alias->ct + 1;

    ithread_mutex_unlock( &gWebMutex );
}

/************************************************************************
* Function: alias_release												
*																		
* Parameters:															
*	IN struct xml_alias_t* alias ; XML alias object										
*																		
* Description: Release the XML document referred to by the IN parameter 
*	Free the allocated buffers associated with this object				
*																		
* Returns:																
*	void																
************************************************************************/
static void
alias_release( IN struct xml_alias_t *alias )
{
    ithread_mutex_lock( &gWebMutex );

    // ignore invalid alias
    if( !is_valid_alias( alias ) ) {
        ithread_mutex_unlock( &gWebMutex );
        return;
    }

    assert( alias->ct > 0 );

    *alias->ct = *alias->ct - 1;
    if( *alias->ct <= 0 ) {
        membuffer_destroy( &alias->doc );
        membuffer_destroy( &alias->name );
        free( alias->ct );
    }
    ithread_mutex_unlock( &gWebMutex );
}

/************************************************************************
* Function: web_server_set_alias										
*																		
* Parameters:															
*	alias_name: webserver name of alias; created by caller and freed by 
*				caller (doesn't even have to be malloc()d .)					
*	alias_content:	the xml doc; this is allocated by the caller; and	
*					freed by the web server											
*	alias_content_length: length of alias body in bytes					
*	last_modified:	time when the contents of alias were last			
*					changed (local time)											
*																		
* Description: Replaces current alias with the given alias. To remove	
*	the current alias, set alias_name to NULL.							
*																		
* Returns:																
*	0 - OK																
*	UPNP_E_OUTOF_MEMORY: note: alias_content is not freed here			
************************************************************************/
int
web_server_set_alias( IN const char *alias_name,
                      IN const char *alias_content,
                      IN size_t alias_content_length,
                      IN time_t last_modified )
{
    int ret_code;
    struct xml_alias_t alias;

    alias_release( &gAliasDoc );

    if( alias_name == NULL ) {
        // don't serve aliased doc anymore
        return 0;
    }

    assert( alias_content != NULL );

    membuffer_init( &alias.doc );
    membuffer_init( &alias.name );
    alias.ct = NULL;

    do {
        // insert leading /, if missing
        if( *alias_name != '/' ) {
            if( membuffer_assign_str( &alias.name, "/" ) != 0 ) {
                break;          // error; out of mem
            }
        }

        ret_code = membuffer_append_str( &alias.name, alias_name );
        if( ret_code != 0 ) {
            break;              // error
        }

        if( ( alias.ct = ( int * )malloc( sizeof( int ) ) ) == NULL ) {
            break;              // error
        }
        *alias.ct = 1;
        membuffer_attach( &alias.doc, ( char * )alias_content,
                          alias_content_length );

        alias.last_modified = last_modified;

        // save in module var
        ithread_mutex_lock( &gWebMutex );
        gAliasDoc = alias;
        ithread_mutex_unlock( &gWebMutex );

        return 0;
    } while( FALSE );

    // error handler

    // free temp alias
    membuffer_destroy( &alias.name );
    membuffer_destroy( &alias.doc );
    free( alias.ct );
    return UPNP_E_OUTOF_MEMORY;
}

/************************************************************************
* Function: web_server_init												
*																		
* Parameters:															
*	none																
*																		
* Description: Initilialize the different documents. Initialize the		
*	memory for root directory for web server. Call to initialize global 
*	XML document. Sets bWebServerState to WEB_SERVER_ENABLED			
*																		
* Returns:																
*	0 - OK																
*	UPNP_E_OUTOF_MEMORY: note: alias_content is not freed here			
************************************************************************/
int
web_server_init( void )
{
    int ret_code;

    if( bWebServerState == WEB_SERVER_DISABLED ) {
        media_list_init(  );    // decode media list
        membuffer_init( &gDocumentRootDir );
        glob_alias_init(  );

        pVirtualDirList = NULL;

        ret_code = ithread_mutex_init( &gWebMutex, NULL );
        if( ret_code == -1 ) {
            return UPNP_E_OUTOF_MEMORY;
        }
        bWebServerState = WEB_SERVER_ENABLED;
    }

    return 0;
}

/************************************************************************
* Function: web_server_destroy											
*																		
* Parameters:															
*	none																
*																		
* Description: Release memory allocated for the global web server root	
*	directory and the global XML document								
*	Resets the flag bWebServerState to WEB_SERVER_DISABLED				
*																		
* Returns:																
*	void																
************************************************************************/
void
web_server_destroy( void )
{
    int ret;

    if( bWebServerState == WEB_SERVER_ENABLED ) {
        membuffer_destroy( &gDocumentRootDir );
        alias_release( &gAliasDoc );

        ithread_mutex_lock( &gWebMutex );
        memset( &gAliasDoc, 0, sizeof( struct xml_alias_t ) );
        ithread_mutex_unlock( &gWebMutex );

        ret = ithread_mutex_destroy( &gWebMutex );
        assert( ret == 0 );
        bWebServerState = WEB_SERVER_DISABLED;
    }
}

/************************************************************************
* Function: get_file_info												
*																		
* Parameters:															
*	IN const char* filename ; 	Filename having the description document
*	OUT struct File_Info * info ; File information object having file 
*								  attributes such as filelength, when was 
*								  the file last modified, whether a file 
*								  or a directory and whether the file or
*								  directory is readable. 
*																		
* Description: Release memory allocated for the global web server root	
*	directory and the global XML document								
*	Resets the flag bWebServerState to WEB_SERVER_DISABLED				
*																		
* Returns:																
*	int																	
************************************************************************/
static int
get_file_info( IN const char *filename,
               OUT struct File_Info *info )
{
    int code;
    struct stat s;
    FILE *fp;
    int rc = 0;

    info->content_type = NULL;

    code = stat( filename, &s );
    if( code == -1 ) {
        return -1;
    }

    if( S_ISDIR( s.st_mode ) ) {
        info->is_directory = TRUE;
    } else if( S_ISREG( s.st_mode ) ) {
        info->is_directory = FALSE;
    } else {
        return -1;
    }

    // check readable
    fp = fopen( filename, "r" );
    info->is_readable = ( fp != NULL );
    if( fp ) {
        fclose( fp );
    }

    info->file_length = s.st_size;
    info->last_modified = s.st_mtime;

    rc = get_content_type( filename, &info->content_type );

    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "file info: %s, length: %d, last_mod=%s readable=%d\n",
                         filename, info->file_length,
                         asctime( gmtime( &info->last_modified ) ),
                         info->is_readable ); )

        return rc;
}

/************************************************************************
* Function: web_server_set_root_dir										
*																		
* Parameters:															
*	IN const char* root_dir ; String having the root directory for the 
*								document		 						
*																		
* Description: Assign the path specfied by the IN const char* root_dir	
*	parameter to the global Document root directory. Also check for		
*	path names ending in '/'											
*																		
* Returns:																
*	int																	
************************************************************************/
int
web_server_set_root_dir( IN const char *root_dir )
{
    int index;
    int ret;

    ret = membuffer_assign_str( &gDocumentRootDir, root_dir );
    if( ret != 0 ) {
        return ret;
    }
    // remove trailing '/', if any
    if( gDocumentRootDir.length > 0 ) {
        index = gDocumentRootDir.length - 1;    // last char
        if( gDocumentRootDir.buf[index] == '/' ) {
            membuffer_delete( &gDocumentRootDir, index, 1 );
        }
    }

    return 0;
}

/************************************************************************
* Function: get_alias													
*																		
* Parameters:															
*	IN const char* request_file ; request file passed in to be compared with
*	OUT struct xml_alias_t* alias ; xml alias object which has a file name 
*									stored										
*   OUT struct File_Info * info	 ; File information object which will be 
*									filled up if the file comparison 
*									succeeds										
*																		
* Description: Compare the files names between the one on the XML alias 
*	the one passed in as the input parameter. If equal extract file 
*	information
*																		
* Returns:																
*	TRUE - On Success													
*	FALSE if request is not an alias									
************************************************************************/
static XINLINE xboolean
get_alias( IN const char *request_file,
           OUT struct xml_alias_t *alias,
           OUT struct File_Info *info )
{
    int cmp;

    cmp = strcmp( alias->name.buf, request_file );
    if( cmp == 0 ) {
        // fill up info
        info->file_length = alias->doc.length;
        info->is_readable = TRUE;
        info->is_directory = FALSE;
        info->last_modified = alias->last_modified;
    }

    return cmp == 0;
}

/************************************************************************
* Function: isFileInVirtualDir											
*																		
* Parameters:															
*	IN char *filePath ; directory path to be tested for virtual directory
*																		
* Description: Compares filePath with paths from the list of virtual
*				directory lists
*																		
* Returns:																
*	BOOLEAN																
************************************************************************/
int
isFileInVirtualDir( IN char *filePath )
{
    virtualDirList *pCurVirtualDir;
    int webDirLen;

    pCurVirtualDir = pVirtualDirList;
    while( pCurVirtualDir != NULL ) {
        webDirLen = strlen( pCurVirtualDir->dirName );
        if( pCurVirtualDir->dirName[webDirLen - 1] == '/' ) {
            if( strncmp( pCurVirtualDir->dirName, filePath, webDirLen ) ==
                0 )
                return TRUE;
        } else {
            if( ( strncmp( pCurVirtualDir->dirName, filePath, webDirLen )
                  == 0 ) && ( filePath[webDirLen] == '/' ) )
                return TRUE;
        }

        pCurVirtualDir = pCurVirtualDir->next;
    }

    return FALSE;
}

/************************************************************************
* Function: ToUpperCase													
*																		
* Parameters:															
*	INOUT char * Str ; Input string to be converted					
*																		
* Description: Converts input string to upper case						
*																		
* Returns:																
*	int																	
************************************************************************/
int
ToUpperCase( char *Str )
{
    int i;

    for( i = 0; i < ( int )strlen( Str ); i++ )
        Str[i] = toupper( Str[i] );
    return 1;

}

/************************************************************************
* Function: StrStr														
*																		
* Parameters:															
*	IN char * S1 ; Input string
*	IN char * S2 ; Input sub-string										
*																		
* Description: Finds a substring from a string							
*																		
* Returns:																
*	char * ptr - pointer to the first occurence of S2 in S1				
************************************************************************/
char *
StrStr( char *S1,
        char *S2 )
{
    char *Str1,
     *Str2;
    char *Ptr,
     *Ret;
    int Pos;

    Str1 = ( char * )malloc( strlen( S1 ) + 2 );
    if(!Str1) return NULL;
    Str2 = ( char * )malloc( strlen( S2 ) + 2 );
    if (!Str2)
    {
       free(Str1);
        return NULL;
    }

    strcpy( Str1, S1 );
    strcpy( Str2, S2 );

    ToUpperCase( Str1 );
    ToUpperCase( Str2 );
    Ptr = strstr( Str1, Str2 );
    if( Ptr == NULL )
        return NULL;

    Pos = Ptr - Str1;

    Ret = S1 + Pos;

    free( Str1 );
    free( Str2 );
    return Ret;

}

/************************************************************************
* Function: StrTok														
*																		
* Parameters:															
*	IN char ** Src ; String containing the token													
*	IN char * del ; Set of delimiter characters														
*																		
* Description: Finds next token in a string							
*																		
* Returns:																
*	char * ptr - pointer to the first occurence of S2 in S1				
************************************************************************/
char *
StrTok( char **Src,
        char *Del )
{
    char *TmpPtr,
     *RetPtr;

    if( *Src != NULL ) {
        RetPtr = *Src;
        TmpPtr = strstr( *Src, Del );
        if( TmpPtr != NULL ) {
            *TmpPtr = '\0';
            *Src = TmpPtr + strlen( Del );
        } else
            *Src = NULL;

        return RetPtr;
    }

    return NULL;
}

/************************************************************************
* Function: GetNextRange												
*																		
* Parameters:															
*	IN char ** SrcRangeStr ; string containing the token / range										
*	OUT int * FirstByte ;	 gets the first byte of the token												
*	OUT int * LastByte	; gets the last byte of the token												
*																		
* Description: Returns a range of integers from a sring					
*																		
* Returns: int	;
*	always returns 1;				
************************************************************************/
int
GetNextRange( char **SrcRangeStr,
              int *FirstByte,
              int *LastByte )
{
    char *Ptr,
     *Tok;
    int i,
      F = -1,
      L = -1;
    int Is_Suffix_byte_Range = 1;

    if( *SrcRangeStr == NULL )
        return -1;

    Tok = StrTok( SrcRangeStr, "," );

    if( ( Ptr = strstr( Tok, "-" ) ) == NULL )
        return -1;
    *Ptr = ' ';
    sscanf( Tok, "%d%d", &F, &L );

    if( F == -1 || L == -1 ) {
        *Ptr = '-';
        for( i = 0; i < ( int )strlen( Tok ); i++ ) {
            if( Tok[i] == '-' ) {
                break;
            } else if( isdigit( Tok[i] ) ) {
                Is_Suffix_byte_Range = 0;
                break;
            }

        }

        if( Is_Suffix_byte_Range ) {
            *FirstByte = L;
            *LastByte = F;
            return 1;
        }
    }

    *FirstByte = F;
    *LastByte = L;
    return 1;

}

/************************************************************************
* Function: CreateHTTPRangeResponseHeader								
*																		
* Parameters:															
*	char * ByteRangeSpecifier ; String containing the range 	
*	long FileLength ; Length of the file													
*	OUT struct SendInstruction * Instr ; SendInstruction object	where the 
*										range operations will be stored
*																		
* Description: Fills in the Offset, read size and contents to send out	
*	as an HTTP Range Response											
*																		
* Returns:																
*	HTTP_BAD_REQUEST													
*	UPNP_E_OUTOF_MEMORY													
*	HTTP_REQUEST_RANGE_NOT_SATISFIABLE									
*	HTTP_OK																
************************************************************************/
int
CreateHTTPRangeResponseHeader( char *ByteRangeSpecifier,
                               long FileLength,
                               OUT struct SendInstruction *Instr )
{

    int FirstByte,
      LastByte;
    char *RangeInput,
     *Ptr;

    Instr->IsRangeActive = 1;
    Instr->ReadSendSize = FileLength;

    if( !ByteRangeSpecifier )
        return HTTP_BAD_REQUEST;

    RangeInput = malloc( strlen( ByteRangeSpecifier ) + 1 );
    if( !RangeInput )
        return UPNP_E_OUTOF_MEMORY;
    strcpy( RangeInput, ByteRangeSpecifier );

    //CONTENT-RANGE: bytes 222-3333/4000  HTTP_PARTIAL_CONTENT
    if( StrStr( RangeInput, "bytes" ) == NULL ||
        ( Ptr = StrStr( RangeInput, "=" ) ) == NULL ) {
        free( RangeInput );
        Instr->IsRangeActive = 0;
        return HTTP_BAD_REQUEST;
    }
    //Jump =
    Ptr = Ptr + 1;

    if( FileLength < 0 ) {
        free( RangeInput );
        return HTTP_REQUEST_RANGE_NOT_SATISFIABLE;
    }

    if( GetNextRange( &Ptr, &FirstByte, &LastByte ) != -1 ) {

        if( FileLength < FirstByte ) {
            free( RangeInput );
            return HTTP_REQUEST_RANGE_NOT_SATISFIABLE;
        }

        if( FirstByte >= 0 && LastByte >= 0 && LastByte >= FirstByte ) {
            if( LastByte >= FileLength )
                LastByte = FileLength - 1;

            Instr->RangeOffset = FirstByte;
            Instr->ReadSendSize = LastByte - FirstByte + 1;
            sprintf( Instr->RangeHeader, "CONTENT-RANGE: bytes %d-%d/%ld\r\n", FirstByte, LastByte, FileLength );   //Data between two range.
        } else if( FirstByte >= 0 && LastByte == -1
                   && FirstByte < FileLength ) {
            Instr->RangeOffset = FirstByte;
            Instr->ReadSendSize = FileLength - FirstByte;
            sprintf( Instr->RangeHeader,
                     "CONTENT-RANGE: bytes %d-%ld/%ld\r\n", FirstByte,
                     FileLength - 1, FileLength );
        } else if( FirstByte == -1 && LastByte > 0 ) {
            if( LastByte >= FileLength ) {
                Instr->RangeOffset = 0;
                Instr->ReadSendSize = FileLength;
                sprintf( Instr->RangeHeader,
                         "CONTENT-RANGE: bytes 0-%ld/%ld\r\n",
                         FileLength - 1, FileLength );
            } else {
                Instr->RangeOffset = FileLength - LastByte;
                Instr->ReadSendSize = LastByte;
                sprintf( Instr->RangeHeader,
                         "CONTENT-RANGE: bytes %ld-%ld/%ld\r\n",
                         FileLength - LastByte + 1, FileLength,
                         FileLength );
            }
        } else {
            free( RangeInput );
            return HTTP_REQUEST_RANGE_NOT_SATISFIABLE;
        }
    } else {
        free( RangeInput );
        return HTTP_REQUEST_RANGE_NOT_SATISFIABLE;
    }

    free( RangeInput );
    return HTTP_OK;
}

/************************************************************************
* Function: CheckOtherHTTPHeaders										
*																		
* Parameters:															
*	IN http_message_t * Req ;  HTTP Request message
*	OUT struct SendInstruction * RespInstr ; Send Instruction object to 
*							data for the response
*	int FileSize ;	Size of the file containing the request document
*																		
* Description: Get header id from the request parameter and take		
*	appropriate action based on the ids.								
*	as an HTTP Range Response											
*																		
* Returns:																
*	HTTP_BAD_REQUEST													
*	UPNP_E_OUTOF_MEMORY													
*	HTTP_REQUEST_RANGE_NOT_SATISFIABLE									
*	HTTP_OK																
************************************************************************/
int
CheckOtherHTTPHeaders( IN http_message_t * Req,
                       OUT struct SendInstruction *RespInstr,
                       int FileSize )
{
    http_header_t *header;
    ListNode *node;

    //NNS: dlist_node* node;
    int index,
      RetCode = HTTP_OK;
    char *TmpBuf;

    TmpBuf = ( char * )malloc( LINE_SIZE );
    if( !TmpBuf )
        return UPNP_E_OUTOF_MEMORY;

    node = ListHead( &Req->headers );

    while( node != NULL ) {
        header = ( http_header_t * ) node->item;

        // find header type.
        index = map_str_to_int( ( const char * )header->name.buf,
                                header->name.length, Http_Header_Names,
                                NUM_HTTP_HEADER_NAMES, FALSE );

        if( header->value.length >= LINE_SIZE ) {
            free( TmpBuf );
            TmpBuf = ( char * )malloc( header->value.length + 1 );
            if( !TmpBuf )
                return UPNP_E_OUTOF_MEMORY;
        }

        memcpy( TmpBuf, header->value.buf, header->value.length );
        TmpBuf[header->value.length] = '\0';
        if( index >= 0 )
            switch ( Http_Header_Names[index].id ) {
                case HDR_TE:   //Request
                    {
                        RespInstr->IsChunkActive = 1;

                        if( strlen( TmpBuf ) > strlen( "gzip" ) ) {
                            if( StrStr( TmpBuf, "trailers" ) != NULL ) {    //means client will accept trailer
                                RespInstr->IsTrailers = 1;
                            }
                        }
                    }
                    break;

                case HDR_CONTENT_LENGTH:
                    {
                        RespInstr->RecvWriteSize = atoi( TmpBuf );
                        break;
                    }

                case HDR_RANGE:
                    if( ( RetCode = CreateHTTPRangeResponseHeader( TmpBuf,
                                                                   FileSize,
                                                                   RespInstr ) )
                        != HTTP_OK ) {
                        free( TmpBuf );
                        return RetCode;
                    }
                    break;
                default:
                    /*
                       TODO 
                     */
                    /*
                       header.value is the value. 
                     */
                    /*
                       case HDR_CONTENT_TYPE: //return 1;
                       case HDR_CONTENT_LANGUAGE://return 1;
                       case HDR_LOCATION: //return 1;
                       case HDR_CONTENT_LOCATION://return 1;
                       case HDR_ACCEPT: //return 1;
                       case HDR_ACCEPT_CHARSET://return 1;
                       case HDR_ACCEPT_LANGUAGE://return 1;
                       case HDR_USER_AGENT: break;//return 1;
                     */

                    //Header check for encoding 
                    /*
                       case HDR_ACCEPT_RANGE: //Server capability.
                       case HDR_CONTENT_RANGE://Response.
                       case HDR_IF_RANGE:
                     */

                    //Header check for encoding 
                    /*
                       case HDR_ACCEPT_ENCODING:
                       if(StrStr(TmpBuf, "identity"))
                       {
                       break;
                       }
                       else return -1;
                       case HDR_CONTENT_ENCODING:
                       case HDR_TRANSFER_ENCODING: //Response
                     */
                    break;
            }

        node = ListNext( &Req->headers, node );

    }

    free( TmpBuf );
    return RetCode;
}

/************************************************************************
* Function: process_request												
*																		
* Parameters:															
*	IN http_message_t *req ; HTTP Request message												
*	OUT enum resp_type *rtype ; Tpye of response											
*	OUT membuffer *headers ; 												
*	OUT membuffer *filename ; Get filename from request document
*	OUT struct xml_alias_t *alias ; Xml alias document from the 
*									request document,										
*	OUT struct SendInstruction * RespInstr ; Send Instruction object 
*					where the response is set up.										
*																		
* Description: Processes the request and returns the result in the OUT	
*	parameters															
*																		
* Returns:																
*	HTTP_BAD_REQUEST													
*	UPNP_E_OUTOF_MEMORY													
*	HTTP_REQUEST_RANGE_NOT_SATISFIABLE									
*	HTTP_OK																
************************************************************************/
static int
process_request( IN http_message_t * req,
                 OUT enum resp_type *rtype,
                 OUT membuffer * headers,
                 OUT membuffer * filename,
                 OUT struct xml_alias_t *alias,
                 OUT struct SendInstruction *RespInstr )
{
    int code;
    int err_code;

    //membuffer content_type;
    char *request_doc;
    struct File_Info finfo;
    xboolean using_alias;
    xboolean using_virtual_dir;
    uri_type *url;
    char *temp_str;
    int resp_major,
      resp_minor;
    xboolean alias_grabbed;
    int dummy;
    struct UpnpVirtualDirCallbacks *pVirtualDirCallback;

    print_http_headers( req );

    url = &req->uri;
    assert( req->method == HTTPMETHOD_GET ||
            req->method == HTTPMETHOD_HEAD
            || req->method == HTTPMETHOD_POST
            || req->method == HTTPMETHOD_SIMPLEGET );

    // init
    request_doc = NULL;
    finfo.content_type = NULL;
    //membuffer_init( &content_type );
    alias_grabbed = FALSE;
    err_code = HTTP_INTERNAL_SERVER_ERROR;  // default error
    using_virtual_dir = FALSE;
    using_alias = FALSE;

    http_CalcResponseVersion( req->major_version, req->minor_version,
                              &resp_major, &resp_minor );

    //
    // remove dots
    //
    request_doc = malloc( url->pathquery.size + 1 );
    if( request_doc == NULL ) {
        goto error_handler;     // out of mem
    }
    memcpy( request_doc, url->pathquery.buff, url->pathquery.size );
    request_doc[url->pathquery.size] = '\0';
    dummy = url->pathquery.size;
    remove_escaped_chars( request_doc, &dummy );
    code = remove_dots( request_doc, url->pathquery.size );
    if( code != 0 ) {
        err_code = HTTP_FORBIDDEN;
        goto error_handler;
    }

    if( *request_doc != '/' ) {
        // no slash
        err_code = HTTP_BAD_REQUEST;
        goto error_handler;
    }

    if( isFileInVirtualDir( request_doc ) ) {
        using_virtual_dir = TRUE;
        RespInstr->IsVirtualFile = 1;
        if( membuffer_assign_str( filename, request_doc ) != 0 ) {
            goto error_handler;
        }

    } else {
        //
        // try using alias
        //
        if( is_valid_alias( &gAliasDoc ) ) {
            alias_grab( alias );
            alias_grabbed = TRUE;

            using_alias = get_alias( request_doc, alias, &finfo );

//+++Add by Shiang for work-around for the Vista
			if(using_alias && (strcmp(request_doc, "/description.xml") == 0))
				finfo.file_length += strlen("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
//---Add by Shiang for work-around for the Vista

            if( using_alias == TRUE ) {
//+++Modify by Shiang for work-around for the Vista
//                finfo.content_type = ixmlCloneDOMString( "text/xml" );
                finfo.content_type = ixmlCloneDOMString( "text/xml; charset=\"utf-8\"" );
//---Modify by Shiang for work-around for the Vista

                if( finfo.content_type == NULL ) {
                    goto error_handler;
                }
            }
        }
    }

    if( using_virtual_dir ) {
        if( req->method != HTTPMETHOD_POST ) {
            // get file info
            pVirtualDirCallback = &virtualDirCallback;
            if( pVirtualDirCallback->get_info( filename->buf, &finfo ) !=
                0 ) {
                err_code = HTTP_NOT_FOUND;
                goto error_handler;
            }
            // try index.html if req is a dir
            if( finfo.is_directory ) {
                if( filename->buf[filename->length - 1] == '/' ) {
                    temp_str = "index.html";
                } else {
                    temp_str = "/index.html";
                }
                if( membuffer_append_str( filename, temp_str ) != 0 ) {
                    goto error_handler;
                }
                // get info
                if( ( pVirtualDirCallback->
                      get_info( filename->buf, &finfo ) != UPNP_E_SUCCESS )
                    || finfo.is_directory ) {
                    err_code = HTTP_NOT_FOUND;
                    goto error_handler;
                }
            }
            // not readable
            if( !finfo.is_readable ) {
                err_code = HTTP_FORBIDDEN;
                goto error_handler;
            }
            // finally, get content type
            // if ( get_content_type(filename->buf, &content_type) != 0 )
            //{
            //  goto error_handler;
            // }
        }
    } else if( !using_alias ) {
        if( gDocumentRootDir.length == 0 ) {
            goto error_handler;
        }
        //
        // get file name
        //

        // filename str
        if( membuffer_assign_str( filename, gDocumentRootDir.buf ) != 0 ||
            membuffer_append_str( filename, request_doc ) != 0 ) {
            goto error_handler; // out of mem
        }
        // remove trailing slashes
        while( filename->length > 0 &&
               filename->buf[filename->length - 1] == '/' ) {
            membuffer_delete( filename, filename->length - 1, 1 );
        }

        if( req->method != HTTPMETHOD_POST ) {
            // get info on file
            if( get_file_info( filename->buf, &finfo ) != 0 ) {
                err_code = HTTP_NOT_FOUND;
                goto error_handler;
            }
            // try index.html if req is a dir
            if( finfo.is_directory ) {
                if( filename->buf[filename->length - 1] == '/' ) {
                    temp_str = "index.html";
                } else {
                    temp_str = "/index.html";
                }
                if( membuffer_append_str( filename, temp_str ) != 0 ) {
                    goto error_handler;
                }
                // get info
                if( get_file_info( filename->buf, &finfo ) != 0 ||
                    finfo.is_directory ) {
                    err_code = HTTP_NOT_FOUND;
                    goto error_handler;
                }
            }
            // not readable
            if( !finfo.is_readable ) {
                err_code = HTTP_FORBIDDEN;
                goto error_handler;
            }

        }
        // finally, get content type
        //      if ( get_content_type(filename->buf, &content_type) != 0 )
        //      {
        //          goto error_handler;
        //      }
    }

    RespInstr->ReadSendSize = finfo.file_length;

    //Check other header field.
    if( ( err_code =
          CheckOtherHTTPHeaders( req, RespInstr,
                                 finfo.file_length ) ) != HTTP_OK ) {
        goto error_handler;
    }

    if( req->method == HTTPMETHOD_POST ) {
        *rtype = RESP_POST;
        err_code = UPNP_E_SUCCESS;
        goto error_handler;
    }

    if( RespInstr->IsRangeActive && RespInstr->IsChunkActive ) {

/* - PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld at users.sourceforge.net>
 * added X-User-Agent header
 */
        
        //Content-Range: bytes 222-3333/4000  HTTP_PARTIAL_CONTENT
        //Transfer-Encoding: chunked
        // K means add chunky header ang G means range header.
        if( http_MakeMessage( headers, resp_major, resp_minor, "RTGKDstcSXcCc", HTTP_PARTIAL_CONTENT, // status code
                              // RespInstr->ReadSendSize,// content length
                              finfo.content_type,
                              //     content_type.buf,            // content type
                              RespInstr,    // Range
                              "LAST-MODIFIED: ",
                              &finfo.last_modified,
                              X_USER_AGENT) != 0 ) {
            goto error_handler;
        }
    } else if( RespInstr->IsRangeActive && !RespInstr->IsChunkActive ) {

        //Content-Range: bytes 222-3333/4000  HTTP_PARTIAL_CONTENT
        //Transfer-Encoding: chunked
        // K means add chunky header ang G means range header.
        if( http_MakeMessage( headers, resp_major, resp_minor, "RNTGDstcSXcCc", HTTP_PARTIAL_CONTENT, // status code
                              RespInstr->ReadSendSize,  // content length
                              finfo.content_type,
                              //content_type.buf,            // content type
                              RespInstr,    //Range Info
                              "LAST-MODIFIED: ",
                              &finfo.last_modified,
                              X_USER_AGENT) != 0 ) {
            goto error_handler;
        }

    } else if( !RespInstr->IsRangeActive && RespInstr->IsChunkActive ) {

        //Content-Range: bytes 222-3333/4000  HTTP_PARTIAL_CONTENT
        //Transfer-Encoding: chunked
        // K means add chunky header ang G means range header.
        if( http_MakeMessage( headers, resp_major, resp_minor, "RKTDstcSXcCc", HTTP_OK,   // status code
                              //RespInstr->ReadSendSize,// content length
                              finfo.content_type,
                              // content_type.buf,            // content type
                              "LAST-MODIFIED: ",
                              &finfo.last_modified,
                              X_USER_AGENT) != 0 ) {
            goto error_handler;
        }

    } else {
        if( RespInstr->ReadSendSize >= 0 ) {
            //Content-Range: bytes 222-3333/4000  HTTP_PARTIAL_CONTENT
            //Transfer-Encoding: chunked
            // K means add chunky header ang G means range header.
            if( http_MakeMessage( headers, resp_major, resp_minor, "RNTDstcSXcCc", HTTP_OK,   // status code
                                  RespInstr->ReadSendSize,  // content length
                                  finfo.content_type,
                                  //content_type.buf,          // content type
                                  "LAST-MODIFIED: ",
                                  &finfo.last_modified,
                                  X_USER_AGENT) != 0 ) {
                goto error_handler;
            }
        } else {
            //Content-Range: bytes 222-3333/4000  HTTP_PARTIAL_CONTENT
            //Transfer-Encoding: chunked
            // K means add chunky header ang G means range header.
            if( http_MakeMessage( headers, resp_major, resp_minor, "RTDstcSXcCc", HTTP_OK,    // status code
                                  //RespInstr->ReadSendSize,// content length
                                  finfo.content_type,
                                  //content_type.buf,          // content type
                                  "LAST-MODIFIED: ",
                                  &finfo.last_modified,
                                  X_USER_AGENT) != 0 ) {
                goto error_handler;
            }
        }
    }
/* -- PATCH END -- */

    if( req->method == HTTPMETHOD_HEAD ) {
        *rtype = RESP_HEADERS;
    } else if( using_alias ) {
        // GET xml
        *rtype = RESP_XMLDOC;
    } else if( using_virtual_dir ) {
        *rtype = RESP_WEBDOC;
    } else {
        // GET filename
        *rtype = RESP_FILEDOC;
    }

    //simple get http 0.9 as specified in http 1.0
    //don't send headers
    if( req->method == HTTPMETHOD_SIMPLEGET ) {
        membuffer_destroy( headers );
    }

    err_code = UPNP_E_SUCCESS;

  error_handler:
    free( request_doc );
    ixmlFreeDOMString( finfo.content_type );
    //  membuffer_destroy( &content_type );
    if( err_code != UPNP_E_SUCCESS && alias_grabbed ) {
        alias_release( alias );
    }

    return err_code;
}

/************************************************************************
* Function: http_RecvPostMessage										
*																		
* Parameters:															
*	http_parser_t* parser ; HTTP Parser object							
*	IN SOCKINFO *info ; Socket Information object													
*	char * filename ; 	File where received data is copied to
*	struct SendInstruction * Instr	; Send Instruction object which gives
*			information whether the file is a virtual file or not.
*																		
* Description: Receives the HTTP post message														
*																		
* Returns:																
*	HTTP_INTERNAL_SERVER_ERROR											
*	HTTP_UNAUTHORIZED													
*	HTTP_REQUEST_RANGE_NOT_SATISFIABLE									
*	HTTP_OK																
************************************************************************/
int
http_RecvPostMessage( http_parser_t * parser,
                      IN SOCKINFO * info,
                      char *filename,
                      struct SendInstruction *Instr )
{

    unsigned int Data_Buf_Size = 1024;
    char Buf[1024];
    int Timeout = 0;
    long Num_Write = 0;
    FILE *Fp;
    parse_status_t status = PARSE_OK;
    xboolean ok_on_close = FALSE;
    unsigned int entity_offset = 0;
    int num_read = 0;
    int ret_code = 0;

    if( Instr && Instr->IsVirtualFile ) {

        Fp = virtualDirCallback.open( filename, UPNP_WRITE );
        if( Fp == NULL ) {
            return HTTP_INTERNAL_SERVER_ERROR;
        }

    } else {
        Fp = fopen( filename, "wb" );
        if( Fp == NULL ) {
            return HTTP_UNAUTHORIZED;
        }
    }

    parser->position = POS_ENTITY;

    do {
        //first parse what has already been gotten
        if( parser->position != POS_COMPLETE ) {
            status = parser_parse_entity( parser );
        }

        if( status == PARSE_INCOMPLETE_ENTITY ) {
            // read until close
            ok_on_close = TRUE;
        } else if( ( status != PARSE_SUCCESS )
                   && ( status != PARSE_CONTINUE_1 )
                   && ( status != PARSE_INCOMPLETE ) ) {
            //error
            fclose( Fp );
            return HTTP_BAD_REQUEST;
        }
        //read more if necessary entity
        while( ( ( entity_offset + Data_Buf_Size ) >
                 parser->msg.entity.length )
               && ( parser->position != POS_COMPLETE ) ) {
            num_read = sock_read( info, Buf, sizeof( Buf ), &Timeout );
            if( num_read > 0 ) {
                // append data to buffer
                ret_code = membuffer_append( &parser->msg.msg,
                                             Buf, num_read );
                if( ret_code != 0 ) {
                    // set failure status
                    parser->http_error_code = HTTP_INTERNAL_SERVER_ERROR;
                    return HTTP_INTERNAL_SERVER_ERROR;
                }
                status = parser_parse_entity( parser );
                if( status == PARSE_INCOMPLETE_ENTITY ) {
                    // read until close
                    ok_on_close = TRUE;
                } else if( ( status != PARSE_SUCCESS )
                           && ( status != PARSE_CONTINUE_1 )
                           && ( status != PARSE_INCOMPLETE ) ) {
                    return HTTP_BAD_REQUEST;
                }
            } else if( num_read == 0 ) {
                if( ok_on_close ) {
                    DBGONLY( UpnpPrintf
                             ( UPNP_INFO, HTTP, __FILE__, __LINE__,
                               "<<< (RECVD) <<<\n%s\n-----------------\n",
                               parser->msg.msg.buf );
                             //print_http_headers( &parser->msg );
                         )

                        parser->position = POS_COMPLETE;
                } else {
                    // partial msg
                    parser->http_error_code = HTTP_BAD_REQUEST; // or response
                    return HTTP_BAD_REQUEST;
                }
            } else {
                return num_read;
            }
        }

        if( ( entity_offset + Data_Buf_Size ) > parser->msg.entity.length ) {
            Data_Buf_Size = parser->msg.entity.length - entity_offset;
        }

        memcpy( Buf, &parser->msg.msg.buf[parser->entity_start_position
                                          + entity_offset],
                Data_Buf_Size );
        entity_offset += Data_Buf_Size;

        if( Instr->IsVirtualFile ) {
            Num_Write = virtualDirCallback.write( Fp, Buf, Data_Buf_Size );
            if( Num_Write < 0 ) {
                virtualDirCallback.close( Fp );
                return HTTP_INTERNAL_SERVER_ERROR;
            }
        } else {
            Num_Write = fwrite( Buf, 1, Data_Buf_Size, Fp );
            if( Num_Write < 0 ) {
                fclose( Fp );
                return HTTP_INTERNAL_SERVER_ERROR;
            }
        }

    } while( ( parser->position != POS_COMPLETE )
             || ( entity_offset != parser->msg.entity.length ) );

    if( Instr->IsVirtualFile ) {
        virtualDirCallback.close( Fp );
    } else {
        fclose( Fp );
    }

    /*
       while(TotalByteReceived < Instr->RecvWriteSize &&
       (NumReceived = sock_read(info,Buf, Data_Buf_Size,&Timeout) ) > 0 ) 
       {
       TotalByteReceived = TotalByteReceived + NumReceived;
       Num_Write = virtualDirCallback.write(Fp, Buf, NumReceived);
       if (ferror(Fp))
       {
       virtualDirCallback.close(Fp);
       return HTTP_INTERNAL_SERVER_ERROR;
       }
       }

       if(TotalByteReceived < Instr->RecvWriteSize)
       {
       return HTTP_INTERNAL_SERVER_ERROR;
       }

       virtualDirCallback.close(Fp);
       }
     */
    return HTTP_OK;
}

/************************************************************************
* Function: web_server_callback											
*																		
* Parameters:															
*	IN http_parser_t *parser ; HTTP Parser Object						
*	INOUT http_message_t* req ; HTTP Message request										
*	IN SOCKINFO *info ;			Socket information object													
*																		
* Description: main entry point into web server;						
*	handles HTTP GET and HEAD requests									
*																		
* Returns:																
*	void																
************************************************************************/
void
web_server_callback( IN http_parser_t * parser,
                     INOUT http_message_t * req,
                     IN SOCKINFO * info )
{
    int ret;
    int timeout = 0;
    enum resp_type rtype = 0;
    membuffer headers;
    membuffer filename;
    struct xml_alias_t xmldoc;
    struct SendInstruction RespInstr;

    //Initialize instruction header.
    RespInstr.IsVirtualFile = 0;
    RespInstr.IsChunkActive = 0;
    RespInstr.IsRangeActive = 0;
    RespInstr.IsTrailers = 0;
    // init
    membuffer_init( &headers );
    membuffer_init( &filename );

    //Process request should create the different kind of header depending on the
    //the type of request.
    ret =
        process_request( req, &rtype, &headers, &filename, &xmldoc,
                         &RespInstr );
    if( ret != UPNP_E_SUCCESS ) {
        // send error code
        http_SendStatusResponse( info, ret, req->major_version,
                                 req->minor_version );
    } else {
        //
        // send response
        switch ( rtype ) {
            case RESP_FILEDOC: // send file, I = further instruction to send data.
                http_SendMessage( info, &timeout, "Ibf", &RespInstr,
                                  headers.buf, headers.length,
                                  filename.buf );
                break;

            case RESP_XMLDOC:  // send xmldoc , I = further instruction to send data.
//+++Modify by shiang for work-around for Vista
#if 0
                http_SendMessage( info, &timeout, "Ibb", &RespInstr,
                                  headers.buf, headers.length,
                                  xmldoc.doc.buf, xmldoc.doc.length );
#else
				DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "shiang: xmldoc.name=%s!\n", xmldoc.name.buf);
         		)
            	if(strcmp(xmldoc.name.buf, "/description.xml") == 0)
            	{
            		char xmlHeader[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
					int xmlHeaderLen;

					xmlHeaderLen = strlen(xmlHeader);
					http_SendMessage( info, &timeout, "Ibbb", &RespInstr,
                                  headers.buf, headers.length, 
                                  xmlHeader, xmlHeaderLen,
                                  xmldoc.doc.buf, xmldoc.doc.length );
            	} else {
	                http_SendMessage( info, &timeout, "Ibb", &RespInstr,
                                  headers.buf, headers.length,
                                  xmldoc.doc.buf, xmldoc.doc.length );
            	}
#endif
//---Modify by shiang for work-around for Vista
                alias_release( &xmldoc );
                break;

            case RESP_WEBDOC:  //, I = further instruction to send data.
                /*
                   http_SendVirtualDirDoc( info, &timeout, "Ibf",&RespInstr,
                   headers.buf, headers.length,
                   filename.buf );  
                 */
                http_SendMessage( info, &timeout, "Ibf", &RespInstr,
                                  headers.buf, headers.length,
                                  filename.buf );
                break;

            case RESP_HEADERS: // headers only
                http_SendMessage( info, &timeout, "b",
                                  headers.buf, headers.length );

                break;
            case RESP_POST:    // headers only
                ret =
                    http_RecvPostMessage( parser, info, filename.buf,
                                          &RespInstr );
                //Send response.

/* - PATCH START - Sergey 'Jin' Bostandzhyan <jin_eld at users.sourceforge.net>
 * added X-User-Agent header
 */
                http_MakeMessage( &headers, 1, 1, "RTDSXcCc", ret,
                                  "text/html", X_USER_AGENT );
/* - PATCH END --- */

                http_SendMessage( info, &timeout, "b", headers.buf,
                                  headers.length );
                break;

            default:
                assert( 0 );
        }
    }

    DBGONLY( UpnpPrintf( UPNP_INFO, HTTP, __FILE__, __LINE__,
                         "webserver: request processed...\n" );
         )

        membuffer_destroy( &headers );
    membuffer_destroy( &filename );
}
