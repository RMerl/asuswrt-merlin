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
* Purpose: This file contains functions for copying strings based on 
* different options.
************************************************************************/

#include "config.h"
#include "upnp.h"
#include "util.h"

/************************************************************************
*	Function :	linecopy
*
*	Parameters :
*		OUT char dest[LINE_SIZE] ;	output buffer
*		IN const char* src ;	input buffer
*
*	Description : Copy no of bytes spcified by the LINE_SIZE constant, 
*		from the source buffer. Null terminate the destination buffer
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
linecopy( OUT char dest[LINE_SIZE],
          IN const char *src )
{
    strncpy( dest, src, LINE_SIZE - 1 );
    dest[LINE_SIZE - 1] = '\0'; // null-terminate if len(src) >= LINE_SIZE
}

/************************************************************************
*	Function :	namecopy
*
*	Parameters :
*		OUT char dest[NAME_SIZE] ;	output buffer
*		IN const char* src ;	input buffer
*
*	Description : Copy no of bytes spcified by the NAME_SIZE constant, 
*		from the source buffer. Null terminate the destination buffer
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
namecopy( OUT char dest[NAME_SIZE],
          IN const char *src )
{
    strncpy( dest, src, NAME_SIZE - 1 );
    dest[NAME_SIZE - 1] = '\0'; // null-terminate if len(src) >= NAME_SIZE
}

/************************************************************************
*	Function :	linecopylen
*
*	Parameters :
*		OUT char dest[LINE_SIZE] ;	output buffer
*		IN const char* src ;	input buffer
*		IN size_t srclen ;	bytes to be copied.
*
*	Description : Determine if the srclen passed in paramter is less than 
*		the permitted LINE_SIZE. If it is use the passed parameter, if not
*		use the permitted LINE_SIZE as the length parameter
*		Copy no of bytes spcified by the LINE_SIZE constant, 
*		from the source buffer. Null terminate the destination buffer
*
*	Return : void ;
*
*	Note :
************************************************************************/
void
linecopylen( OUT char dest[LINE_SIZE],
             IN const char *src,
             IN size_t srclen )
{
    int len;

    len = srclen < ( LINE_SIZE - 1 ) ? srclen : ( LINE_SIZE - 1 );
    strncpy( dest, src, len );
    dest[len] = '\0';
}
