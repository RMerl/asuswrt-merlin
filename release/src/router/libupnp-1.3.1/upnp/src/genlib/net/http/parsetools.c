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
* Purpose: This file a function to extract the header information from	*
* an http message and then matches the data with XML data.				*
************************************************************************/

#include "config.h"
#include <assert.h>
#include "util.h"
#include "membuffer.h"
#include "httpparser.h"
#include "statcodes.h"
#include "parsetools.h"

/************************************************************************
* Function: has_xml_content_type										
*																		
* Parameters:															
*	IN http_message_t* hmsg	; HTTP Message object
*																		
* Description: Find the header from the HTTP message and match the		
*	header for xml data.												
*																		
* Returns:																
*	 BOOLEAN															
************************************************************************/
xboolean
has_xml_content_type( IN http_message_t * hmsg )
{
    memptr hdr_value;

    assert( hmsg );

    // find 'content-type' header which must have text/xml
    if( httpmsg_find_hdr( hmsg, HDR_CONTENT_TYPE, &hdr_value ) != NULL &&
        matchstr( hdr_value.buf, hdr_value.length,
                  "%itext%w/%wxml" ) == PARSE_OK ) {
        return TRUE;
    }
    return FALSE;
}
