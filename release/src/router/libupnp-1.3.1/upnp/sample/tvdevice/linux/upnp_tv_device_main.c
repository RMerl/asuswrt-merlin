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

#include <stdio.h>
#include "sample_util.h"
#include "upnp_tv_device.h"

/******************************************************************************
 * linux_print
 *
 * Description: 
 *       Prints a string to standard out.
 *
 * Parameters:
 *    None
 *
 *****************************************************************************/
void
linux_print( const char *string )
{
    printf( "%s", string );
}

/******************************************************************************
 * TvDeviceCommandLoop
 *
 * Description: 
 *       Function that receives commands from the user at the command prompt
 *       during the lifetime of the device, and calls the appropriate
 *       functions for those commands. Only one command, exit, is currently
 *       defined.
 *
 * Parameters:
 *    None
 *
 *****************************************************************************/
void *
TvDeviceCommandLoop( void *args )
{
    int stoploop = 0;
    char cmdline[100];
    char cmd[100];

    while( !stoploop ) {
        sprintf( cmdline, " " );
        sprintf( cmd, " " );

        SampleUtil_Print( "\n>> " );

        // Get a command line
        fgets( cmdline, 100, stdin );

        sscanf( cmdline, "%s", cmd );

        if( strcasecmp( cmd, "exit" ) == 0 ) {
            SampleUtil_Print( "Shutting down...\n" );
            TvDeviceStop(  );
            exit( 0 );
        } else {
            SampleUtil_Print( "\n   Unknown command: %s\n\n", cmd );
            SampleUtil_Print( "   Valid Commands:\n" );
            SampleUtil_Print( "     Exit\n\n" );
        }

    }

    return NULL;
}

/******************************************************************************
 * main
 *
 * Description: 
 *       Main entry point for tv device application.
 *       Initializes and registers with the sdk.
 *       Initializes the state stables of the service.
 *       Starts the command loop.
 *
 * Parameters:
 *    int argc  - count of arguments
 *    char ** argv -arguments. The application 
 *                  accepts the following optional arguments:
 *
 *                  -ip ipaddress 
 *                  -port port
 *		    -desc desc_doc_name 
 *	            -webdir web_dir_path"
 *		    -help 
 *                 
 *
 *****************************************************************************/
int
main( IN int argc,
      IN char **argv )
{

    unsigned int portTemp = 0;
    char *ip_address = NULL,
     *desc_doc_name = NULL,
     *web_dir_path = NULL;
    ithread_t cmdloop_thread;
    int code;
    unsigned int port = 0;
    int sig;
    sigset_t sigs_to_catch;

    int i = 0;

    SampleUtil_Initialize( linux_print );

    //Parse options
    for( i = 1; i < argc; i++ ) {
        if( strcmp( argv[i], "-ip" ) == 0 ) {
            ip_address = argv[++i];
        } else if( strcmp( argv[i], "-port" ) == 0 ) {
            sscanf( argv[++i], "%u", &portTemp );
        } else if( strcmp( argv[i], "-desc" ) == 0 ) {
            desc_doc_name = argv[++i];
        } else if( strcmp( argv[i], "-webdir" ) == 0 ) {
            web_dir_path = argv[++i];
        } else if( strcmp( argv[i], "-help" ) == 0 ) {
            SampleUtil_Print( "Usage: %s -ip ipaddress -port port"
                              " -desc desc_doc_name -webdir web_dir_path"
                              " -help (this message)\n", argv[0] );
            SampleUtil_Print( "\tipaddress:     IP address of the device"
                              " (must match desc. doc)\n" );
            SampleUtil_Print( "\t\te.g.: 192.168.0.4\n" );
            SampleUtil_Print( "\tport:          Port number to use for "
                              "receiving UPnP messages (must match desc. doc)\n" );
            SampleUtil_Print( "\t\te.g.: 5431\n" );
            SampleUtil_Print
                ( "\tdesc_doc_name: name of device description document\n" );
            SampleUtil_Print( "\t\te.g.: tvdevicedesc.xml\n" );
            SampleUtil_Print
                ( "\tweb_dir_path: Filesystem path where web files "
                  "related to the device are stored\n" );
            SampleUtil_Print( "\t\te.g.: /upnp/sample/tvdevice/web\n" );
            exit( 1 );
        }
    }

    port = ( unsigned short )portTemp;

    TvDeviceStart( ip_address, port, desc_doc_name, web_dir_path,
                   linux_print );

    /*
       start a command loop thread 
     */
    code = ithread_create( &cmdloop_thread, NULL, TvDeviceCommandLoop,
                           NULL );

    /*
       Catch Ctrl-C and properly shutdown 
     */
    sigemptyset( &sigs_to_catch );
    sigaddset( &sigs_to_catch, SIGINT );
    sigwait( &sigs_to_catch, &sig );

    SampleUtil_Print( "Shutting down on signal %d...\n", sig );
    TvDeviceStop(  );
    exit( 0 );
}
