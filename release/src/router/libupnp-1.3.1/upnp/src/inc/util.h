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

#ifndef UTIL_H
#define UTIL_H
#include "upnp.h"

// usually used to specify direction of parameters in functions
#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif


#ifdef NO_DEBUG
#define DBG(x)
#else
#define DBG(x) x
#endif

#define GEMD_OUT_OF_MEMORY -1
#define EVENT_TIMEDOUT -2
#define EVENT_TERMINATE	-3



#define max(a, b)   (((a)>(b))? (a):(b))
#define min(a, b)   (((a)<(b))? (a):(b))



// boolean type in C
typedef char xboolean;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

///////////////////////////
// funcs

#ifdef __cplusplus
extern "C" {
#endif

//void log_error( IN const char *fmt, ... );

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
void linecopy( OUT char dest[LINE_SIZE], IN const char* src );

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
void namecopy( OUT char dest[NAME_SIZE], IN const char* src );

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
void linecopylen( OUT char dest[LINE_SIZE], IN const char* src, IN size_t srclen );


#ifdef __cplusplus
} // extern C
#endif

//////////////////////////////////

// C specific
#ifndef __cplusplus

#define		XINLINE inline

#endif // __cplusplus

#endif /* GENLIB_UTIL_UTIL_H */
