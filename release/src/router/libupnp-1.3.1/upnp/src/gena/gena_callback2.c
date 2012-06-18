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
#if EXCLUDE_GENA == 0
#include "gena.h"
#include "gena_device.h"
#include "gena_ctrlpt.h"

#include "httpparser.h"
#include "httpreadwrite.h"
#include "statcodes.h"
#include "unixutil.h"

/************************************************************************
* Function : error_respond									
*																	
* Parameters:														
*	IN SOCKINFO *info: Structure containing information about the socket
*	IN int error_code: error code that will be in the GENA response
*	IN http_message_t* hmsg: GENA request Packet 
*
* Description:														
*	This function send an error message to the control point in the case
*	incorrect GENA requests.
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
void
error_respond( IN SOCKINFO * info,
               IN int error_code,
               IN http_message_t * hmsg )
{
    int major,
      minor;

    // retrieve the minor and major version from the GENA request
    http_CalcResponseVersion( hmsg->major_version,
                              hmsg->minor_version, &major, &minor );

    http_SendStatusResponse( info, error_code, major, minor );
}

/************************************************************************
* Function : genaCallback									
*																	
* Parameters:														
*	IN http_parser_t *parser: represents the parse state of the request
*	IN http_message_t* request: HTTP message containing GENA request
*	INOUT SOCKINFO *info: Structure containing information about the socket
*
* Description:														
*	This is the callback function called by the miniserver to handle 
*	incoming GENA requests. 
*
* Returns: int
*	UPNP_E_SUCCESS if successful else appropriate error
***************************************************************************/
void
genaCallback( IN http_parser_t * parser,
              IN http_message_t * request,
              INOUT SOCKINFO * info )
{
    xboolean found_function = FALSE;

    if( request->method == HTTPMETHOD_SUBSCRIBE ) {
        DEVICEONLY( found_function = TRUE;
                    if( httpmsg_find_hdr( request, HDR_NT, NULL ) == NULL )
                    {
                    // renew subscription
                    gena_process_subscription_renewal_request
                    ( info, request );}
                    else
                    {
                    // subscribe
                    gena_process_subscription_request( info, request );}

                    DBGONLY( UpnpPrintf
                             ( UPNP_ALL, GENA, __FILE__, __LINE__,
                               "got subscription request\n" ); )
             )
            }
            else
        if( request->method == HTTPMETHOD_UNSUBSCRIBE ) {
            DEVICEONLY( found_function = TRUE;
                        // unsubscribe
                        gena_process_unsubscribe_request( info,
                                                          request ); )
        } else if( request->method == HTTPMETHOD_NOTIFY ) {
            CLIENTONLY( found_function = TRUE;
                        // notify
                        gena_process_notification_event( info, request ); )
        }

        if( !found_function ) {
            // handle missing functions of device or ctrl pt
            error_respond( info, HTTP_NOT_IMPLEMENTED, request );
        }
    }
#endif // EXCLUDE_GENA
